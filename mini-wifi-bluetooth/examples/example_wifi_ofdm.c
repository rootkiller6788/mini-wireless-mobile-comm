/**
 * @file example_wifi_ofdm.c
 * @brief Example: WiFi OFDM symbol construction and EVM measurement
 *
 * Demonstrates building an 802.11a/g OFDM symbol with QPSK data,
 * measuring transmitter EVM, and computing link budget.
 *
 * Knowledge coverage: L1 (OFDM params), L2 (constellation mapping),
 * L3 (IFFT), L5 (OFDM symbol construction), L6 (end-to-end TX chain).
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../include/wifi_bt_types.h"
#include "../include/wifi_phy.h"

int main(void)
{
    printf("=== WiFi OFDM Symbol Construction & EVM Measurement ===\n\n");

    /* Step 1: Initialize OFDM parameters for 20 MHz */
    ofdm_params_t params;
    if (ofdm_params_init(&params, 20.0) != 0) {
        printf("ERROR: Failed to init OFDM params\n");
        return 1;
    }
    printf("OFDM Configuration (20 MHz):\n");
    printf("  FFT size        = %d\n", params.n_fft);
    printf("  Data subcarriers = %d\n", params.n_data_sc);
    printf("  Pilot subcarriers= %d\n", params.n_pilot_sc);
    printf("  Cyclic prefix    = %d samples\n", params.n_cp);
    printf("  Subcarrier spacing = %.1f kHz\n", params.subcarrier_spacing_khz);
    printf("  Symbol duration  = %.1f µs\n", params.symbol_duration_us);

    /* Step 2: Initialize subcarrier map */
    ofdm_subcarrier_map_t map;
    if (ofdm_subcarrier_map_init(&map, 20.0) != 0) {
        printf("ERROR: Failed to init subcarrier map\n");
        return 1;
    }
    printf("\nSubcarrier mapping: %d data SCs, %d pilot SCs, DC nulled\n",
           map.n_data, map.n_pilots);

    /* Step 3: Generate QPSK data symbols */
    int n_data = map.n_data;
    double *data_symbols = (double *)malloc((size_t)(2 * n_data) * sizeof(double));
    if (!data_symbols) { printf("Alloc failed\n"); return 1; }

    printf("\nGenerating QPSK data for %d subcarriers...\n", n_data);
    for (int i = 0; i < n_data; i++) {
        uint32_t bits = (uint32_t)(i & 0x03);  /* Cyclic 00,01,10,11 */
        constellation_map(&data_symbols[2 * i], &data_symbols[2 * i + 1],
                         bits, MOD_QPSK);
    }

    /* Step 4: Build OFDM symbol */
    int total_samples = params.n_fft + params.n_cp;
    double *time_sym = (double *)malloc((size_t)(2 * total_samples) * sizeof(double));
    if (!time_sym) { printf("Alloc failed\n"); return 1; }

    int built = ofdm_build_symbol(time_sym, data_symbols, &map, &params, 0);
    if (built < 0) {
        printf("ERROR: OFDM symbol construction failed\n");
        return 1;
    }
    printf("Built OFDM symbol: %d time-domain samples (%.1f µs CP + %.1f µs FFT)\n",
           built, (double)params.n_cp / params.bandwidth_mhz,
           (double)params.n_fft / params.bandwidth_mhz);

    /* Step 5: Measure EVM of ideal data symbols */
    double evm = compute_evm(data_symbols, data_symbols, n_data, MOD_QPSK);
    printf("\nEVM of ideal TX: %.3f%% (should be ~0%%)\n", evm);

    /* Step 6: Compute peak-to-average power ratio (PAPR) */
    double max_power = 0.0, avg_power = 0.0;
    for (int i = 0; i < total_samples; i++) {
        double p = time_sym[2 * i] * time_sym[2 * i]
                 + time_sym[2 * i + 1] * time_sym[2 * i + 1];
        avg_power += p;
        if (p > max_power) max_power = p;
    }
    avg_power /= (double)total_samples;
    double papr_db = 10.0 * log10(max_power / avg_power);
    printf("PAPR: %.1f dB\n", papr_db);

    /* Step 7: Link budget */
    printf("\n--- Link Budget ---\n");
    double rx_sens = wifi_rx_sensitivity_dbm(24.0, 7.0, 20.0);
    printf("Receiver sensitivity (24 Mbps, NF=7dB): %.1f dBm\n", rx_sens);
    printf("Thermal noise (20 MHz): %.1f dBm\n", thermal_noise_floor_dbm(20e6));
    printf("Free space PL at 10m (2.4 GHz): %.1f dB\n",
           free_space_path_loss_db(10.0, 2.45e9));

    /* Cleanup */
    free(data_symbols);
    free(time_sym);
    ofdm_subcarrier_map_free(&map);

    printf("\n=== Complete ===\n");
    return 0;
}
