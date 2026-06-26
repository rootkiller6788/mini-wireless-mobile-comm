/**
 * @file doppler.c
 * @brief Doppler Spectrum and Time-Varying Channel Implementations (L3, L4, L5)
 *
 * Implements Doppler PSD models, temporal correlation, coherence time,
 * sum-of-sinusoids waveform generation, and LCR/AFD computation.
 *
 * Theorem Coverage:
 *   L3: Jakes/Clarke Doppler PSD for isotropic scattering
 *   L3: Flat (3D) Doppler PSD for indoor/rich scattering
 *   L3: Directional Doppler PSD (von Mises angular distribution)
 *   L4: Clarke's temporal autocorrelation (Bessel J_0)
 *   L4: Coherence time from Doppler spread
 *   L5: Sum-of-sinusoids Doppler fading generator
 *   L5: Level Crossing Rate (LCR) for Rayleigh/Rician
 *   L5: Average Fade Duration (AFD) for Rayleigh/Rician
 *
 * Reference:
 *   Clarke, BSTJ 1968
 *   Jakes, "Microwave Mobile Communications", 1974
 *   Dent, Bottomley, Croft, "Jakes fading model revisited", 1993
 */

#include "doppler.h"
#include "fading.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L3: Doppler PSD Models
 *============================================================================*/

double doppler_jakes_psd(double freq_offset_hz, double f_d_hz,
                          double total_power)
{
    if (f_d_hz <= 0.0 || total_power <= 0.0) return 0.0;

    double abs_f = fabs(freq_offset_hz);

    /* Outside [-f_d, f_d]: PSD = 0 */
    if (abs_f >= f_d_hz) return 0.0;

    /* S(f) = P_r / (pi*f_d*sqrt(1 - (f/f_d)^2)) */
    double ratio = abs_f / f_d_hz;
    double denominator = M_PI * f_d_hz * sqrt(1.0 - ratio * ratio);

    if (denominator < 1e-15) {
        /* Near the edges, approach from interior */
        return total_power / (M_PI * f_d_hz * 1e-7);
    }

    return total_power / denominator;
}

double doppler_flat_psd(double freq_offset_hz, double f_d_hz,
                         double total_power)
{
    if (f_d_hz <= 0.0 || total_power <= 0.0) return 0.0;

    if (fabs(freq_offset_hz) >= f_d_hz) return 0.0;

    /* S(f) = P_r / (2*f_d) for |f| < f_d */
    return total_power / (2.0 * f_d_hz);
}

double doppler_directional_psd(double freq_offset_hz, double f_d_hz,
                                double total_power, double mean_angle_rad,
                                double angular_spread)
{
    (void)mean_angle_rad;  /* reserved for asymmetric scattering model */
    if (f_d_hz <= 0.0 || total_power <= 0.0 || angular_spread <= 0.0) {
        return 0.0;
    }

    double abs_f = fabs(freq_offset_hz);
    if (abs_f >= f_d_hz) return 0.0;

    /* Directional PSD:
     * S(f) = C * exp(kappa * (2*(f/f_d)^2 - 1)) / (pi*f_d*sqrt(1-(f/f_d)^2))
     * where kappa controls angular spread.
     * This is the von Mises angular distribution mapped to Doppler. */

    double ratio = abs_f / f_d_hz;
    double kappa = 1.0 / (angular_spread * angular_spread);

    /* Angular concentration term */
    double ang_term = exp(kappa * (2.0 * ratio * ratio - 1.0));

    double denominator = M_PI * f_d_hz * sqrt(1.0 - ratio * ratio);
    if (denominator < 1e-15) denominator = M_PI * f_d_hz * 1e-7;

    return total_power * ang_term / denominator;
}

/*============================================================================
 * L4: Temporal Correlation and Coherence Time
 *============================================================================*/

double doppler_clarke_acf(double tau_s, double f_d_hz, double sigma_power)
{
    /* Use the fading module's Clarke implementation */
    return fading_clarke_autocorrelation(tau_s, f_d_hz, sigma_power);
}

double doppler_coherence_time_50(double f_d_hz)
{
    /* T_c(50%) = 9/(16*pi*f_d) ~ 0.179/f_d (rigorous)
     * The popular rule of thumb is T_c = 0.423/f_d (more conservative).
     * We use the rigorous formula here. */
    if (f_d_hz <= 0.0) return 1e6;  /* infinite coherence time for static */
    return 9.0 / (16.0 * M_PI * f_d_hz);
}

double doppler_coherence_time_90(double f_d_hz)
{
    /* T_c(90%) = 9/(16*pi*f_d) * sqrt(1 - 0.9^2) = 0.0812/f_d */
    if (f_d_hz <= 0.0) return 1e6;
    return 0.0812 / f_d_hz;
}

double doppler_freq_correlation(double delta_f_hz, double rms_delay_ns)
{
    /* For WSSUS channel:
     * R(Delta_f) = 1 / (1 + (2*pi*Delta_f*sigma_tau)^2)
     *
     * This is the frequency correlation function for an exponential PDP.
     * Coherence bandwidth B_c satisfies R(B_c) = rho (typ 0.5 or 0.9). */

    if (rms_delay_ns <= 0.0) return 1.0;

    double tau_s = rms_delay_ns * 1e-9;
    double arg = 2.0 * M_PI * delta_f_hz * tau_s;

    /* Exponential PDP: R(Delta_f) = 1 / sqrt(1 + arg^2) */
    double corr = 1.0 / sqrt(1.0 + arg * arg);

    return corr;
}

/*============================================================================
 * L5: Sum-of-Sinusoids Doppler Waveform Generator
 *
 * Implements the modified Jakes method (Dent 1993) for generating
 * time-correlated Rayleigh fading waveforms with the correct Doppler
 * spectrum. Each generator instance maintains oscillator phase state.
 *============================================================================*/

struct doppler_waveform_s {
    size_t   N0;           /**< N/4 oscillators for Jakes method */
    double   f_d;          /**< Maximum Doppler shift (Hz) */
    double   fs;           /**< Sample rate (Hz) */
    double   t;            /**< Current time index (s) */
    double  *phases;       /**< Random initial phases */
    double  *alpha_n;      /**< Angle of arrival parameters */
    double  *doppler_n;    /**< Doppler frequencies per oscillator */
};

doppler_waveform_t *doppler_waveform_init(const doppler_params_t *params)
{
    if (!params || params->num_sinusoids < 4 || params->max_doppler_hz <= 0.0) {
        return NULL;
    }

    size_t N = params->num_sinusoids;
    if (N % 4 != 0) N = (N / 4 + 1) * 4;  /* Must be multiple of 4 */
    size_t N0 = N / 4;

    doppler_waveform_t *gen =
        (doppler_waveform_t *)malloc(sizeof(doppler_waveform_t));
    if (!gen) return NULL;

    gen->N0 = N0;
    gen->f_d = params->max_doppler_hz;
    gen->fs = params->psd_resolution_hz > 0 ?
              params->psd_resolution_hz : params->max_doppler_hz * 10.0;
    gen->t = 0.0;

    gen->phases = (double *)malloc((N0 + 1) * sizeof(double));
    gen->alpha_n = (double *)malloc((N0 + 1) * sizeof(double));
    gen->doppler_n = (double *)malloc((N0 + 1) * sizeof(double));

    if (!gen->phases || !gen->alpha_n || !gen->doppler_n) {
        free(gen->phases);
        free(gen->alpha_n);
        free(gen->doppler_n);
        free(gen);
        return NULL;
    }

    /* Initialize oscillator parameters */
    for (size_t n = 1; n <= N0; n++) {
        gen->phases[n] = 2.0 * M_PI * fading_rand_uniform();
        gen->doppler_n[n] = gen->f_d *
            cos(2.0 * M_PI * (double)n / (double)(4.0 * (double)N0));
        gen->alpha_n[n] = 2.0 * M_PI * (double)(n - 1) /
                          (double)(4.0 * (double)N0 + 1.0);
    }

    return gen;
}

double complex doppler_waveform_next(doppler_waveform_t *gen)
{
    if (!gen) return 0.0 + 0.0 * I;

    double real_part = 0.0, imag_part = 0.0;
    double scale = 1.0 / sqrt((double)gen->N0);

    for (size_t n = 1; n <= gen->N0; n++) {
        double angle = 2.0 * M_PI * gen->doppler_n[n] * gen->t +
                       gen->phases[n];
        real_part += cos(gen->alpha_n[n]) * cos(angle);
        imag_part += sin(gen->alpha_n[n]) * cos(angle);
    }

    gen->t += 1.0 / gen->fs;

    return scale * (real_part + imag_part * I);
}

void doppler_waveform_block(doppler_waveform_t *gen, double complex *output,
                             size_t num_samples)
{
    if (!gen || !output) return;

    for (size_t i = 0; i < num_samples; i++) {
        output[i] = doppler_waveform_next(gen);
    }
}

double doppler_waveform_time(const doppler_waveform_t *gen)
{
    return gen ? gen->t : 0.0;
}

void doppler_waveform_reset(doppler_waveform_t *gen)
{
    if (!gen) return;
    gen->t = 0.0;
}

void doppler_waveform_free(doppler_waveform_t *gen)
{
    if (!gen) return;
    free(gen->phases);
    free(gen->alpha_n);
    free(gen->doppler_n);
    free(gen);
}

/*============================================================================
 * L5: Level Crossing Rate and Average Fade Duration
 *============================================================================*/

double doppler_lcr_rayleigh(double f_d_hz, double rho)
{
    if (f_d_hz <= 0.0 || rho <= 0.0) return 0.0;

    /* N_rho = sqrt(2*pi) * f_d * rho * exp(-rho^2) */
    double lcr = sqrt(2.0 * M_PI) * f_d_hz * rho * exp(-rho * rho);

    if (lcr < 0.0) lcr = 0.0;
    return lcr;
}

double doppler_afd_rayleigh(double f_d_hz, double rho)
{
    if (f_d_hz <= 0.0 || rho <= 0.0) return INFINITY;

    /* AFD = (exp(rho^2) - 1) / (sqrt(2*pi) * f_d * rho) */
    double denom = sqrt(2.0 * M_PI) * f_d_hz * rho;
    if (denom < 1e-15) return INFINITY;

    double afd = (exp(rho * rho) - 1.0) / denom;

    if (afd < 0.0) afd = 0.0;
    return afd;
}

double doppler_lcr_rician(double f_d_hz, double rho, double k_linear)
{
    if (f_d_hz <= 0.0 || rho <= 0.0) return 0.0;

    /* N_rho = sqrt(2*pi*(K+1)) * f_d * rho
     *         * exp(-K - (K+1)*rho^2)
     *         * I_0(2*rho*sqrt(K*(K+1))) */

    double kp1 = k_linear + 1.0;
    double term1 = sqrt(2.0 * M_PI * kp1) * f_d_hz * rho;
    double term2 = exp(-k_linear - kp1 * rho * rho);

    /* I_0 argument */
    double i0_arg = 2.0 * rho * sqrt(k_linear * kp1);

    /* Use the I_0 approximation from fading module */
    extern double fading_bessel_i0(double x);
    /* Actually, we need to expose this. Let's use inline approximation. */
    double i0_val;
    {
        double ax = fabs(i0_arg);
        double y, result;
        if (ax < 3.75) {
            y = (i0_arg / 3.75);
            y *= y;
            result = 1.0 + y * (3.5156229 +
                      y * (3.0899424 +
                      y * (1.2067492 +
                      y * (0.2659732 +
                      y * (0.0360768 +
                      y * 0.0045813)))));
        } else {
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
        i0_val = result;
    }

    double lcr = term1 * term2 * i0_val;

    if (lcr < 0.0) lcr = 0.0;
    return lcr;
}

double doppler_afd_rician(double f_d_hz, double rho, double k_linear)
{
    if (f_d_hz <= 0.0 || rho <= 0.0) return INFINITY;

    /* AFD = (1 - Q_1(sqrt(2K), rho*sqrt(2K+2))) / LCR_Rician */
    double lcr = doppler_lcr_rician(f_d_hz, rho, k_linear);
    if (lcr < 1e-15) return INFINITY;

    /* P(r < rho) for Rician = 1 - Q_1(sqrt(2K), rho*sqrt(2K+2))
     * Approximate using the Marcum Q computed in fading.c.
     * For simplicity, use numerical evaluation via the LCR relation:
     * AFD = CDF(rho) / LCR where CDF(rho) is approximated. */

    /* For moderate K, approximate CDF(rho)
     * from the relation of AFD*LCR = CDF */
    double cdf_approx = 1.0 - exp(-(k_linear + 1.0) * rho * rho / 2.0);

    double afd = cdf_approx / lcr;
    if (afd < 0.0) afd = 0.0;
    return afd;
}

int doppler_compute_lcr_afd(const fading_params_t *params,
                             double threshold_db,
                             lcr_afd_result_t *result)
{
    if (!params || !result) return -1;

    double f_d = params->doppler_spread_hz;
    double rho = pow(10.0, threshold_db / 20.0);  /* dB to amplitude ratio */

    result->threshold_linear = rho;
    result->threshold_db = threshold_db;

    switch (params->type) {
    case FADING_RAYLEIGH:
        result->lcr_hz = doppler_lcr_rayleigh(f_d, rho);
        result->afd_s = doppler_afd_rayleigh(f_d, rho);
        break;

    case FADING_RICIAN: {
        double k_linear = pow(10.0, params->k_factor_db / 10.0);
        result->lcr_hz = doppler_lcr_rician(f_d, rho, k_linear);
        result->afd_s = doppler_afd_rician(f_d, rho, k_linear);
        break;
    }

    case FADING_NAKAGAMI_M:
        /* Approximation: map Nakagami-m to equivalent Rician K for LCR
         * (not exact but useful as first-order estimate) */
        {
            double m_eff = params->m_parameter;
            if (m_eff > 1.0) {
                double k_approx = (m_eff * sqrt(1.0 - 1.0 / m_eff) /
                                   (m_eff + sqrt(m_eff * m_eff - m_eff)));
                result->lcr_hz = doppler_lcr_rician(f_d, rho, k_approx);
                result->afd_s = doppler_afd_rician(f_d, rho, k_approx);
            } else {
                result->lcr_hz = doppler_lcr_rayleigh(f_d, rho);
                result->afd_s = doppler_afd_rayleigh(f_d, rho);
            }
        }
        break;

    default:
        result->lcr_hz = doppler_lcr_rayleigh(f_d, rho);
        result->afd_s = doppler_afd_rayleigh(f_d, rho);
        break;
    }

    return 0;
}
