/**
 * @file fading.h
 * @brief Small-Scale and Large-Scale Fading Models (L2, L3, L4, L5)
 *
 * Implements statistical fading channel models for wireless communications.
 * Covers both small-scale fading (Rayleigh, Rician, Nakagami-m) and
 * large-scale fading (log-normal shadowing).
 *
 * L4 Fundamental Laws:
 *   - Clarke's Model of Rayleigh fading autocorrelation
 *   - Jakes Doppler Spectrum for isotropic scattering
 *   - Rician K-factor decomposition
 *
 * L3 Mathematical Structures:
 *   - Complex circularly symmetric Gaussian random variables
 *   - Rayleigh PDF: p(r) = (r/sigma^2)*exp(-r^2/(2*sigma^2))
 *   - Rician PDF: p(r) = (r/sigma^2)*exp(-(r^2+s^2)/(2*sigma^2))*I_0(r*s/sigma^2)
 *   - Nakagami-m PDF: p(r) = (2*m^m*r^(2m-1))/(Gamma(m)*Omega^m)*exp(-m*r^2/Omega)
 *   - Log-normal PDF for shadowing
 *
 * L2 Core Concepts:
 *   - Envelope distribution determines BER performance
 *   - K-factor: ratio of LOS power to diffuse power
 *   - m-parameter: measure of fading severity (m=1 => Rayleigh, m=infty => no fading)
 *   - Shadowing correlation distance
 *
 * Reference: Molisch, "Wireless Communications", 2nd Ed, Ch. 5-6 (2011)
 * Reference: Proakis & Salehi, "Digital Communications", 5th Ed, Ch. 14
 * Reference: Clarke, "A statistical theory of mobile-radio reception", BSTJ 1968
 * Reference: Jakes, "Microwave Mobile Communications", 1974
 *
 * Course Mapping:
 *   MIT 6.450 - Digital Communications (fading channels)
 *   Stanford EE359 - Wireless (Rayleigh/Rician models)
 *   Berkeley EE123 - DSP (fading simulation)
 *   Michigan EECS 455 - Communications (fading statistics)
 */

#ifndef FADING_H
#define FADING_H

#include "channel_defs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L5: Random Number Generation Primitives for Fading
 *
 * Each fading type requires specific generation algorithms.
 *============================================================================*/

/**
 * @brief Generate uniform random number in [0, 1)
 * Uses the system rand() with simple linear congruential generator.
 * For production, replace with Mersenne Twister or PCG.
 * Complexity: O(1)
 */
double fading_rand_uniform(void);

/**
 * @brief Seed the fading random number generator
 * @param seed Seed value
 * Complexity: O(1)
 */
void fading_rand_seed(unsigned int seed);

/**
 * @brief Generate standard normal N(0,1) random variable
 * Uses Box-Muller transform on two uniform random numbers.
 * Complexity: O(1)
 */
double fading_rand_normal(void);

/**
 * @brief Generate complex Gaussian CN(0, sigma^2) random variable
 * @param sigma Standard deviation (real and imaginary parts have variance sigma^2/2)
 * @return Complex Gaussian sample with zero mean, variance sigma^2 per dimension
 * Complexity: O(1)
 */
double complex fading_rand_complex_gaussian(double sigma);

/*============================================================================
 * L3: Fading Distribution PDF, CDF, and Moments
 *============================================================================*/

/**
 * @brief Compute Rayleigh PDF value
 * @param x Envelope amplitude (x >= 0)
 * @param sigma Scale parameter (sigma > 0) — RMS of real/imag Gaussian components
 * @return f_X(x) = (x/sigma^2)*exp(-x^2/(2*sigma^2))
 * Complexity: O(1)
 */
double fading_rayleigh_pdf(double x, double sigma);

/**
 * @brief Compute Rayleigh CDF value
 * @param x Envelope amplitude (x >= 0)
 * @param sigma Scale parameter (sigma > 0)
 * @return F_X(x) = 1 - exp(-x^2/(2*sigma^2))
 * Complexity: O(1)
 */
double fading_rayleigh_cdf(double x, double sigma);

/**
 * @brief Generate Rayleigh-distributed envelope sample
 * r = sqrt(X^2 + Y^2) where X, Y ~ N(0, sigma^2) i.i.d.
 * @param sigma Scale parameter (sigma > 0)
 * @return Rayleigh distributed sample
 * Complexity: O(1)
 */
double fading_generate_rayleigh(double sigma);

/**
 * @brief Compute Rician PDF value
 * @param x Envelope amplitude (x >= 0)
 * @param nu LOS component amplitude (non-centrality parameter)
 * @param sigma Diffuse component scale (sigma > 0)
 * @return f_X(x) = (x/sigma^2)*exp(-(x^2+nu^2)/(2*sigma^2))*I_0(x*nu/sigma^2)
 * Complexity: O(1)
 *
 * Note: I_0 is the modified Bessel function of the first kind, order 0.
 */
double fading_rician_pdf(double x, double nu, double sigma);

/**
 * @brief Compute Rician CDF value (Marcum Q function)
 * @param x Envelope amplitude
 * @param nu LOS amplitude
 * @param sigma Diffuse scale
 * @return F_X(x) = 1 - Q_1(nu/sigma, x/sigma) where Q_1 is Marcum Q
 * Complexity: O(1) — uses numerical approximation
 */
double fading_rician_cdf(double x, double nu, double sigma);

/**
 * @brief Generate Rician-distributed envelope sample
 * r = sqrt((nu + X)^2 + Y^2) where X, Y ~ N(0, sigma^2)
 * @param nu LOS amplitude (>= 0)
 * @param sigma Diffuse scale (> 0)
 * @return Rician sample
 * Complexity: O(1)
 */
double fading_generate_rician(double nu, double sigma);

/**
 * @brief Generate Rician sample from K-factor (dB)
 * @param sigma Diffuse scale
 * @param k_factor_db Rician K-factor = 10*log10(nu^2/(2*sigma^2))
 * @return Rician sample. nu is internally computed as sqrt(2*sigma^2*10^(K/10))
 * Complexity: O(1)
 */
double fading_generate_rician_from_k(double sigma, double k_factor_db);

/**
 * @brief Compute Nakagami-m PDF value
 * @param x Envelope amplitude (x >= 0)
 * @param m Shape parameter (m >= 0.5)
 * @param omega Spread parameter = E[X^2] (omega > 0)
 * @return f_X(x) = (2*m^m/Gamma(m)/omega^m)*x^(2m-1)*exp(-m*x^2/omega)
 * Complexity: O(1)
 */
double fading_nakagami_pdf(double x, double m, double omega);

/**
 * @brief Generate Nakagami-m distributed sample
 * @param m Shape parameter (m >= 0.5)
 * @param omega Spread parameter = E[X^2]
 * @return Nakagami-m sample
 * Complexity: O(m) — uses sum of 2m squared Gaussian samples
 */
double fading_generate_nakagami(double m, double omega);

/**
 * @brief Compute log-normal PDF for shadow fading
 * @param x Value in dB (x is dB value, not linear!)
 * @param mu_db Mean in dB (typically 0 dB)
 * @param sigma_db Standard deviation in dB
 * @return f_X(x) in linear domain
 * Complexity: O(1)
 */
double fading_lognormal_pdf(double x, double mu_db, double sigma_db);

/**
 * @brief Generate log-normal shadow fading sample
 * @param sigma_db Standard deviation in dB (typ 3-12 dB)
 * @return Shadow fading value in dB (zero mean)
 * Complexity: O(1)
 */
double fading_generate_lognormal(double sigma_db);

/**
 * @brief Compute Weibull PDF value (for indoor/V2V channels)
 * @param x Envelope amplitude (x >= 0)
 * @param beta Shape parameter (beta > 0)
 * @param alpha Scale parameter (alpha > 0)
 * @return f_X(x) = (beta/alpha)*(x/alpha)^(beta-1)*exp(-(x/alpha)^beta)
 * Complexity: O(1)
 */
double fading_weibull_pdf(double x, double beta, double alpha);

/**
 * @brief Generate Weibull distributed sample
 * @param beta Shape parameter (beta > 0, beta=2 => Rayleigh)
 * @param alpha Scale parameter (alpha > 0)
 * @return Weibull sample
 * Complexity: O(1)
 */
double fading_generate_weibull(double beta, double alpha);

/*============================================================================
 * L4: Clarke's Model — Autocorrelation of Rayleigh Fading
 *
 * Clarke (1968) derived the autocorrelation function for a mobile receiver
 * in isotropic scattering with vertical antenna:
 *
 *   ACF(tau) = sigma^2 * J_0(2*pi*f_d*tau)
 *
 * where J_0 is the zero-order Bessel function of the first kind.
 * The PSD is the classic "bathtub" (U-shaped) Jakes spectrum.
 *============================================================================*/

/**
 * @brief Compute Clarke's temporal autocorrelation for Rayleigh fading
 * @param tau_s Time lag (seconds)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param sigma_power Mean square value (sigma^2 = P_r/2)
 * @return ACF value at lag tau: R(tau) = sigma^2 * J_0(2*pi*f_d*tau)
 * Complexity: O(1)
 */
double fading_clarke_autocorrelation(double tau_s, double f_d_hz,
                                      double sigma_power);

/**
 * @brief Compute spatial autocorrelation for Rayleigh fading
 * @param delta_d_m Spatial separation (m)
 * @param freq_hz Carrier frequency (Hz)
 * @param sigma_power Mean square value
 * @return R(delta_d) = sigma^2 * J_0(2*pi*delta_d/lambda)
 * Complexity: O(1)
 */
double fading_spatial_autocorrelation(double delta_d_m, double freq_hz,
                                       double sigma_power);

/*============================================================================
 * L5: Sum-of-Sinusoids (Jakes Method) for Correlated Fading
 *
 * Generates time-correlated Rayleigh fading waveforms using the
 * deterministic sum-of-sinusoids approach (Jakes, 1974; modified by
 * Dent, Bottomley, Croft 1993 for multiple uncorrelated waveforms).
 *
 * N oscillators with frequencies f_d*cos(2*pi*n/N) and random phases.
 *============================================================================*/

/**
 * @brief Initialize Jakes sum-of-sinusoids fading generator
 * @param num_sinusoids Number of oscillators N (typ >= 8)
 * @param f_d_hz Maximum Doppler shift (Hz)
 * @param sample_rate_hz Sample rate for time series output (Hz)
 * @return Opaque handle (cast to void*) to use with fading_jakes_next()
 *         Returns NULL on failure.
 * Complexity: O(N) allocation
 */
void *fading_jakes_init(size_t num_sinusoids, double f_d_hz,
                         double sample_rate_hz);

/**
 * @brief Generate next complex fading coefficient from Jakes generator
 * @param handle Opaque handle from fading_jakes_init()
 * @return Complex fading gain at current time instant
 * Complexity: O(N*sin) where N = num_sinusoids
 */
double complex fading_jakes_next(void *handle);

/**
 * @brief Free Jakes generator resources
 * @param handle Opaque handle from fading_jakes_init()
 */
void fading_jakes_free(void *handle);

/*============================================================================
 * L5: Correlated Fading Generation via Cholesky Decomposition
 *
 * Given n channels with specified correlation matrix R (n x n):
 *   L = chol(R) where R = L*L^T
 *   h_corr = L * h_iid   where h_iid ~ CN(0, I_n)
 *
 * This generates n correlated complex Gaussian fading coefficients.
 * Used for MIMO channels with spatial correlation and for
 * multi-antenna diversity analysis.
 *============================================================================*/

/**
 * @brief Compute Cholesky decomposition of real symmetric positive-definite matrix
 * @param A Input symmetric PSD matrix, size n x n (row-major)
 * @param n Matrix dimension
 * @param L Output lower triangular matrix (L*L^T = A), size n x n (row-major)
 * @return 0 on success, -1 if matrix is not positive definite
 * Complexity: O(n^3)
 */
int fading_cholesky_decomp(const double *A, size_t n, double *L);

/**
 * @brief Generate correlated Rayleigh fading coefficients
 * @param corr_matrix Spatial correlation matrix, size num_channels x num_channels
 * @param num_channels Number of correlated channels to generate
 * @param sigma Scale parameter for Rayleigh
 * @param output Array of length num_channels for correlated samples
 * @return 0 on success, -1 on Cholesky failure
 * Complexity: O(n^3 + n^2) for decomposition + multiplication
 */
int fading_generate_correlated_rayleigh(const double *corr_matrix,
                                         size_t num_channels,
                                         double sigma, double *output);

/*============================================================================
 * L2: Fading Channel Parameters
 *============================================================================*/

/**
 * @brief Compute Rician K-factor from LOS and diffuse powers
 * @param p_los_w LOS component power (W)
 * @param p_diffuse_w Diffuse component power (W)
 * @return K (dB) = 10*log10(P_los / P_diffuse)
 * Complexity: O(1)
 */
double fading_compute_k_factor_db(double p_los_w, double p_diffuse_w);

/**
 * @brief Estimate Nakagami m-parameter from measurements
 * Uses the method of moments estimator: m = E^2[X^2] / Var[X^2]
 * @param samples Array of envelope measurements
 * @param num_samples Number of samples
 * @return Estimated m parameter (>= 0.5)
 * Complexity: O(N)
 */
double fading_estimate_m_parameter(const double *samples, size_t num_samples);

/**
 * @brief Convert between Nakagami-m and Rician K-factor (approximation)
 * m approx (1+K)^2 / (1+2*K)  — this maps Rician to approximate Nakagami
 * @param k_factor_db Rician K-factor (dB)
 * @return Equivalent Nakagami m parameter
 * Complexity: O(1)
 */
double fading_k_to_m(double k_factor_db);

#ifdef __cplusplus
}
#endif

#endif /* FADING_H */
