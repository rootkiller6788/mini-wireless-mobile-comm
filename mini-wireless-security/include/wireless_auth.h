/**
 * wireless_auth.h — Wireless Authentication Protocols
 *
 * Covers: EAP, 802.1X, 4-way handshake, PMKID generation, nonce management
 * Knowledge Levels: L1 (state enums), L2 (auth concepts), L6 (WPA2 handshake)
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless Communications (802.11i security)
 *   MIT 6.858 — Computer Systems Security (authentication protocols)
 *   Georgia Tech ECE 6601 — Communication Systems (security layers)
 *
 * References:
 *   IEEE 802.11i-2004: Medium Access Control Security Enhancements
 *   IEEE 802.1X-2010: Port-Based Network Access Control
 *   RFC 3748: Extensible Authentication Protocol (EAP)
 *   RFC 5216: EAP-TLS Authentication Protocol
 */

#ifndef WIRELESS_AUTH_H
#define WIRELESS_AUTH_H

#include <stdint.h>
#include <stddef.h>
#include "wireless_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Authentication Type Definitions
 * ============================================================================ */

/** EAPOL (EAP over LAN) ethertype per IEEE 802.1X */
#define EAPOL_ETHERTYPE         0x888E

/** EAPOL packet types */
#define EAPOL_TYPE_EAP_PACKET   0x00
#define EAPOL_TYPE_EAPOL_START  0x01
#define EAPOL_TYPE_EAPOL_LOGOFF 0x02
#define EAPOL_TYPE_EAPOL_KEY    0x03

/** EAP Code field values */
#define EAP_CODE_REQUEST        1
#define EAP_CODE_RESPONSE       2
#define EAP_CODE_SUCCESS         3
#define EAP_CODE_FAILURE         4

/** EAP Type values (subset relevant to wireless) */
#define EAP_TYPE_IDENTITY       1
#define EAP_TYPE_NOTIFICATION   2
#define EAP_TYPE_MD5_CHALLENGE  4
#define EAP_TYPE_TLS            13
#define EAP_TYPE_TTLS           21
#define EAP_TYPE_PEAP           25
#define EAP_TYPE_MSCHAPV2       26
#define EAP_TYPE_AKA            23
#define EAP_TYPE_SIM            18
#define EAP_TYPE_AKA_PRIME      50

/** Key Descriptor type in EAPOL-Key frames */
#define EAPOL_KEY_DESCRIPTOR_RSN   2   /* WPA2 */
#define EAPOL_KEY_DESCRIPTOR_WPA   254 /* WPA1/WPA */

/** Key Information field bit masks (IEEE 802.11i, Figure 8-30) */
#define KEY_INFO_KEY_TYPE       0x0008  /* 1=Pairwise, 0=Group */
#define KEY_INFO_INSTALL        0x0040
#define KEY_INFO_KEY_ACK        0x0080
#define KEY_INFO_KEY_MIC        0x0100
#define KEY_INFO_SECURE         0x0200
#define KEY_INFO_ERROR          0x0400
#define KEY_INFO_REQUEST        0x0800
#define KEY_INFO_ENCRYPTED_DATA 0x1000
#define KEY_INFO_SMK_MESSAGE    0x2000

/** Nonce sizes */
#define WPA_NONCE_LEN           32
#define WPA_PMKID_LEN           16
#define WPA_PMK_LEN             32
#define WPA_PTK_LEN             48  /* 16(KCK) + 16(KEK) + 16(TK) */
#define WPA_KCK_LEN             16
#define WPA_KEK_LEN             16
#define WPA_TK_LEN              16  /* TKIP: 16, CCMP: 16 */
#define WPA_GTK_LEN             32
#define WPA_MIC_KEY_LEN         16

/** Replay counter size */
#define WPA_REPLAY_CTR_LEN      8

/* ============================================================================
 * L1: EAPOL Frame Structures
 * ============================================================================ */

/** EAPOL header (4 bytes, per 802.1X) */
typedef struct {
    uint8_t  version;           /* Protocol version (0x01 or 0x02) */
    uint8_t  packet_type;       /* EAPOL_TYPE_* */
    uint16_t body_length;       /* Length of body in network byte order */
} eapol_header_t;

/** EAP header (4 bytes, per RFC 3748) */
typedef struct {
    uint8_t  code;              /* Request/Response/Success/Failure */
    uint8_t  identifier;        /* Matches request with response */
    uint16_t length;            /* Entire EAP packet length */
} eap_header_t;

/** EAP expanded type (8 bytes for vendor-specific types) */
typedef struct {
    uint8_t  type;              /* Always 254 for expanded */
    uint32_t vendor_id;         /* IANA SMI vendor ID (network order) */
    uint32_t vendor_type;       /* Vendor-specific type */
} eap_expanded_type_t;

/* ============================================================================
 * EAPOL-Key Frame (IEEE 802.11i, Section 8.5.2)
 * ============================================================================ */

typedef struct {
    uint8_t  descriptor_type;   /* 2 for RSN (WPA2), 254 for WPA */
    uint16_t key_info;          /* Bitfield flags */
    uint16_t key_length;        /* Length of key material */
    uint8_t  key_replay_counter[WPA_REPLAY_CTR_LEN];
    uint8_t  key_nonce[WPA_NONCE_LEN];
    uint8_t  key_iv[16];
    uint8_t  key_rsc[8];        /* Receive Sequence Counter */
    uint8_t  key_id[8];         /* Reserved */
    uint8_t  key_mic[16];       /* MIC (Message Integrity Code) */
    uint16_t key_data_length;   /* Length of key data field */
    /* Followed by: uint8_t key_data[key_data_length] */
} eapol_key_frame_t;

/* ============================================================================
 * WPA/WPA2 Key Hierarchy (L1 + L2)
 * ============================================================================ */

/**
 * WPA2 Key Derivation Hierarchy:
 *
 *   Passphrase ──[PBKDF2]──→ PMK (Pairwise Master Key)
 *                                    │
 *                    ┌───────────────┼───────────────┐
 *                    │ [PRF-384/512]                  │
 *                    ▼                                ▼
 *              PTK (Pairwise Transient Key)    GTK (Group Transient Key)
 *              = KCK || KEK || TK             (from GMK via PRF)
 *
 * Where:
 *   PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256)
 *   PTK = PRF-512(PMK, "Pairwise key expansion",
 *                  min(AA, SPA) || max(AA, SPA) ||
 *                  min(ANonce,SNonce) || max(ANonce,SNonce))
 *   KCK = PTK[0:15]    (Key Confirmation Key)
 *   KEK = PTK[16:31]   (Key Encryption Key)
 *   TK  = PTK[32:47]   (Temporal Key for CCMP/TKIP)
 */

/** Pairwise Master Key (derived from passphrase) */
typedef struct {
    uint8_t key[WPA_PMK_LEN];
} wpa_pmk_t;

/** Pairwise Transient Key */
typedef struct {
    uint8_t kck[WPA_KCK_LEN];  /* Key Confirmation Key */
    uint8_t kek[WPA_KEK_LEN];  /* Key Encryption Key */
    uint8_t tk[WPA_TK_LEN];    /* Temporal Key */
} wpa_ptk_t;

/* ============================================================================
 * 4-Way Handshake State Machine (L2 + L6)
 * ============================================================================ */

/** States of the 4-way handshake (per IEEE 802.11i, Figure 8-29) */
typedef enum {
    HANDSHAKE_IDLE = 0,
    HANDSHAKE_MSG1_SENT,        /* AP → STA: ANonce */
    HANDSHAKE_MSG1_RECEIVED,    /* STA side after receiving ANonce */
    HANDSHAKE_MSG2_SENT,        /* STA → AP: SNonce + MIC */
    HANDSHAKE_MSG2_RECEIVED,    /* AP side after receiving SNonce */
    HANDSHAKE_MSG3_SENT,        /* AP → STA: GTK + MIC */
    HANDSHAKE_MSG3_RECEIVED,    /* STA side after receiving GTK */
    HANDSHAKE_MSG4_SENT,        /* STA → AP: ACK + MIC */
    HANDSHAKE_COMPLETE,         /* Keys installed, secure communication */
    HANDSHAKE_TIMEOUT,          /* Timeout occurred */
    HANDSHAKE_FAILED            /* MIC verification failed */
} handshake_state_t;

/**
 * 4-way handshake context — tracks the full state of a WPA2 handshake
 */
typedef struct {
    handshake_state_t state;

    /* Participant addresses */
    uint8_t supplicant_mac[6];      /* STA MAC (6 bytes) */
    uint8_t authenticator_mac[6];   /* AP MAC (6 bytes) */

    /* SSID for PMK derivation */
    uint8_t  ssid[32];
    size_t   ssid_len;

    /* Key material */
    wpa_pmk_t pmk;                  /* Derived from passphrase or 802.1X */
    wpa_ptk_t ptk;                  /* Derived during handshake */

    /* Nonces exchanged */
    uint8_t  anonce[WPA_NONCE_LEN]; /* Authenticator nonce (random) */
    uint8_t  snonce[WPA_NONCE_LEN]; /* Supplicant nonce (random) */

    /* GTK (delivered in message 3) */
    uint8_t  gtk[WPA_GTK_LEN];
    uint8_t  gtk_id;

    /* Replay counters */
    uint8_t  replay_counter[WPA_REPLAY_CTR_LEN];

    /* PMKID (for PMK caching) */
    uint8_t  pmkid[WPA_PMKID_LEN];

    /* MIC for message 4 verification (STA→AP) */
    uint8_t  msg4_mic[16];

    /* 802.1X integration */
    int      is_enterprise;         /* 1 = 802.1X, 0 = PSK */
} handshake_ctx_t;

/* ============================================================================
 * L5: 4-Way Handshake Protocol Functions
 * ============================================================================ */

/**
 * handshake_init — Initialize 4-way handshake context
 *
 * Sets up fresh nonces and initializes state to IDLE.
 *
 * @param ctx         Handshake context
 * @param ssid        Network SSID
 * @param ssid_len    SSID length
 * @param sta_mac     Supplicant (client) MAC address
 * @param ap_mac      Authenticator (AP) MAC address
 * @param is_enterprise  Whether using 802.1X (1) or PSK (0)
 */
void handshake_init(handshake_ctx_t *ctx,
                    const uint8_t *ssid, size_t ssid_len,
                    const uint8_t *sta_mac, const uint8_t *ap_mac,
                    int is_enterprise);

/**
 * handshake_derive_pmk — Derive PMK from passphrase (WPA2-Personal)
 *
 * PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256)
 *
 * @param ctx       Handshake context
 * @param passphrase Null-terminated ASCII passphrase
 * @return 0 on success
 */
int handshake_derive_pmk(handshake_ctx_t *ctx, const char *passphrase);

/**
 * handshake_build_msg1 — AP builds message 1 (ANonce)
 *
 * Message 1: EAPOL-Key(ANonce, Unicast, KeyAck)
 */
int handshake_build_msg1(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len,
                         size_t max_len);

/**
 * handshake_process_msg1 — STA processes message 1, generates SNonce
 * and derives PTK, then builds message 2.
 */
int handshake_process_msg1(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len);

/**
 * handshake_build_msg2 — STA builds message 2 (SNonce + MIC)
 *
 * Message 2: EAPOL-Key(SNonce, Unicast, KeyAck+KeyMIC, MIC)
 */
int handshake_build_msg2(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len,
                         size_t max_len);

/**
 * handshake_process_msg2 — AP processes message 2
 * Derives PTK, verifies MIC, prepares message 3.
 */
int handshake_process_msg2(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len);

/**
 * handshake_build_msg3 — AP builds message 3 (GTK + MIC)
 *
 * Message 3: EAPOL-Key(ANonce, Install, KeyAck+KeyMIC+Secure+Encrypted, MIC, GTK)
 */
int handshake_build_msg3(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len,
                         size_t max_len,
                         const uint8_t *gtk, size_t gtk_len, uint8_t gtk_id);

/**
 * handshake_process_msg3 — STA processes message 3
 * Verifies MIC, installs GTK, prepares message 4.
 */
int handshake_process_msg3(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len);

/**
 * handshake_build_msg4 — STA builds message 4 (ACK + MIC)
 */
int handshake_build_msg4(handshake_ctx_t *ctx,
                         uint8_t *frame, size_t *frame_len,
                         size_t max_len);

/**
 * handshake_process_msg4 — AP processes message 4
 * Verifies MIC, marks handshake complete.
 */
int handshake_process_msg4(handshake_ctx_t *ctx,
                           const uint8_t *frame, size_t frame_len);

/* ============================================================================
 * L2: PMKID Generation (for PMK caching / PMKID attack)
 * ============================================================================ */

/**
 * handshake_compute_pmkid — Compute PMKID for RSN IE
 *
 * PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AA || SPA)
 *
 * @param pmk    32-byte PMK
 * @param ap_mac Authenticator (AP) MAC
 * @param sta_mac Supplicant (STA) MAC
 * @param pmkid  16-byte output
 */
void handshake_compute_pmkid(const uint8_t *pmk,
                              const uint8_t *ap_mac,
                              const uint8_t *sta_mac,
                              uint8_t *pmkid);

/* ============================================================================
 * L2: Nonce Generation (Cryptographically secure random)
 * ============================================================================ */

/**
 * generate_nonce — Generate a cryptographically secure random nonce
 *
 * Uses the system CSPRNG if available, falls back to a deterministic
 * but unpredictable PRNG seeded with time and process state.
 *
 * @param buf  Output buffer
 * @param len  Length (typically WPA_NONCE_LEN = 32)
 * @return 0 on success
 */
int generate_nonce(uint8_t *buf, size_t len);

/* ============================================================================
 * L5: 802.1X Port-Based Authentication
 * ============================================================================ */

/**
 * dot1x_supplicant_init — Initialize 802.1X supplicant state
 *
 * EAPOL Start → EAP-Request/Identity → EAP-Response/Identity → ...
 *
 * @return 0 on success
 */
int dot1x_supplicant_init(void);

/**
 * dot1x_state_names — Return human-readable state name for debugging
 */
const char* dot1x_state_name(int state);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_AUTH_H */
