/**
 * example_link_budget.c ¡ª L4/L6: End-to-end link budget for NR cell planning
 * Demonstrates: MAPL calculation, cell range estimation, CQI vs distance.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_link.h"

int main(void) {
    printf("=== NR Downlink Link Budget & Cell Range ===

");
    printf("Scenario: 3.5 GHz NR, 100 MHz BW, Urban Macro
");
    printf("%-30s %s
", "Parameter", "Value");
    printf("%-30s %.1f dBm
", "gNB Tx Power", 43.0);
    printf("%-30s %.1f dBi
", "gNB Antenna Gain", 15.0);
    printf("%-30s %.1f dB
", "Tx Cable Loss", 2.0);
    printf("%-30s %.1f dBi
", "UE Antenna Gain", 0.0);
    printf("%-30s %.1f dB
", "UE Noise Figure", 7.0);
    printf("%-30s %.0f MHz
", "Bandwidth", 100.0);

    double eirp = 43.0 + 15.0 - 2.0;
    double nf = noise_floor_dbm(100e6, 7.0);
    printf("%-30s %.1f dBm
", "EIRP", eirp);
    printf("%-30s %.1f dBm
", "Noise Floor", nf);

    printf("
--- Link Budget per Target SINR ---
");
    printf("%-10s %-12s %-12s %-12s %-12s
",
           "SINR(dB)", "MAPL(dB)", "Range(km)", "CQI", "Rate(Mbps)");

    double sinr_vals[] = {-5.0, 0.0, 5.0, 10.0, 15.0, 20.0};
    int n_sinr = sizeof(sinr_vals) / sizeof(sinr_vals[0]);

    for (int i = 0; i < n_sinr; i++) {
        double target_sinr = sinr_vals[i];
        double rx_sens = nf + target_sinr;
        double mapl = eirp + 0.0 - rx_sens - 8.0;
        double range = estimate_cell_range_km(mapl, 3500.0, 30.0, 1.5);
        cqi_t cqi = sinr_to_cqi(target_sinr);
        double rate = cqi_to_data_rate_mbps(cqi, 100.0);

        printf("%-10.1f %-12.1f %-12.2f %-12d %-12.1f
",
               target_sinr, mapl, range, cqi, rate);
    }

    printf("
=== Summary ===
");
    printf("Typical 3.5 GHz urban macro cell: 0.5-1.5 km radius
");
    printf("Cell-edge user (SINR=-5 dB): ~15 Mbps
");
    printf("Cell-center user (SINR=20 dB): ~550 Mbps (100 MHz BW)
");

    return 0;
}
