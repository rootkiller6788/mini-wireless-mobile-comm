/**
 * example_massive_mimo.c — Massive MIMO Asymptotics Demo
 * L7 Application: Show channel hardening and favorable propagation
 */
#include "beamforming_types.h"
#include "beamforming_mimo.h"
#include <stdio.h>

int main(void) {
    printf("=== Massive MIMO Asymptotic Properties ===\n\n");

    /* Test channel hardening: as M grows, ||h_k||^2/M -> 1 */
    printf("Channel Hardening (variance of normalized channel gain):\n");
    printf("  M      Hardening Metric\n");
    printf("  -----  -----------------\n");

    size_t K = 4;
    for (size_t M = 4; M <= 64; M *= 2) {
        mimo_channel ch;
        channel_rayleigh_iid(M, K, &ch);
        channel_normalize(&ch);

        double hardening = channel_hardening_metric(&ch.H);
        printf("  %4zu   %.6f\n", M, hardening);

        channel_free(&ch);
    }
    printf("  (Hardening -> 0 as M -> infinity)\n\n");

    /* Test favorable propagation */
    printf("Favorable Propagation Check:\n");
    printf("  M      Max |inner product|  Favorable?\n");
    printf("  -----  --------------------  ----------\n");

    for (size_t M = 4; M <= 64; M *= 2) {
        mimo_channel ch;
        channel_rayleigh_iid(M, K, &ch);
        channel_normalize(&ch);

        int fp = check_favorable_propagation(&ch.H, 0.3);
        /* Compute max inner product for display */
        double max_ip = 0.0;
        for (size_t i = 0; i < K; i++) {
            for (size_t j = i + 1; j < K; j++) {
                complex_double ip = make_complex(0.0, 0.0);
                double ni = 0.0, nj = 0.0;
                for (size_t m = 0; m < M; m++) {
                    ip = cadd(ip, cmul(cconj(ch.H.data[m*K+i]), ch.H.data[m*K+j]));
                    double mi = complex_abs(ch.H.data[m*K+i]);
                    double mj = complex_abs(ch.H.data[m*K+j]);
                    ni += mi * mi; nj += mj * mj;
                }
                double nip = complex_abs(ip) / (sqrt(ni * nj) + 1e-300);
                if (nip > max_ip) max_ip = nip;
            }
        }
        printf("  %4zu   %.4f               %s\n", M, max_ip, fp ? "YES" : "NO");

        channel_free(&ch);
    }
    printf("  (Channels become asymptotically orthogonal)\n");

    return 0;
}
