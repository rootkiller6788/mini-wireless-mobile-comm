/**
 * @file pathloss.h
 * @brief Path Loss Models for Wireless Channels (L2, L4)
 *
 * Implements fundamental path loss models from Friis free-space equation
 * to modern 3GPP TR 38.901 models. Each model corresponds to a distinct
 * knowledge point in wireless propagation theory.
 *
 * L4 Fundamental Laws:
 *   - Friis Transmission Equation (Friis, 1946)
 *   - Two-Ray Ground Reflection Model
 *   - Okumura-Hata Empirical Model (Hata, 1980)
 *   - COST-231 Walfisch-Ikegami Model
 *
 * L2 Core Concepts:
 *   - Path loss exponent and its physical meaning
 *   - Shadow fading (log-normal) superposition
 *   - Breakpoint distance in two-ray model
 *   - Clutter loss and building penetration loss
 *
 * Reference: Molisch, "Wireless Communications", 2nd Ed, Ch. 4 (2011)
 * Reference: Rappaport, "Wireless Communications", Ch. 3
 * Reference: ITU-R Rec. P.1411-12 (2021)
 * Reference: 3GPP TR 38.901 v16.1.0 (2020)
 *
 * Course Mapping:
 *   Stanford EE359 - Wireless Communications (path loss models)
 *   TUM High-Frequency Engineering (propagation modeling)
 *   ETH 227-0455 - EM Waves & Propagation
 *   Georgia Tech ECE 6350 - EM (wave propagation)
 */

#ifndef PATHLOSS_H
#define PATHLOSS_H

#include "channel_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L4: Friis Free-Space Path Loss
 *
 * Friis Transmission Equation (1946):
 *   P_r = P_t * G_t * G_r * (lambda/(4*pi*d))^2
 *
 * In dB:
 *   PL(dB) = 20*log10(4*pi*d/lambda)
 *          = 32.44 + 20*log10(d_km) + 20*log10(f_MHz)   [ITU-R P.525]
 *
 * Valid for far-field: d >> 2*D^2/lambda (Fraunhofer distance)
 * Valid for free-space, no obstructions, single LOS path.
 *============================================================================*/

/**
 * @brief Compute Friis free-space path loss
 * @param distance_m Distance between Tx and Rx (m)
 * @param freq_hz Carrier frequency (Hz)
 * @return Path loss in dB (positive value)
 * Complexity: O(1)
 */
double pathloss_friis_free_space(double distance_m, double freq_hz);

/**
 * @brief Compute Friis free-space path loss at reference distance
 * @param ref_distance_m Reference distance d_0 (m)
 * @param freq_hz Carrier frequency (Hz)
 * @return Reference path loss PL(d_0) in dB
 * Complexity: O(1)
 */
double pathloss_reference_loss(double ref_distance_m, double freq_hz);

/*============================================================================
 * L4: Two-Ray Ground Reflection Model
 *
 * For distances beyond the breakpoint d_b = 4*h_t*h_r/lambda:
 *   PL(dB) = 40*log10(d) - 10*log10(G_t*G_r*h_t^2*h_r^2)
 *
 * Below breakpoint, path loss oscillates around free-space.
 * This models the ground reflection interfering with the direct path.
 *============================================================================*/

/**
 * @brief Compute breakpoint distance for two-ray model
 * @param tx_height_m Transmit antenna height (m)
 * @param rx_height_m Receive antenna height (m)
 * @param freq_hz Carrier frequency (Hz)
 * @return Breakpoint distance d_b = 4*h_t*h_r/lambda (m)
 * Complexity: O(1)
 */
double pathloss_two_ray_breakpoint(double tx_height_m, double rx_height_m,
                                    double freq_hz);

/**
 * @brief Compute two-ray ground reflection path loss
 * @param distance_m Distance (m)
 * @param tx_height_m Transmit antenna height (m)
 * @param rx_height_m Receive antenna height (m)
 * @param freq_hz Carrier frequency (Hz)
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_two_ray(double distance_m, double tx_height_m,
                         double rx_height_m, double freq_hz);

/*============================================================================
 * L4: Log-Distance Path Loss Model (Empirical)
 *
 * PL(d) = PL(d_0) + 10*n*log10(d/d_0) + X_sigma
 *
 * where n is path loss exponent (typ 2 for free space, 2.7-3.5 urban,
 * 4-6 indoor obstructed), and X_sigma ~ N(0, sigma^2) models shadow fading.
 *============================================================================*/

/**
 * @brief Compute log-distance path loss (deterministic part)
 * @param params Path loss parameters (must set ref_distance_m, ref_loss_db,
 *               path_loss_exponent)
 * @param distance_m Distance (m)
 * @return Path loss in dB (without shadow fading)
 * Complexity: O(1)
 */
double pathloss_log_distance(const pathloss_params_t *params, double distance_m);

/**
 * @brief Generate log-normal shadow fading sample
 * @param sigma_db Standard deviation of shadow fading (dB)
 * @return Shadow fading value in dB (zero mean, std = sigma_db)
 * Complexity: O(1) — uses Box-Muller transform internally
 */
double pathloss_shadow_fading_sample(double sigma_db);

/*============================================================================
 * L4: Okumura-Hata Model (Hata, 1980)
 *
 * Empirical model from extensive measurements in Tokyo (Okumura, 1968).
 * Valid for: 150 <= f_c <= 1500 MHz, 1 <= d <= 20 km,
 *            30 <= h_b <= 200 m, 1 <= h_m <= 10 m.
 *
 * Urban area:
 *   PL(dB) = 69.55 + 26.16*log10(f) - 13.82*log10(h_b) - a(h_m)
 *            + [44.9 - 6.55*log10(h_b)]*log10(d)
 *
 * where a(h_m) is the mobile antenna height correction factor.
 *============================================================================*/

/**
 * @brief Compute Okumura-Hata mobile antenna correction factor
 * @param rx_height_m Mobile antenna height (m), 1-10 m
 * @param freq_mhz Carrier frequency (MHz), 150-1500
 * @param is_large_city 1 for large city, 0 for small/medium city
 * @return Correction factor a(h_m) in dB
 * Complexity: O(1)
 */
double pathloss_okumura_hata_correction(double rx_height_m, double freq_mhz,
                                         int is_large_city);

/**
 * @brief Compute Okumura-Hata path loss for urban area
 * @param distance_km Distance (km), 1-20
 * @param freq_mhz Frequency (MHz), 150-1500
 * @param tx_height_m Base station height (m), 30-200
 * @param rx_height_m Mobile height (m), 1-10
 * @param is_large_city 1 for large city, 0 for small/medium
 * @return Path loss in dB for urban
 * Complexity: O(1)
 */
double pathloss_okumura_hata_urban(double distance_km, double freq_mhz,
                                    double tx_height_m, double rx_height_m,
                                    int is_large_city);

/**
 * @brief Compute Okumura-Hata path loss for suburban area
 * @param distance_km Distance (km)
 * @param freq_mhz Frequency (MHz)
 * @param tx_height_m Base station height (m)
 * @param rx_height_m Mobile height (m)
 * @return Path loss in dB for suburban
 *
 * Suburban correction:
 *   PL_sub = PL_urban - 2*[log10(f/28)]^2 - 5.4
 *
 * Complexity: O(1)
 */
double pathloss_okumura_hata_suburban(double distance_km, double freq_mhz,
                                       double tx_height_m, double rx_height_m);

/**
 * @brief Compute Okumura-Hata path loss for open/rural area
 * @param distance_km Distance (km)
 * @param freq_mhz Frequency (MHz)
 * @param tx_height_m Base station height (m)
 * @param rx_height_m Mobile height (m)
 * @return Path loss in dB for open rural
 *
 * Rural correction:
 *   PL_rural = PL_urban - 4.78*[log10(f)]^2 + 18.33*log10(f) - 40.94
 *
 * Complexity: O(1)
 */
double pathloss_okumura_hata_rural(double distance_km, double freq_mhz,
                                    double tx_height_m, double rx_height_m);

/*============================================================================
 * L4: COST-231 Hata Model
 *
 * Extension of Okumura-Hata for 1500-2000 MHz.
 *   PL(dB) = 46.3 + 33.9*log10(f) - 13.82*log10(h_b) - a(h_m)
 *            + [44.9 - 6.55*log10(h_b)]*log10(d) + C_M
 *
 * where C_M = 0 dB for medium city/suburban, 3 dB for metropolitan.
 *============================================================================*/

/**
 * @brief Compute COST-231 Hata path loss
 * @param distance_km Distance (km), 1-20
 * @param freq_mhz Frequency (MHz), 1500-2000
 * @param tx_height_m Base station height (m)
 * @param rx_height_m Mobile height (m)
 * @param is_metropolitan 1 for metropolitan center (C_M=3dB), 0 otherwise
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_cost231_hata(double distance_km, double freq_mhz,
                              double tx_height_m, double rx_height_m,
                              int is_metropolitan);

/*============================================================================
 * L4: Walfisch-Ikegami Model (COST-231)
 *
 * For urban microcells with building data. Combines:
 *   - Free-space loss
 *   - Rooftop-to-street diffraction
 *   - Multi-screen diffraction along building rows
 *
 * Valid for: 800 <= f <= 2000 MHz, 0.02 <= d <= 5 km,
 *            4 <= h_b <= 50 m, 1 <= h_m <= 3 m.
 *============================================================================*/

/**
 * @brief Compute Walfisch-Ikegami path loss (LOS case)
 * @param distance_km Distance (km)
 * @param freq_mhz Frequency (MHz)
 * @return Path loss in dB (LOS, essentially free-space with street canyon)
 * Complexity: O(1)
 */
double pathloss_walfisch_ikegami_los(double distance_km, double freq_mhz);

/**
 * @brief Compute Walfisch-Ikegami path loss (NLOS case)
 * @param distance_km Distance (km), 0.02-5
 * @param freq_mhz Frequency (MHz), 800-2000
 * @param tx_height_m Base station height (m)
 * @param rx_height_m Mobile height (m)
 * @param building_height_m Average building height (m)
 * @param street_width_m Street width (m)
 * @param building_spacing_m Building separation (m)
 * @param street_angle_deg Street angle relative to direct path (deg)
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_walfisch_ikegami_nlos(double distance_km, double freq_mhz,
                                       double tx_height_m, double rx_height_m,
                                       double building_height_m,
                                       double street_width_m,
                                       double building_spacing_m,
                                       double street_angle_deg);

/*============================================================================
 * L4: ITU Indoor Propagation Model (ITU-R P.1238)
 *
 * PL(dB) = 20*log10(f) + N*log10(d) + L_f(n) - 28
 *
 * where N is distance power loss coefficient, L_f(n) is floor penetration
 * loss factor, n is number of floors between Tx and Rx.
 *============================================================================*/

/**
 * @brief Compute ITU-R P.1238 indoor path loss
 * @param distance_m Distance (m), >= 1
 * @param freq_mhz Frequency (MHz)
 * @param path_loss_coeff Distance power loss coefficient N (typ 20-33)
 * @param num_floors Number of floors between Tx and Rx
 * @param floor_loss_db Loss per floor (dB), typ 10-20 for office
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_itu_indoor(double distance_m, double freq_mhz,
                            double path_loss_coeff, int num_floors,
                            double floor_loss_db);

/*============================================================================
 * L4: 3GPP TR 38.901 Path Loss Models (5G NR)
 *
 * Standard models for 5G NR system-level simulations.
 * Cover UMi (Urban Micro), UMa (Urban Macro), RMa (Rural Macro),
 * InH (Indoor Hotspot) scenarios.
 *============================================================================*/

/**
 * @brief Compute 3GPP TR 38.901 UMi (Urban Micro) path loss
 * @param distance_m 3D distance (m), 10-5000
 * @param freq_ghz Carrier frequency (GHz), 0.5-100
 * @param tx_height_m BS height (m), typ 10
 * @param rx_height_m UE height (m), typ 1.5
 * @param is_los 1 for LOS, 0 for NLOS
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_3gpp_umi(double distance_m, double freq_ghz,
                          double tx_height_m, double rx_height_m, int is_los);

/**
 * @brief Compute 3GPP TR 38.901 UMa (Urban Macro) path loss
 * @param distance_m 3D distance (m), 10-5000
 * @param freq_ghz Carrier frequency (GHz)
 * @param tx_height_m BS height (m), typ 25
 * @param rx_height_m UE height (m), typ 1.5
 * @param is_los 1 for LOS, 0 for NLOS
 * @return Path loss in dB
 * Complexity: O(1)
 */
double pathloss_3gpp_uma(double distance_m, double freq_ghz,
                          double tx_height_m, double rx_height_m, int is_los);

/*============================================================================
 * L2: Generic Path Loss Computation
 *============================================================================*/

/**
 * @brief Compute path loss using the model specified in params
 * @param params Path loss model parameters (including model selection)
 * @param distance_m Distance (m)
 * @return Path loss in dB
 * Complexity: O(1) for most models
 */
double pathloss_compute(const pathloss_params_t *params, double distance_m);

/**
 * @brief Validate path loss model parameters for consistency
 * @param params Path loss parameters to validate
 * @return 0 if valid, non-zero error code if invalid
 * Complexity: O(1)
 */
int pathloss_validate_params(const pathloss_params_t *params);

/**
 * @brief Get path loss model name string
 * @param model Path loss model enum value
 * @return Human-readable model name
 * Complexity: O(1)
 */
const char *pathloss_model_name(pathloss_model_t model);

#ifdef __cplusplus
}
#endif

#endif /* PATHLOSS_H */
