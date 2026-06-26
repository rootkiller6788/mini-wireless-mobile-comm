/**
 * example_wpa2_handshake.c — Complete WPA2 4-Way Handshake Demo
 * L6 Canonical Problem: WPA2 PSK authentication
 * L7 Application: WiFi security (iPhone, Android, IoT devices)
 */
#include "wireless_auth.h"
#include "wireless_crypto.h"
#include "wireless_key_mgmt.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("  %-20s: ", label);
    for (size_t i = 0; i < len && i < 16; i++) printf("%02x", data[i]);
    if (len > 16) printf("...");
    printf("\n");
}

int main(void) {
    printf("=== WPA2 4-Way Handshake Demo ===\n\n");

    /* Network parameters */
    const char *ssid = "MyHomeWiFi";
    const char *passphrase = "securepassword123";
    uint8_t ap_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t sta_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    printf("SSID: %s\n", ssid);
    printf("Passphrase: %s\n", passphrase);
    printf("AP MAC: 00:11:22:33:44:55\n");
    printf("STA MAC: aa:bb:cc:dd:ee:ff\n\n");

    /* Initialize handshake contexts */
    handshake_ctx_t ap_ctx, sta_ctx;
    handshake_init(&ap_ctx, (const uint8_t *)ssid, strlen(ssid), sta_mac, ap_mac, 0);
    handshake_init(&sta_ctx, (const uint8_t *)ssid, strlen(ssid), sta_mac, ap_mac, 0);

    /* Derive PMK from passphrase on both sides */
    printf("Deriving PMK via PBKDF2(4096 iterations)...\n");
    handshake_derive_pmk(&ap_ctx, passphrase);
    handshake_derive_pmk(&sta_ctx, passphrase);

    if (memcmp(ap_ctx.pmk.key, sta_ctx.pmk.key, WPA_PMK_LEN) == 0) {
        printf("  PMK derivation: SUCCESS (both sides match)\n");
        print_hex("PMK", ap_ctx.pmk.key, 16);
    } else {
        printf("  PMK derivation: FAILED (keys do not match!)\n");
        return 1;
    }

    /* === Message 1: AP -> STA === */
    uint8_t frame[512];
    size_t frame_len;

    printf("\n--- Message 1: AP sends ANonce ---\n");
    if (handshake_build_msg1(&ap_ctx, frame, &frame_len, sizeof(frame)) == 0) {
        printf("  AP sent Message 1 (ANonce)\n");
        print_hex("ANonce", ap_ctx.anonce, 16);
    }

    if (handshake_process_msg1(&sta_ctx, frame, frame_len) == 0) {
        printf("  STA received Message 1\n");
        printf("  STA generated SNonce\n");
        printf("  STA derived PTK\n");
    }

    /* === Message 2: STA -> AP === */
    printf("\n--- Message 2: STA sends SNonce + MIC ---\n");
    if (handshake_build_msg2(&sta_ctx, frame, &frame_len, sizeof(frame)) == 0) {
        printf("  STA sent Message 2 (SNonce + MIC)\n");
        print_hex("SNonce", sta_ctx.snonce, 16);
    }

    if (handshake_process_msg2(&ap_ctx, frame, frame_len) == 0) {
        printf("  AP received Message 2\n");
        printf("  AP derived PTK and verified MIC\n");
    }

    /* === Message 3: AP -> STA === */
    printf("\n--- Message 3: AP sends GTK + MIC ---\n");
    uint8_t gtk[32];
    generate_nonce(gtk, 32);
    if (handshake_build_msg3(&ap_ctx, frame, &frame_len, sizeof(frame), gtk, 32, 1) == 0) {
        printf("  AP sent Message 3 (encrypted GTK + MIC)\n");
    }

    if (handshake_process_msg3(&sta_ctx, frame, frame_len) == 0) {
        printf("  STA received Message 3\n");
        printf("  STA verified MIC and installed GTK\n");
    }

    /* === Message 4: STA -> AP === */
    printf("\n--- Message 4: STA sends ACK + MIC ---\n");
    if (handshake_build_msg4(&sta_ctx, frame, &frame_len, sizeof(frame)) == 0) {
        printf("  STA sent Message 4 (ACK)\n");
    }

    if (handshake_process_msg4(&ap_ctx, frame, frame_len) == 0) {
        printf("  AP received Message 4\n");
        printf("  AP verified MIC\n");
    }

    /* === Completion === */
    printf("\n=== Handshake Complete! ===\n");
    printf("AP state: %s\n", ap_ctx.state == HANDSHAKE_COMPLETE ? "COMPLETE" : "FAILED");
    printf("STA state: %s\n", sta_ctx.state == HANDSHAKE_COMPLETE ? "COMPLETE" : "FAILED");

    print_hex("KCK (shared)", ap_ctx.ptk.kck, 16);
    print_hex("TK (shared)", ap_ctx.ptk.tk, 16);

    /* Verify key agreement */
    if (memcmp(ap_ctx.ptk.kck, sta_ctx.ptk.kck, WPA_KCK_LEN) == 0 &&
        memcmp(ap_ctx.ptk.tk, sta_ctx.ptk.tk, WPA_TK_LEN) == 0) {
        printf("\nMutual authentication successful! Keys match.\n");
        printf("Secure communication can now begin using AES-CCMP.\n");
        return 0;
    } else {
        printf("\nKey agreement FAILED. Possible MITM attack.\n");
        return 1;
    }
}
