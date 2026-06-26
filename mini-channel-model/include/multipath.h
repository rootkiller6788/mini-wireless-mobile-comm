/**
 * @file multipath.h
 * @brief Multipath Channel Models — Tapped Delay Line (L3, L5, L6)
 *
 * Implements multipath propagation channel models. The key insight is that
 * wireless signals arrive at the receiver via multiple paths with different
 * delays, attenuations, and phases. This creates:
 *
 *   - Frequency selectivity (when bandwidth > coherence bandwidth)
 *   - Inter-symbol interference (ISI) in time domain
 *   - Frequency-domain notches (selective fading of subcarriers)
 *
 * L3 Mathematical Structures:
 *   - Channel impulse response h(tau) = sum a_n*delta(tau - tau_n)*exp(j*theta_n)
 *   - Channel transfer function H(f) = F{h(tau)} = sum a_n*exp(-j*2*pi*f*tau_n)*exp(j*theta_n)
 *   - Frequency correlation function: R_H(Delta_f) = E[H(f)*H*(f+Delta_f)]
 *
 * L5 Algorithms/Methods:
 *   - Tapped Delay Line (TDL) model — FIR filter with time-varying coefficients
 *   - Power Delay Profile (PDP) generation for standard models
 *   - Frequency-domain channel realization via IDFT
 *   - 3GPP TDL/EPA/EVA/ETU profile generation
 *
 * L6 Canonical Problems:
 *   - OFDM over frequency-selective channel: cyclic prefix length vs delay spread
 *   - Rake receiver combining: maximum ratio combining of multipath
 *   - Equalization to combat ISI: zero-forcing / MMSE
 *
 * Reference: Molisch, "Wireless Communications", 2nd Ed, Ch. 7 (2011)
 * Reference: 3GPP TS 36.101 (LTE UE radio transmission and reception)
 * Reference: 3GPP TR 38.901 (5G NR channel model)
 *
 * Course Mapping:
 *   MIT 6.450 - Digital Communications (multipath, ISI, equalization)
 *   Stanford EE359 - Wireless (delay spread, coherence BW)
 *   Berkeley EE123 - DSP (FIR channel models)
 */

#ifndef MULTIPATH_H
#define MULTIPATH_H

#include "channel_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L3: Power Delay Profile Construction
 *============================================================================*/

/**
 * @brief Allocate a power delay profile with given number of taps
 * @param num_taps Number of multipath components
 * @return Pointer to allocated PDP (caller must free with multipath_pdp_free),
 *         or NULL on allocation failure.
 * Complexity: O(N)
 */
power_delay_profile_t *multipath_pdp_alloc(size_t num_taps);

/**
 * @brief Free a power delay profile
 * @param pdp PDP to free (NULL safe)
 */
void multipath_pdp_free(power_delay_profile_t *pdp);

/**
 * @brief Compute PDP moments: mean delay and RMS delay spread
 * Updates pdp->mean_delay_ns and pdp->rms_delay_ns.
 * @param pdp PDP with taps populated (power values must be linear)
 * Complexity: O(N)
 */
void multipath_pdp_compute_moments(power_delay_profile_t *pdp);

/**
 * @brief Compute coherence bandwidths from PDP and update fields
 * B_c(0.5) = 1/(5*sigma_tau), B_c(0.9) = 1/(50*sigma_tau)
 * @param pdp PDP with rms_delay_ns computed
 * Complexity: O(1)
 */
void multipath_pdp_compute_coherence_bandwidth(power_delay_profile_t *pdp);

/**
 * @brief Normalize PDP so total average power = 0 dB (unit gain)
 * Scales all tap powers such that sum(10^(pwr_dB/10)) = 1.
 * @param pdp PDP to normalize
 * Complexity: O(N)
 */
void multipath_pdp_normalize(power_delay_profile_t *pdp);

/*============================================================================
 * L6: Standard Channel Models — PDP Generation
 *
 * These generate power delay profiles for 3GPP/ITU standard channel models.
 * Each function populates a pre-allocated PDP structure with the specified
 * tap delays, relative powers, and (optionally) Doppler spectra.
 *============================================================================*/

/**
 * @brief Generate 3GPP TDL-A power delay profile
 * TDL-A (short delay spread, 10 ns RMS DS non-LOS)
 * Used for indoor/small-cell evaluation.
 * @param pdp Pre-allocated PDP (will be resized to 3 taps for LOS, 4 for NLOS)
 * @param is_los 1 for LOS profile, 0 for NLOS
 * @param delay_scaling Factor to scale all delays (1.0 = standard)
 * @return 0 on success, -1 on error
 * Complexity: O(1)
 */
int multipath_generate_tdl_a(power_delay_profile_t *pdp, int is_los,
                              double delay_scaling);

/**
 * @brief Generate 3GPP TDL-B power delay profile
 * TDL-B (medium delay spread, 100 ns RMS DS)
 * @param pdp Pre-allocated PDP (resized to appropriate number of taps)
 * @param delay_scaling Factor to scale all delays
 * @return 0 on success
 * Complexity: O(1)
 */
int multipath_generate_tdl_b(power_delay_profile_t *pdp, double delay_scaling);

/**
 * @brief Generate 3GPP TDL-C power delay profile
 * TDL-C (long delay spread, 300 ns RMS DS)
 * Used for urban macro evaluation.
 * @param pdp Pre-allocated PDP
 * @param delay_scaling Delay scaling factor
 * @return 0 on success
 * Complexity: O(1)
 */
int multipath_generate_tdl_c(power_delay_profile_t *pdp, double delay_scaling);

/**
 * @brief Generate LTE EPA (Extended Pedestrian A) power delay profile
 * Low delay spread (~45 ns RMS), typical for indoor/pedestrian.
 * 7 taps, maximum excess delay 410 ns.
 * @param pdp Pre-allocated PDP (will contain 7 taps)
 * @return 0 on success
 * Complexity: O(1)
 */
int multipath_generate_epa(power_delay_profile_t *pdp);

/**
 * @brief Generate LTE EVA (Extended Vehicular A) power delay profile
 * Medium delay spread (~357 ns RMS), typical for vehicular.
 * 9 taps, maximum excess delay 2510 ns.
 * @param pdp Pre-allocated PDP (will contain 9 taps)
 * @return 0 on success
 * Complexity: O(1)
 */
int multipath_generate_eva(power_delay_profile_t *pdp);

/**
 * @brief Generate LTE ETU (Extended Typical Urban) power delay profile
 * High delay spread (~991 ns RMS), typical for urban macro.
 * 9 taps, maximum excess delay 5000 ns.
 * @param pdp Pre-allocated PDP (will contain 9 taps)
 * @return 0 on success
 * Complexity: O(1)
 */
int multipath_generate_etu(power_delay_profile_t *pdp);

/*============================================================================
 * L5: Tapped Delay Line (TDL) Channel Simulation
 *
 * The TDL model is the workhorse for frequency-selective channel simulation.
 * It implements an FIR filter where each tap coefficient is a time-varying
 * complex random process with specified Doppler spectrum.
 *
 * Output: y[n] = sum_{l=0}^{L-1} h_l[n] * x[n-l]
 *
 * where h_l[n] is the l-th tap coefficient at discrete time n.
 *============================================================================*/

/**
 * @brief Opaque TDL channel simulator handle
 */
typedef struct multipath_tdl_s multipath_tdl_t;

/**
 * @brief Initialize TDL channel simulator
 * @param pdp Power delay profile defining tap delays and powers
 * @param sample_rate_hz Sampling rate for input/output signals (Hz)
 * @param carrier_freq_hz Carrier frequency (Hz) — used for Doppler computation
 * @param velocity_ms Mobile velocity (m/s) — 0 for static channel
 * @return Handle to TDL simulator, or NULL on error
 * Complexity: O(L) where L = number of taps
 */
multipath_tdl_t *multipath_tdl_init(const power_delay_profile_t *pdp,
                                     double sample_rate_hz,
                                     double carrier_freq_hz,
                                     double velocity_ms);

/**
 * @brief Process one sample through the TDL channel
 * @param tdl TDL simulator handle
 * @param input Complex baseband input sample
 * @return Complex baseband output sample (sum of convolved taps)
 * Complexity: O(L) where L = number of taps
 */
double complex multipath_tdl_process(multipath_tdl_t *tdl, double complex input);

/**
 * @brief Process a block of samples through the TDL channel
 * @param tdl TDL simulator handle
 * @param input Input sample array, length num_samples
 * @param output Output sample array, length num_samples (pre-allocated)
 * @param num_samples Number of samples to process
 * Complexity: O(N*L)
 */
void multipath_tdl_process_block(multipath_tdl_t *tdl,
                                  const double complex *input,
                                  double complex *output,
                                  size_t num_samples);

/**
 * @brief Update tap coefficients for time-varying channel
 * Advances the fading processes by one sample period.
 * @param tdl TDL simulator handle
 * Complexity: O(L)
 */
void multipath_tdl_advance_fading(multipath_tdl_t *tdl);

/**
 * @brief Get current channel impulse response from TDL
 * @param tdl TDL simulator handle
 * @param cir Output CIR structure (caller allocates)
 * @return 0 on success
 * Complexity: O(num_samples)
 */
int multipath_tdl_get_cir(const multipath_tdl_t *tdl,
                           channel_impulse_response_t *cir);

/**
 * @brief Reset TDL state (re-initialize fading coefficients)
 * @param tdl TDL simulator handle
 */
void multipath_tdl_reset(multipath_tdl_t *tdl);

/**
 * @brief Free TDL simulator resources
 * @param tdl TDL handle (NULL safe)
 */
void multipath_tdl_free(multipath_tdl_t *tdl);

/*============================================================================
 * L5: Frequency-Domain Channel Response
 *============================================================================*/

/**
 * @brief Compute channel frequency response from impulse response
 * H[k] = sum_{n=0}^{N-1} h[n]*exp(-j*2*pi*k*n/N)  [DFT of CIR]
 * @param cir_samples Complex CIR samples, length num_samples
 * @param num_samples Number of CIR samples
 * @param freq_response Output frequency response, length num_subcarriers
 * @param num_subcarriers Number of frequency points (typically FFTsize)
 * @return 0 on success
 * Complexity: O(N*K) for direct DFT
 */
int multipath_freq_response(const double complex *cir_samples,
                             size_t num_samples,
                             double complex *freq_response,
                             size_t num_subcarriers);

/**
 * @brief Compute channel frequency correlation function
 * R_H[delta_k] = (1/K)*sum_{k} H[k]*conj(H[k+delta_k])
 * @param freq_response Frequency response H[k], length num_subcarriers
 * @param num_subcarriers Number of subcarriers
 * @param correlation Output correlation values, length max_delta+1
 * @param max_delta Maximum frequency index delta to compute
 * @return 0 on success
 * Complexity: O(K*max_delta)
 */
int multipath_freq_correlation(const double complex *freq_response,
                                size_t num_subcarriers,
                                double *correlation,
                                size_t max_delta);

/**
 * @brief Compute channel transfer function from PDP
 * H(f) = sum a_l*exp(-j*2*pi*f*tau_l)*exp(j*theta_l)
 * @param pdp Power delay profile
 * @param freq_hz Frequency point (Hz)
 * @return H(f) complex value at given frequency
 * Complexity: O(L)
 */
double complex multipath_transfer_function(const power_delay_profile_t *pdp,
                                            double freq_hz);

/*============================================================================
 * L6: Rake Receiver — Multipath Diversity Combining
 *
 * Rake receiver exploits multipath diversity by combining multiple delayed
 * copies of the signal. Each "finger" despreads a different path.
 *
 * Output decision statistic: z = sum w_l* * y_l
 *
 * where y_l is the matched filter output for path l, w_l are combining weights.
 * MRC (Maximum Ratio Combining): w_l = h_l* (conjugate of channel gain)
 *============================================================================*/

/**
 * @brief Compute Maximum Ratio Combining (MRC) weights for Rake receiver
 * w_l = h_l* (complex conjugate of path gain) — maximizes output SNR
 * @param taps Array of channel tap gains (complex)
 * @param num_taps Number of multipath components
 * @param weights Output MRC weight array, length num_taps
 * Complexity: O(L)
 */
void multipath_rake_mrc_weights(const double complex *taps,
                                 size_t num_taps,
                                 double complex *weights);

/**
 * @brief Compute Equal Gain Combining (EGC) weights
 * w_l = exp(-j*arg(h_l)) — corrects phase, equal amplitudes
 * @param taps Array of channel tap gains
 * @param num_taps Number of taps
 * @param weights Output EGC weight array
 * Complexity: O(L)
 */
void multipath_rake_egc_weights(const double complex *taps,
                                 size_t num_taps,
                                 double complex *weights);

/**
 * @brief Compute post-combining SNR for MRC Rake
 * @param taps Channel tap gains (linear)
 * @param num_taps Number of multipath
 * @param snr_per_path_dB SNR per path (dB), assumed equal
 * @return Combined SNR (dB) = 10*log10(sum|h_l|^2*SNR_linear)
 * Complexity: O(L)
 */
double multipath_rake_snr_mrc(const double complex *taps,
                               size_t num_taps,
                               double snr_per_path_dB);

#ifdef __cplusplus
}
#endif

#endif /* MULTIPATH_H */
