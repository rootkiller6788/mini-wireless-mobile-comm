/**
 * example_attack_demo.c — Wireless Attack Demonstration
 * L6 Canonical Problem: WPA2 security vulnerability analysis
 * L7 Application: Security auditing (Boeing, Detroit automotive, NHS, ISO 27001)
 * L8 Advanced: KRACK detection, SAE timing side-channel
 */
#include "wireless_attack.h"
#include "wireless_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    printf("=== Wireless Attack & Defense Demo ===\n\n");

    /* --- WEP FMS Attack Demo --- */
    printf("--- WEP FMS Key Recovery Attack ---\n");
    printf("Historical context: WEP uses RC4 with 24-bit IV.\n");
    printf("FMS (2001) showed ~60K-6M packets recover a 40-bit WEP key.\n");
    printf("This broke WEP permanently; WPA was the emergency fix.\n\n");

    wep_fms_state_t fms;
    wep_fms_init(&fms, 5);
    printf("Simulating WEP traffic capture...\n");

    int i;
    for (i = 0; i < 200; i++) {
        uint8_t iv[3];
        iv[0] = 3;
        iv[1] = 255;  /* Weak IV pattern */
        iv[2] = (uint8_t)(i & 0xFF);
        uint8_t ks = (uint8_t)((i * 7 + 13) & 0xFF);
        wep_fms_add_packet(&fms, iv, ks);
    }

    uint32_t total, weak; int recovered;
    wep_fms_stats(&fms, &total, &weak, &recovered);
    printf("  Packets captured: %u\n", total);
    printf("  Weak IVs found:   %u\n", weak);
    printf("  Key bytes recovered: %d\n", recovered);
    printf("  Note: 60-300 weak IVs needed per key byte in practice\n\n");

    /* --- WPA2 Dictionary Attack --- */
    printf("--- WPA2 PMKID Dictionary Attack ---\n");
    printf("Scenario: Attacker captures PMKID from RSN IE,\n");
    printf("then runs offline dictionary attack.\n\n");

    pmkid_capture_t target;
    memset(&target, 0, sizeof(target));
    memcpy(target.ssid, "HomeWiFi", 8); target.ssid_len = 8;
    uint8_t mac[6] = {0x00,0x11,0x22,0x33,0x44,0x55};
    memcpy(target.ap_mac, mac, 6);
    memcpy(target.sta_mac, mac, 6);

    /* Pre-compute the correct PMKID for "password123" */
    uint8_t correct_pmk[32];
    pbkdf2_hmac_sha256((const uint8_t*)"password123", 11, target.ssid, 8, 4096, correct_pmk, 32);
    handshake_compute_pmkid(correct_pmk, mac, mac, target.pmkid);

    /* Attacker's dictionary */
    const char *dictionary[] = {
        "12345678", "password", "admin", "letmein",
        "wifi123", "password123", "qwerty", "abc123"
    };
    int dict_size = 8;

    printf("Target SSID: HomeWiFi\n");
    printf("Dictionary size: %d words\n", dict_size);

    wpa2_dictionary_state_t dict_state;
    wpa2_dict_attack_init(&dict_state, &target);

    clock_t start = clock();
    int found = wpa2_dict_attack_try_list(&dict_state, dictionary, dict_size);
    clock_t end = clock();

    if (found >= 0) {
        printf("  KEY FOUND: \"%s\" at position %d\n",
               dictionary[found], found);
        printf("  Attempts: %u\n", dict_state.attempts);
        printf("  Time: %.3f ms\n",
               1000.0 * (end - start) / CLOCKS_PER_SEC);
    } else {
        printf("  Key not in dictionary (tried %u words)\n",
               dict_state.attempts);
    }
    printf("  Note: Real attacks test millions of words.\n");
    printf("  WPA2 with weak passwords (<8 chars) is vulnerable.\n");
    printf("  WPA3/SAE prevents offline dictionary attacks.\n\n");

    /* --- Wireless IDS Demo --- */
    printf("--- Wireless Intrusion Detection System ---\n");
    printf("Monitoring 802.11 traffic for attacks...\n\n");

    ids_state_t ids;
    ids_init(&ids, 60);  /* 60-second window */

    /* Simulate normal traffic */
    printf("Normal traffic: 5 data frames...\n");
    for (i = 0; i < 5; i++) ids_process_frame(&ids, 2, 0, 0);
    printf("  Alarms: 0x%02x\n", ids_check_alarms(&ids));

    /* Simulate deauthentication flood */
    printf("\nDeauth flood attack: 15 deauth frames in 1 second...\n");
    for (i = 0; i < 15; i++) ids_process_frame(&ids, 0, 12, 0);
    int alarms = ids_check_alarms(&ids);
    printf("  Alarms: 0x%02x\n", alarms);
    if (alarms & 0x01) printf("  => DEAUTH FLOOD DETECTED!\n");
    if (alarms & 0x02) printf("  => BRUTE FORCE DETECTED!\n");

    ids_reset_alarms(&ids);
    printf("  Alarms after reset: 0x%02x\n\n", ids_check_alarms(&ids));

    /* --- SAE Timing Side-Channel --- */
    printf("--- SAE Timing Side-Channel Analysis ---\n");
    printf("WPA3/SAE must use constant-time password element derivation.\n");
    printf("Timing variation > 5%% suggests vulnerable implementation.\n\n");

    double timing_samples[100];
    for (i = 0; i < 100; i++) {
        /* Simulate: 95 samples at 100us, 5 outliers at 150us */
        timing_samples[i] = (i < 95) ? 100.0 : 150.0;
    }

    int leak = detect_sae_timing_leak(timing_samples, 100);
    printf("  CV = %.2f%%\n", 100.0 * (50.0/3.0) / 102.5); /* approx */
    if (leak) {
        printf("  => TIMING LEAK DETECTED! Implementation vulnerable.\n");
    } else {
        printf("  => No significant timing leak detected.\n");
    }

    /* --- Cleanup --- */
    if (fms.captured) free(fms.captured);

    printf("\n=== Knowledge Points ===\n");
    printf("L1: Attack taxonomy (crypto, protocol, implementation, physical)\n");
    printf("L2: Attack surface concepts (WEP IV weakness, PMKID exposure)\n");
    printf("L4: FMS statistical theorem (weak IVs leak key bytes)\n");
    printf("L5: Dictionary attack, FMS vote counting, IDS thresholds\n");
    printf("L6: WPA2 vulnerability as canonical security case study\n");
    printf("L7: Boeing avionics, Detroit connected cars, NHS medical WiFi\n");
    printf("L8: Timing side-channel detection (SAE constant-time)\n");

    return 0;
}
