/**
 * example_cell_planning.c ¡ª L6: Hex grid deployment with frequency reuse
 * Demonstrates: hex grid generation, frequency reuse assignment,
 *               co-channel interference, SINR at cell edge.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_model.h"
#include "../include/cell_network_link.h"

int main(void) {
    printf("=== Cell Planning: Hex Grid with Frequency Reuse ===

");

    cell_cluster_t cluster;
    int n = cell_cluster_init_hexagonal(&cluster, 3, 1500.0, 500.0);
    printf("Grid: %d sites, ISD=%.0f m, Area=%.2f km^2

",
           n, cluster.inter_site_distance_m, cluster.area_sqkm);

    int reuse_factors[] = {1, 3, 4, 7};
    int n_reuse = sizeof(reuse_factors) / sizeof(reuse_factors[0]);

    for (int ri = 0; ri < n_reuse; ri++) {
        int N = reuse_factors[ri];
        freq_reuse_assign_groups(&cluster, N);
        printf("--- Reuse N=%d (D/R=%.2f) ---
", N, freq_reuse_d_over_r(N));

        double worst_sir = freq_reuse_worst_sir_db(N, 3.5, 6);
        printf("  Worst-case SIR (gamma=3.5, 6 interferers): %.1f dB
", worst_sir);

        double interf_0 = compute_cochannel_interference_dbm(&cluster, 0, 3.5);
        printf("  Co-channel interference at cell[0]: %.1f dBm
", interf_0);

        double nf = noise_floor_dbm(20e6, 5.0);
        double rx = rx_power_dbm(43.0, 15.0, 0.0,
            hata_urban_macro_db(2140.0, 0.5, 30.0, 1.5), 2.0, 10.0);
        double sinr = sinr_db(rx, interf_0, nf);
        printf("  Cell-edge SINR estimate: %.1f dB

", sinr);
    }

    printf("=== Verification ===
");
    printf("Reuse-N=7 provides ~%.0f dB better SIR than N=1
",
           freq_reuse_worst_sir_db(7, 3.5, 6) - freq_reuse_worst_sir_db(1, 3.5, 6));

    return 0;
}
