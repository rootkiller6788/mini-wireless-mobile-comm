/**
 * wireless_attack.h — Wireless Security Attack Vectors and Countermeasures
 *
 * Covers: WEP cracking (FMS/KoreK), WPA2 PMKID attack, KRACK,
 *         dictionary attacks, evil twin, deauthentication, KARMA
 * Knowledge Levels: L1 (attack type enums), L2 (attack surface concepts),
 *                   L5 (cryptanalysis algorithms), L6 (attack simulation),
 *                   L8 (side-channel attacks)
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless (security vulnerabilities)
 *   MIT 6.858 — Computer Systems Security (WiFi attacks)
 *   Berkeley EE123 — DSP (cryptanalysis)
 *
 * References:
 *   Fluhrer, Mantin, Shamir (2001): "Weaknesses in the Key Scheduling
 *        Algorithm of RC4" — FMS attack on WEP
 *   Tews, Weinmann, Pyshkin (2007): "Breaking 104-bit WEP in less than
 *        60 seconds" — KoreK attack
 *   Vanhoef, Piessens (2017): "Key Reinstallation Attacks: Forcing Nonce
 *        Reuse in WPA2" — KRACK attack
 *   Vanhoef, Piessens (2018): "Release the Kraken: New KRACKs in the
 *        802.11 Standard"
 */

#ifndef WIRELESS_ATTACK_H
#define WIRELESS_ATTACK_H

#include <stdint.h>
#include <stddef.h>
#include "wireless_crypto.h"
#include "wireless_auth.h"
#include "wireless_key_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Attack Taxonomy
 * ============================================================================ */

/** Wireless attack categories */
typedef enum {
    ATTACK_CATEGORY_CRYPTO       = 0,  /* Cryptographic attacks */
    ATTACK_CATEGORY_PROTOCOL     = 1,  /* Protocol vulnerability attacks */
    ATTACK_CATEGORY_IMPLEMENT    = 2,  /* Implementation bugs */
    ATTACK_CATEGORY_PHYSICAL     = 3,  /* Physical layer attacks */
    ATTACK_CATEGORY_SOCIAL       = 4,  /* Social engineering (evil twin) */
    ATTACK_CATEGORY_DOS          = 5,  /* Denial of Service */
    ATTACK_CATEGORY_SIDE_CHANNEL = 6   /* Timing/power side channels */
} attack_category_t;

/** Specific attack identifiers */
typedef enum {
    ATTACK_WEP_FMS              = 0,   /* FMS statistical key recovery */
    ATTACK_WEP_KOREK            = 1,   /* KoreK improved WEP attack */
    ATTACK_WPA2_PMKID           = 2,   /* PMKID capture for offline cracking */
    ATTACK_WPA2_KRACK           = 3,   /* Key Reinstallation Attack */
    ATTACK_WPA2_DICTIONARY      = 4,   /* Dictionary attack on WPA2 PSK */
    ATTACK_DEAUTH               = 5,   /* Deauthentication frame injection */
    ATTACK_EVIL_TWIN            = 6,   /* Rogue AP with same SSID */
    ATTACK_KARMA                = 7,   /* Probe response to any SSID */
    ATTACK_WPA2_HOLES196        = 8,   /* GTK hijacking via 802.11w bypass */
    ATTACK_WPA3_DOWNGRADE       = 9,   /* Downgrade to WPA2 */
    ATTACK_WPA3_SIDE_CHANNEL    = 10,  /* Timing attack on SAE */
    ATTACK_BLUETOOTH_KNOB       = 11,  /* KNOB attack on Bluetooth BR/EDR */
    ATTACK_BLUETOOTH_BIAS       = 12   /* BIAS attack on Bluetooth auth */
} attack_id_t;

/* ============================================================================
 * L1: WEP Attack Data Structures
 * ============================================================================ */

/**
 * Captured WEP IV (Initialization Vector) + first keystream byte
 *
 * The FMS attack exploits the fact that certain "weak" IVs leak
 * information about the RC4 key byte.
 *
 * Weak IV condition (FMS 2001):
 *   (B + 3) < N  where IV = (A+3, N-1, B) and N = 256
 *   This reveals information about key byte K[B+3].
 */
typedef struct {
    uint8_t  iv[3];            /* 24-bit WEP IV (A+3, 255, X) for FMS */
    uint8_t  key_index;        /* WEP key index */
    uint8_t  keystream_byte;   /* First keystream output byte */
    int      is_weak;          /* Whether this is an FMS-weak IV */
    uint32_t packet_number;    /* Sequence for tracking */
} wep_captured_iv_t;

/**
 * WEP key recovery state (FMS attack)
 */
typedef struct {
    uint8_t     target_key[16];     /* Key being recovered */
    int         target_key_len;     /* Usually 5 (40-bit) or 13 (104-bit) */
    int         recovered_bytes;    /* How many key bytes recovered so far */

    /* Vote table: votes[key_byte][candidate] = number of votes */
    int         votes[16][256];

    wep_captured_iv_t *captured;    /* Array of captured IVs */
    size_t      capture_count;
    size_t      capture_capacity;

    /* Statistics */
    uint32_t    total_packets_seen;
    uint32_t    weak_iv_packets;
    int         key_recovered;      /* 1 = complete key found */
} wep_fms_state_t;

/* ============================================================================
 * L1: WPA2 Attack Data Structures
 * ============================================================================ */

/**
 * WPA2 PMKID capture — for offline dictionary attack
 *
 * A PMKID is computed as: HMAC-SHA1-128(PMK, "PMK Name" || AP_MAC || STA_MAC)
 * Capturing it allows offline brute-force of the PSK.
 */
typedef struct {
    uint8_t  ap_mac[6];
    uint8_t  sta_mac[6];
    uint8_t  pmkid[16];           /* Captured PMKID */
    uint8_t  ssid[32];
    size_t   ssid_len;
    uint32_t timestamp;
} pmkid_capture_t;

/**
 * Dictionary attack state for WPA2 PSK cracking
 */
typedef struct {
    pmkid_capture_t target;       /* The PMKID to crack */
    uint8_t        candidate_pmk[PMK_LEN];
    uint8_t        candidate_pmkid[WPA_PMKID_LEN];
    int            found;         /* 1 = passphrase found */
    char           found_passphrase[64];

    uint32_t       attempts;      /* Number of passphrases tried */
    double         elapsed_sec;   /* Time spent */
} wpa2_dictionary_state_t;

/* ============================================================================
 * L1: KRACK Attack Data Structures
 * ============================================================================ */

/**
 * KRACK attack state — forces nonce reuse by replaying message 3
 * of the 4-way handshake with a manipulated replay counter.
 *
 * When the victim reinstalls an already-used PTK, the nonce counter
 * resets, allowing keystream reuse in CCMP (catastrophic break).
 */
typedef struct {
    /* Target info */
    uint8_t  target_mac[6];
    uint8_t  ap_mac[6];

    /* Captured handshake messages */
    uint8_t  msg3_original[512];  /* Original message 3 from AP */
    size_t   msg3_original_len;
    uint8_t  msg3_replayed[512];  /* Replayed (modified) message 3 */
    size_t   msg3_replayed_len;

    /* State */
    int      msg3_captured;
    int      replayed;
    int      nonce_reuse_detected;

    /* Replay counter manipulation */
    uint8_t  original_replay_ctr[8];
    uint8_t  incremented_replay_ctr[8];
} krack_state_t;

/* ============================================================================
 * L1: Rogue AP (Evil Twin) Attack Data Structures
 * ============================================================================ */

typedef struct {
    uint8_t  target_ssid[32];
    size_t   target_ssid_len;
    uint8_t  target_bssid[6];
    uint8_t  rogue_bssid[6];

    /* Beacon parameters to clone */
    int      channel;
    uint8_t  supported_rates[8];
    int      num_rates;

    /* Client tracking */
    int      clients_connected;
    uint8_t  client_macs[16][6];  /* Up to 16 clients */

    /* Credential harvesting */
    int      credentials_captured;
    char     captured_passwords[16][64];
    int      num_captured;
} evil_twin_state_t;

/* ============================================================================
 * L5: Attack Detection and Prevention
 * ============================================================================ */

/**
 * Intrusion detection context for wireless attacks
 */
typedef struct {
    /* Rate-based detection */
    uint32_t deauth_frames_seen;        /* Deauth flood detection */
    uint32_t auth_failures_seen;        /* Brute-force detection */
    uint32_t eapol_start_flood;         /* EAPOL Start flood */
    uint32_t pmkid_requests_seen;       /* PMKID harvesting detection */

    /* Time windows (seconds) */
    int      window_sec;
    uint32_t window_start_time;

    /* Thresholds */
    int      deauth_threshold;          /* Alarm if > N deauth/sec */
    int      auth_fail_threshold;
    int      pmkid_request_threshold;

    /* Alarms */
    int      deauth_flood_alarm;
    int      brute_force_alarm;
    int      pmkid_harvest_alarm;
    int      rogue_ap_alarm;
} ids_state_t;

/* ============================================================================
 * L5: Attack Simulation Functions
 * ============================================================================ */

/**
 * wep_fms_init — Initialize FMS attack state for key recovery
 *
 * The FMS attack works by observing that for certain IVs (weak IVs),
 * the first keystream byte has a statistical bias that reveals one
 * byte of the WEP key.
 *
 * L4 Theorem (FMS 2001): For weak IVs of the form (3, 255, X) with
 * 40-bit WEP, the first keystream output reveals K[B+3] with probability
 * ≈ 5% (vs 0.4% random), enabling key recovery after ~256² * ~300
 * = ~6 million packets.
 */
void wep_fms_init(wep_fms_state_t *state, int key_len);

/**
 * wep_fms_add_packet — Feed one captured packet to the FMS attack
 * Updates vote table if the IV is weak.
 */
void wep_fms_add_packet(wep_fms_state_t *state,
                         const uint8_t *iv,
                         uint8_t keystream_byte);

/**
 * wep_fms_recover_key — Attempt key recovery from accumulated votes
 * @return 0 if key recovered, -1 if more packets needed
 */
int wep_fms_recover_key(wep_fms_state_t *state);

/**
 * wep_fms_stats — Get attack progress statistics
 */
void wep_fms_stats(const wep_fms_state_t *state,
                    uint32_t *total, uint32_t *weak, int *recovered_bytes);

/* ============================================================================
 * WPA2 PMKID Dictionary Attack
 * ============================================================================ */

/**
 * wpa2_dict_attack_init — Initialize dictionary attack state
 */
void wpa2_dict_attack_init(wpa2_dictionary_state_t *state,
                            const pmkid_capture_t *target);

/**
 * wpa2_dict_attack_try — Try a single passphrase candidate
 *
 * Computes PMK = PBKDF2(passphrase, SSID, 4096, 256)
 * then PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AP_MAC || STA_MAC)
 * and compares to target PMKID.
 *
 * @return 1 if match found, 0 if not
 */
int wpa2_dict_attack_try(wpa2_dictionary_state_t *state,
                          const char *passphrase);

/**
 * wpa2_dict_attack_try_list — Try a list of candidate passphrases
 * @param wordlist  Array of null-terminated strings
 * @param count     Number of entries
 * @return Index of matching entry, or -1 if none found
 */
int wpa2_dict_attack_try_list(wpa2_dictionary_state_t *state,
                               const char **wordlist, int count);

/* ============================================================================
 * KRACK Attack Simulation
 * ============================================================================ */

/**
 * krack_init — Initialize KRACK attack state
 */
void krack_init(krack_state_t *state,
                const uint8_t *target_mac, const uint8_t *ap_mac);

/**
 * krack_capture_msg3 — Capture the original message 3 from AP
 */
int krack_capture_msg3(krack_state_t *state,
                        const uint8_t *msg3, size_t msg3_len);

/**
 * krack_build_replay — Build replayed message 3 with incremented counter
 * to force PTK reinstallation on the victim.
 */
int krack_build_replay(krack_state_t *state);

/**
 * krack_detect — Check if a given packet indicates nonce reuse
 *
 * Detects: CCMP replay counter reset → nonce reuse on stream cipher
 */
int krack_detect(const uint8_t *packet, size_t len,
                  const uint8_t *last_replay_ctr);

/* ============================================================================
 * Intrusion Detection Functions
 * ============================================================================ */

/**
 * ids_init — Initialize IDS state with default thresholds
 */
void ids_init(ids_state_t *ids, int window_sec);

/**
 * ids_process_frame — Feed one observed frame to the IDS
 * Updates counters and raises alarms if thresholds exceeded.
 */
void ids_process_frame(ids_state_t *ids, int frame_type,
                        int frame_subtype, uint32_t timestamp_sec);

/**
 * ids_check_alarms — Check and return active alarms as bitmask
 * Returns: bit 0=deauth flood, bit 1=brute force,
 *          bit 2=PMKID harvest, bit 3=rogue AP
 */
int ids_check_alarms(const ids_state_t *ids);

/**
 * ids_reset_alarms — Clear all alarms
 */
void ids_reset_alarms(ids_state_t *ids);

/**
 * ids_get_stats — Get detection statistics
 */
void ids_get_stats(const ids_state_t *ids,
                    uint32_t *deauth_count,
                    uint32_t *auth_fail_count,
                    uint32_t *pmkid_count);

/* ============================================================================
 * L8: Side-Channel Attack Detection
 * ============================================================================ */

/**
 * Detect timing side channel in SAE commit processing
 *
 * SAE uses a hunting-and-pecking loop to find the password element.
 * The number of iterations varies and can leak information about the
 * password via timing. WPA3 mandates constant-time implementation.
 *
 * @param timing_samples Array of SAE execution times (in microseconds)
 * @param num_samples    Number of timing samples
 * @return 1 if timing variation > threshold detected, 0 otherwise
 */
int detect_sae_timing_leak(const double *timing_samples, int num_samples);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_ATTACK_H */
