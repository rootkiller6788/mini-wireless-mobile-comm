/**
 * wireless_crypto.h — Cryptographic Primitives for Wireless Security
 *
 * Covers: AES-128/256, RC4, SHA-256, HMAC-SHA256, AES-CCMP, AES-GCMP
 * Knowledge Levels: L1 (typedefs), L2 (core concepts), L3 (GF(2^8) arithmetic),
 *                    L4 (Shannon's principles), L5 (algorithms)
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless Communications (Security chapter)
 *   MIT 6.875 — Cryptography and Cryptanalysis
 *   Berkeley EE123 — Digital Signal Processing (crypto applications)
 *
 * References:
 *   NIST FIPS 197: Advanced Encryption Standard (AES)
 *   NIST FIPS 180-4: Secure Hash Standard (SHA-256)
 *   NIST FIPS 198-1: Keyed-Hash Message Authentication Code (HMAC)
 *   NIST SP 800-38C: CCM Mode for Authentication and Confidentiality
 *   IEEE 802.11i-2004: WPA2 Security
 */

#ifndef WIRELESS_CRYPTO_H
#define WIRELESS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Core Data Type Definitions
 * ============================================================================ */

/** AES block size: always 128 bits (16 bytes) per FIPS 197 */
#define AES_BLOCK_SIZE      16
#define AES128_KEY_SIZE     16
#define AES256_KEY_SIZE     32
#define AES_MAX_ROUNDS      14   /* AES-256 uses 14 rounds */
#define AES_ROUND_KEY_SIZE  (AES_BLOCK_SIZE * (AES_MAX_ROUNDS + 1))

/** SHA-256 constants per FIPS 180-4 */
#define SHA256_BLOCK_SIZE   64   /* 512-bit message block */
#define SHA256_DIGEST_SIZE  32   /* 256-bit hash output */
#define SHA256_HASH_SIZE    32

/** HMAC-SHA256 output size */
#define HMAC_SHA256_SIZE    32

/** RC4 state size (256 bytes per standard) */
#define RC4_STATE_SIZE      256

/** AES-CCMP nonce size (13 bytes) */
#define CCMP_NONCE_SIZE     13
/** AES-CCMP tag size (usually 8 or 16 bytes) */
#define CCMP_TAG_SIZE       16

/* ============================================================================
 * AES Cipher Context (L1 Definition)
 * ============================================================================ */

/** AES key schedule context */
typedef struct {
    uint8_t   round_keys[AES_ROUND_KEY_SIZE]; /* expanded key material */
    int       nr;                             /* number of rounds: 10/12/14 */
    int       nk;                             /* key size in 32-bit words: 4/6/8 */
} aes_ctx_t;

/* ============================================================================
 * SHA-256 Hash Context
 * ============================================================================ */

typedef struct {
    uint32_t  state[8];          /* intermediate hash state H[0..7] */
    uint64_t  bit_count;         /* total bits processed */
    uint8_t   buffer[SHA256_BLOCK_SIZE]; /* unprocessed message bytes */
    size_t    buffer_len;        /* bytes in buffer */
} sha256_ctx_t;

/* ============================================================================
 * HMAC Context
 * ============================================================================ */

typedef struct {
    sha256_ctx_t inner_ctx;      /* inner hash state */
    sha256_ctx_t outer_ctx;      /* outer hash state */
    uint8_t     key[HMAC_SHA256_SIZE];  /* hashed key */
    size_t      key_len;          /* original key length */
} hmac_sha256_ctx_t;

/* ============================================================================
 * RC4 Stream Cipher State (used in WEP/TKIP)
 * ============================================================================ */

typedef struct {
    uint8_t  S[RC4_STATE_SIZE];  /* permutation state */
    uint8_t  i;                  /* index i */
    uint8_t  j;                  /* index j */
} rc4_ctx_t;

/* ============================================================================
 * AES-CCMP Context (L1 + L2)
 * ============================================================================ */

typedef struct {
    aes_ctx_t aes_ctx;           /* underlying AES cipher */
} ccmp_ctx_t;

/* ============================================================================
 * AES Galois/Counter Mode (GCMP for WPA3) context
 * ============================================================================ */

typedef struct {
    aes_ctx_t aes_ctx;           /* underlying AES cipher */
    uint8_t   H[AES_BLOCK_SIZE]; /* GHASH key: E_K(0^128) */
} gcmp_ctx_t;

/* ============================================================================
 * L5: AES Algorithm — Full Implementation Interfaces
 * ============================================================================ */

/**
 * aes_key_setup — Expand a user key into the round key schedule
 *
 * Theorem (FIPS 197): The AES key schedule expands N_k 32-bit words into
 * N_b*(N_r+1) 32-bit round key words using RotWord, SubWord, and Rcon.
 *
 * Complexity: O(N_r) per key setup.
 *
 * @param ctx   AES context (output)
 * @param key   Raw key bytes (16, 24, or 32 bytes)
 * @param bits  Key size: 128, 192, or 256
 * @return      0 on success, -1 on invalid key size
 */
int aes_key_setup(aes_ctx_t *ctx, const uint8_t *key, int bits);

/**
 * aes_encrypt_block — Encrypt a single 128-bit block (ECB mode primitive)
 *
 * Each round: SubBytes → ShiftRows → MixColumns → AddRoundKey
 * Final round omits MixColumns.
 *
 * @param ctx    AES context with expanded key schedule
 * @param plain  16-byte plaintext block
 * @param cipher 16-byte ciphertext block (output)
 */
void aes_encrypt_block(const aes_ctx_t *ctx, const uint8_t *plain,
                       uint8_t *cipher);

/**
 * aes_decrypt_block — Decrypt a single 128-bit block
 *
 * Inverse rounds: InvShiftRows → InvSubBytes → AddRoundKey → InvMixColumns
 *
 * @param ctx    AES context
 * @param cipher 16-byte ciphertext block
 * @param plain  16-byte plaintext block (output)
 */
void aes_decrypt_block(const aes_ctx_t *ctx, const uint8_t *cipher,
                       uint8_t *plain);

/* ============================================================================
 * L5: SHA-256 Algorithm
 * ============================================================================ */

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t *digest);

/** One-shot SHA-256 hash */
void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest);

/* ============================================================================
 * L5: HMAC-SHA256 Algorithm (per RFC 2104 & FIPS 198-1)
 * ============================================================================ */

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const uint8_t *key, size_t key_len);
void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t *mac);

/** One-shot HMAC-SHA256 */
void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t *mac);

/* ============================================================================
 * L5: RC4 Stream Cipher (WEP legacy, for educational contrast)
 * ============================================================================ */

/**
 * rc4_init — Key Scheduling Algorithm (KSA)
 *
 * Initializes the RC4 state array S[0..255] as a permutation derived from
 * the key.  The key is repeated cyclically to fill a 256-byte internal array.
 *
 * Security note: RC4 is broken for WEP (FMS attack, 2001).  Included for
 * completeness and historical study.  WPA3 mandates AES-GCMP.
 */
void rc4_init(rc4_ctx_t *ctx, const uint8_t *key, size_t key_len);

/**
 * rc4_generate — Pseudo-Random Generation Algorithm (PRGA)
 *
 * @param ctx  Initialized RC4 state
 * @param out  Output buffer
 * @param len  Number of keystream bytes to generate
 */
void rc4_generate(rc4_ctx_t *ctx, uint8_t *out, size_t len);

/**
 * rc4_crypt — Encrypt/decrypt in-place (XOR with keystream)
 *
 * RC4 is symmetric: encryption and decryption are identical operations.
 */
void rc4_crypt(rc4_ctx_t *ctx, uint8_t *data, size_t len);

/* ============================================================================
 * L6: AES-CCMP (Counter with CBC-MAC) — WPA2 mandatory
 * ============================================================================ */

/**
 * ccmp_encrypt — AES-CCM encrypt + authenticate
 *
 * CCM mode = CTR mode encryption + CBC-MAC authentication tag.
 * Defined in NIST SP 800-38C, RFC 3610.
 *
 * L4 Theorem (Jonsson 2002): CCM provides both confidentiality and
 * authenticity under the assumption that AES is a secure block cipher
 * and the nonce is never reused with the same key.
 *
 * @param ctx           Initialized CCMP context
 * @param nonce         13-byte nonce (MUST be unique per key)
 * @param plaintext     Payload to encrypt
 * @param plaintext_len Length of plaintext
 * @param aad           Additional Authenticated Data (e.g., MAC header)
 * @param aad_len       AAD length
 * @param ciphertext    Output ciphertext (same length as plaintext)
 * @param tag           Authentication tag (8 or 16 bytes)
 * @param tag_len       Tag length (4, 6, 8, 10, 12, 14, or 16)
 * @return 0 on success
 */
int ccmp_encrypt(ccmp_ctx_t *ctx,
                 const uint8_t nonce[CCMP_NONCE_SIZE],
                 const uint8_t *plaintext, size_t plaintext_len,
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *ciphertext, uint8_t *tag, int tag_len);

/**
 * ccmp_decrypt — AES-CCM decrypt + verify
 * @return 0 on success (tag verified), -1 on authentication failure
 */
int ccmp_decrypt(ccmp_ctx_t *ctx,
                 const uint8_t nonce[CCMP_NONCE_SIZE],
                 const uint8_t *ciphertext, size_t ciphertext_len,
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *tag, int tag_len,
                 uint8_t *plaintext);

/* ============================================================================
 * L8: AES-GCMP (Galois/Counter Mode) — WPA3 mandatory
 * ============================================================================ */

/**
 * gcmp_init — Initialize GCM context (compute H = E_K(0^128))
 */
void gcmp_init(gcmp_ctx_t *gctx, const uint8_t *key, int key_bits);

/**
 * gcmp_encrypt — AES-GCM encrypt + authenticate
 *
 * GCM = CTR mode + GHASH authentication (Galois field multiplication).
 * Used by WPA3 for its higher throughput and stronger authentication.
 */
int gcmp_encrypt(gcmp_ctx_t *gctx,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *plaintext, size_t plaintext_len,
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *ciphertext, uint8_t *tag, int tag_len);

int gcmp_decrypt(gcmp_ctx_t *gctx,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *ciphertext, size_t ciphertext_len,
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *tag, int tag_len,
                 uint8_t *plaintext);

/* ============================================================================
 * L5: PBKDF2 — Password-Based Key Derivation Function 2 (RFC 2898)
 * ============================================================================ */

/**
 * pbkdf2_hmac_sha256 — Derive key from passphrase
 *
 * Used in WPA2-Personal (WPA-PSK) to derive the Pairwise Master Key (PMK)
 * from the SSID and passphrase.
 *
 * PBKDF2(PRF, Password, Salt, c, dkLen) per RFC 2898 / PKCS #5 v2.0.
 *
 * @param password     User passphrase
 * @param password_len Length of password
 * @param salt         Salt (SSID in WPA2)
 * @param salt_len     Salt length
 * @param iterations   Iteration count (4096 in WPA2)
 * @param dk           Output derived key
 * @param dk_len       Desired key length
 */
void pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
                         const uint8_t *salt, size_t salt_len,
                         uint32_t iterations,
                         uint8_t *dk, size_t dk_len);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/** Constant-time byte comparison (prevents timing side channels) */
int constant_time_memcmp(const uint8_t *a, const uint8_t *b, size_t len);

/** XOR two byte buffers: dst ^= src */
void xor_bytes(uint8_t *dst, const uint8_t *src, size_t len);

/** Increment a byte buffer as a big-endian counter (for CTR mode) */
void ctr_inc(uint8_t *ctr, size_t len);

/* ============================================================================
 * L4: AES Structural Self-Tests — Verification of Cryptographic Properties
 *
 * These functions validate the mathematical structure of AES components,
 * demonstrating the deep connection between GF(2^8) arithmetic and
 * the substitution-permutation network construction.
 * ============================================================================ */

/**
 * Verify the AES S-box is correctly constructed from the GF(2^8)
 * multiplicative inverse followed by the affine transformation:
 *   sbox[x] = affine_transform(gf256_inv(x))
 * Returns 0 if all 256 entries match, non-zero mismatch count otherwise.
 */
int aes_verify_sbox_construction(void);

/**
 * Verify the MixColumns and InvMixColumns matrices are proper inverses
 * in GF(2^8) (MDS matrix pair): inv_mixcol × mixcol = Identity.
 * Returns 0 on success, non-zero mismatch count otherwise.
 */
int aes_verify_mixcol_inverse(void);

/**
 * Run all AES structural integrity verifications.
 * Returns 0 on success (all checks pass), non-zero if any check fails.
 */
int aes_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_CRYPTO_H */
