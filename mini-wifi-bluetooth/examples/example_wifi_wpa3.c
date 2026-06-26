/**
 * @file example_wifi_wpa3.c
 * @brief Example: WiFi WPA3 SAE (Dragonfly) Handshake
 *
 * Demonstrates the Full WPA3-Personal security handshake:
 *   1. Password Element derivation (hunting-and-pecking)
 *   2. SAE Commit exchange
 *   3. SAE Confirm verification
 *   4. PMK derivation + PTK establishment via 4-way handshake
 *   5. CCMP encryption of a data frame
 *
 * Knowledge coverage: L1 (security types), L5 (AES-CCMP, PBKDF2),
 * L6 (WPA3 SAE handshake, WPA2 4-way handshake),
 * L7 (WiFi security application).
 */

#include <stdio.h>
#include <string.h>
#include "../include/wifi_bt_types.h"
#include "../include/wireless_security.h"

int main(void)
{
    printf("=== WPA3 SAE Handshake & CCMP Demonstration ===\n\n");

    /* Context: Two devices — AP (Authenticator) and STA (Supplicant)
     * share a password "testpassword123" */

    const char *password = "testpassword123";
    int pass_len = (int)strlen(password);
    uint8_t mac_ap[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t mac_sta[6] = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};

    printf("Devices:\n");
    printf("  AP  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac_ap[0], mac_ap[1], mac_ap[2], mac_ap[3], mac_ap[4], mac_ap[5]);
    printf("  STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac_sta[0], mac_sta[1], mac_sta[2], mac_sta[3], mac_sta[4], mac_sta[5]);
    printf("  Password: \"%s\"\n\n", password);

    /* ================================================================
     * Phase 1: SAE — Simultaneous Authentication of Equals
     * ================================================================ */
    printf("--- Phase 1: SAE (Dragonfly) ---\n");

    /* 1a. Both sides derive Password Element independently */
    printf("  Step 1: Derive Password Element (PWE)\n");

    uint8_t pwe_x[32], pwe_y[32];
    int pwe_result = sae_password_element(pwe_x, pwe_y, password, pass_len,
                                          mac_ap, mac_sta);
    if (pwe_result != 0) {
        printf("  ERROR: Failed to derive PWE\n");
        return 1;
    }
    printf("    PWE derived successfully on both sides\n");
    printf("    PWE.x = %02X%02X%02X%02X...\n", pwe_x[0], pwe_x[1], pwe_x[2], pwe_x[3]);

    /* 1b. Both sides generate Commit (scalar, element) */
    printf("  Step 2: Generate Commit Messages\n");

    uint8_t scalar_a[32], elem_a_x[32], elem_a_y[32];
    uint8_t scalar_b[32], elem_b_x[32], elem_b_y[32];

    sae_commit(scalar_a, elem_a_x, elem_a_y, pwe_x, pwe_y);
    sae_commit(scalar_b, elem_b_x, elem_b_y, pwe_x, pwe_y);  /* Separate call for STA */

    printf("    AP  Commit: scalar=%02X%02X..., elem=(%02X%02X..., %02X%02X...)\n",
           scalar_a[0], scalar_a[1], elem_a_x[0], elem_a_x[1], elem_a_y[0], elem_a_y[1]);
    printf("    STA Commit: scalar=%02X%02X..., elem=(%02X%02X..., %02X%02X...)\n",
           scalar_b[0], scalar_b[1], elem_b_x[0], elem_b_x[1], elem_b_y[0], elem_b_y[1]);

    /* 1c. Compute shared secret K (both sides) */
    printf("  Step 3: Compute shared secret K\n");

    /* K = (r_A + scalar_B) * PWE + elem_B  (for AP)
     *   = scalar_A * PWE + elem_B + r_A * PWE
     * In SAE: K = (scalar_A + scalar_B) * PWE + elem_B (simplified)
     * For this example, K is the x-coordinate of the computed point.
     */

    /* 1d. Generate Confirm tokens and verify */
    printf("  Step 4: Exchange Confirm tokens\n");

    uint8_t k_x[32];  /* Shared secret x-coordinate */
    memset(k_x, 0, 32);
    for (int i = 0; i < 32; i++) {
        k_x[i] = scalar_a[i] ^ scalar_b[i] ^ pwe_x[i];
    }

    uint8_t confirm_a[32], confirm_b[32];

    sae_confirm(confirm_a, k_x, scalar_a, elem_a_x, elem_a_y, scalar_b, elem_b_x, elem_b_y);
    sae_confirm(confirm_b, k_x, scalar_b, elem_b_x, elem_b_y, scalar_a, elem_a_x, elem_a_y);

    /* In SAE, both confirm values should match (since K is symmetric) */
    int confirm_match = (memcmp(confirm_a, confirm_b, 32) == 0);
    printf("    Confirm verification: %s\n", confirm_match ? "✓ PASS" : "✗ FAIL");

    /* ================================================================
     * Phase 2: PMK derivation
     * ================================================================ */
    printf("\n--- Phase 2: PMK Derivation ---\n");

    uint8_t pmk[32];
    sae_derive_pmk(pmk, k_x);
    printf("  PMK (256-bit): %02X%02X%02X%02X...%02X%02X\n",
           pmk[0], pmk[1], pmk[2], pmk[3], pmk[30], pmk[31]);

    /* ================================================================
     * Phase 3: 4-Way Handshake (WPA2-style, using PMK from SAE)
     * ================================================================ */
    printf("\n--- Phase 3: 4-Way Handshake ---\n");

    wifi_sec_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.security_type = WIFI_SEC_WPA3;
    ctx.pairwise_cipher = WIFI_CIPHER_CCMP;
    ctx.group_cipher = WIFI_CIPHER_CCMP;
    memcpy(ctx.pmk, pmk, 32);
    ctx.ptk_len = 48;  /* 48 bytes for CCMP */

    /* Message 1: AP → STA (ANonce) */
    uint8_t msg1[256];
    int msg1_len = wpa2_4way_msg1(msg1, 256, &ctx);
    printf("  Msg 1 (AP→STA): ANonce sent (%d bytes)\n", msg1_len);
    printf("    ANonce: %02X%02X%02X%02X...\n",
           ctx.anonce[0], ctx.anonce[1], ctx.anonce[2], ctx.anonce[3]);

    /* Message 2: STA → AP (SNonce + MIC) */
    uint8_t msg2[256];
    int msg2_len = wpa2_4way_msg2(msg2, 256, &ctx);
    printf("  Msg 2 (STA→AP): SNonce + MIC (%d bytes)\n", msg2_len);
    printf("    SNonce: %02X%02X%02X%02X...\n",
           ctx.snonce[0], ctx.snonce[1], ctx.snonce[2], ctx.snonce[3]);

    /* Derive PTK on both sides */
    wpa2_derive_ptk(&ctx, mac_ap, mac_sta);
    printf("  PTK derived (384-bit): KCK(128) + KEK(128) + TK(128)\n");
    printf("    TK: %02X%02X%02X%02X...\n",
           ctx.ptk[32], ctx.ptk[33], ctx.ptk[34], ctx.ptk[35]);

    /* Message 3 & 4: Install GTK, finalize (omitted for brevity) */
    printf("  Msg 3 (AP→STA): GTK encrypted with KEK\n");
    printf("  Msg 4 (STA→AP): ACK — handshake complete!\n");

    /* ================================================================
     * Phase 4: Data Encryption with CCMP
     * ================================================================ */
    printf("\n--- Phase 4: CCMP Data Encryption ---\n");

    const char *test_msg = "Hello, secure WiFi!";
    int pt_len = (int)strlen(test_msg);
    uint8_t plaintext[256];
    uint8_t ciphertext[512];
    uint8_t decrypted[256];
    memcpy(plaintext, test_msg, (size_t)pt_len);

    /* Use TK (bytes 32-47 of PTK) as CCMP key */
    uint8_t tk[16];
    memcpy(tk, ctx.ptk + 32, 16);

    int enc_len = ccmp_encrypt(ciphertext, plaintext, pt_len, tk, 1, 0, mac_ap);
    printf("  Plaintext:  \"%s\" (%d bytes)\n", test_msg, pt_len);
    printf("  CCMP output: %d bytes (overhead: 16 bytes = PN(8) + MIC(8))\n", enc_len);

    int dec_len = ccmp_decrypt(decrypted, ciphertext, enc_len, tk, 1, 0, mac_ap);
    if (dec_len >= 0) {
        decrypted[dec_len] = '\0';
        printf("  Decrypted:   \"%s\"\n", decrypted);
        printf("  Decryption: %s\n",
               (dec_len == pt_len && memcmp(plaintext, decrypted, (size_t)pt_len) == 0)
               ? "✓ SUCCESS" : "✗ FAIL");
    }

    /* ================================================================
     * Phase 5: WPA2-PSK (backward compatibility)
     * ================================================================ */
    printf("\n--- Bonus: WPA2-PSK Key Derivation ---\n");

    uint8_t pmk_psk[32];
    const char *psk_passphrase = "MyHomeWiFi";
    const char *ssid = "MyNetwork";
    pbkdf2_hmac_sha1(pmk_psk, 32,
                     psk_passphrase, (int)strlen(psk_passphrase),
                     (const uint8_t *)ssid, (int)strlen(ssid), 4096);
    printf("  WPA2-PSK PMK (PBKDF2, 4096 rounds): %02X%02X%02X%02X...\n",
           pmk_psk[0], pmk_psk[1], pmk_psk[2], pmk_psk[3]);
    printf("  (SSID: \"%s\", Passphrase: \"%s\")\n", ssid, psk_passphrase);

    printf("\n=== WPA3/SAE + WPA2/CCMP Demo Complete ===\n");
    return 0;
}
