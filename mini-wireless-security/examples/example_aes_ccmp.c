/**
 * example_aes_ccmp.c — AES-CCMP Encrypted Frame Demo
 * L6 Canonical Problem: Authenticated encryption for WiFi frames
 * L7 Application: 802.11 data confidentiality (every WiFi packet)
 */
#include "wireless_crypto.h"
#include <stdio.h>
#include <string.h>

static void hex(const char *l, const uint8_t *d, size_t n) {
    printf("  %-18s: ", l);
    for (size_t i = 0; i < n && i < 32; i++) printf("%02x ", d[i]);
    printf("\n\n");
}

int main(void) {
    printf("=== AES-CCMP Wireless Frame Encryption ===\n\n");

    /* WPA2 CCMP key (derived from PTK in real handshake) */
    uint8_t tk[16] = {0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
                       0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};

    /* CCMP nonce: Priority(1) | Source Address(6) | Packet Number(6) */
    uint8_t nonce[13] = {0x00, 0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,
                          0x00,0x00,0x00,0x00,0x00,0x01};

    /* 802.11 MAC header as AAD (Additional Authenticated Data)
     * FC(2) + Dur(2) + Addr1(6) + Addr2(6) + Addr3(6) = 22 bytes */
    uint8_t aad[22] = {0x08,0x02,                      /* Frame Control */
                       0xFF,0xFF,                      /* Duration */
                       0x00,0x11,0x22,0x33,0x44,0x55, /* Addr1 */
                       0xAA,0xBB,0xCC,0xDD,0xEE,0xFF, /* Addr2 */
                       0x00,0x00,0x00,0x00,0x00,0x00  /* Addr3 */};
    size_t aad_len = 22;

    /* Plaintext payload */
    const char *payload = "WiFi Data: This packet contains sensitive user information!";
    size_t pt_len = strlen(payload);

    printf("Transmitting WiFi frame:\n");
    hex("Temporal Key", tk, 16);
    hex("CCMP Nonce", nonce, 13);
    printf("  Payload (%zu bytes): %s\n", pt_len, payload);

    /* Initialize CCMP with AES-128 */
    ccmp_ctx_t ctx;
    aes_key_setup(&ctx.aes_ctx, tk, 128);

    /* Encrypt */
    uint8_t ciphertext[256], tag[16];
    int tag_len = 8;  /* 802.11 uses 8-byte MIC */

    if (ccmp_encrypt(&ctx, nonce,
                     (const uint8_t *)payload, pt_len,
                     aad, aad_len,
                     ciphertext, tag, tag_len) != 0) {
        printf("ERROR: CCMP encryption failed!\n");
        return 1;
    }

    printf("\nEncrypted frame:\n");
    hex("Ciphertext", ciphertext, pt_len);
    hex("MIC (Tag)", tag, tag_len);

    /* Decrypt and verify on receiver side */
    printf("\n=== Receiver Side ===\n");
    uint8_t decrypted[256];

    if (ccmp_decrypt(&ctx, nonce,
                     ciphertext, pt_len,
                     aad, aad_len,
                     tag, tag_len,
                     decrypted) == 0) {
        hex("Decrypted", decrypted, pt_len);
        printf("  Plaintext: %s\n", decrypted);
        printf("\nSUCCESS: Frame decrypted and integrity verified!\n");
    } else {
        printf("FAILED: Authentication failed — possible tampering detected!\n");
        return 1;
    }

    /* Demonstrate tamper detection */
    printf("\n=== Tamper Detection Test ===\n");
    ciphertext[5] ^= 0x01;  /* Flip one bit in ciphertext */
    if (ccmp_decrypt(&ctx, nonce, ciphertext, pt_len,
                     aad, aad_len, tag, tag_len, decrypted) != 0) {
        printf("CORRECTLY DETECTED: Tampered frame rejected!\n");
        printf("This is how WPA2 protects against bit-flipping attacks.\n");
    }

    printf("\n=== Knowledge Points ===\n");
    printf("L1: CCMP = CTR mode + CBC-MAC (generic composition)\n");
    printf("L2: Authenticated Encryption = confidentiality + integrity\n");
    printf("L4: CCM security theorem (Jonsson 2002)\n");
    printf("L5: AES-CTR + AES-CBC-MAC implementation\n");
    printf("L6: 802.11i CCMP frame protection (canonical problem)\n");
    printf("L7: Every WPA2 WiFi frame uses this exact mechanism\n");

    return 0;
}
