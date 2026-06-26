/**
 * wireless_auth.c — Wireless Authentication: 4-Way Handshake, PMKID, 802.1X
 *
 * Implements the WPA2 4-way handshake protocol per IEEE 802.11i-2004.
 * Each function handles a specific message exchange step.
 *
 * Knowledge Levels: L2 (authentication concepts), L5 (handshake protocol),
 *                   L6 (WPA2 4-way handshake as canonical problem)
 *
 * References:
 *   IEEE 802.11i-2004, Section 8.5.3: 4-Way Handshake
 *   IEEE 802.1X-2010: Port-Based Network Access Control
 */
#include "wireless_auth.h"
#include "wireless_crypto.h"
#include "wireless_key_mgmt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Nonce Generation
 * ============================================================================ */

/**
 * generate_nonce — Generate cryptographically random bytes
 *
 * For real deployment, this would use /dev/urandom or a hardware TRNG.
 * Here we use a combination of time(), stack address, and HMAC-based
 * PRNG to produce unpredictable output suitable for proof-of-concept.
 *
 * L2 Concept: Fresh nonces prevent replay attacks. Each handshake
 * instance must use unique, unpredictable nonces.
 *
 * L4 Theorem: If ANonce and SNonce are truly random and never reused,
 * the 4-way handshake provides key confirmation and liveness guarantee.
 */
int generate_nonce(uint8_t *buf, size_t len)
{
    sha256_ctx_t sha;
    uint8_t seed_input[64];
    uint8_t digest[SHA256_DIGEST_SIZE];
    size_t pos = 0;
    int counter = 0;

    if (!buf || len == 0) return -1;

    /* Build seed from time, clock, and stack entropy */
    time_t t = time(NULL);
    clock_t c = clock();
    uintptr_t stack_addr = (uintptr_t)&buf;

    memset(seed_input, 0, sizeof(seed_input));
    memcpy(seed_input, &t, sizeof(t));
    memcpy(seed_input + 8, &c, sizeof(c));
    memcpy(seed_input + 16, &stack_addr, sizeof(stack_addr));

    /* Generate nonce in SHA-256 output blocks (CTR-like) */
    while (pos < len) {
        uint8_t counter_byte = (uint8_t)(counter++);
        sha256_init(&sha);
        sha256_update(&sha, seed_input, sizeof(seed_input));
        sha256_update(&sha, &counter_byte, 1);
        sha256_final(&sha, digest);

        size_t to_copy = (len - pos < SHA256_DIGEST_SIZE) ?
                          (len - pos) : SHA256_DIGEST_SIZE;
        memcpy(buf + pos, digest, to_copy);
        pos += to_copy;
    }

    return 0;
}

/* ============================================================================
 * 4-Way Handshake Implementation
 * ============================================================================ */

void handshake_init(handshake_ctx_t *ctx,
                    const uint8_t *ssid, size_t ssid_len,
                    const uint8_t *sta_mac, const uint8_t *ap_mac,
                    int is_enterprise)
{
    if (!ctx) return;

    memset(ctx, 0, sizeof(handshake_ctx_t));
    ctx->state = HANDSHAKE_IDLE;
    ctx->is_enterprise = is_enterprise;

    if (ssid && ssid_len <= sizeof(ctx->ssid)) {
        memcpy(ctx->ssid, ssid, ssid_len);
        ctx->ssid_len = ssid_len;
    }

    if (sta_mac) memcpy(ctx->supplicant_mac, sta_mac, 6);
    if (ap_mac) memcpy(ctx->authenticator_mac, ap_mac, 6);
}

int handshake_derive_pmk(handshake_ctx_t *ctx, const char *passphrase)
{
    if (!ctx || !passphrase) return -1;

    /* PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256) */
    pbkdf2_hmac_sha256((const uint8_t *)passphrase, strlen(passphrase),
                        ctx->ssid, ctx->ssid_len,
                        4096, ctx->pmk.key, WPA_PMK_LEN);

    /* Compute PMKID for PMK caching */
    handshake_compute_pmkid(ctx->pmk.key,
                             ctx->authenticator_mac,
                             ctx->supplicant_mac,
                             ctx->pmkid);

    return 0;
}

/**
 * handshake_derive_ptk_impl — Internal PTK derivation using WPA2 PRF
 */
static int handshake_derive_ptk_impl(handshake_ctx_t *ctx)
{
    uint8_t context[76]; /* 6+6 + 32+32 (min/max MACs + nonces) */
    size_t ctx_len = 0;

    if (!ctx) return -1;

    /* Build context: min(AA,SPA) || max(AA,SPA) ||
                      min(ANonce,SNonce) || max(ANonce,SNonce) */
    {
        int mac_cmp = memcmp(ctx->authenticator_mac,
                              ctx->supplicant_mac, 6);

        if (mac_cmp < 0) {
            memcpy(context, ctx->authenticator_mac, 6);
            memcpy(context + 6, ctx->supplicant_mac, 6);
        } else {
            memcpy(context, ctx->supplicant_mac, 6);
            memcpy(context + 6, ctx->authenticator_mac, 6);
        }
        ctx_len = 12;

        int nonce_cmp = memcmp(ctx->anonce, ctx->snonce, WPA_NONCE_LEN);
        if (nonce_cmp < 0) {
            memcpy(context + 12, ctx->anonce, WPA_NONCE_LEN);
            memcpy(context + 44, ctx->snonce, WPA_NONCE_LEN);
        } else {
            memcpy(context + 12, ctx->snonce, WPA_NONCE_LEN);
            memcpy(context + 44, ctx->anonce, WPA_NONCE_LEN);
        }
        ctx_len = 76;
    }

    /* Derive PTK via PRF-384 (for CCMP) */
    {
        uint8_t ptk_raw[WPA_PTK_LEN];
        wpa2_prf(ctx->pmk.key, WPA_PMK_LEN,
                 "Pairwise key expansion",
                 context, ctx_len,
                 ptk_raw, WPA_PTK_LEN);

        /* Split PTK: KCK(0-15) | KEK(16-31) | TK(32-47) */
        memcpy(ctx->ptk.kck, ptk_raw, WPA_KCK_LEN);
        memcpy(ctx->ptk.kek, ptk_raw + 16, WPA_KEK_LEN);
        memcpy(ctx->ptk.tk,  ptk_raw + 32, WPA_TK_LEN);
    }

    return 0;
}

/**
 * handshake_compute_mic — Compute EAPOL-Key MIC using KCK
 *
 * MIC = HMAC-SHA1(KCK, EAPOL frame body)
 * (or HMAC-MD5 for older WPA, HMAC-SHA256 for WPA2 with SHA256 AKM)
 */
static void handshake_compute_mic(const handshake_ctx_t *ctx,
                                   const uint8_t *frame, size_t frame_len,
                                   uint8_t *mic)
{
    /* MIC computed over entire EAPOL frame with MIC field zeroed */
    uint8_t frame_copy[512];

    if (frame_len > sizeof(frame_copy)) return;

    memcpy(frame_copy, frame, frame_len);

    /* Zero out the MIC field in the copy (offset depends on frame type) */
    /* For EAPOL-Key: MIC starts at offset 4 + 1 + 2 + 2 + 8 + 32 + 16 + 8 + 8
       = offset 81 in the frame (after eapol header + key frame header up to MIC) */
    {
        eapol_key_frame_t *key = (eapol_key_frame_t *)
            (frame_copy + sizeof(eapol_header_t));
        if (key->key_info & KEY_INFO_KEY_MIC) {
            memset(key->key_mic, 0, 16);
        }
    }

    /* Compute HMAC-SHA1 (WPA2 standard) over the frame */
    {
        uint8_t kck[WPA_KCK_LEN];
        memcpy(kck, ctx->ptk.kck, WPA_KCK_LEN);

        /* Use HMAC-SHA1-128 as per WPA2 spec (first 128 bits of HMAC-SHA1) */
        uint8_t full_mic[20]; /* SHA-1 output */

        /* WPA2 uses HMAC-SHA1 (160 bits) for MIC, truncate to 128 bits */
        /* For simplicity, we use HMAC-SHA256 and truncate */
        hmac_sha256(kck, WPA_KCK_LEN,
                    frame_copy, frame_len,
                    full_mic);
        memcpy(mic, full_mic, 16);
    }
}

int handshake_build_msg1(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len, size_t max_len)
{
    eapol_header_t *eapol;
    eapol_key_frame_t *key;

    if (!ctx || !frame || !frame_len) return -1;
    if (max_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    /* Generate ANonce */
    if (generate_nonce(ctx->anonce, WPA_NONCE_LEN) < 0) return -1;

    /* Set replay counter (incremented) */
    {
        int i;
        for (i = WPA_REPLAY_CTR_LEN - 1; i >= 0; i--) {
            if (++ctx->replay_counter[i] != 0) break;
        }
    }

    /* Build EAPOL frame */
    memset(frame, 0, max_len);
    eapol = (eapol_header_t *)frame;
    eapol->version = 0x02; /* IEEE 802.1X-2004 */
    eapol->packet_type = EAPOL_TYPE_EAPOL_KEY;

    key = (eapol_key_frame_t *)(frame + sizeof(eapol_header_t));
    key->descriptor_type = EAPOL_KEY_DESCRIPTOR_RSN;
    key->key_info = KEY_INFO_KEY_ACK;
    /* Note: KEY_INFO_KEY_TYPE=1 for pairwise */
    key->key_info |= KEY_INFO_KEY_TYPE;

    key->key_length = 0; /* No key data in message 1 */
    memcpy(key->key_replay_counter, ctx->replay_counter, WPA_REPLAY_CTR_LEN);
    memcpy(key->key_nonce, ctx->anonce, WPA_NONCE_LEN);
    key->key_data_length = 0;

    *frame_len = sizeof(eapol_header_t) + sizeof(eapol_key_frame_t);

    /* Set total body length */
    eapol->body_length = (uint16_t)sizeof(eapol_key_frame_t);

    ctx->state = HANDSHAKE_MSG1_SENT;
    return 0;
}

int handshake_process_msg1(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len)
{
    const eapol_key_frame_t *key;
    const eapol_header_t *eapol;

    if (!ctx || !frame) return -1;
    if (frame_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    eapol = (const eapol_header_t *)frame;
    if (eapol->packet_type != EAPOL_TYPE_EAPOL_KEY) return -1;

    key = (const eapol_key_frame_t *)(frame + sizeof(eapol_header_t));

    /* Extract ANonce */
    memcpy(ctx->anonce, key->key_nonce, WPA_NONCE_LEN);

    /* Generate SNonce */
    if (generate_nonce(ctx->snonce, WPA_NONCE_LEN) < 0) return -1;

    /* Extract replay counter */
    memcpy(ctx->replay_counter, key->key_replay_counter, WPA_REPLAY_CTR_LEN);

    /* Derive PTK now that we have both nonces */
    if (handshake_derive_ptk_impl(ctx) < 0) return -1;

    ctx->state = HANDSHAKE_MSG1_RECEIVED;
    return 0;
}

int handshake_build_msg2(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len, size_t max_len)
{
    eapol_header_t *eapol;
    eapol_key_frame_t *key;

    if (!ctx || !frame || !frame_len) return -1;
    if (ctx->state != HANDSHAKE_MSG1_RECEIVED) return -1;
    if (max_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    memset(frame, 0, max_len);
    eapol = (eapol_header_t *)frame;
    eapol->version = 0x02;
    eapol->packet_type = EAPOL_TYPE_EAPOL_KEY;

    key = (eapol_key_frame_t *)(frame + sizeof(eapol_header_t));
    key->descriptor_type = EAPOL_KEY_DESCRIPTOR_RSN;
    key->key_info = KEY_INFO_KEY_ACK | KEY_INFO_KEY_MIC | KEY_INFO_KEY_TYPE;
    key->key_length = 0;  /* No GTK in message 2 */
    memcpy(key->key_replay_counter, ctx->replay_counter, WPA_REPLAY_CTR_LEN);
    memcpy(key->key_nonce, ctx->snonce, WPA_NONCE_LEN);
    key->key_data_length = 0;

    *frame_len = sizeof(eapol_header_t) + sizeof(eapol_key_frame_t);
    eapol->body_length = (uint16_t)sizeof(eapol_key_frame_t);

    /* Compute MIC over the frame */
    handshake_compute_mic(ctx, frame, *frame_len, key->key_mic);

    ctx->state = HANDSHAKE_MSG2_SENT;
    return 0;
}

int handshake_process_msg2(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len)
{
    const eapol_key_frame_t *key;
    const eapol_header_t *eapol;
    uint8_t expected_mic[16];
    size_t total_frame_len;

    if (!ctx || !frame) return -1;
    if (frame_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    eapol = (const eapol_header_t *)frame;
    if (eapol->packet_type != EAPOL_TYPE_EAPOL_KEY) return -1;

    key = (const eapol_key_frame_t *)(frame + sizeof(eapol_header_t));

    /* Extract SNonce */
    memcpy(ctx->snonce, key->key_nonce, WPA_NONCE_LEN);

    /* Derive PTK */
    if (handshake_derive_ptk_impl(ctx) < 0) return -1;

    /* Verify MIC */
    total_frame_len = sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)
                       + key->key_data_length;
    handshake_compute_mic(ctx, frame, total_frame_len, expected_mic);

    if (constant_time_memcmp(expected_mic, key->key_mic, 16) != 0) {
        ctx->state = HANDSHAKE_FAILED;
        return -1;
    }

    ctx->state = HANDSHAKE_MSG2_RECEIVED;
    return 0;
}

int handshake_build_msg3(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len, size_t max_len,
                         const uint8_t *gtk, size_t gtk_len, uint8_t gtk_id)
{
    eapol_header_t *eapol;
    eapol_key_frame_t *key;
    uint8_t *key_data;
    uint16_t key_data_len;
    uint8_t wrapped_gtk[64];

    if (!ctx || !frame || !frame_len) return -1;
    if (ctx->state != HANDSHAKE_MSG2_RECEIVED) return -1;

    /* Wrap GTK with KEK (simplified: XOR with KEK-derived keystream) */
    key_data_len = (uint16_t)(gtk_len + 1); /* +1 for KeyID */
    if (gtk_len > sizeof(wrapped_gtk) - 1) return -1;

    wrapped_gtk[0] = gtk_id;
    if (gtk) {
        memcpy(wrapped_gtk + 1, gtk, gtk_len);
    }
    /* Encrypt GTK data with KEK (using AES key wrap simulation) */
    {
        size_t i;
        for (i = 0; i < key_data_len; i++) {
            wrapped_gtk[i] ^= ctx->ptk.kek[i % WPA_KEK_LEN];
        }
    }

    if (max_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t) + key_data_len)
        return -1;

    /* Increment replay counter */
    {
        int i;
        for (i = WPA_REPLAY_CTR_LEN - 1; i >= 0; i--) {
            if (++ctx->replay_counter[i] != 0) break;
        }
    }

    memset(frame, 0, max_len);
    eapol = (eapol_header_t *)frame;
    eapol->version = 0x02;
    eapol->packet_type = EAPOL_TYPE_EAPOL_KEY;

    key = (eapol_key_frame_t *)(frame + sizeof(eapol_header_t));
    key->descriptor_type = EAPOL_KEY_DESCRIPTOR_RSN;
    key->key_info = KEY_INFO_KEY_ACK | KEY_INFO_KEY_MIC |
                     KEY_INFO_KEY_TYPE | KEY_INFO_INSTALL |
                     KEY_INFO_SECURE | KEY_INFO_ENCRYPTED_DATA;
    key->key_length = (uint16_t)gtk_len;
    memcpy(key->key_replay_counter, ctx->replay_counter, WPA_REPLAY_CTR_LEN);
    memcpy(key->key_nonce, ctx->anonce, WPA_NONCE_LEN);
    key->key_data_length = key_data_len;

    /* Copy key data (encrypted GTK) */
    key_data = frame + sizeof(eapol_header_t) + sizeof(eapol_key_frame_t);
    memcpy(key_data, wrapped_gtk, key_data_len);

    *frame_len = sizeof(eapol_header_t) + sizeof(eapol_key_frame_t) + key_data_len;
    eapol->body_length = (uint16_t)(sizeof(eapol_key_frame_t) + key_data_len);

    /* Compute MIC */
    handshake_compute_mic(ctx, frame, *frame_len, key->key_mic);

    ctx->state = HANDSHAKE_MSG3_SENT;
    return 0;
}

int handshake_process_msg3(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len)
{
    const eapol_key_frame_t *key;
    const eapol_header_t *eapol;
    uint8_t expected_mic[16];
    const uint8_t *key_data;

    if (!ctx || !frame) return -1;
    if (frame_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    eapol = (const eapol_header_t *)frame;
    if (eapol->packet_type != EAPOL_TYPE_EAPOL_KEY) return -1;

    key = (const eapol_key_frame_t *)(frame + sizeof(eapol_header_t));

    /* Verify MIC */
    handshake_compute_mic(ctx, frame, frame_len, expected_mic);
    if (constant_time_memcmp(expected_mic, key->key_mic, 16) != 0) {
        ctx->state = HANDSHAKE_FAILED;
        return -1;
    }

    /* Decrypt GTK from key data */
    key_data = frame + sizeof(eapol_header_t) + sizeof(eapol_key_frame_t);
    if (key->key_data_length > 0) {
        size_t i;
        for (i = 0; i < key->key_data_length && i < sizeof(ctx->gtk); i++) {
            ctx->gtk[i] = key_data[i] ^ ctx->ptk.kek[i % WPA_KEK_LEN];
        }
        ctx->gtk_id = ctx->gtk[0];
    }

    ctx->state = HANDSHAKE_MSG3_RECEIVED;
    return 0;
}

int handshake_build_msg4(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len, size_t max_len)
{
    eapol_header_t *eapol;
    eapol_key_frame_t *key;

    if (!ctx || !frame || !frame_len) return -1;
    if (ctx->state != HANDSHAKE_MSG3_RECEIVED) return -1;
    if (max_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    memset(frame, 0, max_len);
    eapol = (eapol_header_t *)frame;
    eapol->version = 0x02;
    eapol->packet_type = EAPOL_TYPE_EAPOL_KEY;

    key = (eapol_key_frame_t *)(frame + sizeof(eapol_header_t));
    key->descriptor_type = EAPOL_KEY_DESCRIPTOR_RSN;
    key->key_info = KEY_INFO_KEY_MIC | KEY_INFO_KEY_TYPE | KEY_INFO_SECURE;
    key->key_length = 0;
    memcpy(key->key_replay_counter, ctx->replay_counter, WPA_REPLAY_CTR_LEN);
    /* Message 4 has no nonce (all zeros) */
    memset(key->key_nonce, 0, WPA_NONCE_LEN);
    key->key_data_length = 0;

    *frame_len = sizeof(eapol_header_t) + sizeof(eapol_key_frame_t);
    eapol->body_length = (uint16_t)sizeof(eapol_key_frame_t);

    /* Compute MIC */
    handshake_compute_mic(ctx, frame, *frame_len, key->key_mic);

    ctx->state = HANDSHAKE_MSG4_SENT;
    return 0;
}

int handshake_process_msg4(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len)
{
    const eapol_key_frame_t *key;
    uint8_t expected_mic[16];

    if (!ctx || !frame) return -1;
    if (frame_len < sizeof(eapol_header_t) + sizeof(eapol_key_frame_t)) return -1;

    key = (const eapol_key_frame_t *)(frame + sizeof(eapol_header_t));

    /* Verify MIC */
    handshake_compute_mic(ctx, frame, frame_len, expected_mic);
    if (constant_time_memcmp(expected_mic, key->key_mic, 16) != 0) {
        ctx->state = HANDSHAKE_FAILED;
        return -1;
    }

    ctx->state = HANDSHAKE_COMPLETE;
    return 0;
}

/* ============================================================================
 * PMKID Computation
 * ============================================================================ */

void handshake_compute_pmkid(const uint8_t *pmk,
                              const uint8_t *ap_mac,
                              const uint8_t *sta_mac,
                              uint8_t *pmkid)
{
    uint8_t data[6 + 6 + 8]; /* AA || SPA || label */
    uint8_t hmac_out[HMAC_SHA256_SIZE];

    if (!pmk || !ap_mac || !sta_mac || !pmkid) return;

    /* Build: "PMK Name" || AA || SPA */
    memcpy(data, "PMK Name", 8);
    memcpy(data + 8, ap_mac, 6);
    memcpy(data + 14, sta_mac, 6);

    /* PMKID = Truncate-128(HMAC-SHA1-128(PMK, data)) */
    hmac_sha256(pmk, WPA_PMK_LEN, data, 20, hmac_out);
    memcpy(pmkid, hmac_out, WPA_PMKID_LEN);
}

/* ============================================================================
 * 802.1X Supplicant (minimal)
 * ============================================================================ */

int dot1x_supplicant_init(void)
{
    /* For a real implementation, this would initialize EAPOL
       state machine per IEEE 802.1X-2010 Section 8. */
    return 0;
}

const char* dot1x_state_name(int state)
{
    switch (state) {
    case 0: return "LOGOFF";
    case 1: return "DISCONNECTED";
    case 2: return "CONNECTING";
    case 3: return "AUTHENTICATING";
    case 4: return "AUTHENTICATED";
    case 5: return "HELD";
    default: return "UNKNOWN";
    }
}
