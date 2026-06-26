/**
 * @file doppler.h
 * @brief Doppler Spectrum and Time-Varying Channel Models (L3, L4, L5)
 *
 * Models the Doppler effect in mobile wireless channels. Relative motion
 * between transmitter and receiver causes frequency dispersion (Doppler spread)
 * which determines:
 *
 *   - Coherence time: T_c ~ 0.423 / f_d (time over which channel is static)
 *   - Fast vs slow fading classification
 *   - Time-selective fading (frequency dispersion in time)
 *
 * L3 Mathematical Structures:
 *   - Doppler PSD S(f) — Fourier transform of temporal autocorrelation
 *   - Jakes (Clarke) PSD for 2D isotropic scattering
 *   - Doppler spectrum for directional/beamformed channels
 *
 * L4 Fundamental Laws:
 *   - f_d = v*f_c/c — Doppler shift formula
 *   - Wiener-Khinchin theorem: ACF <-> PSD via Fourier transform
 *   - Bello's system functions for WSSUS channels
 *
 * L5 Algorithms:
 *   - Sum-of-sinusoids (SOS) Doppler generator
 *   - Filtered Gaussian noise Doppler generation
 *   - Level crossing rate (LCR) and average fade duration (AFD) computation
 *
 * Reference: Clarke, "A statistical theory of mobile-radio reception", BSTJ 1968
 * Reference: Jakes, "Microwave Mobile Communications", 1974
 * Reference: Molisch, "Wireless Communications", 2nd Ed, Ch. 5, 8 (2011)
 *
 * Course Mapping:
 *   Stanford EE359 - Wireless (Doppler, coherence time)
 *   MIT 6.450 - Digital Communications (time-varying channels)
 *   Berkeley EE123 - DSP (Doppler PSD)
 */

#ifndef DOPPLER_H
#define DOPPLER_H

#include "channel_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L3: Doppler Spectrum Models
 *============================================================================*/

/**
 * @brief Compute Jakes (bathtub) Doppler PSD value at frequency offset f
 *
 * For 2D isotropic scattering:
 *   S(f) = P_r / (pi*f_d*sqrt(1 - (f/f_d)^2))    for |f| < f_d
 *        = 0                                       otherwise
 *
 * @param freq_offset_hz Frequency offset from carrier f (Hz), |f| <= f_d
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param total_power Total received power P_r (linear)
 * @return PSD value S(f) in linear units (W/Hz). Returns 0 for |f| >= f_d.
 * Complexity: O(1)
 */
double doppler_jakes_psd(double freq_offset_hz, double f_d_hz,
                          double total_power);

/**
 * @brief Compute flat (uniform) Doppler PSD for 3D isotropic scattering
 * S(f) = P_r / (2*f_d)  for |f| < f_d, 0 otherwise.
 * Used for indoor channels with rich 3D scattering.
 * @param freq_offset_hz Frequency offset (Hz)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param total_power Total power (linear)
 * @return PSD value S(f)
 * Complexity: O(1)
 */
double doppler_flat_psd(double freq_offset_hz, double f_d_hz,
                         double total_power);

/**
 * @brief Compute asymmetric Doppler PSD for directional scattering
 * Models channels where scatterers are clustered around a mean angle.
 * S(f) = P_r/(pi*f_d*sqrt(1-(f/f_d)^2)) * exp(k*cos(theta - theta_0))
 * @param freq_offset_hz Frequency offset (Hz)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param total_power Total power
 * @param mean_angle_rad Mean AoA (radians)
 * @param angular_spread Angular concentration parameter
 * @return PSD value S(f)
 * Complexity: O(1)
 */
double doppler_directional_psd(double freq_offset_hz, double f_d_hz,
                                double total_power, double mean_angle_rad,
                                double angular_spread);

/*============================================================================
 * L4: Temporal Correlation and Coherence Time
 *============================================================================*/

/**
 * @brief Compute Clarke's temporal autocorrelation (Bessel J_0 model)
 *
 * ACF(tau) = sigma^2 * J_0(2*pi*f_d*tau)
 *
 * @param tau_s Time lag (s)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param sigma_power Mean power (sigma^2)
 * @return Autocorrelation value at lag tau
 * Complexity: O(1) — uses Bessel J_0 numerical approximation
 */
double doppler_clarke_acf(double tau_s, double f_d_hz, double sigma_power);

/**
 * @brief Compute coherence time (50% correlation threshold)
 * T_c = 9/(16*pi*f_d) ~ 0.179/f_d (rigorous)
 * Or T_c ~ 0.423/f_d (popular rule of thumb, more conservative)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @return Coherence time (s)
 * Complexity: O(1)
 */
double doppler_coherence_time_50(double f_d_hz);

/**
 * @brief Compute coherence time (90% correlation threshold)
 * T_c ~ 0.0812 / f_d
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @return Coherence time (s)
 * Complexity: O(1)
 */
double doppler_coherence_time_90(double f_d_hz);

/**
 * @brief Compute frequency correlation for WSSUS channel
 * Using Bello's system functions: scatterer function relates Doppler PSD,
 * delay PSD, and frequency-time correlation.
 * @param delta_f_hz Frequency separation (Hz)
 * @param rms_delay_ns RMS delay spread (ns)
 * @return Frequency correlation coefficient (0 to 1)
 * Complexity: O(1)
 */
double doppler_freq_correlation(double delta_f_hz, double rms_delay_ns);

/*============================================================================
 * L5: Doppler Spectrum Generation (Sum-of-Sinusoids)
 *
 * The Jakes method synthesizes a Rayleigh fading waveform with specified
 * Doppler spectrum using a deterministic sum of sinusoids:
 *
 * h(t) = h_I(t) + j*h_Q(t)
 *
 * h_I(t) = (2/sqrt(N))*sum_{n=1}^{N} cos(beta_n)*cos(2*pi*f_d*cos(alpha_n)*t + phi_n)
 * h_Q(t) = (2/sqrt(N))*sum_{n=1}^{N} sin(beta_n)*cos(2*pi*f_d*cos(alpha_n)*t + phi_n)
 *
 * Modified Jakes (Dent 1993) ensures uncorrelated I and Q components.
 *============================================================================*/

/**
 * @brief Opaque Doppler waveform generator handle
 */
typedef struct doppler_waveform_s doppler_waveform_t;

/**
 * @brief Initialize Doppler Rayleigh fading waveform generator
 * @param params Doppler parameters (f_d, N_sinusoids, sample_rate)
 * @return Handle to generator, or NULL on error
 * Complexity: O(N_sinusoids) allocation
 */
doppler_waveform_t *doppler_waveform_init(const doppler_params_t *params);

/**
 * @brief Generate next complex fading sample from Doppler generator
 * @param gen Waveform generator handle
 * @return Complex fading coefficient at next time step
 * Complexity: O(N_sinusoids)
 */
double complex doppler_waveform_next(doppler_waveform_t *gen);

/**
 * @brief Generate block of Doppler fading samples
 * @param gen Waveform generator
 * @param output Output array, length num_samples
 * @param num_samples Number of samples to generate
 * Complexity: O(N_sinusoids * num_samples)
 */
void doppler_waveform_block(doppler_waveform_t *gen, double complex *output,
                             size_t num_samples);

/**
 * @brief Get current time index of Doppler generator
 * @param gen Waveform generator
 * @return Current time in seconds
 * Complexity: O(1)
 */
double doppler_waveform_time(const doppler_waveform_t *gen);

/**
 * @brief Reset Doppler generator to time zero
 * @param gen Waveform generator
 */
void doppler_waveform_reset(doppler_waveform_t *gen);

/**
 * @brief Free Doppler waveform generator
 * @param gen Generator handle (NULL safe)
 */
void doppler_waveform_free(doppler_waveform_t *gen);

/*============================================================================
 * L5: Level Crossing Rate and Average Fade Duration
 *
 * These are second-order statistics that characterize how often and how long
 * the signal envelope falls below a given threshold.
 *
 * LCR — level crossing rate:
 *   N_rho = integral(0 to inf) r_dot * f_{r,r_dot}(rho, r_dot) dr_dot
 *
 * AFD — average fade duration:
 *   tau_rho = P(r <= rho) / N_rho
 *============================================================================*/

/**
 * @brief Compute Level Crossing Rate for Rayleigh fading
 *
 * N_rho = sqrt(2*pi) * f_d * rho * exp(-rho^2)
 *
 * where rho = R_thresh / R_rms is the normalized threshold amplitude.
 *
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param rho Normalized threshold (threshold/R_rms in linear)
 * @return LCR in crossings per second
 * Complexity: O(1)
 */
double doppler_lcr_rayleigh(double f_d_hz, double rho);

/**
 * @brief Compute Average Fade Duration for Rayleigh fading
 *
 * AFD = (exp(rho^2) - 1) / (sqrt(2*pi) * f_d * rho)
 *
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param rho Normalized threshold
 * @return AFD in seconds
 * Complexity: O(1)
 */
double doppler_afd_rayleigh(double f_d_hz, double rho);

/**
 * @brief Compute LCR for Rician fading
 *
 * N_rho = sqrt(2*pi*(K+1)) * f_d * rho * exp(-K-(K+1)*rho^2) * I_0(2*rho*sqrt(K*(K+1)))
 *
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param rho Normalized threshold
 * @param k_linear Rician K-factor (linear, not dB)
 * @return LCR in crossings per second
 * Complexity: O(1)
 */
double doppler_lcr_rician(double f_d_hz, double rho, double k_linear);

/**
 * @brief Compute AFD for Rician fading
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param rho Normalized threshold
 * @param k_linear Rician K-factor (linear)
 * @return AFD in seconds
 * Complexity: O(1)
 */
double doppler_afd_rician(double f_d_hz, double rho, double k_linear);

/**
 * @brief Compute full LCR/AFD result for given fading parameters
 * @param params Fading parameters (type, doppler_spread_hz)
 * @param threshold_db Threshold in dB relative to RMS level
 * @param result Output LCR/AFD result
 * @return 0 on success, -1 if fading type not supported
 * Complexity: O(1)
 */
int doppler_compute_lcr_afd(const fading_params_t *params,
                             double threshold_db,
                             lcr_afd_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_H */
