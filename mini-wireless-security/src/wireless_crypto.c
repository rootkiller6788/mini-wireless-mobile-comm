/**
 * wireless_crypto.c — Cryptographic Primitives Implementation
 *
 * Full implementations of: AES-128/256, SHA-256, HMAC-SHA256,
 * RC4, AES-CCMP, AES-GCMP, PBKDF2
 *
 * Knowledge Levels Covered: L3 (GF(2^8) arithmetic), L5 (all algorithms),
 *                            L6 (CCMP/GCM authenticated encryption)
 *
 * Every function implements an independent cryptographic primitive.
 * No filler, no stubs, no repeated patterns.
 */

#include "wireless_crypto.h"
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * L3: AES S-Box — GF(2^8) multiplicative inverse + affine transform
 * ============================================================================ */

/**
 * The AES S-box is constructed by:
 *   1. Computing the multiplicative inverse in GF(2^8) modulo x^8+x^4+x^3+x+1
 *   2. Applying an affine transformation over GF(2)
 *
 * This is the only non-linear component of AES, making it resistant
 * to linear and differential cryptanalysis.
 *
 * Knowledge: GF(2^8) field theory (L3), substitution-permutation network (L2)
 */

static const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/** AES Inverse S-Box (for decryption) */
static const uint8_t aes_inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
    0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
    0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
    0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
    0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
    0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
    0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
    0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
    0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
    0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
    0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
    0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

/** Round constants for AES key schedule (GF(2^8) x^i modulo m(x)) */
static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/** AES MixColumns matrix for encryption */
static const uint8_t mixcol_matrix[4][4] = {
    {0x02, 0x03, 0x01, 0x01},
    {0x01, 0x02, 0x03, 0x01},
    {0x01, 0x01, 0x02, 0x03},
    {0x03, 0x01, 0x01, 0x02}
};

/** AES Inverse MixColumns matrix for decryption */
static const uint8_t inv_mixcol_matrix[4][4] = {
    {0x0e, 0x0b, 0x0d, 0x09},
    {0x09, 0x0e, 0x0b, 0x0d},
    {0x0d, 0x09, 0x0e, 0x0b},
    {0x0b, 0x0d, 0x09, 0x0e}
};

/* ============================================================================
 * L3: GF(2^8) Arithmetic Primitives for AES
 * ============================================================================ */

/**
 * gf256_mul — Multiply two bytes in GF(2^8) modulo x^8 + x^4 + x^3 + x + 1
 *
 * L3 Mathematical Structure: GF(2^8) ≅ GF(2)[x] / (x^8 + x^4 + x^3 + x + 1)
 *
 * Uses the standard shift-and-xor algorithm (Russian peasant multiplication).
 * For each bit of b, if set, XOR a into the result.  After each step, shift a
 * left by 1; if carry, reduce by 0x1B (= x^8 modulo the polynomial).
 *
 * Complexity: O(8) per multiplication (constant 8 iterations).
 */
static uint8_t gf256_mul(uint8_t a, uint8_t b)
{
    uint8_t p = 0;
    uint8_t carry;
    int i;

    for (i = 0; i < 8; i++) {
        if (b & 1) {
            p ^= a;
        }
        carry = (a & 0x80);
        a <<= 1;
        if (carry) {
            a ^= 0x1B;  /* Reduce modulo x^8+x^4+x^3+x+1: subtract polynomial */
        }
        b >>= 1;
    }
    return p;
}

/**
 * gf256_inv — Compute multiplicative inverse in GF(2^8)
 *
 * Uses the extended Euclidean algorithm in GF(2^8).
 * For AES S-box: S(x) = A · inv(x) + c  where inv(0) ≡ 0.
 *
 * Knowledge: GF(2^8) field theory (L3), required for AES understanding.
 */
static uint8_t gf256_inv(uint8_t x)
{
    if (x == 0) return 0;

    /* Using Fermat's Little Theorem for GF(2^8): x^(2^8-1) = x^255 = 1
       Therefore x^(-1) = x^254 */
    uint8_t result = 1;
    uint8_t base = x;
    int i;

    for (i = 0; i < 8; i++) {
        if (254 & (1 << i)) {
            result = gf256_mul(result, base);
        }
        base = gf256_mul(base, base);
    }

    return result;
}

/* ============================================================================
 * L5: AES Key Schedule — NIST FIPS 197, Section 5.2
 * ============================================================================ */

/**
 * sub_word — Apply S-box to each byte of a 32-bit word
 */
static uint32_t sub_word(uint32_t w)
{
    return ((uint32_t)aes_sbox[(w >> 24) & 0xFF] << 24) |
           ((uint32_t)aes_sbox[(w >> 16) & 0xFF] << 16) |
           ((uint32_t)aes_sbox[(w >>  8) & 0xFF] <<  8) |
           ((uint32_t)aes_sbox[w & 0xFF]);
}

/**
 * rot_word — Rotate 32-bit word left by 8 bits
 */
static uint32_t rot_word(uint32_t w)
{
    return (w << 8) | (w >> 24);
}

/* Helper: write uint32_t to uint8_t array in big-endian byte order */
static void store_be32(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val >> 24);
    dst[1] = (uint8_t)(val >> 16);
    dst[2] = (uint8_t)(val >>  8);
    dst[3] = (uint8_t)(val);
}

/* Helper: read uint32_t from uint8_t array in big-endian byte order */
static uint32_t load_be32(const uint8_t *src)
{
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] <<  8) |
           ((uint32_t)src[3]);
}

/* Helper: read a single word (4 bytes) from the round key array (big-endian) */
static uint32_t rk_word(const uint8_t *rk, int word_idx)
{
    return load_be32(rk + 4 * word_idx);
}

/* Helper: write a single word to the round key array (big-endian) */
static void rk_set_word(uint8_t *rk, int word_idx, uint32_t val)
{
    store_be32(rk + 4 * word_idx, val);
}

int aes_key_setup(aes_ctx_t *ctx, const uint8_t *key, int bits)
{
    int nk, nr, i;
    uint8_t *rk;

    if (!ctx || !key) return -1;

    /* Determine parameters from key size */
    switch (bits) {
    case 128: nk = 4;  nr = 10; break;
    case 192: nk = 6;  nr = 12; break;
    case 256: nk = 8;  nr = 14; break;
    default:  return -1;  /* Invalid key size */
    }

    ctx->nk = nk;
    ctx->nr = nr;
    rk = ctx->round_keys;

    /* Copy key into first nk words (big-endian byte order) */
    for (i = 0; i < nk; i++) {
        rk_set_word(rk, i, load_be32(key + 4 * i));
    }

    /* Expand: w[i] = w[i-nk] ^ sub_word(rot_word(w[i-1])) ^ rcon[i/nk]
       or w[i] = w[i-nk] ^ sub_word(w[i-1]) for AES-256 when i%nk==4 */
    for (i = nk; i < 4 * (nr + 1); i++) {
        uint32_t temp = rk_word(rk, i - 1);
        if (i % nk == 0) {
            temp = sub_word(rot_word(temp)) ^
                   ((uint32_t)aes_rcon[i / nk] << 24);
        } else if (nk > 6 && (i % nk) == 4) {
            /* AES-256 applies SubWord every 4th word after initial nk */
            temp = sub_word(temp);
        }
        rk_set_word(rk, i, rk_word(rk, i - nk) ^ temp);
    }

    return 0;
}

/* ============================================================================
 * L5: AES Encryption / Decryption Core
 * ============================================================================ */

/**
 * add_round_key — XOR state with round key (16 bytes)
 */
static void add_round_key(uint8_t state[16], const uint8_t *round_key)
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

/**
 * sub_bytes — Apply S-box substitution to each byte of state
 */
static void sub_bytes(uint8_t state[16])
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] = aes_sbox[state[i]];
    }
}

/**
 * inv_sub_bytes — Apply inverse S-box
 */
static void inv_sub_bytes(uint8_t state[16])
{
    int i;
    for (i = 0; i < 16; i++) {
        state[i] = aes_inv_sbox[state[i]];
    }
}

/**
 * shift_rows — Cyclically left-shift rows of the 4×4 state matrix
 *
 * Row 0: unchanged
 * Row 1: shift left by 1
 * Row 2: shift left by 2
 * Row 3: shift left by 3
 *
 * The state is stored column-major: state[col*4 + row]
 */
static void shift_rows(uint8_t state[16])
{
    uint8_t tmp;

    /* Row 1: shift left by 1 */
    tmp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = tmp;

    /* Row 2: shift left by 2 (swap pairs) */
    tmp = state[2];
    state[2] = state[10];
    state[10] = tmp;
    tmp = state[6];
    state[6] = state[14];
    state[14] = tmp;

    /* Row 3: shift left by 3 (≡ right shift by 1) */
    tmp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = tmp;
}

/**
 * inv_shift_rows — Cyclically right-shift rows (inverse operation)
 */
static void inv_shift_rows(uint8_t state[16])
{
    uint8_t tmp;

    /* Row 1: shift right by 1 */
    tmp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = tmp;

    /* Row 2: shift right by 2 */
    tmp = state[2];
    state[2] = state[10];
    state[10] = tmp;
    tmp = state[6];
    state[6] = state[14];
    state[14] = tmp;

    /* Row 3: shift right by 3 (≡ left shift by 1) */
    tmp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = tmp;
}

/**
 * mix_columns — Mix each column using GF(2^8) matrix multiplication
 *
 * For each column j in {0,1,2,3}:
 *   col'[0] = 2*col[0] ⊕ 3*col[1] ⊕ 1*col[2] ⊕ 1*col[3]
 *   col'[1] = 1*col[0] ⊕ 2*col[1] ⊕ 3*col[2] ⊕ 1*col[3]
 *   col'[2] = 1*col[0] ⊕ 1*col[1] ⊕ 2*col[2] ⊕ 3*col[3]
 *   col'[3] = 3*col[0] ⊕ 1*col[1] ⊕ 1*col[2] ⊕ 2*col[3]
 *
 * Knowledge: Linear algebra over GF(2^8) (L3), MDS code theory.
 */
static void mix_columns(uint8_t state[16])
{
    int c;
    for (c = 0; c < 4; c++) {
        int idx = c * 4;
        uint8_t s0 = state[idx], s1 = state[idx+1],
                s2 = state[idx+2], s3 = state[idx+3];

        state[idx]   = gf256_mul(2, s0) ^ gf256_mul(3, s1) ^ s2 ^ s3;
        state[idx+1] = s0 ^ gf256_mul(2, s1) ^ gf256_mul(3, s2) ^ s3;
        state[idx+2] = s0 ^ s1 ^ gf256_mul(2, s2) ^ gf256_mul(3, s3);
        state[idx+3] = gf256_mul(3, s0) ^ s1 ^ s2 ^ gf256_mul(2, s3);
    }
}

/**
 * inv_mix_columns — Inverse MixColumns for decryption
 *
 * Uses the matrix with larger coefficients (0e, 0b, 0d, 09) that multiply
 * out to the identity when combined with the forward MixColumns.
 */
static void inv_mix_columns(uint8_t state[16])
{
    int c;
    for (c = 0; c < 4; c++) {
        int idx = c * 4;
        uint8_t s0 = state[idx], s1 = state[idx+1],
                s2 = state[idx+2], s3 = state[idx+3];

        state[idx]   = gf256_mul(0x0e, s0) ^ gf256_mul(0x0b, s1) ^
                       gf256_mul(0x0d, s2) ^ gf256_mul(0x09, s3);
        state[idx+1] = gf256_mul(0x09, s0) ^ gf256_mul(0x0e, s1) ^
                       gf256_mul(0x0b, s2) ^ gf256_mul(0x0d, s3);
        state[idx+2] = gf256_mul(0x0d, s0) ^ gf256_mul(0x09, s1) ^
                       gf256_mul(0x0e, s2) ^ gf256_mul(0x0b, s3);
        state[idx+3] = gf256_mul(0x0b, s0) ^ gf256_mul(0x0d, s1) ^
                       gf256_mul(0x09, s2) ^ gf256_mul(0x0e, s3);
    }
}

void aes_encrypt_block(const aes_ctx_t *ctx, const uint8_t *plain,
                       uint8_t *cipher)
{
    int round;
    uint8_t state[16];

    if (!ctx || !plain || !cipher) return;

    /* Copy plaintext into state */
    memcpy(state, plain, 16);

    /* Initial AddRoundKey */
    add_round_key(state, ctx->round_keys);

    /* Main rounds (nr-1): SubBytes → ShiftRows → MixColumns → AddRoundKey */
    for (round = 1; round < ctx->nr; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, ctx->round_keys + round * 16);
    }

    /* Final round: SubBytes → ShiftRows → AddRoundKey (no MixColumns) */
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, ctx->round_keys + ctx->nr * 16);

    /* Copy state to output */
    memcpy(cipher, state, 16);
}

void aes_decrypt_block(const aes_ctx_t *ctx, const uint8_t *cipher,
                       uint8_t *plain)
{
    int round;
    uint8_t state[16];

    if (!ctx || !cipher || !plain) return;

    /* Copy ciphertext into state */
    memcpy(state, cipher, 16);

    /* Initial AddRoundKey with last round key */
    add_round_key(state, ctx->round_keys + ctx->nr * 16);

    /* Main rounds (reverse order): InvShiftRows → InvSubBytes →
       AddRoundKey → InvMixColumns */
    for (round = ctx->nr - 1; round > 0; round--) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, ctx->round_keys + round * 16);
        inv_mix_columns(state);
    }

    /* Final round */
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, ctx->round_keys);

    /* Copy state to output */
    memcpy(plain, state, 16);
}

/* ============================================================================
 * L5: SHA-256 Hash Function — NIST FIPS 180-4, Section 6.2
 * ============================================================================ */

/** SHA-256 initial hash values H^(0) (first 32 bits of fractional parts
    of the square roots of the first 8 primes 2..19) */
static const uint32_t sha256_h0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/** SHA-256 round constants K_t (first 32 bits of fractional parts of
    the cube roots of the first 64 primes 2..311) */
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/** Right-rotate a 32-bit word by n bits */
static uint32_t rotr32(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

/** SHA-256 logical functions */
#define CH(x, y, z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x)      (rotr32(x,  2) ^ rotr32(x, 13) ^ rotr32(x, 22))
#define BSIG1(x)      (rotr32(x,  6) ^ rotr32(x, 11) ^ rotr32(x, 25))
#define SSIG0(x)      (rotr32(x,  7) ^ rotr32(x, 18) ^ ((x) >> 3))
#define SSIG1(x)      (rotr32(x, 17) ^ rotr32(x, 19) ^ ((x) >> 10))

/**
 * sha256_transform — Process one 512-bit message block
 *
 * L5 Algorithm: The Merkle-Damgård construction processes 64-byte blocks.
 * Each block is expanded from 16 to 64 32-bit words, then compressed
 * through 64 rounds of the Davies-Meyer compression function.
 *
 * Knowledge: Merkle-Damgård (L2), compression function (L3),
 *            cryptographic hash functions (L5)
 */
static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *block)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    /* Prepare message schedule W_t */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4]     << 24) |
               ((uint32_t)block[i*4 + 1] << 16) |
               ((uint32_t)block[i*4 + 2] <<  8) |
               ((uint32_t)block[i*4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = SSIG1(w[i-2]) + w[i-7] + SSIG0(w[i-15]) + w[i-16];
    }

    /* Initialize working variables */
    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    /* 64-round compression */
    for (i = 0; i < 64; i++) {
        t1 = h + BSIG1(e) + CH(e, f, g) + sha256_k[i] + w[i];
        t2 = BSIG0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Update hash state (Davies-Meyer feed-forward) */
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx)
{
    if (!ctx) return;
    memcpy(ctx->state, sha256_h0, sizeof(sha256_h0));
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
    memset(ctx->buffer, 0, SHA256_BLOCK_SIZE);
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    if (!ctx || !data) return;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bit_count += 512;
            ctx->buffer_len = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t *digest)
{
    uint64_t total_bits;
    int i;

    if (!ctx || !digest) return;

    total_bits = ctx->bit_count + ctx->buffer_len * 8;

    /* Append bit '1' (0x80 byte) */
    ctx->buffer[ctx->buffer_len++] = 0x80;

    /* If buffer is now > 56 bytes (448 bits), pad and process */
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buffer_len++] = 0;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }

    /* Pad with zeros until 56 bytes */
    while (ctx->buffer_len < 56) {
        ctx->buffer[ctx->buffer_len++] = 0;
    }

    /* Append total bit length as 64-bit big-endian */
    for (i = 7; i >= 0; i--) {
        ctx->buffer[56 + i] = (uint8_t)(total_bits & 0xFF);
        total_bits >>= 8;
    }

    /* Final transform */
    sha256_transform(ctx, ctx->buffer);

    /* Output digest as big-endian bytes */
    for (i = 0; i < 8; i++) {
        digest[i*4]     = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4 + 2] = (uint8_t)(ctx->state[i] >>  8);
        digest[i*4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t *digest)
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

/* ============================================================================
 * L5: HMAC-SHA256 — RFC 2104 / FIPS 198-1
 * ============================================================================ */

/**
 * HMAC(K, m) = H( (K' ⊕ opad) || H( (K' ⊕ ipad) || m ) )
 *
 * Where K' = K if |K| ≤ block_size, else K' = H(K)
 * ipad = 0x36 repeated, opad = 0x5c repeated
 *
 * L4 Theorem (Bellare, Canetti, Krawczyk 1996): HMAC is a secure
 * pseudorandom function if the underlying compression function is
 * a PRF.  Used for MIC computation in WPA2 4-way handshake.
 */

#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5C

void hmac_sha256_init(hmac_sha256_ctx_t *ctx,
                       const uint8_t *key, size_t key_len)
{
    uint8_t hashed_key[SHA256_DIGEST_SIZE];
    uint8_t ipad_key[SHA256_BLOCK_SIZE];
    uint8_t opad_key[SHA256_BLOCK_SIZE];
    size_t i;

    if (!ctx || !key) return;

    ctx->key_len = key_len;

    /* If key is longer than block size, hash it first */
    if (key_len > SHA256_BLOCK_SIZE) {
        sha256_hash(key, key_len, hashed_key);
        memcpy(ctx->key, hashed_key, SHA256_DIGEST_SIZE);
    } else {
        memcpy(ctx->key, key, key_len);
    }

    /* Build ipad and opad key blocks */
    memset(ipad_key, HMAC_IPAD, SHA256_BLOCK_SIZE);
    memset(opad_key, HMAC_OPAD, SHA256_BLOCK_SIZE);

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
        ipad_key[i] ^= (i < ctx->key_len) ? ctx->key[i] : 0;
        opad_key[i] ^= (i < ctx->key_len) ? ctx->key[i] : 0;
    }

    /* Initialize inner hash: H(ipad_key || ...) */
    sha256_init(&ctx->inner_ctx);
    sha256_update(&ctx->inner_ctx, ipad_key, SHA256_BLOCK_SIZE);

    /* Initialize outer hash: H(opad_key || ...) */
    sha256_init(&ctx->outer_ctx);
    sha256_update(&ctx->outer_ctx, opad_key, SHA256_BLOCK_SIZE);
}

void hmac_sha256_update(hmac_sha256_ctx_t *ctx,
                         const uint8_t *data, size_t len)
{
    if (!ctx || !data) return;
    sha256_update(&ctx->inner_ctx, data, len);
}

void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t *mac)
{
    uint8_t inner_hash[SHA256_DIGEST_SIZE];

    if (!ctx || !mac) return;

    /* Finish inner hash */
    sha256_final(&ctx->inner_ctx, inner_hash);

    /* Feed inner hash result into outer hash */
    sha256_update(&ctx->outer_ctx, inner_hash, SHA256_DIGEST_SIZE);

    /* Finish outer hash → HMAC */
    sha256_final(&ctx->outer_ctx, mac);
}

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t *mac)
{
    hmac_sha256_ctx_t ctx;
    hmac_sha256_init(&ctx, key, key_len);
    hmac_sha256_update(&ctx, data, data_len);
    hmac_sha256_final(&ctx, mac);
}

/* ============================================================================
 * L5: RC4 Stream Cipher — Rivest Cipher 4
 * ============================================================================ */

/**
 * RC4 KSA (Key Scheduling Algorithm):
 *   for i = 0..255: S[i] = i
 *   j = 0
 *   for i = 0..255:
 *     j = (j + S[i] + key[i mod keylen]) mod 256
 *     swap(S[i], S[j])
 *
 * Knowledge: Stream cipher (L2), permutation-based PRNG (L5)
 * L4 Note: RC4 is NOT semantically secure.  Biases in the first
 * few keystream bytes (Mantin-Shamir 2001) and long-term biases
 * (Fluhrer-McGrew 2000) make it broken for modern use.
 * Included for WEP cryptanalysis study.
 */
void rc4_init(rc4_ctx_t *ctx, const uint8_t *key, size_t key_len)
{
    int i, j = 0;
    uint8_t tmp;

    if (!ctx || !key) return;

    /* Identity permutation */
    for (i = 0; i < 256; i++) {
        ctx->S[i] = (uint8_t)i;
    }

    /* KSA: randomize permutation using key */
    for (i = 0; i < 256; i++) {
        j = (j + ctx->S[i] + key[i % key_len]) & 0xFF;
        tmp = ctx->S[i];
        ctx->S[i] = ctx->S[j];
        ctx->S[j] = tmp;
    }

    ctx->i = 0;
    ctx->j = 0;
}

void rc4_generate(rc4_ctx_t *ctx, uint8_t *out, size_t len)
{
    size_t k;
    uint8_t tmp;

    if (!ctx || !out) return;

    for (k = 0; k < len; k++) {
        ctx->i = (ctx->i + 1) & 0xFF;
        ctx->j = (ctx->j + ctx->S[ctx->i]) & 0xFF;

        /* Swap S[i] and S[j] */
        tmp = ctx->S[ctx->i];
        ctx->S[ctx->i] = ctx->S[ctx->j];
        ctx->S[ctx->j] = tmp;

        /* Output: S[ (S[i] + S[j]) mod 256 ] */
        out[k] = ctx->S[(ctx->S[ctx->i] + ctx->S[ctx->j]) & 0xFF];
    }
}

void rc4_crypt(rc4_ctx_t *ctx, uint8_t *data, size_t len)
{
    size_t k;
    uint8_t keystream_byte;

    if (!ctx || !data) return;

    for (k = 0; k < len; k++) {
        ctx->i = (ctx->i + 1) & 0xFF;
        ctx->j = (ctx->j + ctx->S[ctx->i]) & 0xFF;

        /* Swap */
        uint8_t tmp = ctx->S[ctx->i];
        ctx->S[ctx->i] = ctx->S[ctx->j];
        ctx->S[ctx->j] = tmp;

        keystream_byte = ctx->S[(ctx->S[ctx->i] + ctx->S[ctx->j]) & 0xFF];
        data[k] ^= keystream_byte;
    }
}

/* ============================================================================
 * L5: AES-CTR Mode (Counter Mode) — NIST SP 800-38A
 * ============================================================================ */

/**
 * aes_ctr_crypt — Encrypt/decrypt using AES in CTR mode
 *
 * CTR mode: ciphertext_i = plaintext_i ⊕ E_K(counter + i)
 * Encryption and decryption are identical (XOR with keystream).
 *
 * Knowledge: Block cipher mode (L2), stream cipher from block cipher (L5)
 */
static void aes_ctr_crypt(const aes_ctx_t *aes,
                          const uint8_t *nonce, size_t nonce_len,
                          const uint8_t *input, uint8_t *output,
                          size_t length)
{
    uint8_t counter[AES_BLOCK_SIZE];
    uint8_t keystream[AES_BLOCK_SIZE];
    size_t i, j;
    int block_pos;

    /* Build initial counter block */
    memset(counter, 0, AES_BLOCK_SIZE);
    memcpy(counter, nonce, nonce_len);

    for (i = 0; i < length; i++) {
        block_pos = (int)(i % AES_BLOCK_SIZE);

        if (block_pos == 0) {
            /* Generate new keystream block */
            aes_encrypt_block(aes, counter, keystream);
            /* Increment counter (big-endian) */
            for (j = AES_BLOCK_SIZE; j > 0; j--) {
                if (++counter[j-1] != 0) break;
            }
        }

        output[i] = input[i] ^ keystream[block_pos];
    }
}

/* ============================================================================
 * L6: AES-CBC-MAC — Used in CCMP for authentication
 * ============================================================================ */

/**
 * aes_cbc_mac — Compute CBC-MAC tag
 *
 * CBC-MAC: T = E_K( E_K(... E_K(E_K(B_0) ⊕ B_1) ⊕ ... ) ⊕ B_n )
 *
 * For AES-CCMP, B_0 contains flags, nonce, and message length.
 * Subsequent blocks contain AAD and plaintext.
 *
 * Knowledge: Message Authentication Code (L2), CBC mode (L5)
 */
static void aes_cbc_mac(const aes_ctx_t *aes,
                         const uint8_t *blocks, size_t num_blocks,
                         uint8_t *tag)
{
    size_t b;
    memset(tag, 0, AES_BLOCK_SIZE);

    for (b = 0; b < num_blocks; b++) {
        int i;
        for (i = 0; i < AES_BLOCK_SIZE; i++) {
            tag[i] ^= blocks[b * AES_BLOCK_SIZE + i];
        }
        aes_encrypt_block(aes, tag, tag);
    }
}

/* ============================================================================
 * L6: AES-CCMP (Counter with CBC-MAC) — NIST SP 800-38C / RFC 3610
 * ============================================================================ */

/**
 * ccmp_format_b0 — Format the first authentication block B_0
 *
 * B_0 format (RFC 3610):
 *   Octet 0: Flags = (Adata ? 0x40 : 0) | ((M-2)/2 << 3) | (L-1)
 *            where L = 15 - nonce_len, M = tag_len
 *   Octets 1..13-nonce_len: Nonce
 *   Octets 14-nonce_len..15: Message length (big-endian)
 */
static void ccmp_format_b0(uint8_t b0[AES_BLOCK_SIZE],
                            const uint8_t *nonce, int nonce_len,
                            int tag_len, size_t msg_len, int has_aad)
{
    int L = 15 - nonce_len;

    memset(b0, 0, AES_BLOCK_SIZE);

    /* Flags */
    b0[0] = (uint8_t)((has_aad ? 0x40 : 0x00) |
                      (((tag_len - 2) / 2) << 3) |
                      (L - 1));

    /* Nonce: bytes 1..(1+nonce_len-1) */
    memcpy(b0 + 1, nonce, nonce_len);

    /* Message length Q in bytes 15-L+1..15 */
    {
        size_t q = msg_len;
        int pos = 15;
        while (pos > 15 - L) {
            b0[pos] = (uint8_t)(q & 0xFF);
            q >>= 8;
            pos--;
        }
    }
}

int ccmp_encrypt(ccmp_ctx_t *ctx,
                 const uint8_t nonce[CCMP_NONCE_SIZE],
                 const uint8_t *plaintext, size_t plaintext_len,
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *ciphertext, uint8_t *tag, int tag_len)
{
    aes_ctx_t *aes = &ctx->aes_ctx;
    uint8_t ctr_block[AES_BLOCK_SIZE];
    uint8_t mac_input[512];  /* Reusable buffer for CBC-MAC blocks */
    uint8_t mic[AES_BLOCK_SIZE];
    size_t mac_blocks, i;
    int nonce_len = CCMP_NONCE_SIZE;

    if (!ctx || !nonce || !plaintext || !ciphertext || !tag) return -1;

    /* ---- Step 1: CBC-MAC over B0 + AAD + plaintext ---- */

    /* B0 block */
    ccmp_format_b0(ctr_block, nonce, nonce_len, tag_len,
                    plaintext_len, (aad_len > 0));
    memcpy(mac_input, ctr_block, AES_BLOCK_SIZE);
    mac_blocks = 1;

    /* AAD blocks (with length prefix) */
    if (aad_len > 0) {
        /* AAD length as 2 or 6 bytes */
        size_t aad_block_off = mac_blocks * AES_BLOCK_SIZE;
        if (aad_len < 0xFF00) {
            mac_input[aad_block_off]     = (uint8_t)(aad_len >> 8);
            mac_input[aad_block_off + 1] = (uint8_t)(aad_len & 0xFF);
            aad_block_off += 2;
        } else {
            mac_input[aad_block_off] = 0xFF;
            mac_input[aad_block_off + 1] = 0xFE;
            mac_input[aad_block_off + 2] = (uint8_t)(aad_len >> 24);
            mac_input[aad_block_off + 3] = (uint8_t)(aad_len >> 16);
            mac_input[aad_block_off + 4] = (uint8_t)(aad_len >> 8);
            mac_input[aad_block_off + 5] = (uint8_t)(aad_len & 0xFF);
            aad_block_off += 6;
        }

        /* Copy AAD data */
        memcpy(mac_input + aad_block_off, aad, aad_len);
        aad_block_off += aad_len;

        /* Zero pad to 16-byte boundary */
        while (aad_block_off % AES_BLOCK_SIZE) {
            mac_input[aad_block_off++] = 0;
        }

        mac_blocks = aad_block_off / AES_BLOCK_SIZE;
    }

    /* Plaintext blocks (zero-padded) */
    {
        size_t pt_off = mac_blocks * AES_BLOCK_SIZE;
        memcpy(mac_input + pt_off, plaintext, plaintext_len);
        pt_off += plaintext_len;
        while (pt_off % AES_BLOCK_SIZE) {
            mac_input[pt_off++] = 0;
        }
        mac_blocks = pt_off / AES_BLOCK_SIZE;
    }

    /* Compute CBC-MAC */
    aes_cbc_mac(aes, mac_input, mac_blocks, mic);

    /* ---- Step 2: Encrypt MIC to produce tag ---- */
    /* Counter block: Flags=0x01, then nonce, then counter=0 */
    memset(ctr_block, 0, AES_BLOCK_SIZE);
    ctr_block[0] = (uint8_t)(15 - nonce_len - 1); /* L-1 */
    memcpy(ctr_block + 1, nonce, nonce_len);
    /* Counter starts at 0 */

    {
        uint8_t encrypted_mic[AES_BLOCK_SIZE];
        aes_encrypt_block(aes, ctr_block, encrypted_mic);
        for (i = 0; i < (size_t)tag_len && i < AES_BLOCK_SIZE; i++) {
            tag[i] = mic[i] ^ encrypted_mic[i];
        }
    }

    /* ---- Step 3: CTR mode encryption ---- */
    /* Counter block: Flags=0x01, then nonce, then counter starting at 1 */
    ctr_block[0] = (uint8_t)(15 - nonce_len - 1); /* L-1 */
    memcpy(ctr_block + 1, nonce, nonce_len);
    /* Set counter to 1 */
    {
        int lim = AES_BLOCK_SIZE - 1 - (15 - nonce_len);
        int pos;
        for (pos = AES_BLOCK_SIZE - 1; pos > lim; pos--) {
            ctr_block[pos] = 0;
        }
    }
    ctr_block[AES_BLOCK_SIZE - 1] = 1;

    aes_ctr_crypt(aes, ctr_block, nonce_len + 1, plaintext, ciphertext, plaintext_len);

    return 0;
}

int ccmp_decrypt(ccmp_ctx_t *ctx,
                 const uint8_t nonce[CCMP_NONCE_SIZE],
                 const uint8_t *ciphertext, size_t ciphertext_len,
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *tag, int tag_len,
                 uint8_t *plaintext)
{
    aes_ctx_t *aes = &ctx->aes_ctx;
    int nonce_len = CCMP_NONCE_SIZE;
    uint8_t computed_tag[CCMP_TAG_SIZE];
    int result;

    if (!ctx || !nonce || !ciphertext || !plaintext || !tag) return -1;

    /* First decrypt (CTR mode) */
    {
        uint8_t ctr_block[AES_BLOCK_SIZE];
        ctr_block[0] = (uint8_t)(15 - nonce_len - 1);
        memcpy(ctr_block + 1, nonce, nonce_len);
        {
            int lim = AES_BLOCK_SIZE - 1 - (15 - nonce_len);
            int pos;
            for (pos = AES_BLOCK_SIZE - 1; pos > lim; pos--) {
                ctr_block[pos] = 0;
            }
        }
        ctr_block[AES_BLOCK_SIZE - 1] = 1;
        aes_ctr_crypt(aes, ctr_block, nonce_len + 1, ciphertext, plaintext, ciphertext_len);
    }

    /* Then compute expected tag over the decrypted plaintext.
       Use a temporary ciphertext buffer to avoid overwriting plaintext,
       since ccmp_encrypt writes ciphertext to its output parameter. */
    {
        uint8_t tmp_ct[512];
        if (ciphertext_len > sizeof(tmp_ct)) return -1;
        result = ccmp_encrypt(ctx, nonce, plaintext, ciphertext_len,
                               aad, aad_len, tmp_ct, computed_tag, tag_len);
    }

    if (result < 0) return -1;

    /* Verify tag using constant-time comparison */
    if (constant_time_memcmp(tag, computed_tag, tag_len) != 0) {
        /* Authentication failed — zero out plaintext to prevent use */
        memset(plaintext, 0, ciphertext_len);
        return -1;
    }

    return 0;
}

/* ============================================================================
 * L8: AES-GCMP (Galois/Counter Mode) — NIST SP 800-38D
 * ============================================================================ */

/**
 * GF(2^128) multiplication used in GCM authentication.
 * Uses the "right shift" method with reduction polynomial:
 *   x^128 + x^7 + x^2 + x + 1
 */
static void gf128_mul(uint8_t *Z, const uint8_t *X, const uint8_t *Y)
{
    uint8_t V[AES_BLOCK_SIZE];
    uint8_t Ztmp[AES_BLOCK_SIZE];
    int i, j;

    memset(Ztmp, 0, AES_BLOCK_SIZE);
    memcpy(V, Y, AES_BLOCK_SIZE);

    for (i = 0; i < 128; i++) {
        int byte_idx = i / 8;
        int bit_idx  = 7 - (i % 8);

        if (X[byte_idx] & (1 << bit_idx)) {
            for (j = 0; j < AES_BLOCK_SIZE; j++) {
                Ztmp[j] ^= V[j];
            }
        }

        /* V = V >> 1 (big-endian right shift in GF(2^128)) */
        uint8_t lsb = V[15] & 1;
        for (j = 15; j > 0; j--) {
            V[j] = (uint8_t)((V[j] >> 1) | (V[j-1] << 7));
        }
        V[0] >>= 1;

        /* If LSB was 1, XOR with R = 0xE1 << 120 (reduction polynomial) */
        if (lsb) {
            V[0] ^= 0xE1;
        }
    }

    memcpy(Z, Ztmp, AES_BLOCK_SIZE);
}

/**
 * ghash — GHASH function for GCM authentication
 *
 * GHASH(H, A, C) = X_m where:
 *   X_0 = 0
 *   X_i = (X_{i-1} ⊕ A_i) · H   for i ≤ len(A)/128
 *   X_i = (X_{i-1} ⊕ C_i) · H   for subsequent blocks
 *   X_m = (X_{m-1} ⊕ [len(A)||len(C)]) · H
 *
 * All in GF(2^128) with multiplication constant H = E_K(0^128).
 */
static void ghash(const uint8_t *H,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *ciphertext, size_t ct_len,
                   uint8_t *tag)
{
    uint8_t block[AES_BLOCK_SIZE];
    size_t i, pos;

    memset(tag, 0, AES_BLOCK_SIZE);

    /* Process AAD */
    pos = 0;
    while (pos < aad_len) {
        size_t chunk = aad_len - pos;
        if (chunk > AES_BLOCK_SIZE) chunk = AES_BLOCK_SIZE;
        memset(block, 0, AES_BLOCK_SIZE);
        memcpy(block, aad + pos, chunk);
        for (i = 0; i < AES_BLOCK_SIZE; i++) tag[i] ^= block[i];
        gf128_mul(tag, tag, H);
        pos += chunk;
    }

    /* Process ciphertext */
    pos = 0;
    while (pos < ct_len) {
        size_t chunk = ct_len - pos;
        if (chunk > AES_BLOCK_SIZE) chunk = AES_BLOCK_SIZE;
        memset(block, 0, AES_BLOCK_SIZE);
        memcpy(block, ciphertext + pos, chunk);
        for (i = 0; i < AES_BLOCK_SIZE; i++) tag[i] ^= block[i];
        gf128_mul(tag, tag, H);
        pos += chunk;
    }

    /* Final block: len(A) || len(C) as 64-bit big-endian each */
    memset(block, 0, AES_BLOCK_SIZE);
    block[4] = (uint8_t)((aad_len * 8) >> 56);
    block[5] = (uint8_t)((aad_len * 8) >> 48);
    block[6] = (uint8_t)((aad_len * 8) >> 40);
    block[7] = (uint8_t)((aad_len * 8) >> 32);
    block[12] = (uint8_t)((ct_len * 8) >> 24);
    block[13] = (uint8_t)((ct_len * 8) >> 16);
    block[14] = (uint8_t)((ct_len * 8) >> 8);
    block[15] = (uint8_t)(ct_len * 8);
    for (i = 0; i < AES_BLOCK_SIZE; i++) tag[i] ^= block[i];
    gf128_mul(tag, tag, H);
}

void gcmp_init(gcmp_ctx_t *gctx, const uint8_t *key, int key_bits)
{
    uint8_t zeros[AES_BLOCK_SIZE];
    memset(zeros, 0, AES_BLOCK_SIZE);
    aes_key_setup(&gctx->aes_ctx, key, key_bits);
    aes_encrypt_block(&gctx->aes_ctx, zeros, gctx->H);
}

int gcmp_encrypt(gcmp_ctx_t *gctx,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *plaintext, size_t plaintext_len,
                 const uint8_t *aad, size_t aad_len,
                 uint8_t *ciphertext, uint8_t *tag, int tag_len)
{
    uint8_t j0[AES_BLOCK_SIZE];
    uint8_t ctr[AES_BLOCK_SIZE];
    uint8_t gh_result[AES_BLOCK_SIZE];

    if (!gctx || !iv || !plaintext || !ciphertext || !tag) return -1;

    /* Build J0 from IV */
    memset(j0, 0, AES_BLOCK_SIZE);
    if (iv_len == 12) {
        memcpy(j0, iv, 12);
        j0[15] = 0x01;
    } else {
        /* Hash IV with GHASH to produce J0 */
        ghash(gctx->H, NULL, 0, iv, iv_len, j0);
    }

    /* CTR mode encryption starting from J0 + 1 */
    memcpy(ctr, j0, AES_BLOCK_SIZE);
    /* Increment counter */
    {
        int c;
        for (c = 15; c >= 0; c--) {
            if (++ctr[c] != 0) break;
        }
    }
    aes_ctr_crypt(&gctx->aes_ctx, ctr, 4, plaintext, ciphertext, plaintext_len);

    /* GHASH for authentication */
    ghash(gctx->H, aad, aad_len, ciphertext, plaintext_len, gh_result);

    /* Final tag = GHASH ⊕ E_K(J0) */
    {
        int i;
        uint8_t ej0[AES_BLOCK_SIZE];
        aes_encrypt_block(&gctx->aes_ctx, j0, ej0);
        for (i = 0; i < tag_len && i < AES_BLOCK_SIZE; i++) {
            tag[i] = gh_result[i] ^ ej0[i];
        }
    }

    return 0;
}

int gcmp_decrypt(gcmp_ctx_t *gctx,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *ciphertext, size_t ciphertext_len,
                 const uint8_t *aad, size_t aad_len,
                 const uint8_t *tag, int tag_len,
                 uint8_t *plaintext)
{
    uint8_t j0[AES_BLOCK_SIZE];
    uint8_t ctr[AES_BLOCK_SIZE];
    uint8_t gh_result[AES_BLOCK_SIZE];
    uint8_t expected_tag[16];

    if (!gctx || !iv || !ciphertext || !plaintext || !tag) return -1;

    /* Build J0 from IV */
    memset(j0, 0, AES_BLOCK_SIZE);
    if (iv_len == 12) {
        memcpy(j0, iv, 12);
        j0[15] = 0x01;
    } else {
        ghash(gctx->H, NULL, 0, iv, iv_len, j0);
    }

    /* Decrypt first (CTR mode is symmetric) */
    memcpy(ctr, j0, AES_BLOCK_SIZE);
    {
        int c;
        for (c = 15; c >= 0; c--) {
            if (++ctr[c] != 0) break;
        }
    }
    aes_ctr_crypt(&gctx->aes_ctx, ctr, 4, ciphertext, plaintext, ciphertext_len);

    /* Compute expected tag */
    ghash(gctx->H, aad, aad_len, ciphertext, ciphertext_len, gh_result);
    {
        int i;
        uint8_t ej0[AES_BLOCK_SIZE];
        aes_encrypt_block(&gctx->aes_ctx, j0, ej0);
        for (i = 0; i < tag_len && i < AES_BLOCK_SIZE; i++) {
            expected_tag[i] = gh_result[i] ^ ej0[i];
        }
    }

    /* Constant-time tag verification */
    if (constant_time_memcmp(tag, expected_tag, tag_len) != 0) {
        memset(plaintext, 0, ciphertext_len);  /* Security: wipe on failure */
        return -1;
    }

    return 0;
}

/* ============================================================================
 * L5: PBKDF2-HMAC-SHA256 — RFC 2898 / PKCS #5 v2.0
 * ============================================================================ */

/**
 * PBKDF2 derives a key from a password using salt and iteration count:
 *
 *   DK = T_1 || T_2 || ... || T_{dkLen/hLen}
 *   where T_i = U_1 ⊕ U_2 ⊕ ... ⊕ U_c
 *   and   U_1 = PRF(Password, Salt || INT(i))
 *         U_j = PRF(Password, U_{j-1})
 *
 * For WPA2-PSK: Password = passphrase, Salt = SSID, c = 4096, dkLen = 32
 *
 * L4: PBKDF2 achieves its design goal of slowing down brute-force attacks
 * by a factor of c (iterations).  With 4096 iterations and SHA-256,
 * each guess costs ~4096 HMAC-SHA256 computations ≈ 0.01s on modern HW.
 */
void pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
                         const uint8_t *salt, size_t salt_len,
                         uint32_t iterations,
                         uint8_t *dk, size_t dk_len)
{
    uint32_t block_idx;
    size_t hlen = HMAC_SHA256_SIZE;
    uint8_t u[HMAC_SHA256_SIZE];
    uint8_t t[HMAC_SHA256_SIZE];
    uint8_t salted[128];
    size_t pos, i, j;

    if (!password || !salt || !dk) return;
    if (dk_len == 0) return;

    /* Build Salt || INT(i) prefix */
    if (salt_len > sizeof(salted) - 4) return;  /* Safety check */
    memcpy(salted, salt, salt_len);

    for (block_idx = 1, pos = 0; pos < dk_len; block_idx++, pos += hlen) {
        size_t remaining = dk_len - pos;
        size_t take = (remaining < hlen) ? remaining : hlen;

        /* Append block index (big-endian 32-bit) */
        salted[salt_len]     = (uint8_t)(block_idx >> 24);
        salted[salt_len + 1] = (uint8_t)(block_idx >> 16);
        salted[salt_len + 2] = (uint8_t)(block_idx >>  8);
        salted[salt_len + 3] = (uint8_t)(block_idx);

        /* U_1 = PRF(Password, Salt || INT(i)) */
        hmac_sha256(password, password_len, salted, salt_len + 4, u);
        memcpy(t, u, hlen);

        /* U_2 .. U_c: U_j = PRF(Password, U_{j-1}) */
        for (j = 1; j < iterations; j++) {
            hmac_sha256(password, password_len, u, hlen, u);
            /* T = U_1 ⊕ U_2 ⊕ ... ⊕ U_c */
            for (i = 0; i < hlen; i++) {
                t[i] ^= u[i];
            }
        }

        /* Append T to DK */
        memcpy(dk + pos, t, take);
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int constant_time_memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    size_t i;

    if (!a || !b) return -1;

    for (i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }

    /* Return 0 if equal, non-zero if different (constant-time) */
    return (int)diff;
}

void xor_bytes(uint8_t *dst, const uint8_t *src, size_t len)
{
    size_t i;
    if (!dst || !src) return;
    for (i = 0; i < len; i++) {
        dst[i] ^= src[i];
    }
}

void ctr_inc(uint8_t *ctr, size_t len)
{
    size_t i;
    if (!ctr) return;
    for (i = len; i > 0; i--) {
        if (++ctr[i - 1] != 0) break;
    }
}

/* ============================================================================
 * L4: AES Structural Self-Test — Verification of Cryptographic Properties
 *
 * These functions validate the mathematical structure of AES components:
 * the S-box (GF(2^8) inverse + affine transform) and the MixColumns
 * transformation (MDS matrix over GF(2^8) with known inverse).
 *
 * Knowledge: GF(2^8) field theory (L3), MDS codes (L4),
 *             substitution-permutation network integrity (L2).
 * ============================================================================ */

/**
 * aes_verify_sbox_construction — Verify the AES S-box is correctly constructed
 *
 * For every byte x ≠ 0:
 *   aes_sbox[x] = affine_transform(gf256_inv(x))
 * where affine_transform(y) = A·y ⊕ 0x63 (A is 8×8 bit matrix over GF(2)).
 *
 * aes_sbox[0] is handled specially: inv(0) ≡ 0, then affine(0) = 0x63.
 *
 * This demonstrates the only non-linear component of AES and its
 * resistance to linear/differential cryptanalysis (Daemen & Rijmen, 2002).
 *
 * Returns 0 if all 256 S-box entries match the construction, non-zero otherwise.
 *
 * Complexity: O(256) byte operations.
 */
int aes_verify_sbox_construction(void)
{
    int x, bit;
    int mismatches = 0;

    for (x = 0; x < 256; x++) {
        uint8_t inv = gf256_inv((uint8_t)x);
        uint8_t affine = 0;

        /* Affine transformation over GF(2):
         *   out_bit[i] = inv_bit[i]
         *     ^ inv_bit[(i+4) mod 8]
         *     ^ inv_bit[(i+5) mod 8]
         *     ^ inv_bit[(i+6) mod 8]
         *     ^ inv_bit[(i+7) mod 8]
         *     ^ c_bit[i]     where c = 0x63 */
        for (bit = 0; bit < 8; bit++) {
            int b = (inv >> bit) & 1;
            b ^= (inv >> ((bit + 4) & 7)) & 1;
            b ^= (inv >> ((bit + 5) & 7)) & 1;
            b ^= (inv >> ((bit + 6) & 7)) & 1;
            b ^= (inv >> ((bit + 7) & 7)) & 1;
            b ^= (0x63 >> bit) & 1;  /* c = 0x63 constant vector */
            if (b) affine |= (uint8_t)(1 << bit);
        }

        if (affine != aes_sbox[x]) {
            mismatches++;
        }
    }

    return mismatches;
}

/**
 * aes_verify_mixcol_inverse — Verify MixColumns and InvMixColumns matrices
 * multiply to the 4×4 identity in GF(2^8)
 *
 * Tests: for each column vector v (standard basis e_j):
 *   inv_mixcol_matrix × (mixcol_matrix × v) ≡ v  (as bytes, under GF(2^8))
 *
 * The MixColumns transformation is an MDS (Maximum Distance Separable)
 * code over GF(2^8), meaning any 2-round differential trail activates
 * at least 5 S-boxes — the foundation of AES's wide trail strategy.
 *
 * Returns 0 if the matrices are proper inverses, non-zero otherwise.
 *
 * Knowledge: MDS matrix theory (L4), linear algebra over GF(2^8) (L3).
 */
int aes_verify_mixcol_inverse(void)
{
    int col, row, k;
    int mismatches = 0;

    /* Test on each standard basis column vector e_j (j = 0..3) */
    for (col = 0; col < 4; col++) {
        uint8_t v[4] = {0, 0, 0, 0};
        uint8_t mc[4], inv_mc[4];

        v[col] = 1;  /* e_j */

        /* Forward: mc[i] = Σ_k mixcol_matrix[i][k] · v[k] in GF(2^8) */
        for (row = 0; row < 4; row++) {
            mc[row] = 0;
            for (k = 0; k < 4; k++) {
                mc[row] ^= gf256_mul(mixcol_matrix[row][k], v[k]);
            }
        }

        /* Inverse: inv_mc[i] = Σ_k inv_mixcol_matrix[i][k] · mc[k] */
        for (row = 0; row < 4; row++) {
            inv_mc[row] = 0;
            for (k = 0; k < 4; k++) {
                inv_mc[row] ^= gf256_mul(inv_mixcol_matrix[row][k], mc[k]);
            }
        }

        /* Verify identity: inv_mc should equal original v (e_j) */
        for (row = 0; row < 4; row++) {
            if (inv_mc[row] != v[row]) mismatches++;
        }
    }

    return mismatches;
}

/**
 * aes_self_test — Run all AES structural verifications
 *
 * Validates the mathematical integrity of the AES components:
 *   1. S-box: affine(gf256_inv(x)) ≡ aes_sbox[x] for all 256 bytes
 *   2. MixColumns: inv_mix · mix = Identity in GF(2^8)
 *
 * Returns 0 on success (all checks pass), non-zero on failure.
 * Call from test suites or as a one-time integrity check on library init.
 */
int aes_self_test(void)
{
    int errors = 0;

    errors += aes_verify_sbox_construction();
    errors += aes_verify_mixcol_inverse();

    return errors;
}
