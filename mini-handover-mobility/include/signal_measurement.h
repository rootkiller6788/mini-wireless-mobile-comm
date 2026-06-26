/**
 * @file signal_measurement.h
 * @brief Signal measurement and radio propagation models (L1, L3, L4)
 *
 * Implements physical layer measurements and propagation models that drive
 * handover decisions in cellular and WiFi systems.
 *
 * Knowledge Coverage:
 *   L1 - Definitions: RSRP, RSRQ, RSSI, SINR computation
 *   L3 - Mathematical structures: Path loss models (Okumura-Hata, COST 231,
 *        3GPP TR 38.901), fading models (Rayleigh, Rician, Shadow), Kalman filter
 *   L4 - Fundamental Laws: Friis transmission equation, Doppler shift,
 *        Log-distance path loss law
 *
 * References:
 *   - 3GPP TS 36.214 (LTE Physical Layer Measurements)
 *   - 3GPP TS 38.215 (NR Physical Layer Measurements)
 *   - 3GPP TR 38.901 (Study on channel model for frequencies 0.5 to 100 GHz)
 *   - Rappaport, "Wireless Communications: Principles and Practice" (2002)
 *   - Molisch, "Wireless Communications" (2011)
 */

#ifndef SIGNAL_MEASUREMENT_H
#define SIGNAL_MEASUREMENT_H

#include "handover_types.h"

/* ============================================================================
 * L1: Core Measurement Computation Functions
 * ============================================================================ */

/**
 * meas_compute_rssi - Compute RSSI (Received Signal Strength Indicator).
 *
 * RSSI represents the total received wideband power, including all signals,
 * interference, and thermal noise within the measurement bandwidth.
 *
 *   RSSI = P_tx + G_tx + G_rx - PL - L_body - L_penetration + Shadowing
 *
 * Units: dBm.
 *
 * @param tx_power_dbm       Transmitter power in dBm.
 * @param tx_antenna_gain_dbi Transmitter antenna gain in dBi.
 * @param rx_antenna_gain_dbi Receiver antenna gain in dBi.
 * @param path_loss_db        Total path loss in dB.
 * @param shadow_fading_db    Shadow fading component in dB.
 * @param body_loss_db        Body loss in dB.
 * @param penetration_loss_db Building penetration loss in dB.
 * @return RSSI in dBm.
 */
double meas_compute_rssi(double tx_power_dbm,
                         double tx_antenna_gain_dbi,
                         double rx_antenna_gain_dbi,
                         double path_loss_db,
                         double shadow_fading_db,
                         double body_loss_db,
                         double penetration_loss_db);

/**
 * meas_compute_rsrp - Compute RSRP (Reference Signal Received Power).
 *
 * RSRP (3GPP TS 36.214 §5.1.1) is the linear average of the power of
 * resource elements that carry cell-specific reference signals (CRS in LTE,
 * SSB or CSI-RS in NR) within the considered measurement bandwidth.
 *
 *   RSRP = RSSI_per_RE - Noise_power_per_RE
 *
 * @param rssi_dbm           Total RSSI in dBm.
 * @param measurement_bw_hz   Measurement bandwidth in Hz.
 * @param reference_signal_power_ratio Ratio of RS power to total power.
 * @param noise_figure_db     Receiver noise figure in dB.
 * @return RSRP in dBm.
 */
double meas_compute_rsrp(double rssi_dbm,
                         double measurement_bw_hz,
                         double reference_signal_power_ratio,
                         double noise_figure_db);

/**
 * meas_compute_rsrq - Compute RSRQ (Reference Signal Received Quality).
 *
 * RSRQ (3GPP TS 36.214 §5.1.3) is defined as:
 *   RSRQ = N × RSRP / RSSI
 *
 * where N is the number of resource blocks in the RSSI measurement bandwidth.
 * RSRQ provides a measure of signal quality including interference.
 *
 * @param rsrp_dbm           RSRP in dBm.
 * @param rssi_dbm           RSSI in dBm.
 * @param num_resource_blocks Number of RBs in measurement bandwidth.
 * @return RSRQ in dB.
 */
double meas_compute_rsrq(double rsrp_dbm,
                         double rssi_dbm,
                         int    num_resource_blocks);

/**
 * meas_compute_sinr - Compute SINR (Signal to Interference plus Noise Ratio).
 *
 *   SINR = P_signal / (P_interference + P_noise)
 *   SINR_dB = 10·log10(SINR)
 *
 * The thermal noise power is:
 *   P_noise = k·T·B·NF  (linear)
 *   P_noise_dBm = -174 + 10·log10(B) + NF_dB
 *
 * where k = Boltzmann constant (1.38e-23 J/K), T = 290 K, B = bandwidth in Hz.
 *
 * @param signal_power_dbm   Signal power in dBm.
 * @param interference_power_dbm Interference power in dBm.
 * @param bandwidth_hz        Bandwidth in Hz.
 * @param noise_figure_db     Receiver noise figure in dB.
 * @return SINR in dB.
 */
double meas_compute_sinr(double signal_power_dbm,
                         double interference_power_dbm,
                         double bandwidth_hz,
                         double noise_figure_db);

/* ============================================================================
 * L3: Path Loss Models
 * ============================================================================ */

/**
 * meas_friis_free_space_path_loss - Free space path loss (Friis equation).
 *
 * L4 Fundamental Law — Friis Transmission Equation:
 *   PL(dB) = 20·log10(4πd/λ) = 32.45 + 20·log10(d_km) + 20·log10(f_MHz)
 *
 * This is the fundamental propagation law for LOS conditions in free space.
 *
 * @param distance_m         Distance between transmitter and receiver (m).
 * @param frequency_hz        Carrier frequency (Hz).
 * @return Path loss in dB.
 */
double meas_friis_free_space_path_loss(double distance_m, double frequency_hz);

/**
 * meas_log_distance_path_loss - Log-distance path loss model.
 *
 * Generalized path loss model with path loss exponent:
 *   PL(d) = PL(d₀) + 10·n·log10(d/d₀)
 *
 * where n is the path loss exponent:
 *   n = 2.0  Free space
 *   n = 2.7–3.5 Urban area cellular
 *   n = 3.0–5.0 Indoor obstructed
 *
 * @param distance_m         Distance (m).
 * @param reference_distance_m Reference distance d₀ (m).
 * @param reference_loss_db   Path loss at reference distance (dB).
 * @param path_loss_exponent  Path loss exponent n.
 * @return Path loss in dB.
 */
double meas_log_distance_path_loss(double distance_m,
                                   double reference_distance_m,
                                   double reference_loss_db,
                                   double path_loss_exponent);

/**
 * meas_okumura_hata_path_loss - Okumura-Hata path loss model.
 *
 * Empirical model for urban macro-cellular systems (150–1500 MHz).
 * Used extensively in cellular network planning.
 *
 * Urban area:
 *   PL = 69.55 + 26.16·log10(f) - 13.82·log10(h_b) - a(h_m)
 *        + (44.9 - 6.55·log10(h_b))·log10(d)
 *
 * where:
 *   a(h_m) = (1.1·log10(f) - 0.7)·h_m - (1.56·log10(f) - 0.8)  for medium city
 *
 * @param frequency_mhz      Carrier frequency (150–1500 MHz).
 * @param distance_km        Distance (1–20 km).
 * @param bs_height_m        Base station antenna height (30–200 m).
 * @param ue_height_m        UE antenna height (1–10 m).
 * @param area_type          0=urban, 1=suburban, 2=rural.
 * @return Path loss in dB.
 */
double meas_okumura_hata_path_loss(double frequency_mhz,
                                   double distance_km,
                                   double bs_height_m,
                                   double ue_height_m,
                                   int    area_type);

/**
 * meas_cost231_hata_path_loss - COST 231 Hata path loss model.
 *
 * Extension of Okumura-Hata for 1500–2000 MHz (used in GSM-1800, LTE 1800):
 *   PL = 46.3 + 33.9·log10(f) - 13.82·log10(h_b) - a(h_m)
 *        + (44.9 - 6.55·log10(h_b))·log10(d) + C_m
 *
 * C_m = 0 dB for medium cities, 3 dB for metropolitan centers.
 *
 * @param frequency_mhz      Frequency (1500–2000 MHz).
 * @param distance_km        Distance (1–20 km).
 * @param bs_height_m        BS height (30–200 m).
 * @param ue_height_m        UE height (1–10 m).
 * @param is_metropolitan    True for metropolitan center correction.
 * @return Path loss in dB.
 */
double meas_cost231_hata_path_loss(double frequency_mhz,
                                   double distance_km,
                                   double bs_height_m,
                                   double ue_height_m,
                                   bool   is_metropolitan);

/**
 * meas_3gpp_38_901_uma_path_loss - 3GPP TR 38.901 UMa path loss.
 *
 * 3GPP 5G NR channel model for Urban Macro (UMa) scenario.
 * Applicable for 0.5–100 GHz.
 *
 * LOS: PL = 28.0 + 22·log10(d_3D) + 20·log10(f_c)  for 10m < d < 5km
 * NLOS: PL = max(PL_LOS, PL_NLOS')
 *   PL_NLOS' = 13.54 + 39.08·log10(d_3D) + 20·log10(f_c) - 0.6·(h_UT - 1.5)
 *
 * @param distance_3d_m      3D distance (m).
 * @param frequency_ghz      Frequency (0.5–100 GHz).
 * @param bs_height_m        BS height (typically 25 m).
 * @param ue_height_m        UE height (1.5–22.5 m).
 * @param is_los             True for LOS, false for NLOS.
 * @return Path loss in dB.
 */
double meas_3gpp_38_901_uma_path_loss(double distance_3d_m,
                                      double frequency_ghz,
                                      double bs_height_m,
                                      double ue_height_m,
                                      bool   is_los);

/* ============================================================================
 * L3: Fading Models
 * ============================================================================ */

/**
 * meas_shadow_fading_generate - Generate log-normal shadow fading sample.
 *
 * Shadow fading (slow fading) is modeled as a log-normal random process
 * with exponential spatial correlation:
 *
 *   S[t+Δt] = ρ(Δt)·S[t] + sqrt(1-ρ²)·σ·N(0,1)
 *
 * where ρ(Δt) = exp(-v·Δt / d_corr), v = speed, d_corr = correlation distance.
 *
 * @param prev_shadow_db     Previous shadow fading value.
 * @param std_dev_db         Standard deviation (σ, typical 6-10 dB).
 * @param speed_mps          UE speed.
 * @param dt_seconds         Time step.
 * @param correlation_distance_m Decorrelation distance (typical 20-100 m).
 * @return New shadow fading value in dB.
 */
double meas_shadow_fading_generate(double prev_shadow_db,
                                   double std_dev_db,
                                   double speed_mps,
                                   double dt_seconds,
                                   double correlation_distance_m);

/**
 * meas_rayleigh_fading_generate - Generate Rayleigh fading envelope.
 *
 * Rayleigh fading models multipath propagation without a dominant LOS component.
 * The envelope follows a Rayleigh distribution:
 *
 *   f(r) = (r/σ²)·exp(-r²/(2σ²)), r ≥ 0
 *
 * Generated via the sum-of-sinusoids method (Jakes' model):
 *   r(t) = sqrt( (Σ cos(ω_d·t·cos(α_n) + φ_n))² + (Σ sin(ω_d·t·cos(α_n) + φ_n))² )
 *
 * @param num_oscillators    Number of oscillators (≥8 recommended).
 * @param doppler_freq_hz    Maximum Doppler frequency.
 * @param time_seconds       Current time.
 * @return Rayleigh fading envelope amplitude (linear scale).
 */
double meas_rayleigh_fading_generate(int    num_oscillators,
                                     double doppler_freq_hz,
                                     double time_seconds);

/**
 * meas_rician_fading_generate - Generate Rician fading envelope.
 *
 * Rician fading includes a dominant LOS component plus scattered multipath:
 *
 *   f(r) = (r/σ²)·exp(-(r²+A²)/(2σ²))·I₀(rA/σ²), r ≥ 0
 *
 * K-factor = A²/(2σ²) (ratio of LOS power to scatter power).
 *   K = 0 → Rayleigh, K → ∞ → no fading.
 *
 * @param num_oscillators    Number of oscillators for scattered component.
 * @param doppler_freq_hz    Maximum Doppler frequency.
 * @param time_seconds       Current time.
 * @param k_factor_db        Rician K-factor in dB.
 * @return Rician fading envelope amplitude (linear scale).
 */
double meas_rician_fading_generate(int    num_oscillators,
                                   double doppler_freq_hz,
                                   double time_seconds,
                                   double k_factor_db);

/* ============================================================================
 * L5: Signal Prediction — Kalman Filter for RSRP Tracking
 * ============================================================================ */

/**
 * meas_kalman_filter_rsrp - Kalman filter for RSRP prediction and smoothing.
 *
 * State-space model for RSRP tracking:
 *   State: x = [RSRP, RSRP_rate]
 *   Measurement: z = RSRP_measured
 *
 * Prediction:
 *   x̂_k⁻ = F·x̂_{k-1}   where F = [[1, Δt], [0, 1]]
 *   P_k⁻ = F·P_{k-1}·Fᵀ + Q
 *
 * Update:
 *   K_k = P_k⁻·Hᵀ·(H·P_k⁻·Hᵀ + R)⁻¹
 *   x̂_k = x̂_k⁻ + K_k·(z_k - H·x̂_k⁻)
 *   P_k = (I - K_k·H)·P_k⁻
 *
 * where H = [1, 0], Q = process noise covariance, R = measurement noise variance.
 *
 * @param measured_rsrp_dbm  Current RSRP measurement.
 * @param dt_seconds         Time since last measurement.
 * @param process_noise_var  Process noise variance.
 * @param measurement_noise_var Measurement noise variance.
 * @param[in,out] state_rsrp Estimated RSRP state (updated).
 * @param[in,out] state_rate Estimated RSRP rate of change (updated).
 * @param[in,out] cov_p11    Covariance P[0][0] (updated).
 * @param[in,out] cov_p12    Covariance P[0][1] (updated).
 * @param[in,out] cov_p22    Covariance P[1][1] (updated).
 * @return Filtered RSRP estimate in dBm.
 */
double meas_kalman_filter_rsrp(double  measured_rsrp_dbm,
                               double  dt_seconds,
                               double  process_noise_var,
                               double  measurement_noise_var,
                               double *state_rsrp,
                               double *state_rate,
                               double *cov_p11,
                               double *cov_p12,
                               double *cov_p22);

/* ============================================================================
 * L3: Layer-3 Filtering (3GPP TS 36.331 §5.5.3.2)
 * ============================================================================ */

/**
 * meas_l3_filter - Layer-3 (RRC) filtering of measurements.
 *
 * 3GPP specifies a first-order IIR filter for L3 measurement smoothing:
 *   F_n = (1-a)·F_{n-1} + a·M_n
 *
 * where a = 1/2^(k/4), k ∈ {0, 1, ..., 19} is the filter coefficient.
 * Higher k means more smoothing (slower response).
 *
 * @param prev_filtered      Previous filtered measurement.
 * @param new_measurement    New raw measurement.
 * @param filter_coeff_k     Filter coefficient k [0, 19].
 * @return Filtered measurement.
 */
double meas_l3_filter(double prev_filtered,
                      double new_measurement,
                      int    filter_coeff_k);

#endif /* SIGNAL_MEASUREMENT_H */
