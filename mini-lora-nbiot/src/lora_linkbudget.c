/**
 * @file lora_linkbudget.c
 * @brief LoRa/NB-IoT Link Budget Calculator -- Range, sensitivity, energy optimization
 *
 * Knowledge: L4 Friis equation, Shannon-Hartley, link budget, MCL
 *            L5 energy optimization, battery life modeling
 *            L7 smart metering, asset tracking, agriculture LPWAN applications
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lora_phy.h"
#include "lora_nbiot_common.h"
#include "nbiot_phy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ======================================================================
   L4: Fundamental Laws -- Path Loss Models
   ====================================================================== */

/*
 * Friis free-space path loss.
 *
 * PL(dB) = 20*log10(4*pi*d/lambda)
 *        = 20*log10(d) + 20*log10(f) + 32.45
 *
 * where d = distance (m), f = frequency (MHz), 32.45 = 20*log10(4*pi*1e6/c)
 *
 * Assumptions:
 *   - Far-field: d > 2*D^2/lambda (Fraunhofer distance)
 *   - Isotropic antennas (or gains accounted separately)
 *   - No obstructions, no reflections
 *
 * @param distance_m  Distance in meters
 * @param freq_mhz    Frequency in MHz
 * @return Path loss in dB
 */
double friis_path_loss_db(double distance_m, double freq_mhz)
{
    if (distance_m <= 0.0 || freq_mhz <= 0.0) return 0.0;

    /*
     * lambda = c / f = 300 / f_MHz meters
     * PL = 20*log10(4*pi*d / lambda)
     *    = 20*log10(4*pi*d*f_MHz / 300)
     *    = 20*log10(d) + 20*log10(f_MHz) + 20*log10(4*pi/300)
     *    = 20*log10(d) + 20*log10(f_MHz) - 27.55
     *
     * Wait, let me recalculate:
     * PL = (4*pi*d/lambda)^2 in linear, so in dB:
     * PL_dB = 20*log10(4*pi*d/lambda)
     *       = 20*log10(4*pi) + 20*log10(d) - 20*log10(lambda)
     *       = 22.0 + 20*log10(d) - 20*log10(c/f)
     *       = 22.0 + 20*log10(d) - 20*log10(300/f_MHz)
     *       = 22.0 + 20*log10(d) - (49.54 - 20*log10(f_MHz))
     *       = 22.0 + 20*log10(d) - 49.54 + 20*log10(f_MHz)
     *       = 20*log10(d) + 20*log10(f_MHz) - 27.54
     *
     * Actually the standard form is:
     * PL_dB = 32.45 + 20*log10(d_km) + 20*log10(f_MHz)
     *        = 32.45 + 20*log10(d_m/1000) + 20*log10(f_MHz)
     *        = 32.45 + 20*log10(d_m) - 60 + 20*log10(f_MHz)
     *        = 20*log10(d_m) + 20*log10(f_MHz) - 27.55
     */
    return 20.0 * log10(distance_m) + 20.0 * log10(freq_mhz) - 27.55;
}

/*
 * Log-distance path loss model.
 *
 * PL(d) = PL(d0) + 10 * n * log10(d / d0)
 *
 * This empirical model extends the free-space model to account for
 * environmental clutter. The path loss exponent n captures how quickly
 * signal attenuates with distance:
 *   n = 2.0: free space
 *   n = 2.7-3.5: urban cellular
 *   n = 4-6: indoor
 *   n = 1.6-1.8: indoor hallway (waveguide effect)
 *
 * Shadow fading can be added as a log-normal random variable
 * with standard deviation sigma (typically 6-12 dB).
 *
 * @param distance_m  Distance in meters
 * @param model       Path loss model parameters
 * @return Path loss in dB
 */
double log_distance_path_loss_db(double distance_m,
                                  const path_loss_model_t *model)
{
    if (!model || distance_m <= 0.0) return 0.0;

    double d0 = model->reference_distance_m;
    if (d0 <= 0.0) d0 = 1.0;

    double n = model->path_loss_exponent;
    double PL_d0 = model->reference_loss_db;

    /* If reference loss not provided, compute from Friis */
    if (PL_d0 <= 0.0) {
        PL_d0 = friis_path_loss_db(d0, model->frequency_mhz);
    }

    return PL_d0 + 10.0 * n * log10(distance_m / d0);
}

/*
 * Okumura-Hata urban path loss model.
 *
 * Valid for:
 *   - Frequency: 150-1500 MHz
 *   - Distance: 1-20 km
 *   - Base station height: 30-200 m
 *   - Mobile station height: 1-10 m
 *
 * Formula (urban, medium-small city):
 *   PL = 69.55 + 26.16*log10(f) - 13.82*log10(h_b)
 *      - a(h_m) + (44.9 - 6.55*log10(h_b))*log10(d)
 *
 * Mobile antenna height correction factor a(h_m):
 *   Medium-small city:
 *     a = (1.1*log10(f) - 0.7)*h_m - (1.56*log10(f) - 0.8)
 *   Large city (f <= 200 MHz):
 *     a = 8.29*(log10(1.54*h_m))^2 - 1.1
 *   Large city (f >= 400 MHz):
 *     a = 3.2*(log10(11.75*h_m))^2 - 4.97
 *
 * @param distance_km  Distance in km (1-20)
 * @param freq_mhz     Frequency in MHz (150-1500)
 * @param tx_height_m  Base station height (30-200m)
 * @param rx_height_m  Mobile height (1-10m)
 * @param large_city   1 for large city, 0 for medium-small
 * @return Path loss in dB
 */
double okumura_hata_path_loss_db(double distance_km, double freq_mhz,
                                  double tx_height_m, double rx_height_m,
                                  int large_city)
{
    /* Clamp inputs to valid ranges */
    if (distance_km < 1.0) distance_km = 1.0;
    if (distance_km > 20.0) distance_km = 20.0;
    if (freq_mhz < 150.0) freq_mhz = 150.0;
    if (freq_mhz > 1500.0) freq_mhz = 1500.0;

    double log_f = log10(freq_mhz);
    double log_hb = log10(tx_height_m);
    double log_d = log10(distance_km);

    /* Base loss */
    double pl = 69.55 + 26.16 * log_f - 13.82 * log_hb;

    /* Mobile antenna height correction */
    double a_hm;
    if (large_city) {
        if (freq_mhz <= 200.0) {
            a_hm = 8.29 * pow(log10(1.54 * rx_height_m), 2.0) - 1.1;
        } else {
            a_hm = 3.2 * pow(log10(11.75 * rx_height_m), 2.0) - 4.97;
        }
    } else {
        a_hm = (1.1 * log_f - 0.7) * rx_height_m
             - (1.56 * log_f - 0.8);
    }

    pl -= a_hm;

    /* Distance-dependent term */
    pl += (44.9 - 6.55 * log_hb) * log_d;

    return pl;
}

/* ======================================================================
   L4: Link Budget Calculation
   ====================================================================== */

/*
 * Complete link budget computation.
 *
 * Received power: P_rx = P_tx + G_tx + G_rx - PL - L_misc
 * Link margin:    M    = P_rx - S_rx
 *
 * where:
 *   P_tx  = transmitter output power (dBm)
 *   G_tx  = transmitter antenna gain (dBi)
 *   G_rx  = receiver antenna gain (dBi)
 *   PL    = path loss (dB)
 *   L_misc = miscellaneous losses: cable, connector, fading margin (dB)
 *   S_rx  = receiver sensitivity (dBm)
 *
 * Link margin > 0: link viable (with margin for fading)
 * Link margin = 0: exactly at sensitivity limit
 * Link margin < 0: insufficient signal for reliable communication
 *
 * @param rx_power_dbm Output: received power at the receiver input
 * @return Link margin in dB
 */
double link_budget_compute(double tx_power_dbm, double tx_antenna_gain_db,
                            double rx_antenna_gain_db, double path_loss_db,
                            double misc_loss_db, double rx_sensitivity_dbm,
                            double *rx_power_dbm)
{
    double eirp = tx_power_dbm + tx_antenna_gain_db;
    double rx_power = eirp + rx_antenna_gain_db - path_loss_db - misc_loss_db;

    if (rx_power_dbm) *rx_power_dbm = rx_power;

    return rx_power - rx_sensitivity_dbm;
}

/*
 * Maximum range from link budget.
 *
 * Uses the log-distance model to solve for distance where
 * link margin = 0 (received power = sensitivity).
 *
 * d_max = d0 * 10^((P_tx + G_tx + G_rx - L_misc - S_rx - PL(d0)) / (10*n))
 *
 * @param model     Path loss model
 * @param tx_power_dbm  Transmitter power in dBm
 * @param tx_antenna_gain_db TX antenna gain in dBi
 * @param rx_antenna_gain_db RX antenna gain in dBi
 * @param rx_sensitivity_dbm Receiver sensitivity in dBm
 * @param misc_loss_db Miscellaneous losses in dB
 * @return Maximum range in meters
 */
double max_range_from_link_budget(const path_loss_model_t *model,
                                   double tx_power_dbm,
                                   double tx_antenna_gain_db,
                                   double rx_antenna_gain_db,
                                   double rx_sensitivity_dbm,
                                   double misc_loss_db)
{
    if (!model) return 0.0;

    double d0 = model->reference_distance_m;
    if (d0 <= 0.0) d0 = 1.0;

    double n = model->path_loss_exponent;
    if (n <= 0.0) n = 2.0;

    double PL_d0 = model->reference_loss_db;
    if (PL_d0 <= 0.0) {
        PL_d0 = friis_path_loss_db(d0, model->frequency_mhz);
    }

    /*
     * Link budget equation at max range:
     *   P_tx + G_tx + G_rx - PL_d0 - 10*n*log10(d_max/d0) - L_misc = S_rx
     *
     * Solve for d_max:
     *   10*n*log10(d_max/d0) = P_tx + G_tx + G_rx - PL_d0 - L_misc - S_rx
     *   log10(d_max/d0) = (P_tx + G_tx + G_rx - PL_d0 - L_misc - S_rx) / (10*n)
     *   d_max = d0 * 10^(exponent)
     */
    double margin = tx_power_dbm + tx_antenna_gain_db + rx_antenna_gain_db
                  - PL_d0 - misc_loss_db - rx_sensitivity_dbm;

    double exponent = margin / (10.0 * n);
    return d0 * pow(10.0, exponent);
}

/* ======================================================================
   L4: Shannon-Hartley and MCL
   ====================================================================== */

/*
 * Shannon channel capacity.
 *
 * C = BW * log2(1 + SNR)
 *
 * This is the theoretical maximum data rate for an AWGN channel.
 * LoRa and NB-IoT operate far from this bound due to practical
 * constraints (modulation order, FEC overhead, repetition coding).
 *
 * Example: BW=125kHz, SNR=-7.5dB (SF7 minimum)
 *   SNR_linear = 10^(-7.5/10) = 0.178
 *   C = 125000 * log2(1.178) = 125000 * 0.236 = 29.5 kbps
 *
 * Actual LoRa rate at SF7/BW125/CR4/5 = 5.47 kbps
 * Efficiency = 5.47/29.5 = 18.5% (typical for practical systems)
 *
 * @param bw_hz      Bandwidth in Hz
 * @param snr_linear SNR (linear, not dB)
 * @return Channel capacity in bits/second
 */
double shannon_capacity(double bw_hz, double snr_linear)
{
    if (bw_hz <= 0.0 || snr_linear < 0.0) return 0.0;
    return bw_hz * log2(1.0 + snr_linear);
}

/*
 * NB-IoT Maximum Coupling Loss (MCL).
 *
 * MCL is the maximum allowable path loss between UE and eNB
 * for which the service remains available. It is the key metric
 * for coverage comparison across cellular IoT technologies.
 *
 * MCL = P_tx - S_rx
 *
 * NB-IoT target: 164 dB (3GPP TR 45.820)
 * LTE-M target:  156 dB
 * GPRS:          144 dB
 * LTE Cat-1:     140 dB
 *
 * The 164 dB target enables deep indoor coverage (basements,
 * underground parking, remote rural areas).
 *
 * @param tx_power_dbm  Transmitter power in dBm
 * @param rx_sens_dbm   Receiver sensitivity in dBm
 * @return MCL in dB
 */
double nbiot_max_coupling_loss(double tx_power_dbm, double rx_sens_dbm)
{
    return tx_power_dbm - rx_sens_dbm;
}

/* ======================================================================
   L5: Energy and Battery Life
   ====================================================================== */

/*
 * Energy per transmitted bit.
 *
 * E_b = P_tx * T_bit = P_tx / R_b
 *
 * where P_tx is in Watts (converted from dBm) and R_b is bit rate in bps.
 *
 * Example: P_tx = 14 dBm (25 mW), SF12/BW125 -> R_b = 293 bps
 *   E_b = 0.025 / 293 = 85.3 uJ/bit
 *
 * @param tx_power_dbm  Transmitter power in dBm
 * @param bit_rate_bps  Bit rate in bits/second
 * @return Energy per bit in Joules
 */
double energy_per_bit_joules(double tx_power_dbm, double bit_rate_bps)
{
    if (bit_rate_bps <= 0.0) return 0.0;
    double tx_power_watts = pow(10.0, (tx_power_dbm - 30.0) / 10.0);
    return tx_power_watts / bit_rate_bps;
}

/*
 * Battery life estimation for LPWAN device.
 *
 * Battery capacity in Joules:
 *   E_cap = capacity_mAh * voltage_V * 3.6  (3.6 = 3600/1000 conversion)
 *
 * Average power consumption per cycle:
 *   P_avg = (T_tx * I_tx * V + T_rx * I_rx * V + T_sleep * I_sleep * V) / T_cycle
 *
 * Battery life:
 *   T_life = E_cap / P_avg  (in hours)
 *          = E_cap / P_avg / 8760  (in years)
 *
 * @param battery_capacity_mah  Battery capacity in mAh
 * @param battery_voltage_v     Nominal battery voltage
 * @param tx_interval_sec       Interval between transmissions
 * @param tx_duration_sec       Duration of each transmission
 * @param tx_current_ma         Current draw during TX
 * @param rx_duration_sec       Duration of RX window
 * @param rx_current_ma         Current draw during RX
 * @param sleep_current_ua      Current draw in sleep (uA)
 * @return Estimated battery life in years
 */
double battery_life_estimate_years(double battery_capacity_mah,
                                    double battery_voltage_v,
                                    double tx_interval_sec,
                                    double tx_duration_sec,
                                    double tx_current_ma,
                                    double rx_duration_sec,
                                    double rx_current_ma,
                                    double sleep_current_ua)
{
    /* Total energy capacity in Joules */
    double e_cap = battery_capacity_mah * battery_voltage_v * 3.6;  /* Joules */

    /* Energy per cycle: E = V * I * t for each phase */
    double e_tx = battery_voltage_v * (tx_current_ma / 1000.0) * tx_duration_sec;
    double e_rx = battery_voltage_v * (rx_current_ma / 1000.0) * rx_duration_sec;

    /* Sleep energy = V * I_sleep * (T_cycle - T_tx - T_rx) */
    double sleep_duration = tx_interval_sec - tx_duration_sec - rx_duration_sec;
    if (sleep_duration < 0.0) sleep_duration = 0.0;
    double e_sleep = battery_voltage_v * (sleep_current_ua / 1e6) * sleep_duration;

    double e_cycle = e_tx + e_rx + e_sleep;
    if (e_cycle <= 0.0) return 0.0;

    /* Number of cycles = E_cap / E_cycle */
    double num_cycles = e_cap / e_cycle;

    /* Total life in years */
    return num_cycles * tx_interval_sec / (365.25 * 24.0 * 3600.0);
}

/*
 * Find energy-optimal spreading factor.
 *
 * For a given link distance and data payload, find the SF that
 * minimizes total energy consumption.
 *
 * Trade-off:
 *   - Higher SF = longer range, better sensitivity
 *   - Lower SF = shorter airtime, less energy per packet
 *   - Higher SF may avoid retransmissions, saving energy overall
 *
 * Energy model:
 *   E_total = P_tx * T_packet * (1 / delivery_probability)
 *
 * where delivery_probability accounts for the link margin.
 * If SNR margin is negative, multiple retransmissions may be needed.
 *
 * @param distance_m    Link distance in meters
 * @param model         Path loss model
 * @param data_bytes    Payload size in bytes
 * @param delay_tol_sec Maximum acceptable delay
 * @param optimal_sf    Output: recommended SF
 * @return Estimated energy in Joules per successful delivery
 */
double energy_optimal_sf(double distance_m, const path_loss_model_t *model,
                          uint16_t data_bytes, double delay_tol_sec,
                          uint8_t *optimal_sf)
{
    if (!model || !optimal_sf) return 0.0;

    double path_loss = log_distance_path_loss_db(distance_m, model);
    double best_energy = 1e9;  /* Start with very high energy */
    uint8_t best_sf = 7;

    for (uint8_t sf = 7; sf <= 12; sf++) {
        lora_phy_params_t params;
        lora_phy_params_init_default(&params);
        params.sf = (lora_spreading_factor_t)sf;
        params.payload_len = (uint8_t)data_bytes;

        /* Recalculate derived parameters */
        params.num_chips = (uint32_t)1 << sf;
        params.symbol_period = (double)params.num_chips / (double)params.bw;
        params.chirp_rate = (double)params.bw / params.symbol_period;
        params.bit_rate = lora_bit_rate(params.sf, params.bw, params.cr);

        /* Compute airtime */
        double airtime = lora_packet_airtime(&params);

        /* Check if airtime meets delay constraint */
        if (airtime > delay_tol_sec) continue;

        /* Compute received SNR */
        double sensitivity = lora_receiver_sensitivity(params.sf, params.bw, 6.0);
        double rx_power;
        link_budget_compute(14.0, 0.0, 0.0, path_loss, 0.0,
                            sensitivity, &rx_power);
        double snr_margin = rx_power - sensitivity;

        /*
         * Delivery probability model:
         *   P_success = 1 / (1 + exp(-alpha * snr_margin))
         * This logistic model gives ~50% at margin=0, ~90% at margin=+10dB.
         */
        double alpha = 0.5;
        double p_success = 1.0 / (1.0 + exp(-alpha * snr_margin));
        if (p_success < 0.01) p_success = 0.01;

        /* Energy: TX power (14 dBm = 25 mW) * airtime / delivery_prob */
        double tx_power_w = 0.025;  /* 14 dBm */
        double e_per_tx = tx_power_w * airtime;
        double e_total = e_per_tx / p_success;

        if (e_total < best_energy) {
            best_energy = e_total;
            best_sf = sf;
        }
    }

    *optimal_sf = best_sf;
    return best_energy;
}

/* ======================================================================
   L7: Application Profiles -- Payload Encoding
   ====================================================================== */

/*
 * Encode smart meter report into compact binary format.
 *
 * Format (simplified LoRaWAN-compatible):
 *   [meter_id 4B] [reading 4B float] [timestamp 4B] [status 1B]
 *   Total: 13 bytes
 *
 * Real implementations would use more compact encoding
 * (e.g., delta compression, variable-length integers).
 */
int smart_meter_encode(const smart_meter_report_t *report,
                        uint8_t *buffer, size_t buf_size)
{
    if (!report || !buffer || buf_size < 13) return -1;

    size_t idx = 0;

    /* Meter ID (big-endian) */
    buffer[idx++] = (report->meter_id >> 24) & 0xFF;
    buffer[idx++] = (report->meter_id >> 16) & 0xFF;
    buffer[idx++] = (report->meter_id >> 8)  & 0xFF;
    buffer[idx++] =  report->meter_id        & 0xFF;

    /* Reading value (IEEE 754 float) */
    uint32_t reading_bits;
    memcpy(&reading_bits, &report->reading_value, 4);
    buffer[idx++] = (reading_bits >> 24) & 0xFF;
    buffer[idx++] = (reading_bits >> 16) & 0xFF;
    buffer[idx++] = (reading_bits >> 8)  & 0xFF;
    buffer[idx++] =  reading_bits        & 0xFF;

    /* Timestamp */
    buffer[idx++] = (report->reading_timestamp >> 24) & 0xFF;
    buffer[idx++] = (report->reading_timestamp >> 16) & 0xFF;
    buffer[idx++] = (report->reading_timestamp >> 8)  & 0xFF;
    buffer[idx++] =  report->reading_timestamp        & 0xFF;

    /* Status byte */
    uint8_t status = ((report->battery_level_pct / 10) << 4)
                   | ((report->valve_status & 1) << 3)
                   | ((report->tamper_flag & 1) << 2)
                   | ((report->alarm_flag & 1) << 1);
    buffer[idx++] = status;

    return (int)idx;
}

int smart_meter_decode(const uint8_t *buffer, size_t len,
                        smart_meter_report_t *report)
{
    if (!buffer || !report || len < 13) return -1;

    report->meter_id = ((uint32_t)buffer[0] << 24)
                     | ((uint32_t)buffer[1] << 16)
                     | ((uint32_t)buffer[2] << 8)
                     |  (uint32_t)buffer[3];

    uint32_t reading_bits = ((uint32_t)buffer[4] << 24)
                          | ((uint32_t)buffer[5] << 16)
                          | ((uint32_t)buffer[6] << 8)
                          |  (uint32_t)buffer[7];
    memcpy(&report->reading_value, &reading_bits, 4);

    report->reading_timestamp = ((uint32_t)buffer[8]  << 24)
                              | ((uint32_t)buffer[9]  << 16)
                              | ((uint32_t)buffer[10] << 8)
                              |  (uint32_t)buffer[11];

    uint8_t status = buffer[12];
    report->battery_level_pct = (status >> 4) * 10;
    report->valve_status = (status >> 3) & 1;
    report->tamper_flag  = (status >> 2) & 1;
    report->alarm_flag   = (status >> 1) & 1;

    return 0;
}

/*
 * Encode asset tracker report.
 *
 * Format: [device_id 4B] [lat 4B] [lon 4B] [flags 1B] [gw_count 1B]
 *         [rssi_avg 1B] [battery 2B] [motion 1B]
 * Total: 18 bytes
 */
int asset_tracker_encode(const asset_tracker_report_t *report,
                          uint8_t *buffer, size_t buf_size)
{
    if (!report || !buffer || buf_size < 18) return -1;

    size_t idx = 0;

    /* Device ID */
    buffer[idx++] = (report->device_id >> 24) & 0xFF;
    buffer[idx++] = (report->device_id >> 16) & 0xFF;
    buffer[idx++] = (report->device_id >> 8)  & 0xFF;
    buffer[idx++] =  report->device_id        & 0xFF;

    /* Latitude (scaled integer: degrees * 1e6, big-endian) */
    int32_t lat_scaled = (int32_t)(report->latitude * 1e6);
    buffer[idx++] = (lat_scaled >> 24) & 0xFF;
    buffer[idx++] = (lat_scaled >> 16) & 0xFF;
    buffer[idx++] = (lat_scaled >> 8)  & 0xFF;
    buffer[idx++] =  lat_scaled        & 0xFF;

    /* Longitude */
    int32_t lon_scaled = (int32_t)(report->longitude * 1e6);
    buffer[idx++] = (lon_scaled >> 24) & 0xFF;
    buffer[idx++] = (lon_scaled >> 16) & 0xFF;
    buffer[idx++] = (lon_scaled >> 8)  & 0xFF;
    buffer[idx++] =  lon_scaled        & 0xFF;

    /* Flags */
    uint8_t flags = (report->has_gnss ? 0x80 : 0x00);
    buffer[idx++] = flags;

    /* Gateway count */
    buffer[idx++] = report->gateway_count;

    /* Average RSSI (quantized to -dBm, offset by 200) */
    double rssi_avg = 0;
    uint8_t n_gw = report->gateway_count;
    if (n_gw > 8) n_gw = 8;
    if (n_gw > 0) {
        for (int i = 0; i < n_gw; i++) rssi_avg += report->rssi_dbm[i];
        rssi_avg /= (double)n_gw;
    }
    uint8_t rssi_byte = (uint8_t)((-rssi_avg > 0) ? (-rssi_avg) : 0);
    buffer[idx++] = rssi_byte;

    /* Battery voltage (scaled: V * 100) */
    uint16_t batt_scaled = (uint16_t)(report->battery_v * 100.0);
    buffer[idx++] = (batt_scaled >> 8) & 0xFF;
    buffer[idx++] =  batt_scaled       & 0xFF;

    /* Motion state */
    buffer[idx++] = report->motion_state;

    return (int)idx;
}

int asset_tracker_decode(const uint8_t *buffer, size_t len,
                          asset_tracker_report_t *report)
{
    if (!buffer || !report || len < 18) return -1;

    report->device_id = ((uint32_t)buffer[0] << 24)
                      | ((uint32_t)buffer[1] << 16)
                      | ((uint32_t)buffer[2] << 8)
                      |  (uint32_t)buffer[3];

    int32_t lat_scaled = ((int32_t)buffer[4] << 24)
                       | ((int32_t)buffer[5] << 16)
                       | ((int32_t)buffer[6] << 8)
                       |  (int32_t)buffer[7];
    report->latitude = (double)lat_scaled / 1e6;

    int32_t lon_scaled = ((int32_t)buffer[8]  << 24)
                       | ((int32_t)buffer[9]  << 16)
                       | ((int32_t)buffer[10] << 8)
                       |  (int32_t)buffer[11];
    report->longitude = (double)lon_scaled / 1e6;

    report->has_gnss = (buffer[12] & 0x80) ? 1 : 0;
    report->gateway_count = buffer[13];

    /* RSSI */
    double rssi_val = -(double)buffer[14];
    for (int i = 0; i < 8; i++) report->rssi_dbm[i] = rssi_val;

    uint16_t batt_scaled = ((uint16_t)buffer[15] << 8) | buffer[16];
    report->battery_v = (double)batt_scaled / 100.0;

    report->motion_state = buffer[17];

    return 0;
}

/*
 * Encode agriculture sensor report.
 *
 * Format: [sensor_id 4B] [soil_moist 2B] [soil_temp 2B]
 *         [air_temp 2B] [humidity 2B] [leaf_wet 2B]
 *         [solar 2B] [battery 2B] [solar_panel 2B]
 * Total: 20 bytes
 */
int agriculture_sensor_encode(const agriculture_sensor_report_t *report,
                               uint8_t *buffer, size_t buf_size)
{
    if (!report || !buffer || buf_size < 20) return -1;

    size_t idx = 0;

    /* Sensor ID */
    buffer[idx++] = (report->sensor_id >> 24) & 0xFF;
    buffer[idx++] = (report->sensor_id >> 16) & 0xFF;
    buffer[idx++] = (report->sensor_id >> 8)  & 0xFF;
    buffer[idx++] =  report->sensor_id        & 0xFF;

    /* Scaled sensor values (value * 10, big-endian int16) */
    int16_t sm = (int16_t)(report->soil_moisture_pct * 10.0);
    buffer[idx++] = (sm >> 8) & 0xFF; buffer[idx++] = sm & 0xFF;

    int16_t st = (int16_t)((report->soil_temperature_c + 50.0) * 10.0);
    buffer[idx++] = (st >> 8) & 0xFF; buffer[idx++] = st & 0xFF;

    int16_t at = (int16_t)((report->air_temperature_c + 50.0) * 10.0);
    buffer[idx++] = (at >> 8) & 0xFF; buffer[idx++] = at & 0xFF;

    int16_t ah = (int16_t)(report->air_humidity_pct * 10.0);
    buffer[idx++] = (ah >> 8) & 0xFF; buffer[idx++] = ah & 0xFF;

    int16_t lw = (int16_t)(report->leaf_wetness * 1000.0);
    buffer[idx++] = (lw >> 8) & 0xFF; buffer[idx++] = lw & 0xFF;

    uint16_t sr = report->solar_radiation_wm2;
    buffer[idx++] = (sr >> 8) & 0xFF; buffer[idx++] = sr & 0xFF;

    uint16_t bv = (uint16_t)(report->battery_v * 100.0);
    buffer[idx++] = (bv >> 8) & 0xFF; buffer[idx++] = bv & 0xFF;

    uint16_t sv = (uint16_t)(report->solar_panel_v * 100.0);
    buffer[idx++] = (sv >> 8) & 0xFF; buffer[idx++] = sv & 0xFF;

    return (int)idx;
}

int agriculture_sensor_decode(const uint8_t *buffer, size_t len,
                               agriculture_sensor_report_t *report)
{
    if (!buffer || !report || len < 20) return -1;

    report->sensor_id = ((uint32_t)buffer[0] << 24)
                      | ((uint32_t)buffer[1] << 16)
                      | ((uint32_t)buffer[2] << 8)
                      |  (uint32_t)buffer[3];

    int16_t sm = ((int16_t)buffer[4] << 8) | buffer[5];
    report->soil_moisture_pct = (double)sm / 10.0;

    int16_t st = ((int16_t)buffer[6] << 8) | buffer[7];
    report->soil_temperature_c = (double)st / 10.0 - 50.0;

    int16_t at = ((int16_t)buffer[8] << 8) | buffer[9];
    report->air_temperature_c = (double)at / 10.0 - 50.0;

    int16_t ah = ((int16_t)buffer[10] << 8) | buffer[11];
    report->air_humidity_pct = (double)ah / 10.0;

    int16_t lw = ((int16_t)buffer[12] << 8) | buffer[13];
    report->leaf_wetness = (double)lw / 1000.0;

    report->solar_radiation_wm2 = ((uint16_t)buffer[14] << 8) | buffer[15];

    uint16_t bv = ((uint16_t)buffer[16] << 8) | buffer[17];
    report->battery_v = (double)bv / 100.0;

    uint16_t sv = ((uint16_t)buffer[18] << 8) | buffer[19];
    report->solar_panel_v = (double)sv / 100.0;

    return 0;
}

/*
 * Get technology profile for comparison.
 * Returns static data; do not free the returned pointer.
 */
const lpwan_tech_profile_t *lpwan_get_profile(lpwan_technology_t tech)
{
    static const lpwan_tech_profile_t profiles[] = {
        { LPWAN_LORA,  250.0, 50000.0, 5.0, 20.0, 30.0, 10.0, 1.5, 20.0, 157.0, 10.0 },
        { LPWAN_NBIOT, 160.0, 250000.0, 10.0, 15.0, 200.0, 50.0, 3.0, 23.0, 164.0, 10.0 },
        { LPWAN_LTEM,  300.0, 1000000.0, 8.0, 12.0, 400.0, 80.0, 5.0, 23.0, 156.0, 7.0 },
        { LPWAN_SIGFOX, 100.0, 600.0, 10.0, 50.0, 50.0, 10.0, 0.5, 14.0, 162.0, 8.0 },
    };

    int idx = (int)tech;
    if (idx >= 0 && idx < 4) return &profiles[idx];
    return &profiles[0];  /* Default to LoRa */
}

/*
 * NB-IoT link budget analysis.
 *
 * Computes received SNR for NB-IoT based on cell configuration
 * and estimated path loss.
 *
 * @param config       NB-IoT cell configuration
 * @param tx_dbm       Transmitter power in dBm
 * @param path_loss_db Estimated path loss in dB
 * @param snr_out      Output: estimated SNR in dB
 * @return 0 if link viable, -1 if insufficient
 */
int nbiot_link_budget(const nbiot_cell_config_t *config,
                       double tx_dbm, double path_loss_db,
                       double *snr_out)
{
    if (!config || !snr_out) return -1;

    /*
     * NB-IoT receiver noise figure: ~3 dB (UE), ~5 dB (eNB)
     * Thermal noise in 180 kHz: -174 + 10*log10(180000) = -121.4 dBm
     * Total noise floor: -121.4 + NF = -118.4 dBm (UE, NF=3)
     *
     * Received power: P_rx = P_tx - PL
     * SNR = P_rx - noise_floor
     */
    double noise_figure = 3.0;        /* UE NF */
    double thermal_noise = -174.0 + 10.0 * log10(NBIOT_TOTAL_BW_KHZ * 1000.0);
    double noise_floor = thermal_noise + noise_figure;

    double rx_power = tx_dbm - path_loss_db;
    double snr = rx_power - noise_floor;

    *snr_out = snr;

    /*
     * NB-IoT minimum SNR for each CE level (approximate):
     *   CE0 (normal):  -6 dB
     *   CE1 (robust):  -12 dB
     *   CE2 (extreme): -15 dB
     */
    double snr_min;
    switch (config->ce_level) {
        case NBIOT_CE_LEVEL_0: snr_min = -6.0;  break;
        case NBIOT_CE_LEVEL_1: snr_min = -12.0; break;
        case NBIOT_CE_LEVEL_2: snr_min = -15.0; break;
        default: snr_min = -6.0;
    }

    return (snr >= snr_min) ? 0 : -1;
}

/*
 * Shannon capacity for NB-IoT 180 kHz channel.
 */
double nbiot_shannon_capacity(double snr_linear)
{
    return NBIOT_TOTAL_BW_KHZ * 1000.0 * log2(1.0 + snr_linear);
}

/*
 * Effective data rate with repetition coding.
 * R_eff = R_peak / N_repetitions
 */
double nbiot_effective_rate(double peak_rate_bps, uint16_t num_reps)
{
    if (num_reps == 0) return peak_rate_bps;
    return peak_rate_bps / (double)num_reps;
}
