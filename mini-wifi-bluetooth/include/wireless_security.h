/**
 * @file wireless_security.h
 * @brief Wireless Security — WPA2/WPA3, 4-Way Handshake, SAE, AES-CCM (L4,L5,L6)
 *
 * Implements WiFi and Bluetooth security protocols:
 *   - WPA2 4-Way Handshake (802.11i)
 *   - WPA3 Simultaneous Authentication of Equals (SAE / Dragonfly)
 *   - AES-CCMP encryption/decryption
 *   - AES-GCMP (WPA3)
 *   - PBKDF2 key derivation (WPA-Personal, SAE password element)
 *   - Bluetooth Secure Simple Pairing (SSP)
 *
 * Reference: IEEE Std 802.11-2020, Clause 12 "Security"
 * Reference: Bluetooth Core Specification v5.4, Vol 1, Part A, §5.4
 */
#ifndef WIRELESS_SECURITY_H
#define WIRELESS_SECURITY_H

#include "wifi_bt_types.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * AES-CCMP (AES-CCM) — WiFi WPA2 Core Encryption (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief AES-128 block cipher (single block encrypt)
 *
 * Standard AES-128 with 10 rounds. Used as the core building block
 * for CCMP, GCMP, and various key derivation functions.
 *
 * Rounds: AddRoundKey → (SubBytes+ShiftRows+MixColumns+AddRoundKey)×9
 *         → SubBytes+ShiftRows+AddRoundKey
 *
 * S-box: GF(2⁸) multiplicative inverse + affine transform
 *
 * @param output     16-byte ciphertext block
 * @param input      16-byte plaintext block
 * @param key        16-byte AES key
 *
 * Complexity: O(1) — 10 rounds × 16 bytes
 */
void aes128_encrypt_block(uint8_t output[16], const uint8_t input[16],
                          const uint8_t key[16]);

/**
 * @brief AES-128 block cipher (single block decrypt)
 *
 * Inverse AES: AddRoundKey → (InvShiftRows+InvSubBytes+AddRoundKey+InvMixColumns)×9
 *              → InvShiftRows+InvSubBytes+AddRoundKey
 *
 * @param output     16-byte plaintext block
 * @param input      16-byte ciphertext block
 * @param key        16-byte AES key
 */
void aes128_decrypt_block(uint8_t output[16], const uint8_t input[16],
                          const uint8_t key[16]);

/**
 * @brief AES-CBC-MAC (Cipher Block Chaining Message Authentication Code)
 *
 * Used in CCMP for MIC computation. CBC-MAC with zero IV:
 *   X₁ = AES(K, B₁)
 *   Xᵢ = AES(K, Xᵢ₋₁ XOR Bᵢ)
 *   MIC = Truncate₈(Xₙ)  (first 8 bytes for WPA2)
 *
 * @param mic         8-byte MIC output
 * @param data        Data to authenticate
 * @param data_len    Data length
 * @param key         AES key
 *
 * Complexity: O(data_len / 16)
 */
void aes_cbc_mac(uint8_t mic[8], const uint8_t *data, int data_len,
                 const uint8_t key[16]);

/**
 * @brief AES-CTR mode encryption/decryption (symmetric)
 *
 * Counter mode: Output = Plaintext XOR AES(K, Counter_block)
 * The counter block is incremented for each 16-byte block.
 *
 * Used in CCMP for data confidentiality.
 *
 * @param output       Output buffer (same length as input)
 * @param input        Input data
 * @param data_len     Data length
 * @param key          AES key
 * @param nonce        13-byte nonce (CCM nonce format)
 *
 * Complexity: O(data_len / 16)
 */
void aes_ctr_crypt(uint8_t *output, const uint8_t *input, int data_len,
                   const uint8_t key[16], const uint8_t nonce[13]);

/**
 * @brief CCMP encrypt (AES-CCM for WPA2)
 *
 * Full CCMP encryption with MIC generation. Steps:
 *   1. Construct AAD (Additional Authenticated Data) from MAC header
 *   2. Compute MIC via AES-CBC-MAC
 *   3. Encrypt payload + MIC via AES-CTR
 *
 * Output format: [Encrypted Payload] [Encrypted MIC (8 bytes)]
 *
 * @param output         Output buffer
 * @param plaintext      Plaintext payload
 * @param plaintext_len  Payload length
 * @param key            Temporal Key (TK, 16 bytes)
 * @param pn             Packet Number (48-bit, for nonce)
 * @param priority       QoS priority / frame control byte 1
 * @param addr2          TA (MAC header address 2, 6 bytes)
 * @return Total CCMP output length (plaintext_len + 16 overhead), or -1
 *
 * Complexity: O(plaintext_len)
 * Reference: IEEE 802.11i / 802.11-2020 §12.5.3
 */
int ccmp_encrypt(uint8_t *output, const uint8_t *plaintext, int plaintext_len,
                 const uint8_t key[16], uint64_t pn, uint8_t priority,
                 const uint8_t addr2[6]);

/**
 * @brief CCMP decrypt with MIC verification
 *
 * @param plaintext      Output plaintext
 * @param ciphertext     Encrypted payload + encrypted MIC
 * @param ciphertext_len Total length (including 8-byte MIC)
 * @param key            Temporal Key
 * @param pn             Packet Number
 * @param priority       Frame control byte 1
 * @param addr2          TA
 * @return Plaintext length on success, -1 on MIC failure
 */
int ccmp_decrypt(uint8_t *plaintext, const uint8_t *ciphertext, int ciphertext_len,
                 const uint8_t key[16], uint64_t pn, uint8_t priority,
                 const uint8_t addr2[6]);

/* ==========================================================================
 * WPA2 4-Way Handshake (L6 Canonical Problem)
 * ========================================================================== */

/**
 * @brief PRF (Pseudo-Random Function) for WPA2 key derivation (HMAC-SHA1 based)
 *
 * PRF-384(K, A, B) for WPA2 PTK derivation:
 *   PTK = PRF-384(PMK, "Pairwise key expansion", min(AA,SPA) || max(AA,SPA) ||
 *                  min(ANonce,SNonce) || max(ANonce,SNonce))
 *
 * Uses iterative HMAC-SHA1:
 *   R = HMAC-SHA1(K, A || 0 || B) || HMAC-SHA1(K, A || 1 || B) || ...
 *
 * @param output       Output buffer
 * @param output_len   Desired output length (e.g., 48 for PTK, 64 for full)
 * @param key          PMK (32 bytes)
 * @param key_len      Key length
 * @param label        String label (e.g., "Pairwise key expansion")
 * @param context      Context data (MAC addresses + nonces)
 * @param context_len  Context length
 * @return 0 on success, -1 on error
 *
 * Complexity: O(output_len)
 * Reference: IEEE 802.11-2020 §12.7.1.6.2
 */
int wpa2_prf(uint8_t *output, int output_len, const uint8_t *key, int key_len,
             const char *label, const uint8_t *context, int context_len);

/**
 * @brief WPA2 4-Way Handshake — Message 1 (AP → STA)
 *
 * AP sends ANonce to STA. STA derives PTK from PMK + ANonce + SNonce + addresses.
 *
 * @param msg1         Output: EAPOL-Key frame for Message 1
 * @param max_len      Max frame length
 * @param ctx          Security context (ANonce filled by AP)
 * @return Frame length, or -1 on error
 */
int wpa2_4way_msg1(uint8_t *msg1, int max_len, wifi_sec_context_t *ctx);

/**
 * @brief WPA2 4-Way Handshake — Message 2 (STA → AP)
 *
 * STA responds with SNonce + MIC(PTK, msg2). AP derives PTK and verifies MIC.
 *
 * @param msg2         Output: EAPOL-Key frame
 * @param max_len      Max frame length
 * @param ctx          Security context (SNonce filled by STA)
 * @return Frame length, or -1 on error
 */
int wpa2_4way_msg2(uint8_t *msg2, int max_len, wifi_sec_context_t *ctx);

/**
 * @brief WPA2 4-Way Handshake — derive PTK
 *
 * PTK = PRF-384(PMK, "Pairwise key expansion",
 *               Min(AA,SPA) || Max(AA,SPA) || Min(ANonce,SNonce) || Max(ANonce,SNonce))
 *
 * PTK components (64 bytes total for CCMP):
 *   - KCK: PTK[0:15]   (Key Confirmation Key, 128-bit)
 *   - KEK: PTK[16:31]  (Key Encryption Key, 128-bit)
 *   - TK:  PTK[32:47]  (Temporal Key, 128-bit for CCMP)
 *
 * @param ctx          Security context (PMK, ANonce, SNonce must be set)
 * @param aa           Authenticator Address (AP MAC)
 * @param spa          Supplicant Address (STA MAC)
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int wpa2_derive_ptk(wifi_sec_context_t *ctx, const uint8_t aa[6], const uint8_t spa[6]);

/**
 * @brief WPA2 4-Way Handshake — verify MIC
 *
 * Computes PTK and verifies the Message Integrity Code over the
 * EAPOL-Key frame using KCK and HMAC-SHA1.
 *
 * @param eapol_frame  EAPOL-Key frame (including MIC field set to 0)
 * @param frame_len    Frame length
 * @param kck          Key Confirmation Key (16 bytes)
 * @param mic          Output: Computed MIC (16 bytes)
 * @return 0 if computed successfully
 */
int wpa2_compute_mic(uint8_t mic[16], const uint8_t *eapol_frame, int frame_len,
                     const uint8_t kck[16]);

/* ==========================================================================
 * WPA3 SAE — Simultaneous Authentication of Equals (L6 Canonical Problem)
 * ========================================================================== */

/**
 * @brief SAE Password Element derivation (Hunting-and-Pecking)
 *
 * WPA3's SAE uses the Dragonfly key exchange with a password-derived
 * point on an elliptic curve (P-256).
 *
 * The hunting-and-pecking algorithm iterates to find an (x, y) that
 * satisfies the curve equation:
 *   1. seed = HMAC-SHA256(password, MAC1 || MAC2 || counter)
 *   2. x = seed mod p
 *   3. y² = x³ + ax + b (mod p) → check if y² is a quadratic residue
 *   4. If yes: y = sqrt(y²) mod p → PWE = (x, y)
 *   5. If no: increment counter and repeat (typically < 5 iterations)
 *
 * Curve: NIST P-256 (secp256r1)
 *   p = 2^256 - 2^224 + 2^192 + 2^96 - 1
 *   a = p - 3 = -3
 *   b = 0x5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B
 *
 * @param pwe_x       Output: PWE x-coordinate
 * @param pwe_y       Output: PWE y-coordinate
 * @param password    Password string (UTF-8)
 * @param password_len Password length
 * @param mac1        MAC address 1 (STA or AP)
 * @param mac2        MAC address 2
 * @return 0 on success (PWE found), -1 on failure
 *
 * Complexity: O(iterations × 1) — typically O(1)
 * Reference: IEEE 802.11-2020 §12.4.8.2.2 (Dragonfly)
 *            Harkins, D., RFC 7664 "Dragonfly Key Exchange"
 */
int sae_password_element(uint8_t pwe_x[32], uint8_t pwe_y[32],
                         const char *password, int password_len,
                         const uint8_t mac1[6], const uint8_t mac2[6]);

/**
 * @brief SAE Commit message (generating scalar+element)
 *
 * Each peer generates a random private scalar (r) and a random mask (m),
 * then computes:
 *   - scalar = (r + m) mod q         where q = curve order
 *   - element = -m · PWE             (inverse of mask times PWE)
 *
 * The commit message contains: scalar, element_x, element_y
 *
 * @param scalar_out      Output: commit scalar
 * @param element_x_out   Output: commit element x
 * @param element_y_out   Output: commit element y
 * @param pwe_x           Input: Password Element x
 * @param pwe_y           Input: Password Element y
 * @return 0 on success
 */
int sae_commit(uint8_t scalar_out[32], uint8_t element_x_out[32],
               uint8_t element_y_out[32], const uint8_t pwe_x[32],
               const uint8_t pwe_y[32]);

/**
 * @brief SAE Confirm message (key confirmation)
 *
 * After exchanging Commit messages, each peer computes:
 *   K = r_self · (scalar_peer · PWE + element_peer)  (shared secret)
 *   confirm = HMAC-SHA256(K, scalar_self || element_self || scalar_peer || element_peer)
 *
 * @param confirm_out    Output: 32-byte confirm token
 * @param k_x           Shared secret x-coordinate
 * @param scalar_self    Local commit scalar
 * @param element_self_x Local element x
 * @param element_self_y Local element y
 * @param scalar_peer    Peer commit scalar
 * @param element_peer_x Peer element x
 * @param element_peer_y Peer element y
 * @return 0 on success
 */
int sae_confirm(uint8_t confirm_out[32], const uint8_t k_x[32],
                const uint8_t scalar_self[32], const uint8_t element_self_x[32],
                const uint8_t element_self_y[32], const uint8_t scalar_peer[32],
                const uint8_t element_peer_x[32], const uint8_t element_peer_y[32]);

/**
 * @brief Derive PMK from SAE shared secret
 *
 * PMK = HMAC-SHA256(K, "SAE KCK and PMK" || 0x01)
 *
 * @param pmk          Output: 32-byte PMK
 * @param k_x          Shared secret K (x-coordinate)
 * @return 0 on success
 */
int sae_derive_pmk(uint8_t pmk[32], const uint8_t k_x[32]);

/* ==========================================================================
 * PBKDF2 Key Derivation (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief PBKDF2-HMAC-SHA1 key derivation
 *
 * Used in WPA-Personal (WPA2-PSK) to derive PMK from passphrase:
 *   PMK = PBKDF2(Passphrase, SSID, ssid_len, 4096, 256)
 *
 * PBKDF2: DK = T₁ || T₂ || ... || T_{dkLen/hLen}
 *   T₁ = F(Password, Salt, 1) = U₁ XOR ... XOR U_c
 *   U₁ = PRF(Password, Salt || INT(i))
 *   Uⱼ = PRF(Password, Uⱼ₋₁)
 *
 * @param dk           Output: derived key
 * @param dk_len       Desired key length
 * @param password     Password/passphrase
 * @param password_len Password length
 * @param salt         Salt (SSID for WPA2-PSK)
 * @param salt_len     Salt length
 * @param iterations   Iteration count (4096 for WPA2)
 * @return 0 on success
 *
 * Complexity: O(dk_len × iterations)
 * Reference: RFC 2898 / PKCS #5 v2.0
 */
int pbkdf2_hmac_sha1(uint8_t *dk, int dk_len,
                     const char *password, int password_len,
                     const uint8_t *salt, int salt_len, int iterations);

/* ==========================================================================
 * HMAC-SHA1 (L3 Mathematical Structure)
 * ========================================================================== */

/**
 * @brief HMAC-SHA1 computation
 *
 * HMAC(K, m) = SHA1((K' XOR opad) || SHA1((K' XOR ipad) || m))
 * where K' = K padded to 64 bytes, ipad = 0x36 repeated, opad = 0x5C repeated
 *
 * @param digest       Output 20-byte HMAC-SHA1
 * @param key          HMAC key
 * @param key_len      Key length
 * @param data         Message to authenticate
 * @param data_len     Message length
 *
 * Complexity: O(data_len)
 * Reference: RFC 2104
 */
void hmac_sha1(uint8_t digest[20], const uint8_t *key, int key_len,
               const uint8_t *data, int data_len);

/**
 * @brief HMAC-SHA256 computation
 *
 * HMAC-SHA256 uses SHA-256 with 64-byte block size.
 *
 * @param digest       Output 32-byte HMAC-SHA256
 * @param key          HMAC key
 * @param key_len      Key length
 * @param data         Message
 * @param data_len     Message length
 *
 * Complexity: O(data_len)
 */
void hmac_sha256(uint8_t digest[32], const uint8_t *key, int key_len,
                 const uint8_t *data, int data_len);

/* ==========================================================================
 * Bluetooth Secure Simple Pairing (SSP) — L6 Canonical Problem
 * ========================================================================== */

/**
 * @brief Bluetooth SSP — Numeric Comparison (association model)
 *
 * SSP Numeric Comparison protects against MITM attacks by having both
 * devices display a 6-digit number derived from ECDH public keys.
 *
 * Both devices compute:
 *   V = SHA-256(PK_A_x || PK_B_x || N_A || N_B) mod 10⁶
 * Both devices show V; user confirms they're equal.
 *
 * @param display_value Output: 6-digit numeric comparison value
 * @param pk_a_x        Device A public key x-coordinate (P-256)
 * @param pk_b_x        Device B public key x-coordinate (P-256)
 * @param nonce_a       Device A nonce (128-bit)
 * @param nonce_b       Device B nonce (128-bit)
 * @return 0 on success
 */
int bt_ssp_numeric_compare(int *display_value, const uint8_t pk_a_x[32],
                           const uint8_t pk_b_x[32], const uint8_t nonce_a[16],
                           const uint8_t nonce_b[16]);

/**
 * @brief Bluetooth SSP — Just Works (no MITM protection)
 *
 * Just Works mode uses the same ECDH key exchange as Numeric Comparison
 * but without user verification. Suitable for devices without displays.
 *
 * Link key = SHA-256(DHKey || "btlk" || BD_ADDR_A || BD_ADDR_B)
 *
 * @param link_key      Output: 16-byte link key
 * @param dhkey         DHKey from ECDH (P-256)
 * @param addr_a        Initiator BD_ADDR
 * @param addr_b        Responder BD_ADDR
 * @return 0 on success
 */
int bt_ssp_just_works_link_key(uint8_t link_key[16], const uint8_t dhkey[32],
                               const bt_address_t *addr_a, const bt_address_t *addr_b);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_SECURITY_H */
