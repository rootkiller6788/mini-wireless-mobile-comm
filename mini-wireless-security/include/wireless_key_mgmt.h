/**
 * wireless_key_mgmt.h — Wireless Key Management and Derivation
 *
 * Covers: WPA2 PRF, key hierarchy, GTK derivation, rekeying, PMK caching
 * Knowledge Levels: L1 (key structs), L2 (key hierarchy concept),
 *                   L4 (key derivation theorems), L5 (PRF algorithms),
 *                   L6 (WPA2 key management)
 *
 * Course Mapping:
 *   MIT 6.875 — Cryptography (key derivation functions)
 *   Stanford EE359 — Wireless Security (key management)
 *   Berkeley EE123 — DSP (hash-based KDF construction)
 *
 * References:
 *   IEEE 802.11i-2004, Section 8.5.1: Key Hierarchy
 *   NIST SP 800-108: Recommendation for Key Derivation Using Pseudorandom Functions
 *   RFC 5869: HMAC-based Extract-and-Expand Key Derivation Function (HKDF)
 */

#ifndef WIRELESS_KEY_MGMT_H
#define WIRELESS_KEY_MGMT_H

#include <stdint.h>
#include <stddef.h>
#include "wireless_crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Key Type Definitions
 * ============================================================================ */

/** WPA2 Key sizes */
#define PMK_LEN             32   /* Pairwise Master Key: 256 bits */
#define PTK_LEN             48   /* PTK for CCMP: 384 bits */
#define PTK_LEN_TKIP        64   /* PTK for TKIP: 512 bits */
#define GTK_LEN             32   /* Group Temporal Key */
#define GMK_LEN             32   /* Group Master Key */
#define KCK_LEN             16   /* Key Confirmation Key */
#define KEK_LEN             16   /* Key Encryption Key */
#define TK_LEN              16   /* Temporal Key for CCMP */
#define WPA_MIC_LEN         16

/** Key derivation lengths */
#define PRF_OUTPUT_MAX      512

/** WPA2 Pairwise key expansion label */
#define WPA_PAIRWISE_LABEL  "Pairwise key expansion"
#define WPA_GROUP_LABEL     "Group key expansion"
#define WPA_PMK_NAME_LABEL  "PMK Name"

/* ============================================================================
 * L1: Key Hierarchy Structures
 * ============================================================================ */

/**
 * WPA2 Key Hierarchy:
 *
 *   MSK (Enterprise)           Passphrase (Personal)
 *        |                            |
 *        v                            v  [PBKDF2]
 *   ┌─────────────────────────────────────┐
 *   │           PMK (256 bits)            │
 *   └─────────────────────────────────────┘
 *        |                            |
 *        v  [PRF-384]                 v  [PRF-256]
 *   ┌─────────────────┐          ┌──────────────┐
 *   │  PTK (384/512)  │          │  GTK (256)   │
 *   │  KCK | KEK | TK │          │ Group keys   │
 *   └─────────────────┘          └──────────────┘
 */

/** Pairwise Master Key */
typedef struct {
    uint8_t bytes[PMK_LEN];
    int     is_valid;              /* Whether PMK has been derived */
} pmk_t;

/** Pairwise Transient Key (48 bytes for CCMP) */
typedef struct {
    uint8_t kck[KCK_LEN];   /* Key Confirmation Key — used for MIC */
    uint8_t kek[KEK_LEN];   /* Key Encryption Key — used to wrap GTK */
    uint8_t tk[TK_LEN];     /* Temporal Key — used for CCMP data encryption */
} ptk_t;

/** Group Transient Key */
typedef struct {
    uint8_t bytes[GTK_LEN];
    uint8_t key_id;              /* Key ID (0-3) */
    uint8_t tx;                  /* Whether this key is for TX */
} gtk_t;

/** Group Master Key */
typedef struct {
    uint8_t bytes[GMK_LEN];
} gmk_t;

/** Key Confirmation Key */
typedef struct {
    uint8_t bytes[KCK_LEN];
} kck_t;

/** Key Encryption Key */
typedef struct {
    uint8_t bytes[KEK_LEN];
} kek_t;

/* ============================================================================
 * Key Management Context
 * ============================================================================ */

/**
 * Key management context — holds all key material for a wireless session
 */
typedef struct {
    /* Master keys */
    pmk_t  pmk;                    /* Pairwise Master Key */
    gmk_t  gmk;                    /* Group Master Key (AP only) */

    /* Session keys */
    ptk_t  ptk;                    /* Pairwise Transient Key */
    gtk_t  gtk;                    /* Current Group Transient Key */

    /* MAC addresses (used as input to PRF) */
    uint8_t supplicant_mac[6];     /* Client MAC */
    uint8_t authenticator_mac[6];  /* AP MAC */

    /* Nonces from handshake */
    uint8_t anonce[32];
    uint8_t snonce[32];

    /* SSID for PSK derivation */
    uint8_t  ssid[32];
    size_t   ssid_len;

    /* Cipher suite in use */
    int      cipher_suite;         /* 0=CCMP, 1=TKIP, 2=GCMP */

    /* Rekey counter */
    uint32_t rekey_counter;
} key_mgmt_ctx_t;

/** Cipher suite identifiers */
enum {
    CIPHER_SUITE_CCMP = 0,         /* AES-CCMP (WPA2) */
    CIPHER_SUITE_TKIP = 1,         /* TKIP (WPA) */
    CIPHER_SUITE_GCMP = 2,         /* AES-GCMP (WPA3) */
    CIPHER_SUITE_WEP40 = 3,        /* WEP-40 (historical) */
    CIPHER_SUITE_WEP104 = 4        /* WEP-104 (historical) */
};

/* ============================================================================
 * L5: WPA2 Pseudorandom Function (PRF) — IEEE 802.11i, Section 8.5.1.1
 * ============================================================================ */

/**
 * wpa2_prf — WPA2 Pseudorandom Function
 *
 * PRF(K, A, B, Len) defined as:
 *   for i = 0 to (Len+159)/160:
 *     R = R || HMAC-SHA1(K, A || 0x00 || B || i)
 *   return R[0..Len-1]
 *
 * This is the core key derivation function in WPA2.
 * Used to derive PTK from PMK, GTK from GMK, and other key material.
 *
 * Complexity: O(Len * HMAC-SHA1 time)
 *
 * @param key       Input key (PMK or GMK)
 * @param key_len   Key length (32 for PMK)
 * @param label     ASCII label (e.g., "Pairwise key expansion")
 * @param context   Concatenated context: min(AA,SPA) || max(AA,SPA) ||
 *                                        min(ANonce,SNonce) || max(ANonce,SNonce)
 * @param context_len Context length
 * @param output    Output buffer
 * @param output_len Desired output length
 */
void wpa2_prf(const uint8_t *key, size_t key_len,
              const char *label,
              const uint8_t *context, size_t context_len,
              uint8_t *output, size_t output_len);

/* ============================================================================
 * L5: PTK Derivation — IEEE 802.11i, Section 8.5.1.2
 * ============================================================================ */

/**
 * derive_ptk — Derive the Pairwise Transient Key
 *
 * PTK = PRF-X(PMK, "Pairwise key expansion",
 *              min(AA,SPA) || max(AA,SPA) ||
 *              min(ANonce,SNonce) || max(ANonce,SNonce))
 *
 * where X = 384 for CCMP, 512 for TKIP
 *
 * Theorem (IEEE 802.11i): If PRF is a secure pseudorandom function and
 * ANonce/SNonce are random, then PTK is indistinguishable from random.
 *
 * @param ctx    Key management context (must have PMK, nonces, MACs set)
 * @return 0 on success, -1 if PMK not valid
 */
int derive_ptk(key_mgmt_ctx_t *ctx);

/* ============================================================================
 * L5: GTK Derivation
 * ============================================================================ */

/**
 * derive_gtk — Derive Group Transient Key from Group Master Key
 *
 * GTK = PRF-256(GMK, "Group key expansion",
 *                AA || GN | GNonce)
 *
 * @param ctx   Key management context
 * @param gnonce 32-byte group nonce
 * @return 0 on success
 */
int derive_gtk(key_mgmt_ctx_t *ctx, const uint8_t *gnonce);

/* ============================================================================
 * L5: HKDF Extract-and-Expand (RFC 5869) — used in WPA3
 * ============================================================================ */

/**
 * hkdf_extract — HMAC-based Extract phase
 *
 * PRK = HMAC-Hash(salt, IKM)
 *
 * @param salt      Optional salt (may be NULL for zero-length salt)
 * @param salt_len  Salt length
 * @param ikm       Input Keying Material
 * @param ikm_len   IKM length
 * @param prk       Output: Pseudorandom Key (SHA256_DIGEST_SIZE bytes)
 */
void hkdf_extract(const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len,
                  uint8_t *prk);

/**
 * hkdf_expand — HMAC-based Expand phase
 *
 * OKM = HMAC-Hash(PRK, info || 0x01) ||
 *       HMAC-Hash(PRK, OKM[0] || info || 0x02) || ...
 *
 * @param prk        Pseudorandom Key from extract
 * @param prk_len    PRK length
 * @param info       Context/application-specific info
 * @param info_len   Info length
 * @param okm        Output Keying Material
 * @param okm_len    Desired output length
 */
void hkdf_expand(const uint8_t *prk, size_t prk_len,
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len);

/**
 * hkdf — Full HKDF (extract + expand) one-shot
 */
void hkdf(const uint8_t *salt, size_t salt_len,
          const uint8_t *ikm, size_t ikm_len,
          const uint8_t *info, size_t info_len,
          uint8_t *okm, size_t okm_len);

/* ============================================================================
 * L6: Key Management Operations
 * ============================================================================ */

/**
 * key_mgmt_init — Initialize key management context
 */
void key_mgmt_init(key_mgmt_ctx_t *ctx);

/**
 * key_mgmt_set_psk — Set PMK from pre-shared key (enterprise or direct)
 */
void key_mgmt_set_psk(key_mgmt_ctx_t *ctx, const uint8_t *psk, size_t psk_len);

/**
 * key_mgmt_derive_from_passphrase — Derive PMK from WPA2-Personal passphrase
 *
 * PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256)
 */
int key_mgmt_derive_from_passphrase(key_mgmt_ctx_t *ctx,
                                     const char *passphrase);

/**
 * key_mgmt_install_keys — Mark keys as installed (post handshake)
 */
int key_mgmt_install_keys(key_mgmt_ctx_t *ctx);

/**
 * key_mgmt_rekey — Trigger rekeying (update GTK, optionally PTK)
 */
int key_mgmt_rekey(key_mgmt_ctx_t *ctx);

/**
 * key_mgmt_get_tk — Get current Temporal Key for data encryption
 */
const uint8_t* key_mgmt_get_tk(const key_mgmt_ctx_t *ctx);

/**
 * key_mgmt_get_gtk — Get current Group Temporal Key
 */
const uint8_t* key_mgmt_get_gtk(const key_mgmt_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_KEY_MGMT_H */
