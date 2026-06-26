/**
 * @file example_mimo_capacity.c
 * @brief Example: MIMO Channel Capacity Analysis (L6, L8)
 *
 * Demonstrates the Telatar-Foschini MIMO capacity theorem:
 *   C = log2(det(I + (rho/N_t)*HH^H))  bps/Hz
 *
 * Compares capacity across configurations:
 *   1. SISO 1x1 — baseline
 *   2. SIMO 1x4 — receive diversity
 *   3. MISO 4x1 — transmit diversity
 *   4. MIMO 2x2 — spatial multiplexing
 *   5. MIMO 4x4 — full spatial multiplexing
 *   6. Massive MIMO 4x64 — asymptotic orthogonality
 *
 * L6: This is the canonical MIMO capacity calculation
 * L8: Massive MIMO with channel hardening
 *
 * Reference: Telatar, "Capacity of Multi-antenna Gaussian Channels", 1999
 * Reference: 3GPP TR 38.803 (5G NR MIMO evaluation)
 */

#include "../include/channel_defs.h"
#include "../include/mimo_channel.h"
#include "../include/fading.h"
#include <stdio.h>
#include <math.h>

static void analyze_mimo_config(size_t n_rx, size_t n_tx,
                                 const char *label, int is_massive)
{
    mimo_channel_matrix_t *mimo = mimo_channel_alloc(n_rx, n_tx);
    if (!mimo) {
        printf("  %-16s: allocation failed\n", label);
        return;
    }

    /* Generate channel */
    if (is_massive) {
        mimo_generate_massive_iid(mimo);
    } else {
        mimo_generate_iid_rayleigh(mimo);
    }

    /* Compute capacities at multiple SNR points */
    double snr_dbs[] = {0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0};
    int n_snr = sizeof(snr_dbs) / sizeof(snr_dbs[0]);

    printf("  %-16s:", label);
    for (int i = 0; i < n_snr; i++) {
        channel_capacity_t cap = {0};
        mimo_capacity_equal_power(mimo, snr_dbs[i], 1.0, &cap);
        printf(" %5.1f", cap.spectral_efficiency);
        mimo_capacity_free(&cap);
    }
    printf(" bps/Hz\n");

    /* Additional metrics at 20 dB SNR */
    channel_capacity_t cap = {0};
    mimo_capacity_equal_power(mimo, 20.0, 1.0, &cap);

    /* Compare equal-power vs water-filling at 20 dB */
    channel_capacity_t cap_wf = {0};
    mimo_capacity_waterfilling(mimo, 20.0, 1.0, &cap_wf);

    double wf_gain = cap_wf.spectral_efficiency - cap.spectral_efficiency;

    double cond = mimo_condition_number(mimo);
    double div_order = mimo_diversity_order(mimo);
    size_t rank = mimo_rank(mimo, -20.0);

    printf("                 @20dB SE=%.2f bps/Hz, WF gain=%.2f bps, "
           "cond=%.1f, rank=%zu, div=%.1f\n",
           cap.spectral_efficiency, wf_gain, cond, rank, div_order);

    if (is_massive) {
        double hardening = mimo_channel_hardening_metric(mimo);
        printf("                 Channel hardening: %.4f (0=perfect)\n", hardening);
    }

    mimo_capacity_free(&cap);
    mimo_capacity_free(&cap_wf);
    mimo_channel_free(mimo);
}

int main(void)
{
    printf("=== MIMO Channel Capacity Analysis ===\n");
    printf("Reference: Telatar 1999, Foschini & Gans 1998\n\n");

    fading_rand_seed(42);

    printf("Spectral Efficiency (bps/Hz) vs SNR (dB)\n");
    printf("Configuration      SNR=  0    5   10   15   20   25   30\n");
    printf("------------------  ----------------------------------------\n");

    /* SISO 1x1 */
    analyze_mimo_config(1, 1, "SISO 1x1", 0);

    /* SIMO 1x4 — receive diversity */
    analyze_mimo_config(4, 1, "SIMO 1x4", 0);

    /* MISO 4x1 — transmit diversity (no CSIT) */
    analyze_mimo_config(1, 4, "MISO 4x1", 0);

    /* MIMO 2x2 */
    analyze_mimo_config(2, 2, "MIMO 2x2", 0);

    /* MIMO 4x4 */
    analyze_mimo_config(4, 4, "MIMO 4x4", 0);

    /* Massive MIMO 4x64 */
    analyze_mimo_config(4, 64, "Massive 4x64", 1);

    printf("\n=== Analysis ===\n");
    printf("L4 Telatar Theorem: MIMO capacity scales linearly with\n");
    printf("  min(N_rx, N_tx) at high SNR in rich scattering.\n\n");
    printf("Key observations:\n");
    printf("  SISO: C = log2(1+SNR) ~ 6.66 bps/Hz at 20 dB\n");
    printf("  SIMO 1x4: Rx diversity, log-det grow limited by 1 Tx\n");
    printf("  MISO 4x1: Tx diversity without CSIT, same as SISO\n");
    printf("  MIMO 2x2: Up to 2x SISO capacity ~13 bps/Hz\n");
    printf("  MIMO 4x4: Up to 4x SISO capacity ~27 bps/Hz\n");
    printf("  Massive 4x64: Channel hardening, near-deterministic\n");
    printf("    capacity, water-filling gain diminishes as N_tx grows\n\n");
    printf("L8 Advanced: In massive MIMO, (1/N_tx)*HH^H -> I_Nrx\n");
    printf("  (channel hardening), simplifying receiver design.\n");

    return 0;
}
