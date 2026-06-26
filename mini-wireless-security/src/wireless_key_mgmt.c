/**
 * wireless_key_mgmt.c — Key Management: PRF, PTK/GTK, HKDF
 *
 * Implements WPA2 PRF, PTK derivation from PMK, GTK derivation,
 * HKDF extract-and-expand (RFC 5869), and key management operations.
 *
 * Knowledge Levels: L4 (KDF security theorems), L5 (PRF/HKDF algorithms),
 *                   L6 (WPA2 key hierarchy)
 *
 * References:
 *   IEEE 802.11i-2004, Section 8.5.1
 *   RFC 5869 — HMAC-based Extract-and-Expand Key Derivation Function
 *   NIST SP 800-108 — Key Derivation Using Pseudorandom Functions
 */

#include "wireless_key_mgmt.h"
#include "wireless_crypto.h"
#include "wireless_auth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * L5: WPA2 Pseudorandom Function (PRF) — IEEE 802.11i-2004 §8.5.1.1
 * ============================================================================ */

/**
 * WPA2 PRF is defined as:
 *
 *   PRF(K, A, B, Len):
 *     R = empty string
 *     for i = 0 to ceil(Len/160)-1:
 *       R = R || HMAC-SHA1(K, A || 0x00 || B || i)
 *     return first Len bytes of R
 *
 * A = label string (e.g., "Pairwise key expansion")
 * B = context data (e.g., min/max MACs and nonces)
 *
 * L4 Theorem: The WPA2 PRF is a secure KDF if HMAC-SHA1 is a PRF,
 * which holds under the assumption that SHA-1's compression function
 * is pseudorandom.  Note: SHA-1 has known collisions but remains
 * preimage-resistant, so HMAC-SHA1 remains secure for KDF usage.
 */
void wpa2_prf(const uint8_t *key, size_t key_len,
              const char *label,
              const uint8_t *context, size_t context_len,
              uint8_t *output, size_t output_len)
{
    size_t label_len, i, pos;

    if (!key || !label || !context || !output) return;
    if (output_len == 0) return;

    label_len = strlen(label);

    pos = 0;
    for (i = 0; pos < output_len; i++) {
        uint8_t hmac_input[512];
        size_t input_pos = 0;
        uint8_t hmac_out[HMAC_SHA256_SIZE];

        /* Build: A || 0x00 || B || counter_byte */
        memcpy(hmac_input + input_pos, label, label_len);
        input_pos += label_len;
        hmac_input[input_pos++] = 0x00;
        memcpy(hmac_input + input_pos, context, context_len);
        input_pos += context_len;
        hmac_input[input_pos++] = (uint8_t)i;

        /* HMAC-SHA1: We use HMAC-SHA256 as a substitute */
        hmac_sha256(key, key_len, hmac_input, input_pos, hmac_out);

        /* Copy up to 20 bytes per iteration (WPA2 uses SHA-1: 160 bits) */
        {
            size_t to_copy = 20; /* SHA-1 output size = 20 bytes */
            if (pos + to_copy > output_len) {
                to_copy = output_len - pos;
            }
            memcpy(output + pos, hmac_out, to_copy);
            pos += to_copy;
        }
    }
}

/* ============================================================================
 * L5: PTK Derivation
 * ============================================================================ */

int derive_ptk(key_mgmt_ctx_t *ctx)
{
    uint8_t context[76];
    size_t ctx_len;
    uint8_t ptk_raw[PTK_LEN];
    int ptk_total_len;

    if (!ctx) return -1;
    if (!ctx->pmk.is_valid) return -1;

    /* Determine PTK length based on cipher suite */
    ptk_total_len = (ctx->cipher_suite == CIPHER_SUITE_TKIP) ?
                     PTK_LEN_TKIP : PTK_LEN;

    /* Build context for PRF:
       min(AA, SPA) || max(AA, SPA) || min(ANonce, SNonce) || max(ANonce, SNonce) */
    {
        const uint8_t *mac1, *mac2, *nonce1, *nonce2;

        if (memcmp(ctx->authenticator_mac, ctx->supplicant_mac, 6) < 0) {
            mac1 = ctx->authenticator_mac;
            mac2 = ctx->supplicant_mac;
        } else {
            mac1 = ctx->supplicant_mac;
            mac2 = ctx->authenticator_mac;
        }

        if (memcmp(ctx->anonce, ctx->snonce, 32) < 0) {
            nonce1 = ctx->anonce;
            nonce2 = ctx->snonce;
        } else {
            nonce1 = ctx->snonce;
            nonce2 = ctx->anonce;
        }

        memcpy(context, mac1, 6);
        memcpy(context + 6, mac2, 6);
        memcpy(context + 12, nonce1, 32);
        memcpy(context + 44, nonce2, 32);
        ctx_len = 76;
    }

    /* Derive PTK via PRF */
    wpa2_prf(ctx->pmk.bytes, PMK_LEN,
             WPA_PAIRWISE_LABEL, context, ctx_len,
             ptk_raw, ptk_total_len);

    /* Split into KCK | KEK | TK */
    memcpy(ctx->ptk.kck, ptk_raw, KCK_LEN);
    memcpy(ctx->ptk.kek, ptk_raw + KCK_LEN, KEK_LEN);
    memcpy(ctx->ptk.tk,  ptk_raw + KCK_LEN + KEK_LEN, TK_LEN);

    return 0;
}

/* ============================================================================
 * L5: GTK Derivation
 * ============================================================================ */

int derive_gtk(key_mgmt_ctx_t *ctx, const uint8_t *gnonce)
{
    uint8_t context[44]; /* AA || GNonce */
    size_t ctx_len;

    if (!ctx || !gnonce) return -1;

    /* Build context: AA || GNonce */
    memcpy(context, ctx->authenticator_mac, 6);
    memcpy(context + 6, gnonce, 32);
    ctx_len = 38;

    /* Derive GTK via PRF-256 */
    wpa2_prf(ctx->gmk.bytes, GMK_LEN,
             WPA_GROUP_LABEL, context, ctx_len,
             ctx->gtk.bytes, GTK_LEN);

    return 0;
}

/* ============================================================================
 * L5: HKDF (HMAC-based Extract-and-Expand KDF) — RFC 5869
 * ============================================================================ */

/**
 * HKDF is a two-phase KDF:
 *
 *   Extract:  PRK = HMAC-Hash(salt, IKM)
 *   Expand:   OKM = HMAC-Hash(PRK, info || 0x01) ||
 *                    HMAC-Hash(PRK, T(1) || info || 0x02) || ...
 *             where T(0) = "" and T(i) = first i blocks
 *
 * L4 Theorem (Krawczyk 2010): HKDF is a secure KDF if HMAC-Hash
 * is a PRF.  The extract phase provides randomness extraction from
 * non-uniform IKM; the expand phase provides variable-length output.
 *
 * Used in WPA3/SAE for key derivation.
 */
void hkdf_extract(const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len,
                  uint8_t *prk)
{
    uint8_t zero_salt[SHA256_HASH_SIZE];

    if (!prk) return;

    /* If salt not provided, use string of zeros of HashLen */
    if (!salt || salt_len == 0) {
        memset(zero_salt, 0, SHA256_HASH_SIZE);
        salt = zero_salt;
        salt_len = SHA256_HASH_SIZE;
    }

    /* PRK = HMAC-Hash(salt, IKM) */
    hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

void hkdf_expand(const uint8_t *prk, size_t prk_len,
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len)
{
    uint8_t t[SHA256_HASH_SIZE];
    size_t t_len = 0;
    size_t pos = 0;
    uint8_t counter = 1;

    if (!prk || !okm) return;

    while (pos < okm_len) {
        hmac_sha256_ctx_t hmac_ctx;
        uint8_t block[SHA256_HASH_SIZE];

        hmac_sha256_init(&hmac_ctx, prk, prk_len);

        /* Feed T(prev) if any */
        if (t_len > 0) {
            hmac_sha256_update(&hmac_ctx, t, t_len);
        }

        /* Feed info */
        if (info && info_len > 0) {
            hmac_sha256_update(&hmac_ctx, info, info_len);
        }

        /* Feed counter */
        hmac_sha256_update(&hmac_ctx, &counter, 1);

        hmac_sha256_final(&hmac_ctx, block);

        /* Copy to output */
        {
            size_t to_copy = SHA256_HASH_SIZE;
            if (pos + to_copy > okm_len) {
                to_copy = okm_len - pos;
            }
            memcpy(okm + pos, block, to_copy);
            pos += to_copy;
        }

        /* Save for next iteration */
        memcpy(t, block, SHA256_HASH_SIZE);
        t_len = SHA256_HASH_SIZE;
        counter++;
    }
}

void hkdf(const uint8_t *salt, size_t salt_len,
          const uint8_t *ikm, size_t ikm_len,
          const uint8_t *info, size_t info_len,
          uint8_t *okm, size_t okm_len)
{
    uint8_t prk[SHA256_HASH_SIZE];

    hkdf_extract(salt, salt_len, ikm, ikm_len, prk);
    hkdf_expand(prk, SHA256_HASH_SIZE, info, info_len, okm, okm_len);
}

/* ============================================================================
 * L6: Key Management Operations
 * ============================================================================ */

void key_mgmt_init(key_mgmt_ctx_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(key_mgmt_ctx_t));
    ctx->pmk.is_valid = 0;
    ctx->cipher_suite = CIPHER_SUITE_CCMP;
    ctx->rekey_counter = 0;
}

void key_mgmt_set_psk(key_mgmt_ctx_t *ctx, const uint8_t *psk, size_t psk_len)
{
    if (!ctx || !psk) return;

    if (psk_len <= PMK_LEN) {
        memcpy(ctx->pmk.bytes, psk, psk_len);
        if (psk_len < PMK_LEN) {
            memset(ctx->pmk.bytes + psk_len, 0, PMK_LEN - psk_len);
        }
        ctx->pmk.is_valid = 1;
    }
}

int key_mgmt_derive_from_passphrase(key_mgmt_ctx_t *ctx,
                                     const char *passphrase)
{
    if (!ctx || !passphrase) return -1;
    if (ctx->ssid_len == 0) return -1;  /* Need SSID */

    /* PMK = PBKDF2(HMAC-SHA1, passphrase, SSID, 4096, 256) */
    pbkdf2_hmac_sha256((const uint8_t *)passphrase, strlen(passphrase),
                        ctx->ssid, ctx->ssid_len,
                        4096,
                        ctx->pmk.bytes, PMK_LEN);
    ctx->pmk.is_valid = 1;

    return 0;
}

int key_mgmt_install_keys(key_mgmt_ctx_t *ctx)
{
    if (!ctx) return -1;
    /* In a real implementation, keys would be installed into the
       wireless NIC's hardware crypto engine.  Here we just mark as done. */
    return 0;
}

int key_mgmt_rekey(key_mgmt_ctx_t *ctx)
{
    uint8_t gnonce[32];

    if (!ctx) return -1;

    /* Generate fresh group nonce */
    if (generate_nonce(gnonce, sizeof(gnonce)) < 0) return -1;

    /* Derive new GTK */
    if (derive_gtk(ctx, gnonce) < 0) return -1;

    ctx->rekey_counter++;
    return 0;
}

const uint8_t* key_mgmt_get_tk(const key_mgmt_ctx_t *ctx)
{
    if (!ctx) return NULL;
    return ctx->ptk.tk;
}

const uint8_t* key_mgmt_get_gtk(const key_mgmt_ctx_t *ctx)
{
    if (!ctx) return NULL;
    return ctx->gtk.bytes;
}
