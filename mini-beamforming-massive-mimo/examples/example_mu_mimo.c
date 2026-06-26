/**
 * example_mu_mimo.c — Multi-User MIMO Precoding Demo
 * L6 Canonical Problem: MU-MIMO downlink with ZF vs MRT
 */
#include "beamforming_types.h"
#include "beamforming_precoder.h"
#include "beamforming_mimo.h"
#include <stdio.h>

int main(void) {
    printf("=== Multi-User MIMO Precoding Example ===\n\n");

    /* 4 BS antennas, 4 single-antenna users */
    size_t M = 4, K = 4;
    mimo_channel ch;
    channel_rayleigh_iid(M, K, &ch);
    channel_normalize(&ch);

    printf("Configuration: %zu BS antennas, %zu users\n", M, K);
    printf("SNR: 10 dB\n\n");

    double snr_linear = 10.0;  /* 10 dB */
    double noise_var = 1.0;

    /* MRT precoding */
    precoder_result pr_mrt = precoder_result_alloc(K, 1, K);
    precoder_mrt(&ch.H, &pr_mrt);
    double rate_mrt = mu_mimo_sum_rate(&ch.H, &pr_mrt, noise_var);

    /* ZF precoding */
    precoder_result pr_zf = precoder_result_alloc(K, 1, K);
    precoder_zf(&ch.H, &pr_zf);
    double rate_zf = mu_mimo_sum_rate(&ch.H, &pr_zf, noise_var);

    /* MMSE precoding */
    precoder_result pr_mmse = precoder_result_alloc(K, 1, K);
    precoder_mmse(&ch.H, &pr_mmse);
    double rate_mmse = mu_mimo_sum_rate(&ch.H, &pr_mmse, noise_var);

    printf("Sum-rate comparison (bps/Hz):\n");
    printf("  MRT:  %.4f\n", rate_mrt);
    printf("  ZF:   %.4f\n", rate_zf);
    printf("  MMSE: %.4f\n", rate_mmse);

    /* MIMO capacity upper bound */
    mimo_capacity cap = mimo_capacity_alloc(M < K ? M : K);
    mimo_capacity_openloop(&ch, snr_linear, &cap);
    printf("  Capacity: %.4f (upper bound)\n", cap.capacity_bps_hz);

    precoder_result_free(&pr_mrt);
    precoder_result_free(&pr_zf);
    precoder_result_free(&pr_mmse);
    mimo_capacity_free(&cap);
    channel_free(&ch);
    return 0;
}
