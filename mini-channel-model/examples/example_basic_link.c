/**
 * @file example_basic_link.c
 * @brief Example: Basic Wireless Link Budget Analysis (L4, L6, L7)
 *
 * Demonstrates a complete link budget calculation for a cellular downlink:
 *   1. Path loss using Okumura-Hata model (urban macrocell)
 *   2. Shadow fading margin
 *   3. Thermal noise floor computation
 *   4. SNR at receiver
 *   5. Shannon capacity estimation
 *   6. Doppler shift and coherence time
 *
 * This is a canonical L6 problem: end-to-end wireless link design.
 *
 * Reference scenario: Urban macrocell at 900 MHz
 *   - BS height: 30 m, MS height: 1.5 m
 *   - Distance: 1 km
 *   - Bandwidth: 10 MHz
 *   - Tx power: 43 dBm (20 W)
 *   - BS antenna gain: 15 dBi, MS antenna gain: 0 dBi
 *   - Mobile speed: 30 m/s (~108 km/h)
 */

#include "../include/channel_defs.h"
#include "../include/pathloss.h"
#include "../include/fading.h"
#include "../include/doppler.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== Basic Wireless Link Budget ===\n\n");

    /* ---- System Parameters ---- */
    double carrier_freq_hz = 900e6;
    double bandwidth_hz = 10e6;       /* 10 MHz LTE carrier */
    double tx_power_dbm = 43.0;       /* 20 W = 43 dBm */
    double tx_antenna_gain_dbi = 15.0;
    double rx_antenna_gain_dbi = 0.0;
    double noise_figure_db = 5.0;     /* Receiver noise figure */
    double temperature_k = 290.0;
    double distance_m = 1000.0;        /* 1 km */
    double tx_height_m = 30.0;        /* BS height */
    double rx_height_m = 1.5;         /* MS height */
    double velocity_ms = 30.0;        /* ~108 km/h */
    double shadow_margin_db = 8.7;    /* 90% edge coverage (sigma=8 dB) */

    printf("System Parameters:\n");
    printf("  Carrier:       %.0f MHz\n", carrier_freq_hz / 1e6);
    printf("  Bandwidth:     %.0f MHz\n", bandwidth_hz / 1e6);
    printf("  Tx Power:      %.1f dBm (%.1f W)\n",
           tx_power_dbm, pow(10.0, (tx_power_dbm - 30.0) / 10.0));
    printf("  Distance:      %.0f m (%.1f km)\n", distance_m, distance_m / 1000.0);
    printf("  Velocity:      %.0f m/s (%.0f km/h)\n", velocity_ms, velocity_ms * 3.6);
    printf("\n");

    /* ---- Step 1: Path Loss (Okumura-Hata Urban) ---- */
    double freq_mhz = carrier_freq_hz / 1e6;
    double distance_km = distance_m / 1000.0;

    double pl_okumura = pathloss_okumura_hata_urban(
        distance_km, freq_mhz, tx_height_m, rx_height_m, 1);

    double pl_friis = pathloss_friis_free_space(distance_m, carrier_freq_hz);

    printf("Step 1: Path Loss\n");
    printf("  Friis (free-space):  %.1f dB\n", pl_friis);
    printf("  Okumura-Hata (urban): %.1f dB\n", pl_okumura);
    printf("  Excess loss (vs free-space): %.1f dB\n", pl_okumura - pl_friis);
    printf("\n");

    /* ---- Step 2: Link Budget ---- */
    double eirp_dbm = tx_power_dbm + tx_antenna_gain_dbi;
    double rx_power_dbm = eirp_dbm - pl_okumura + rx_antenna_gain_dbi;
    double rx_power_with_shadow_dbm = rx_power_dbm - shadow_margin_db;
    double rx_power_w = pow(10.0, (rx_power_with_shadow_dbm - 30.0) / 10.0);

    printf("Step 2: Link Budget\n");
    printf("  EIRP:              %.1f dBm\n", eirp_dbm);
    printf("  Rx power (no shadow): %.1f dBm\n", rx_power_dbm);
    printf("  Shadow margin:     %.1f dB\n", shadow_margin_db);
    printf("  Rx power (with margin): %.1f dBm (%.3e W)\n",
           rx_power_with_shadow_dbm, rx_power_w);
    printf("\n");

    /* ---- Step 3: Noise and SNR ---- */
    double noise_dbm = channel_noise_power_dbm(bandwidth_hz, temperature_k,
                                                noise_figure_db);
    double noise_w = pow(10.0, (noise_dbm - 30.0) / 10.0);
    double snr_db = rx_power_with_shadow_dbm - noise_dbm;
    double snr_linear = pow(10.0, snr_db / 10.0);

    printf("Step 3: Noise and SNR\n");
    printf("  Noise floor:      %.1f dBm (%.3e W)\n", noise_dbm, noise_w);
    printf("  SNR:              %.1f dB (%.0f linear)\n", snr_db, snr_linear);
    printf("  Rx sensitivity check: %s\n",
           snr_db > 0 ? "PASS (>0 dB)" : "FAIL (<0 dB)");
    printf("\n");

    /* ---- Step 4: Shannon Capacity ---- */
    double capacity_bps = bandwidth_hz * log2(1.0 + snr_linear);
    double spectral_efficiency = capacity_bps / bandwidth_hz;

    printf("Step 4: Shannon Capacity\n");
    printf("  SISO capacity:    %.2f Mbps\n", capacity_bps / 1e6);
    printf("  Spectral efficiency: %.2f bps/Hz\n", spectral_efficiency);
    printf("  Theoretical peak: %.2f Mbps (64QAM R=3/4)\n",
           bandwidth_hz * 4.5 / 1e6);  /* 6 bps/Hz * 0.75 */
    printf("\n");

    /* ---- Step 5: Doppler Analysis ---- */
    double fd = channel_doppler_shift(velocity_ms, carrier_freq_hz);
    double tc = channel_coherence_time(fd);
    double wavelength = channel_wavelength(carrier_freq_hz);

    printf("Step 5: Doppler and Coherence\n");
    printf("  Wavelength:       %.3f m\n", wavelength);
    printf("  Max Doppler:      %.1f Hz\n", fd);
    printf("  Coherence time:   %.2f ms\n", tc * 1000.0);
    printf("  Fading type:      %s\n",
           (tc > 1e-3) ? "Slow fading" : "Fast fading");
    printf("\n");

    /* ---- Step 6: Fading Margins ---- */
    double lcr_10db = doppler_lcr_rayleigh(fd, pow(10.0, -10.0 / 20.0));
    double lcr_0db = doppler_lcr_rayleigh(fd, 1.0);
    double afd_10db = doppler_afd_rayleigh(fd, pow(10.0, -10.0 / 20.0));

    printf("Step 6: Fading Statistics\n");
    printf("  LCR at 0 dB:      %.1f crossings/s\n", lcr_0db);
    printf("  LCR at -10 dB:    %.1f crossings/s\n", lcr_10db);
    printf("  AFD at -10 dB:    %.3f ms\n", afd_10db * 1000.0);
    printf("\n");

    /* ---- Summary ---- */
    printf("===== Link Budget Summary =====\n");
    printf("  Carrier:          %.0f MHz\n", carrier_freq_hz / 1e6);
    printf("  Distance:         %.1f km\n", distance_km);
    printf("  Path loss:        %.1f dB\n", pl_okumura);
    printf("  Rx power:         %.1f dBm\n", rx_power_with_shadow_dbm);
    printf("  SNR:              %.1f dB\n", snr_db);
    printf("  Capacity:         %.2f Mbps\n", capacity_bps / 1e6);
    printf("  Doppler:          %.0f Hz\n", fd);
    printf("  Coherence time:   %.2f ms\n", tc * 1000.0);
    printf("  Link status:      %s\n", snr_db > 0 ? "VIABLE" : "FAILED");
    printf("===============================\n");

    return 0;
}
