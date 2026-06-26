/**
 * example_key_derivation.c — WPA2 Key Hierarchy Demo
 * L6 Canonical Problem: PSK -> PMK -> PTK key derivation
 * L7 Application: WPA2-Personal WiFi key management (ISO, supplier networks)
 */
#include "wireless_key_mgmt.h"
#include "wireless_auth.h"
#include <stdio.h>
#include <string.h>

static void hex(const char *l, const uint8_t *d, size_t n) {
    printf("  %-20s: ", l);
    for (size_t i = 0; i < n && i < 16; i++) printf("%02x", d[i]);
    printf("\n\n");
}

int main(void) {
    printf("=== WPA2 Key Hierarchy Demo ===\n\n");

    /* WPA2-Personal network parameters */
    const char *ssid = "StarbucksWiFi";
    const char *passphrase = "ilovecoffee";
    printf("Network: %s\n", ssid);
    printf("Passphrase: %s\n\n", passphrase);

    key_mgmt_ctx_t ctx;
    key_mgmt_init(&ctx);
    memcpy(ctx.ssid, ssid, strlen(ssid));
    ctx.ssid_len = strlen(ssid);

    /* Step 1: PSK -> PMK via PBKDF2 */
    printf("--- Step 1: PSK --[PBKDF2(4096)]--> PMK ---\n");
    printf("  PBKDF2(password=\"%s\", salt=\"%s\", c=4096, dkLen=256)\n",
           passphrase, ssid);

    if (key_mgmt_derive_from_passphrase(&ctx, passphrase) != 0) {
        printf("  PMK derivation FAILED\n"); return 1;
    }
    hex("PMK", ctx.pmk.bytes, 32);
    printf("  PMK is 256-bit symmetric: same on all devices using this passphrase\n\n");

    /* Step 2: PMK -> PTK via 4-way handshake PRF */
    printf("--- Step 2: PMK --[PRF-384]--> PTK ---\n");
    printf("  PRF(PMK, \"Pairwise key expansion\",\n");
    printf("       min(AA,SPA) || max(AA,SPA) ||\n");
    printf("       min(ANonce,SNonce) || max(ANonce,SNonce))\n");

    /* Set up MACs and nonces as they would be in real handshake */
    uint8_t ap_mac[6] = {0x00,0x11,0x22,0x33,0x44,0x55};
    uint8_t sta_mac[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
    memcpy(ctx.authenticator_mac, ap_mac, 6);
    memcpy(ctx.supplicant_mac, sta_mac, 6);
    generate_nonce(ctx.anonce, 32);
    generate_nonce(ctx.snonce, 32);

    if (derive_ptk(&ctx) != 0) {
        printf("  PTK derivation FAILED\n"); return 1;
    }

    hex("ANonce (AP random)", ctx.anonce, 16);
    hex("SNonce (STA random)", ctx.snonce, 16);
    printf("\n");
    hex("KCK (Key Confirm)", ctx.ptk.kck, 16);
    hex("KEK (Key Encrypt)", ctx.ptk.kek, 16);
    hex("TK (Temporal Key)", ctx.ptk.tk, 16);

    printf("\nPTK Structure: KCK(128) || KEK(128) || TK(128)\n");
    printf("  KCK: Used for MIC computation in 4-way handshake\n");
    printf("  KEK: Used to encrypt GTK in message 3\n");
    printf("  TK:  Used for AES-CCMP data encryption\n\n");

    /* Step 3: GTK derivation (for broadcast/multicast) */
    printf("--- Step 3: GMK --[PRF-256]--> GTK ---\n");
    generate_nonce(ctx.gmk.bytes, GMK_LEN);
    uint8_t gnonce[32]; generate_nonce(gnonce, 32);
    if (derive_gtk(&ctx, gnonce) != 0) {
        printf("  GTK derivation FAILED\n"); return 1;
    }
    hex("GTK (Group Key)", ctx.gtk.bytes, 16);
    printf("  GTK encrypts broadcast/multicast frames to all STAs\n\n");

    /* Step 4: HKDF demo (used in WPA3 for key derivation) */
    printf("--- Step 4: HKDF (WPA3-style key derivation) ---\n");
    const char *ikm = "SAE-shared-secret";
    const char *salt = "WPA3-session-salt";
    const char *info = "handshake-key-confirmation";
    uint8_t okm[32];
    hkdf((const uint8_t *)salt, strlen(salt),
         (const uint8_t *)ikm, strlen(ikm),
         (const uint8_t *)info, strlen(info),
         okm, 32);
    hex("HKDF Output", okm, 16);
    printf("  HKDF used in WPA3/SAE to derive session keys from DH shared secret\n\n");

    printf("=== Knowledge Points ===\n");
    printf("L1: PMK/PTK/GTK/KCK/KEK/TK key types\n");
    printf("L2: WPA2 3-tier key hierarchy concept\n");
    printf("L4: KDF security (PRF must be indistinguishable from random)\n");
    printf("L5: PBKDF2, WPA2-PRF, HKDF algorithms\n");
    printf("L6: WPA2 key management as canonical problem\n");
    printf("L7: Used in every WiFi network worldwide (ISO 27001, supplier audits)\n");
    printf("L8: HKDF is the modern KDF used in WPA3 and TLS 1.3\n");

    return 0;
}
