/**
 * @file multipath.c
 * @brief Multipath Channel Model Implementations (L3, L5, L6)
 *
 * Implements Tapped Delay Line (TDL) model, standard 3GPP/LTE power delay
 * profiles, and Rake receiver combining algorithms.
 *
 * Theorem Coverage:
 *   L3: Power Delay Profile moments (mean delay, RMS delay spread)
 *   L5: Tapped Delay Line FIR filter simulation
 *   L5: Frequency-domain channel response (DFT of CIR)
 *   L5: Frequency correlation function
 *   L6: 3GPP TDL-A/B/C profile generation
 *   L6: LTE EPA/EVA/ETU profile generation
 *   L6: Rake MRC/EGC combining
 *
 * Reference:
 *   3GPP TS 36.101 - LTE UE radio transmission and reception
 *   3GPP TR 38.901 - 5G NR channel model
 *   Proakis & Salehi, "Digital Communications", 5th Ed, Ch. 14
 */

#include "multipath.h"
#include "fading.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L3: Power Delay Profile Construction
 *============================================================================*/

power_delay_profile_t *multipath_pdp_alloc(size_t num_taps)
{
    if (num_taps == 0) return NULL;

    power_delay_profile_t *pdp =
        (power_delay_profile_t *)malloc(sizeof(power_delay_profile_t));
    if (!pdp) return NULL;

    pdp->taps = (channel_tap_t *)calloc(num_taps, sizeof(channel_tap_t));
    if (!pdp->taps) {
        free(pdp);
        return NULL;
    }

    pdp->num_taps = num_taps;
    pdp->mean_delay_ns = 0.0;
    pdp->rms_delay_ns = 0.0;
    pdp->max_delay_ns = 0.0;
    pdp->coh_bw_50_hz = 0.0;
    pdp->coh_bw_90_hz = 0.0;

    return pdp;
}

void multipath_pdp_free(power_delay_profile_t *pdp)
{
    if (!pdp) return;
    free(pdp->taps);
    free(pdp);
}

void multipath_pdp_compute_moments(power_delay_profile_t *pdp)
{
    if (!pdp || pdp->num_taps == 0) return;

    double sum_power_linear = 0.0;
    double sum_weighted_delay = 0.0;
    double max_delay = 0.0;

    /* First pass: sum powers (convert dB to linear) and find max delay */
    for (size_t i = 0; i < pdp->num_taps; i++) {
        double power_linear = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        sum_power_linear += power_linear;
        sum_weighted_delay += power_linear * pdp->taps[i].delay_ns;
        if (pdp->taps[i].delay_ns > max_delay) {
            max_delay = pdp->taps[i].delay_ns;
        }
    }

    pdp->max_delay_ns = max_delay;

    if (sum_power_linear < 1e-15) {
        pdp->mean_delay_ns = 0.0;
        pdp->rms_delay_ns = 0.0;
        return;
    }

    /* Mean excess delay (1st moment) */
    pdp->mean_delay_ns = sum_weighted_delay / sum_power_linear;

    /* RMS delay spread (sqrt of 2nd central moment) */
    double sum_weighted_sq = 0.0;
    for (size_t i = 0; i < pdp->num_taps; i++) {
        double power_linear = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        double delay_diff = pdp->taps[i].delay_ns - pdp->mean_delay_ns;
        sum_weighted_sq += power_linear * delay_diff * delay_diff;
    }

    pdp->rms_delay_ns = sqrt(sum_weighted_sq / sum_power_linear);
}

void multipath_pdp_compute_coherence_bandwidth(power_delay_profile_t *pdp)
{
    if (!pdp || pdp->rms_delay_ns <= 0.0) {
        if (pdp) {
            pdp->coh_bw_50_hz = 1e12; /* huge = nearly flat */
            pdp->coh_bw_90_hz = 1e12;
        }
        return;
    }

    /* B_c(50%) = 1/(5 * sigma_tau_in_seconds) */
    double rms_delay_s = pdp->rms_delay_ns * 1e-9;
    pdp->coh_bw_50_hz = 1.0 / (5.0 * rms_delay_s);
    pdp->coh_bw_90_hz = 1.0 / (50.0 * rms_delay_s);
}

void multipath_pdp_normalize(power_delay_profile_t *pdp)
{
    if (!pdp || pdp->num_taps == 0) return;

    double sum_power_linear = 0.0;
    for (size_t i = 0; i < pdp->num_taps; i++) {
        sum_power_linear += pow(10.0, pdp->taps[i].avg_power_db / 10.0);
    }

    if (sum_power_linear < 1e-15) {
        /* Set all taps equal if zero power */
        double eq_power_db = 10.0 * log10(1.0 / (double)pdp->num_taps);
        for (size_t i = 0; i < pdp->num_taps; i++) {
            pdp->taps[i].avg_power_db = eq_power_db;
        }
        return;
    }

    /* Normalize: P_norm_dB = P_dB - 10*log10(sum_p_linear) */
    double norm_factor_db = 10.0 * log10(sum_power_linear);
    for (size_t i = 0; i < pdp->num_taps; i++) {
        pdp->taps[i].avg_power_db -= norm_factor_db;
    }
}

/*============================================================================
 * L6: Standard Channel Model PDP Generation
 *============================================================================*/

/* Helper: set tap values with linear power (auto-converted to dB) */
static void pdp_set_tap(channel_tap_t *tap, double delay_ns, double power_linear,
                         double doppler_hz)
{
    tap->delay_ns = delay_ns;
    tap->avg_power_db = 10.0 * log10(power_linear);
    tap->doppler_shift_hz = doppler_hz;
    tap->gain = 1.0 + 0.0 * I;  /* unit gain, will be overwritten by fading */
}

int multipath_generate_tdl_a(power_delay_profile_t *pdp, int is_los,
                              double delay_scaling)
{
    if (!pdp) return -1;

    size_t num_taps = is_los ? 3 : 4;

    /* Reallocate taps */
    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    if (is_los) {
        /* TDL-A LOS: 3 taps, 10 ns RMS DS */
        pdp_set_tap(&pdp->taps[0], 0.0 * delay_scaling, 0.5012, 0.0);
        pdp_set_tap(&pdp->taps[1], 5.0 * delay_scaling, 0.3162, 0.0);
        pdp_set_tap(&pdp->taps[2], 10.0 * delay_scaling, 0.1826, 0.0);
    } else {
        /* TDL-A NLOS: 4 taps, 10 ns RMS DS (scaled) */
        pdp_set_tap(&pdp->taps[0], 0.0 * delay_scaling, 0.5012, 0.0);
        pdp_set_tap(&pdp->taps[1], 5.0 * delay_scaling, 0.3162, 0.0);
        pdp_set_tap(&pdp->taps[2], 10.0 * delay_scaling, 0.1585, 0.0);
        pdp_set_tap(&pdp->taps[3], 15.0 * delay_scaling, 0.0241, 0.0);
    }

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

int multipath_generate_tdl_b(power_delay_profile_t *pdp, double delay_scaling)
{
    if (!pdp) return -1;
    size_t num_taps = 4;

    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    /* TDL-B: medium delay spread, ~100 ns RMS DS (ITU Vehicular A-like) */
    pdp_set_tap(&pdp->taps[0], 0.0 * delay_scaling, 0.5012, 0.0);
    pdp_set_tap(&pdp->taps[1], 50.0 * delay_scaling, 0.3162, 0.0);
    pdp_set_tap(&pdp->taps[2], 120.0 * delay_scaling, 0.1259, 0.0);
    pdp_set_tap(&pdp->taps[3], 200.0 * delay_scaling, 0.0567, 0.0);

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

int multipath_generate_tdl_c(power_delay_profile_t *pdp, double delay_scaling)
{
    if (!pdp) return -1;
    size_t num_taps = 6;

    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    /* TDL-C: long delay spread, ~300 ns RMS DS */
    pdp_set_tap(&pdp->taps[0], 0.0 * delay_scaling, 0.3162, 0.0);
    pdp_set_tap(&pdp->taps[1], 65.0 * delay_scaling, 0.1995, 0.0);
    pdp_set_tap(&pdp->taps[2], 135.0 * delay_scaling, 0.1259, 0.0);
    pdp_set_tap(&pdp->taps[3], 290.0 * delay_scaling, 0.1995, 0.0);
    pdp_set_tap(&pdp->taps[4], 490.0 * delay_scaling, 0.1259, 0.0);
    pdp_set_tap(&pdp->taps[5], 750.0 * delay_scaling, 0.0330, 0.0);

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

int multipath_generate_epa(power_delay_profile_t *pdp)
{
    if (!pdp) return -1;
    size_t num_taps = 7;

    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    /* LTE EPA: Excess tap delay [ns], Relative power [dB]
     * Tap 0:   0 ns,   0.0 dB
     * Tap 1:  30 ns,  -1.0 dB
     * Tap 2:  70 ns,  -2.0 dB
     * Tap 3:  90 ns,  -3.0 dB
     * Tap 4: 110 ns,  -8.0 dB
     * Tap 5: 190 ns, -17.2 dB
     * Tap 6: 410 ns, -20.8 dB */
    double delays[] = {0.0, 30.0, 70.0, 90.0, 110.0, 190.0, 410.0};
    double powers_db[] = {0.0, -1.0, -2.0, -3.0, -8.0, -17.2, -20.8};

    for (size_t i = 0; i < num_taps; i++) {
        pdp->taps[i].delay_ns = delays[i];
        pdp->taps[i].avg_power_db = powers_db[i];
        pdp->taps[i].doppler_shift_hz = 0.0;
        pdp->taps[i].gain = 1.0 + 0.0 * I;
    }

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

int multipath_generate_eva(power_delay_profile_t *pdp)
{
    if (!pdp) return -1;
    size_t num_taps = 9;

    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    /* LTE EVA: Excess tap delay [ns], Relative power [dB] */
    double delays[] = {0.0, 30.0, 150.0, 310.0, 370.0, 710.0, 1090.0, 1730.0, 2510.0};
    double powers_db[] = {0.0, -1.5, -1.4, -3.6, -0.6, -9.1, -7.0, -12.0, -16.9};

    for (size_t i = 0; i < num_taps; i++) {
        pdp->taps[i].delay_ns = delays[i];
        pdp->taps[i].avg_power_db = powers_db[i];
        pdp->taps[i].doppler_shift_hz = 0.0;
        pdp->taps[i].gain = 1.0 + 0.0 * I;
    }

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

int multipath_generate_etu(power_delay_profile_t *pdp)
{
    if (!pdp) return -1;
    size_t num_taps = 9;

    channel_tap_t *new_taps = (channel_tap_t *)realloc(
        pdp->taps, num_taps * sizeof(channel_tap_t));
    if (!new_taps) return -1;
    pdp->taps = new_taps;
    pdp->num_taps = num_taps;

    /* LTE ETU: Excess tap delay [ns], Relative power [dB] */
    double delays[] = {0.0, 50.0, 120.0, 200.0, 230.0, 500.0, 1600.0, 2300.0, 5000.0};
    double powers_db[] = {-1.0, -1.0, -1.0, 0.0, 0.0, 0.0, -3.0, -5.0, -7.0};

    for (size_t i = 0; i < num_taps; i++) {
        pdp->taps[i].delay_ns = delays[i];
        pdp->taps[i].avg_power_db = powers_db[i];
        pdp->taps[i].doppler_shift_hz = 0.0;
        pdp->taps[i].gain = 1.0 + 0.0 * I;
    }

    multipath_pdp_normalize(pdp);
    multipath_pdp_compute_moments(pdp);
    multipath_pdp_compute_coherence_bandwidth(pdp);
    return 0;
}

/*============================================================================
 * L5: Tapped Delay Line (TDL) Channel Simulation
 *============================================================================*/

struct multipath_tdl_s {
    size_t           num_taps;
    double          *tap_delays_ns;     /**< Delays in ns */
    double          *tap_power_linear;  /**< Linear power per tap */
    double complex  *tap_coefficients;  /**< Current complex tap gains */
    double complex  *delay_buffer;      /**< Ring buffer for input samples */
    size_t           buffer_length;     /**< Length of ring buffer */
    size_t           buffer_pos;        /**< Current write position */
    void            *fading_gen;        /**< Jakes fading generator per tap */
    double           sample_rate_hz;
    double           carrier_freq_hz;
    double           velocity_ms;
};

multipath_tdl_t *multipath_tdl_init(const power_delay_profile_t *pdp,
                                     double sample_rate_hz,
                                     double carrier_freq_hz,
                                     double velocity_ms)
{
    if (!pdp || pdp->num_taps == 0 || sample_rate_hz <= 0.0 ||
        carrier_freq_hz <= 0.0) {
        return NULL;
    }

    multipath_tdl_t *tdl = (multipath_tdl_t *)malloc(sizeof(multipath_tdl_t));
    if (!tdl) return NULL;

    tdl->num_taps = pdp->num_taps;
    tdl->sample_rate_hz = sample_rate_hz;
    tdl->carrier_freq_hz = carrier_freq_hz;
    tdl->velocity_ms = velocity_ms;

    /* Allocate arrays */
    tdl->tap_delays_ns = (double *)malloc(pdp->num_taps * sizeof(double));
    tdl->tap_power_linear = (double *)malloc(pdp->num_taps * sizeof(double));
    tdl->tap_coefficients = (double complex *)malloc(
        pdp->num_taps * sizeof(double complex));

    if (!tdl->tap_delays_ns || !tdl->tap_power_linear || !tdl->tap_coefficients) {
        free(tdl->tap_delays_ns);
        free(tdl->tap_power_linear);
        free(tdl->tap_coefficients);
        free(tdl);
        return NULL;
    }

    /* Compute max delay in samples for ring buffer */
    double max_delay_ns = 0.0;
    for (size_t i = 0; i < pdp->num_taps; i++) {
        tdl->tap_delays_ns[i] = pdp->taps[i].delay_ns;
        tdl->tap_power_linear[i] = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        tdl->tap_coefficients[i] = 1.0 + 0.0 * I;
        if (pdp->taps[i].delay_ns > max_delay_ns) {
            max_delay_ns = pdp->taps[i].delay_ns;
        }
    }

    /* Buffer length: max delay in samples + 1 */
    double max_delay_s = max_delay_ns * 1e-9;
    tdl->buffer_length = (size_t)ceil(max_delay_s * sample_rate_hz) + 2;
    if (tdl->buffer_length < 4) tdl->buffer_length = 4;

    tdl->delay_buffer = (double complex *)calloc(
        tdl->buffer_length, sizeof(double complex));
    if (!tdl->delay_buffer) {
        free(tdl->tap_delays_ns);
        free(tdl->tap_power_linear);
        free(tdl->tap_coefficients);
        free(tdl);
        return NULL;
    }

    tdl->buffer_pos = 0;
    tdl->fading_gen = NULL;

    return tdl;
}

void multipath_tdl_advance_fading(multipath_tdl_t *tdl)
{
    if (!tdl) return;

    /* Generate independent Rayleigh fading for each tap */
    for (size_t i = 0; i < tdl->num_taps; i++) {
        /* Simple: generate i.i.d. complex Gaussian per tap,
         * scaled by sqrt(power). For correlated fading, use Jakes. */
        double power_linear = tdl->tap_power_linear[i];
        double sigma = sqrt(power_linear / 2.0);
        double real_part = sigma * fading_rand_normal();
        double imag_part = sigma * fading_rand_normal();
        tdl->tap_coefficients[i] = real_part + imag_part * I;
    }
}

double complex multipath_tdl_process(multipath_tdl_t *tdl, double complex input)
{
    if (!tdl) return 0.0 + 0.0 * I;

    /* Write input to ring buffer at current position */
    tdl->delay_buffer[tdl->buffer_pos] = input;

    /* Advance fading */
    multipath_tdl_advance_fading(tdl);

    /* Convolve: y = sum h_l * x[n - delay_l] */
    double complex output = 0.0 + 0.0 * I;

    for (size_t i = 0; i < tdl->num_taps; i++) {
        double delay_samples_double = tdl->tap_delays_ns[i] * 1e-9 *
                                       tdl->sample_rate_hz;
        size_t delay_samples = (size_t)delay_samples_double;

        if (delay_samples >= tdl->buffer_length) {
            delay_samples = tdl->buffer_length - 1;
        }

        /* Read from ring buffer at position (pos - delay) mod buffer_length */
        size_t read_pos;
        if (delay_samples <= tdl->buffer_pos) {
            read_pos = tdl->buffer_pos - delay_samples;
        } else {
            read_pos = tdl->buffer_length + tdl->buffer_pos - delay_samples;
        }

        output += tdl->tap_coefficients[i] * tdl->delay_buffer[read_pos];
    }

    /* Advance ring buffer position */
    tdl->buffer_pos = (tdl->buffer_pos + 1) % tdl->buffer_length;

    return output;
}

void multipath_tdl_process_block(multipath_tdl_t *tdl,
                                  const double complex *input,
                                  double complex *output,
                                  size_t num_samples)
{
    if (!tdl || !input || !output) return;

    for (size_t n = 0; n < num_samples; n++) {
        output[n] = multipath_tdl_process(tdl, input[n]);
    }
}

int multipath_tdl_get_cir(const multipath_tdl_t *tdl,
                           channel_impulse_response_t *cir)
{
    if (!tdl || !cir) return -1;

    cir->pdp.num_taps = tdl->num_taps;
    cir->carrier_freq_hz = tdl->carrier_freq_hz;
    cir->bandwidth_hz = tdl->sample_rate_hz;

    /* Copy current tap coefficients as CIR samples */
    cir->num_samples = tdl->num_taps;
    free(cir->cir_samples);
    cir->cir_samples = (double complex *)malloc(
        cir->num_samples * sizeof(double complex));
    if (!cir->cir_samples) return -1;

    for (size_t i = 0; i < tdl->num_taps; i++) {
        cir->cir_samples[i] = tdl->tap_coefficients[i];
    }

    cir->sample_spacing_ns = 1e9 / tdl->sample_rate_hz;
    return 0;
}

void multipath_tdl_reset(multipath_tdl_t *tdl)
{
    if (!tdl) return;
    tdl->buffer_pos = 0;
    memset(tdl->delay_buffer, 0, tdl->buffer_length * sizeof(double complex));
    for (size_t i = 0; i < tdl->num_taps; i++) {
        tdl->tap_coefficients[i] = 1.0 + 0.0 * I;
    }
}

void multipath_tdl_free(multipath_tdl_t *tdl)
{
    if (!tdl) return;
    free(tdl->tap_delays_ns);
    free(tdl->tap_power_linear);
    free(tdl->tap_coefficients);
    free(tdl->delay_buffer);
    if (tdl->fading_gen) fading_jakes_free(tdl->fading_gen);
    free(tdl);
}

/*============================================================================
 * L5: Frequency-Domain Channel Response
 *============================================================================*/

int multipath_freq_response(const double complex *cir_samples,
                             size_t num_samples,
                             double complex *freq_response,
                             size_t num_subcarriers)
{
    if (!cir_samples || !freq_response || num_samples == 0 ||
        num_subcarriers == 0) {
        return -1;
    }

    /* H[k] = sum_{n=0}^{N-1} h[n] * exp(-j*2*pi*k*n/N_dft)
     * where N_dft is the DFT size (>= num_subcarriers) */
    size_t n_dft = num_subcarriers;

    for (size_t k = 0; k < num_subcarriers; k++) {
        double complex h_k = 0.0 + 0.0 * I;
        for (size_t n = 0; n < num_samples; n++) {
            double angle = -2.0 * M_PI * (double)k * (double)n / (double)n_dft;
            h_k += cir_samples[n] * (cos(angle) + sin(angle) * I);
        }
        freq_response[k] = h_k;
    }

    return 0;
}

int multipath_freq_correlation(const double complex *freq_response,
                                size_t num_subcarriers,
                                double *correlation,
                                size_t max_delta)
{
    if (!freq_response || !correlation || num_subcarriers == 0) {
        return -1;
    }

    for (size_t delta = 0; delta <= max_delta; delta++) {
        double sum_corr = 0.0;
        size_t count = 0;

        for (size_t k = 0; k + delta < num_subcarriers; k++) {
            /* R_H[delta] = conj(H[k]) * H[k+delta] */
            double complex prod = conj(freq_response[k]) *
                                  freq_response[k + delta];
            sum_corr += creal(prod); /* correlation of real parts */
            count++;
        }

        if (count > 0) {
            correlation[delta] = sum_corr / (double)count;
        } else {
            correlation[delta] = 0.0;
        }
    }

    return 0;
}

double complex multipath_transfer_function(const power_delay_profile_t *pdp,
                                            double freq_hz)
{
    if (!pdp) return 1.0 + 0.0 * I;

    double complex H = 0.0 + 0.0 * I;

    for (size_t i = 0; i < pdp->num_taps; i++) {
        double delay_s = pdp->taps[i].delay_ns * 1e-9;
        double power_linear = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        double amplitude = sqrt(power_linear);
        double phase = -2.0 * M_PI * freq_hz * delay_s;
        H += amplitude * (cos(phase) + sin(phase) * I);
    }

    return H;
}

/*============================================================================
 * L6: Rake Receiver Combining
 *============================================================================*/

void multipath_rake_mrc_weights(const double complex *taps,
                                 size_t num_taps,
                                 double complex *weights)
{
    if (!taps || !weights) return;

    /* MRC: w_l = h_l* (complex conjugate) */
    for (size_t i = 0; i < num_taps; i++) {
        weights[i] = conj(taps[i]);
    }
}

void multipath_rake_egc_weights(const double complex *taps,
                                 size_t num_taps,
                                 double complex *weights)
{
    if (!taps || !weights) return;

    /* EGC: w_l = exp(-j*arg(h_l)) = conj(h_l)/|h_l| */
    for (size_t i = 0; i < num_taps; i++) {
        double mag = cabs(taps[i]);
        if (mag < 1e-15) {
            weights[i] = 1.0 + 0.0 * I;
        } else {
            weights[i] = conj(taps[i]) / mag;
        }
    }
}

double multipath_rake_snr_mrc(const double complex *taps,
                               size_t num_taps,
                               double snr_per_path_dB)
{
    if (!taps || num_taps == 0) return snr_per_path_dB;

    double snr_linear = pow(10.0, snr_per_path_dB / 10.0);
    double sum_power = 0.0;

    for (size_t i = 0; i < num_taps; i++) {
        double power = creal(taps[i] * conj(taps[i]));
        sum_power += power;
    }

    /* MRC combined SNR = sum(|h_l|^2) * SNR_per_path */
    double combined_snr_linear = sum_power * snr_linear;
    if (combined_snr_linear < 1e-15) return -30.0;

    return 10.0 * log10(combined_snr_linear);
}
