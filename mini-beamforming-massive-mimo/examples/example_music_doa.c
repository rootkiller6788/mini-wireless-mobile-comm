/**
 * example_music_doa.c — MUSIC DOA Estimation Demo
 * L6 Canonical Problem: Resolve two closely-spaced sources
 */
#include "beamforming_types.h"
#include "beamforming_array.h"
#include "beamforming_doa.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main(void) {
    printf("=== MUSIC DOA Estimation Example ===\n\n");

    /* 8-element ULA at 3 GHz */
    ula_geometry ula = ula_init(3.0e9, 8, 0.5);
    size_t M = ula.num_elements;
    size_t N_snap = 200;
    size_t K = 2;
    double src1 = 30.0 * M_PI / 180.0;
    double src2 = 35.0 * M_PI / 180.0;

    printf("Array: %zu elements, lambda/2 spacing\n", M);
    printf("Sources at: %.1f deg, %.1f deg\n", src1*180.0/M_PI, src2*180.0/M_PI);
    printf("Snapshots: %zu\n\n", N_snap);

    /* Generate synthetic data */
    array_snapshot_buffer buf = snapshot_buffer_alloc(M, N_snap);
    complex_vector sv1 = cvec_alloc(M), sv2 = cvec_alloc(M);
    steering_direction_1d d1, d2;
    d1.theta_rad = src1; d1.sin_theta = sin(src1);
    d2.theta_rad = src2; d2.sin_theta = sin(src2);
    ula_steering_vector(&ula, d1, &sv1);
    ula_steering_vector(&ula, d2, &sv2);

    for (size_t n = 0; n < N_snap; n++) {
        double ph1 = 2.0 * M_PI * rand() / RAND_MAX;
        double ph2 = 2.0 * M_PI * rand() / RAND_MAX;
        for (size_t m = 0; m < M; m++) {
            complex_double sig = cadd(cmul(sv1.data[m], cexpj(ph1)),
                                     cmul(sv2.data[m], cexpj(ph2)));
            double nr = 0.05 * (rand() / (double)RAND_MAX - 0.5);
            double ni = 0.05 * (rand() / (double)RAND_MAX - 0.5);
            cmat_set(&buf.data, m, n, cadd(sig, make_complex(nr, ni)));
        }
    }

    /* Run MUSIC */
    music_config mc = music_config_default(M, N_snap, K);
    mc.angle_grid_size = 360;
    doa_result dr = doa_result_alloc(K, mc.angle_grid_size);
    doa_music(&buf, &ula, &mc, &dr);

    printf("Detected %zu sources:\n", dr.num_sources);
    for (size_t i = 0; i < dr.num_sources; i++)
        printf("  Source %zu: %.2f deg (confidence: %.3f)\n",
               i+1, dr.sources[i].angle_rad * 180.0 / M_PI,
               dr.sources[i].confidence);

    printf("\nMUSIC Spectrum (peak-normalized):\n");
    for (size_t i = 0; i < dr.spectrum_length; i += 5) {
        double ang = dr.angle_grid[i] * 180.0 / M_PI;
        printf("  %6.1f deg: %e\n", ang, dr.spectrum[i]);
    }

    doa_result_free(&dr);
    snapshot_buffer_free(&buf);
    cvec_free(&sv1); cvec_free(&sv2);
    return 0;
}
