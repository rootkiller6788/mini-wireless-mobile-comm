/**
 * example_secrecy_capacity.c — Physical Layer Secrecy Capacity Demo
 * L6 Canonical Problem: Wyner wiretap channel secrecy rate
 * L7 Application: 5G/6G physical layer security (NASA, Mars comms)
 * L8 Advanced: MIMO artificial noise precoding
 */
#define _USE_MATH_DEFINES
#include "wireless_phy_sec.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main(void) {
    printf("=== Physical Layer Security: Secrecy Capacity Demo ===\n\n");

    /* --- Wyner's Secrecy Capacity Theorem --- */
    printf("--- Wyner's Secrecy Capacity Theorem (1975) ---\n");
    printf("C_s = max{ log2(1+SNR_bob) - log2(1+SNR_eve), 0 } bits/s/Hz\n\n");

    printf("Scenario 1: Bob is closer to AP than Eve\n");
    double snr_bob_db = 20.0;   /* Bob at 10m */
    double snr_eve_db = 5.0;    /* Eve at 30m */
    double cs = wyner_secrecy_capacity_db(snr_bob_db, snr_eve_db);
    printf("  SNR_Bob = %.1f dB, SNR_Eve = %.1f dB\n", snr_bob_db, snr_eve_db);
    printf("  Secrecy Capacity = %.4f bits/s/Hz\n", cs);
    printf("  => Secure communication IS possible\n\n");

    printf("Scenario 2: Eve is closer than Bob (hostile environment)\n");
    snr_bob_db = 5.0;
    snr_eve_db = 15.0;
    cs = wyner_secrecy_capacity_db(snr_bob_db, snr_eve_db);
    printf("  SNR_Bob = %.1f dB, SNR_Eve = %.1f dB\n", snr_bob_db, snr_eve_db);
    printf("  Secrecy Capacity = %.4f bits/s/Hz\n", cs);
    printf("  => Information-theoretic secrecy is IMPOSSIBLE\n");
    printf("  => Must use higher-layer crypto (WPA2/WPA3)\n\n");

    /* --- Rayleigh Fading Channel --- */
    printf("--- Ergodic Secrecy Capacity (Rayleigh Fading) ---\n");
    double avg_main = 10.0, avg_eve = 8.0;
    double erg_cs = fading_secrecy_capacity(avg_main, avg_eve, 50000);
    printf("  Avg SNR: Bob=%.1fdB, Eve=%.1fdB\n", avg_main, avg_eve);
    printf("  Ergodic Secrecy Capacity = %.4f bits/s/Hz\n", erg_cs);
    printf("  (Monte Carlo, 50000 samples)\n");
    printf("  => Even when avg Eve SNR < Bob SNR, fading provides opportunities\n\n");

    /* --- Channel-based Key Generation --- */
    printf("--- Channel-Based Secret Key Generation ---\n");
    channel_key_gen_ctx_t key_ctx;
    channel_key_gen_init(&key_ctx);

    /* Simulate CSI measurements with noise */
    int i;
    for (i = 0; i < 200; i++) {
        double mag = 1.0 + 0.1 * sin(2.0 * M_PI * i / 50.0) + 0.05 * ((double)rand() / RAND_MAX - 0.5);
        double phase = 0.5 * sin(2.0 * M_PI * i / 30.0);
        channel_key_gen_add_sample(&key_ctx, mag, phase);
    }

    int bits = channel_key_gen_quantize(&key_ctx, 2);
    printf("  CSI samples collected: %zu\n", key_ctx.num_samples);
    printf("  Quantization: %d bits extracted\n", bits);
    printf("  Bits per sample: %d\n", key_ctx.bits_per_sample);

    /* Reconcile and amplify */
    channel_key_gen_reconcile(&key_ctx, key_ctx.raw_bits, key_ctx.raw_bit_len, 3, 0.05);
    channel_key_gen_amplify(&key_ctx, 128);

    size_t key_len;
    const uint8_t *final_key = channel_key_gen_get_key(&key_ctx, &key_len);
    printf("  Final key: %zu bits (after reconciliation + amplification)\n", key_len);
    printf("  Key[0..7]: ");
    for (i = 0; i < 8 && i * 8 < (int)key_len; i++)
        printf("%02x ", final_key[i]);
    printf("\n\n");

    channel_key_gen_free(&key_ctx);

    /* --- Secrecy Metrics --- */
    printf("--- Complete Secrecy Metrics ---\n");
    wiretap_channel_t channel = {20.0, 10.0, 0.3, 1, 5.0};
    secrecy_metrics_t metrics;
    compute_secrecy_metrics(&channel, 2.0, &metrics);
    printf("  Secrecy Capacity:    %.4f bits/s/Hz\n", metrics.secrecy_capacity);
    printf("  Achievable Rate:     %.4f bits/s/Hz\n", metrics.secrecy_rate);
    printf("  Outage Probability:  %.4f\n", metrics.secrecy_outage_prob);
    printf("  Equivocation:        %.4f bits\n", metrics.equivocation);
    printf("  Key Gen Rate:        %.4f bits/sample\n", metrics.key_generation_rate);
    printf("\n");

    printf("=== Knowledge Points ===\n");
    printf("L1: Wiretap channel model, CSI samples, secrecy metrics structs\n");
    printf("L2: Physical layer security concept (complement to crypto)\n");
    printf("L3: Mutual information, equivocation, ergodic capacity\n");
    printf("L4: Wyner's theorem (proved 1975 Bell System Tech J)\n");
    printf("L5: Channel key gen (quantize, reconcile, amplify)\n");
    printf("L6: Secrecy capacity computation (canonical information theory problem)\n");
    printf("L7: 5G/6G PLS, NASA deep-space comms, Mars rover security\n");
    printf("L8: MIMO wiretap, artificial noise precoding, RIS\n");
    printf("L9: RIS-assisted secrecy (emerging 6G research, 2024+)\n");

    return 0;
}
