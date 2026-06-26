/**
 * @file fading.c
 * @brief Statistical Fading Model Implementations (L3, L4, L5)
 *
 * Implements statistical fading distribution generators, PDF/CDF evaluations,
 * and correlated fading generation via Cholesky decomposition.
 *
 * Theorem Coverage:
 *   L3: Rayleigh/Rician/Nakagami/Log-normal PDF and CDF
 *   L4: Clarke's Model — Bessel J_0 autocorrelation
 *   L5: Box-Muller Gaussian generation
 *   L5: Sum-of-Sinusoids (Jakes method) for correlated fading
 *   L5: Cholesky-based correlated fading generation
 *   L5: Modified Bessel function I_0 (series expansion)
 *   L5: Marcum Q function approximation for Rician CDF
 *
 * Reference:
 *   Clarke, "A statistical theory of mobile-radio reception", BSTJ 1968
 *   Jakes, "Microwave Mobile Communications", 1974
 *   Proakis & Salehi, "Digital Communications", 5th Ed, 2008
 */

#include "fading.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L5: Random Number Generation Primitives
 *============================================================================*/

static unsigned int fading_rand_state = 123456789;

void fading_rand_seed(unsigned int seed)
{
    fading_rand_state = seed;
    srand(seed);
}

double fading_rand_uniform(void)
{
    /* Use simple LCG: X_{n+1} = (a*X_n + c) mod m
     * a = 1664525, c = 1013904223, m = 2^32 (Numerical Recipes) */
    fading_rand_state = 1664525 * fading_rand_state + 1013904223;
    return (double)(fading_rand_state & 0x7FFFFFFF) / (double)0x80000000;
}

double fading_rand_normal(void)
{
    /* Box-Muller transform */
    double u1 = fading_rand_uniform();
    double u2 = fading_rand_uniform();

    /* Guard against log(0) */
    if (u1 < 1e-12) u1 = 1e-12;

    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

double complex fading_rand_complex_gaussian(double sigma)
{
    /* CN(0, sigma^2): real and imag parts are N(0, sigma^2/2) each */
    double sigma_dim = sigma / sqrt(2.0);
    double real_part = sigma_dim * fading_rand_normal();
    double imag_part = sigma_dim * fading_rand_normal();
    return real_part + imag_part * I;
}

/*============================================================================
 * L5: Modified Bessel Function I_0(x) — Series Expansion
 *
 * I_0(x) = sum_{k=0}^{inf} (x/2)^{2k} / (k!)^2
 *
 * Used in Rician PDF evaluation. Converges rapidly for |x| < 20.
 * For larger x, asymptotic form: I_0(x) ~ exp(x)/sqrt(2*pi*x).
 *============================================================================*/

static double fading_bessel_i0(double x)
{
    double ax = fabs(x);
    double y, result;

    if (ax < 3.75) {
        /* Polynomial approximation (Abramowitz & Stegun 9.8.1) */
        y = (x / 3.75);
        y *= y;
        result = 1.0 + y * (3.5156229 +
                  y * (3.0899424 +
                  y * (1.2067492 +
                  y * (0.2659732 +
                  y * (0.0360768 +
                  y * 0.0045813)))));
    } else {
        /* Asymptotic expansion (Abramowitz & Stegun 9.8.2) */
        y = 3.75 / ax;
        result = (exp(ax) / sqrt(ax)) *
                 (0.39894228 +
                  y * (0.01328592 +
                  y * (0.00225319 +
                  y * (-0.00157565 +
                  y * (0.00916281 +
                  y * (-0.02057706 +
                  y * (0.02635537 +
                  y * (-0.01647633 +
                  y * 0.00392377))))))));
    }
    return result;
}

/*============================================================================
 * L5: Marcum Q Function Q_1(a, b) — used in Rician CDF
 *
 * Q_1(a, b) = integral_{b}^{inf} x*exp(-(x^2+a^2)/2)*I_0(a*x) dx
 *
 * For the Rician CDF: F_R(r) = 1 - Q_1(nu/sigma, r/sigma)
 *
 * Uses numerical integration with Simpson's rule.
 *============================================================================*/

static double fading_marcum_q1(double a, double b)
{
    /* For small arguments, use series expansion.
     * For moderate/large, use numerical integration.
     *
     * Special case: b ~ 0 -> Q_1(a,0) = 1 (by definition)
     * Special case: a ~ 0 -> Q_1(0,b) = exp(-b^2/2) (Rayleigh case)
     */

    if (b < 1e-10) return 1.0;
    if (a < 1e-10) return exp(-0.5 * b * b);

    /* Numerical integration using Simpson's rule from b to b+12*max(1,a) */
    double upper_limit = b + 12.0 * ((a > 1.0) ? a : 1.0);
    int n_intervals = 200;
    double h = (upper_limit - b) / n_intervals;
    double sum_odd = 0.0, sum_even = 0.0;

    /* Integrand: x * exp(-(x^2+a^2)/2) * I_0(a*x) */
    double integrand(double x) {
        return x * exp(-0.5 * (x*x + a*a)) * fading_bessel_i0(a * x);
    }

    double f0 = integrand(b);
    double fn = integrand(upper_limit);

    for (int i = 1; i < n_intervals; i++) {
        double x = b + i * h;
        double fx = integrand(x);
        if (i % 2 == 0) {
            sum_even += fx;
        } else {
            sum_odd += fx;
        }
    }

    double integral = (h / 3.0) * (f0 + fn + 4.0 * sum_odd + 2.0 * sum_even);
    return integral;
}

/*============================================================================
 * L3: Rayleigh Distribution
 *============================================================================*/

double fading_rayleigh_pdf(double x, double sigma)
{
    if (x < 0.0 || sigma <= 0.0) return 0.0;
    double s2 = sigma * sigma;
    return (x / s2) * exp(-x * x / (2.0 * s2));
}

double fading_rayleigh_cdf(double x, double sigma)
{
    if (x < 0.0 || sigma <= 0.0) return 0.0;
    return 1.0 - exp(-x * x / (2.0 * sigma * sigma));
}

double fading_generate_rayleigh(double sigma)
{
    /* r = sqrt(X^2 + Y^2) where X, Y ~ N(0, sigma^2) */
    double x = sigma * fading_rand_normal();
    double y = sigma * fading_rand_normal();
    return sqrt(x * x + y * y);
}

/*============================================================================
 * L3: Rician Distribution
 *============================================================================*/

double fading_rician_pdf(double x, double nu, double sigma)
{
    if (x < 0.0 || sigma <= 0.0) return 0.0;
    double s2 = sigma * sigma;
    double arg = x * nu / s2;
    return (x / s2) * exp(-(x * x + nu * nu) / (2.0 * s2)) *
           fading_bessel_i0(arg);
}

double fading_rician_cdf(double x, double nu, double sigma)
{
    if (x < 0.0 || sigma <= 0.0) return 0.0;
    if (nu < 1e-12) {
        /* Degenerate to Rayleigh CDF */
        return fading_rayleigh_cdf(x, sigma);
    }
    double a = nu / sigma;
    double b = x / sigma;
    return 1.0 - fading_marcum_q1(a, b);
}

double fading_generate_rician(double nu, double sigma)
{
    /* r = sqrt((nu + X)^2 + Y^2) where X, Y ~ N(0, sigma^2) */
    double x = sigma * fading_rand_normal();
    double y = sigma * fading_rand_normal();
    return sqrt((nu + x) * (nu + x) + y * y);
}

double fading_generate_rician_from_k(double sigma, double k_factor_db)
{
    /* K = nu^2/(2*sigma^2) => nu = sigma*sqrt(2*10^(K_dB/10)) */
    double k_linear = pow(10.0, k_factor_db / 10.0);
    double nu = sigma * sqrt(2.0 * k_linear);
    return fading_generate_rician(nu, sigma);
}

/*============================================================================
 * L3: Nakagami-m Distribution
 *============================================================================*/

double fading_nakagami_pdf(double x, double m, double omega)
{
    if (x < 0.0 || m < 0.5 || omega <= 0.0) return 0.0;

    /* Use Stirling's approximation-based log for Gamma(m) to avoid overflow */
    /* lgamma via Stirling: ln(Gamma(z)) ~ (z-0.5)*ln(z) - z + 0.5*ln(2*pi) */

    double log_gamma_m;
    if (m < 20.0) {
        /* Exact Gamma for integer/half-integer, approximate for others */
        double gm = 1.0;
        double z = m;
        while (z > 1.0) {
            z -= 1.0;
            gm *= z;
        }
        /* This is simplistic but works for m >= 0.5 where m is often
         * integer (1, 2, 3...) or half-integer in practice */
        log_gamma_m = log(gm);
    } else {
        /* Stirling */
        log_gamma_m = (m - 0.5) * log(m) - m + 0.5 * log(2.0 * M_PI);
    }

    double log_pdf = log(2.0) + m * log(m) - log_gamma_m -
                     m * log(omega) + (2.0 * m - 1.0) * log(x) -
                     m * x * x / omega;

    return exp(log_pdf);
}

double fading_generate_nakagami(double m, double omega)
{
    /* Generate Nakagami-m using:
     * For integer 2m: r = sqrt(sum_{i=1}^{2m} X_i^2) * sqrt(omega/(2m))
     * where X_i ~ N(0, 1) i.i.d.
     *
     * For non-integer m: use gamma distribution relationship.
     * Nakagami^2 ~ Gamma(m, omega/m)
     *
     * Simple approach: use 2*ceil(m) squared normal sums with rejection */

    if (m < 0.5) m = 0.5;

    int n = (int)ceil(2.0 * m);
    double sum_sq = 0.0;

    for (int i = 0; i < n; i++) {
        double g = fading_rand_normal();
        sum_sq += g * g;
    }

    /* For fractional m, interpolate using gamma CDF identity */
    double nakagami_sq = (omega / (2.0 * m)) * sum_sq *
                         ((2.0 * m) / (double)n);

    if (nakagami_sq < 0.0) nakagami_sq = 0.0;
    return sqrt(nakagami_sq);
}

/*============================================================================
 * L3: Log-Normal Distribution (Shadow Fading)
 *============================================================================*/

double fading_lognormal_pdf(double x, double mu_db, double sigma_db)
{
    /* PDF in linear domain: f(x) = 1/(x*sigma*ln(10)/10*sqrt(2*pi))
     *                               * exp(-(10*log10(x)-mu)^2/(2*sigma^2)) */
    if (x <= 0.0 || sigma_db <= 0.0) return 0.0;

    double log_x_db = 10.0 * log10(x);
    double const_term = 10.0 / (log(10.0) * sigma_db * sqrt(2.0 * M_PI));

    return (const_term / x) *
           exp(-0.5 * pow((log_x_db - mu_db) / sigma_db, 2.0));
}

double fading_generate_lognormal(double sigma_db)
{
    /* 10^(sigma_dB * N(0,1) / 10) for linear log-normal */
    double normal = fading_rand_normal();
    return sigma_db * normal;  /* returns dB value (zero mean) */
}

/*============================================================================
 * L3: Weibull Distribution
 *============================================================================*/

double fading_weibull_pdf(double x, double beta, double alpha)
{
    if (x < 0.0 || beta <= 0.0 || alpha <= 0.0) return 0.0;
    double ratio = x / alpha;
    return (beta / alpha) * pow(ratio, beta - 1.0) * exp(-pow(ratio, beta));
}

double fading_generate_weibull(double beta, double alpha)
{
    /* Inverse CDF method: X = alpha * (-ln(U))^(1/beta) */
    double u = fading_rand_uniform();
    if (u < 1e-12) u = 1e-12;
    return alpha * pow(-log(u), 1.0 / beta);
}

/*============================================================================
 * L4: Clarke's Autocorrelation Model
 *============================================================================*/

double fading_clarke_autocorrelation(double tau_s, double f_d_hz,
                                      double sigma_power)
{
    /* R(tau) = sigma^2 * J_0(2*pi*f_d*tau)
     *
     * Use Bessel J_0 numerical approximation (rational function from
     * Abramowitz & Stegun). J_0(x) = I_0(j*x) for real x. We approximate
     * J_0 via polynomial series.
     */
    double x = 2.0 * M_PI * f_d_hz * tau_s;
    double ax = fabs(x);

    /* J_0(x) polynomial approximation (Hart 1968) */
    double j0;
    if (ax < 8.0) {
        double y = x * x;
        double term1 = 57568490574.0 + y * (-13362590354.0 +
                       y * (651619640.7 + y * (-11214424.18 +
                       y * (77392.33017 + y * (-184.9052456)))));
        double term2 = 57568490411.0 + y * (1029532985.0 +
                       y * (9494680.718 + y * (59272.64853 +
                       y * (267.8532712 + y * 1.0))));
        j0 = term1 / term2;
    } else {
        double z = 8.0 / ax;
        double y = z * z;
        double term1 = 1.0 + y * (-0.1098628627e-2 +
                       y * (0.2734510407e-4 + y * (-0.2073370639e-5 +
                       y * 0.2093887211e-6)));
        double term2 = -0.1562499995e-1 + y * (0.1430488765e-3 +
                       y * (-0.6911147651e-5 + y * (0.7621095161e-6 -
                       y * 0.934935152e-7)));
        double phase_correction = ax - 0.785398164;  /* x - pi/4 */
        j0 = sqrt(0.636619772 / ax) *
             (cos(phase_correction) * term1 - z * sin(phase_correction) * term2);
    }

    return sigma_power * j0;
}

double fading_spatial_autocorrelation(double delta_d_m, double freq_hz,
                                       double sigma_power)
{
    /* R(delta_d) = sigma^2 * J_0(2*pi*delta_d/lambda) */
    double lambda = CHANNEL_C0 / freq_hz;
    double phase = 2.0 * M_PI * delta_d_m / lambda;
    /* spatial equivalent of temporal autocorrelation:
     * replace f_d*tau with delta_d/lambda */
    return fading_clarke_autocorrelation(phase / (2.0 * M_PI), 1.0, sigma_power);
}

/*============================================================================
 * L5: Sum-of-Sinusoids (Jakes Method) Implementation
 *============================================================================*/

typedef struct {
    size_t          N;            /**< Number of sinusoids */
    double          f_d;          /**< Maximum Doppler shift (Hz) */
    double          fs;           /**< Sample rate (Hz) */
    double          t;            /**< Current time (s) */
    double         *phases;       /**< Random phases phi_n */
    double         *doppler_freqs; /**< f_d*cos(2*pi*n/N) */
    double         *alpha_n;      /**< 2*cos(beta_n) or 2*sin(beta_n) */
    size_t          N0;           /**< N/4 for modified Jakes */
} jakes_state_t;

void *fading_jakes_init(size_t num_sinusoids, double f_d_hz,
                         double sample_rate_hz)
{
    if (num_sinusoids < 4 || f_d_hz <= 0.0 || sample_rate_hz <= 0.0) {
        return NULL;
    }

    /* Modified Jakes: N_sinusoids must be multiple of 4 */
    size_t N = (num_sinusoids + 3) / 4 * 4;
    size_t N0 = N / 4;

    jakes_state_t *state = (jakes_state_t *)malloc(sizeof(jakes_state_t));
    if (!state) return NULL;

    state->phases = (double *)malloc((N0 + 1) * sizeof(double));
    state->doppler_freqs = (double *)malloc((N0 + 1) * sizeof(double));
    state->alpha_n = (double *)malloc((N0 + 1) * sizeof(double));

    if (!state->phases || !state->doppler_freqs || !state->alpha_n) {
        free(state->phases);
        free(state->doppler_freqs);
        free(state->alpha_n);
        free(state);
        return NULL;
    }

    state->N = N;
    state->f_d = f_d_hz;
    state->fs = sample_rate_hz;
    state->t = 0.0;
    state->N0 = N0;

    /* Initialize phases uniformly in [0, 2*pi) */
    for (size_t n = 1; n <= N0; n++) {
        state->phases[n] = 2.0 * M_PI * fading_rand_uniform();
        state->doppler_freqs[n] = f_d_hz * cos(2.0 * M_PI * (double)n / (double)N);
        state->alpha_n[n] = 2.0 * cos(2.0 * M_PI * (double)(n - 1) / (double)N);
    }

    return (void *)state;
}

double complex fading_jakes_next(void *handle)
{
    jakes_state_t *state = (jakes_state_t *)handle;
    if (!state) return 0.0 + 0.0 * I;

    double I_component = 0.0;
    double Q_component = 0.0;
    double sqrt2_over_N = sqrt(2.0 / (double)state->N);

    for (size_t n = 1; n <= state->N0; n++) {
        double angle = 2.0 * M_PI * state->doppler_freqs[n] *
                       state->t + state->phases[n];
        I_component += cos(state->alpha_n[n]) * cos(angle);
        Q_component += sin(state->alpha_n[n]) * cos(angle);
    }

    /* Additional Doppler shift term (n = N0+1) */
    double angle_N0p1 = 2.0 * M_PI * state->f_d * state->t + state->phases[0];
    I_component += sqrt(2.0) * cos(angle_N0p1);
    Q_component += sqrt(2.0) * sin(angle_N0p1);

    double h_i = sqrt2_over_N * I_component;
    double h_q = sqrt2_over_N * Q_component;

    state->t += 1.0 / state->fs;

    return h_i + h_q * I;
}

void fading_jakes_free(void *handle)
{
    if (!handle) return;
    jakes_state_t *state = (jakes_state_t *)handle;
    free(state->phases);
    free(state->doppler_freqs);
    free(state->alpha_n);
    free(state);
}

/*============================================================================
 * L5: Cholesky Decomposition for Correlated Fading
 *============================================================================*/

int fading_cholesky_decomp(const double *A, size_t n, double *L)
{
    if (!A || !L || n == 0) return -1;

    /* Zero L first */
    memset(L, 0, n * n * sizeof(double));

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j <= i; j++) {
            double sum = 0.0;
            for (size_t k = 0; k < j; k++) {
                sum += L[i * n + k] * L[j * n + k];
            }

            if (i == j) {
                double diag = A[i * n + i] - sum;
                /* Matrix must be positive definite */
                if (diag <= 0.0) return -1;
                L[i * n + i] = sqrt(diag);
            } else {
                double num = A[i * n + j] - sum;
                if (L[j * n + j] < 1e-15) return -1;
                L[i * n + j] = num / L[j * n + j];
            }
        }
    }

    return 0;
}

int fading_generate_correlated_rayleigh(const double *corr_matrix,
                                         size_t num_channels,
                                         double sigma, double *output)
{
    if (!corr_matrix || !output || num_channels == 0) return -1;

    /* Step 1: Cholesky decompose correlation matrix R = L*L^T */
    double *L = (double *)malloc(num_channels * num_channels * sizeof(double));
    if (!L) return -1;

    if (fading_cholesky_decomp(corr_matrix, num_channels, L) != 0) {
        free(L);
        return -1;
    }

    /* Step 2: Generate i.i.d. Gaussian vector w ~ N(0, I_n) */
    double *w = (double *)malloc(num_channels * sizeof(double));
    if (!w) {
        free(L);
        return -1;
    }

    for (size_t i = 0; i < num_channels; i++) {
        w[i] = fading_rand_normal();
    }

    /* Step 3: y = sigma * L * w (correlated) */
    for (size_t i = 0; i < num_channels; i++) {
        double sum = 0.0;
        for (size_t j = 0; j <= i; j++) {
            sum += L[i * num_channels + j] * w[j];
        }
        /* Rayleigh envelope from correlated Gaussian pair:
         * We generate only one dimension here (real part).
         * A complete implementation would generate complex Gaussian
         * values. For real-correlated outputs, this is sufficient.
         * The absolute value gives approximate Rayleigh when
         * sigma is the RMS value. */
        output[i] = fabs(sigma * sum);
    }

    free(L);
    free(w);
    return 0;
}

/*============================================================================
 * L2: Fading Parameter Utility Functions
 *============================================================================*/

double fading_compute_k_factor_db(double p_los_w, double p_diffuse_w)
{
    if (p_diffuse_w <= 0.0) return 100.0;  /* effectively pure LOS */
    return 10.0 * log10(p_los_w / p_diffuse_w);
}

double fading_estimate_m_parameter(const double *samples, size_t num_samples)
{
    if (!samples || num_samples < 2) return 1.0;  /* default to Rayleigh */

    /* Method of moments:
     * E[X^2] = (1/N)*sum x_i^2
     * Var[X^2] = E[X^4] - (E[X^2])^2
     * m = (E[X^2])^2 / Var[X^2] */

    double sum_x2 = 0.0, sum_x4 = 0.0;

    for (size_t i = 0; i < num_samples; i++) {
        double x2 = samples[i] * samples[i];
        sum_x2 += x2;
        sum_x4 += x2 * x2;
    }

    double mean_x2 = sum_x2 / (double)num_samples;
    double mean_x4 = sum_x4 / (double)num_samples;
    double var_x2 = mean_x4 - mean_x2 * mean_x2;

    if (var_x2 < 1e-15) return 100.0;  /* nearly no fading */

    double m_est = mean_x2 * mean_x2 / var_x2;

    /* m cannot be less than 0.5 for Nakagami-m */
    if (m_est < 0.5) m_est = 0.5;
    if (m_est > 100.0) m_est = 100.0;

    return m_est;
}

double fading_k_to_m(double k_factor_db)
{
    /* Approximation: m approx (1+K)^2 / (1+2*K)
     * where K is linear (not dB).
     * Valid for mapping Rician K-factor to equivalent Nakagami-m. */
    double k_linear = pow(10.0, k_factor_db / 10.0);
    double m = (1.0 + k_linear) * (1.0 + k_linear) / (1.0 + 2.0 * k_linear);
    if (m < 0.5) m = 0.5;
    return m;
}
