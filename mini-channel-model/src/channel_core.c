/**
 * @file channel_core.c
 * @brief Core channel utility functions (L1, L2)
 *
 * Implements the fundamental utility functions declared in channel_defs.h:
 * wavelength computation, dB/linear conversions, noise power, Doppler shift,
 * coherence metrics, and channel classification.
 *
 * Each function implements one distinct knowledge point:
 *   - Wavelength: lambda = c/f (basic EM wave property)
 *   - dB conversion: logarithmic power ratios (fundamental to all RF)
 *   - Noise power: k*T*B*F (thermal noise floor, critical for link budget)
 *   - Doppler shift: v*f_c/c (relative motion effect)
 *   - Coherence time: T_c = 0.423/f_d (channel stationarity time)
 *   - Delay spread: second central moment of PDP (multipath metric)
 *   - Coherence bandwidth: inverse of delay spread (frequency selectivity)
 *   - Channel classification: flat/freq-selective, slow/fast fading
 *
 * Reference: Molisch, "Wireless Communications", 2nd Ed, 2011
 * Reference: Rappaport, "Wireless Communications", Ch. 4-5
 */

#include "channel_defs.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L1: Fundamental EM and RF Computations
 *============================================================================*/

double channel_wavelength(double freq_hz)
{
    if (freq_hz <= 0.0) return -1.0;
    return CHANNEL_C0 / freq_hz;
}

double channel_db_to_linear(double db)
{
    return pow(10.0, db / 10.0);
}

double channel_linear_to_db(double linear)
{
    if (linear <= 0.0) return -200.0;  /* -inf in dB, practical floor */
    return 10.0 * log10(linear);
}

double channel_rx_power_dbm(double tx_power_dbm, double path_loss_db)
{
    return tx_power_dbm - path_loss_db;
}

double channel_noise_power_dbm(double bandwidth_hz, double temperature_k,
                               double noise_figure_db)
{
    /* N_0 = k*T*B (thermal noise in watts)
     * N_dBm = 10*log10(k*T*B*1000) + NF_dB
     *       = 10*log10(k*T*1000) + 10*log10(B) + NF_dB
     *
     * At T=290K: 10*log10(k*T*1000) = -174 dBm/Hz
     * So: N_dBm = -174 + 10*log10(B) + NF_dB  */

    if (bandwidth_hz <= 0.0 || temperature_k <= 0.0) return -200.0;

    /* k*T term in watts/Hz */
    double noise_density_w_per_hz = CHANNEL_K_BOLTZMANN * temperature_k;

    /* Total noise power in watts */
    double noise_power_w = noise_density_w_per_hz * bandwidth_hz;

    /* Convert to dBm */
    double noise_dbm = 10.0 * log10(noise_power_w * 1000.0);

    /* Add noise figure */
    noise_dbm += noise_figure_db;

    return noise_dbm;
}

/*============================================================================
 * L2: Doppler and Coherence Metrics
 *============================================================================*/

double channel_doppler_shift(double velocity_ms, double freq_hz)
{
    if (freq_hz <= 0.0) return 0.0;

    /* f_d = v * f_c / c (maximum Doppler shift for 2D isotropic) */
    return fabs(velocity_ms) * freq_hz / CHANNEL_C0;
}

double channel_coherence_time(double doppler_hz)
{
    if (doppler_hz <= 0.0) return 1e6;  /* effectively infinite */

    /* Conservative rule of thumb: T_c = 0.423 / f_d
     * This corresponds to ~50% autocorrelation. */
    return 0.423 / doppler_hz;
}

/*============================================================================
 * L2: Multipath and Coherence Metrics
 *============================================================================*/

double channel_rms_delay_spread(const power_delay_profile_t *pdp)
{
    if (!pdp || pdp->num_taps == 0) return 0.0;

    /* First moment: mean excess delay */
    double sum_power_linear = 0.0;
    double sum_weighted = 0.0;

    for (size_t i = 0; i < pdp->num_taps; i++) {
        double power_linear = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        sum_power_linear += power_linear;
        sum_weighted += power_linear * pdp->taps[i].delay_ns;
    }

    if (sum_power_linear < 1e-15) return 0.0;

    double mean_delay = sum_weighted / sum_power_linear;

    /* Second central moment: RMS delay spread */
    double sum_weighted_sq = 0.0;
    for (size_t i = 0; i < pdp->num_taps; i++) {
        double power_linear = pow(10.0, pdp->taps[i].avg_power_db / 10.0);
        double dev = pdp->taps[i].delay_ns - mean_delay;
        sum_weighted_sq += power_linear * dev * dev;
    }

    double rms_delay = sqrt(sum_weighted_sq / sum_power_linear);
    return rms_delay;
}

double channel_coherence_bandwidth(double rms_delay_ns, double correlation_level)
{
    if (rms_delay_ns <= 0.0) return 1e12;  /* infinite BW for zero delay */

    double rms_delay_s = rms_delay_ns * 1e-9;

    if (correlation_level >= 0.9) {
        /* Conservative: B_c(90%) = 1 / (50 * sigma_tau) */
        return 1.0 / (50.0 * rms_delay_s);
    } else {
        /* Standard: B_c(50%) = 1 / (5 * sigma_tau) */
        return 1.0 / (5.0 * rms_delay_s);
    }
}

selectivity_type_t channel_classify_selectivity(double signal_bw_hz,
                                                 double coherence_bw_hz)
{
    if (coherence_bw_hz <= 0.0) {
        /* Zero coherence BW means fully correlated = flat */
        return SELECTIVITY_FLAT;
    }

    /* Rule of thumb:
     * Signal BW << Coherence BW => Flat fading
     * Signal BW > Coherence BW  => Frequency-selective
     *
     * Threshold: B_s > B_c/10 => frequency-selective
     * More precisely: B_s > B_c => frequency-selective */
    if (signal_bw_hz > coherence_bw_hz) {
        return SELECTIVITY_FREQ_SELECTIVE;
    } else {
        return SELECTIVITY_FLAT;
    }
}

timevar_type_t channel_classify_timevar(double symbol_duration_s,
                                         double coherence_time_s)
{
    if (coherence_time_s <= 0.0) {
        /* Zero coherence time = infinitely fast fading */
        return TIMEVAR_FAST;
    }

    /* T_s = symbol duration
     * T_c = coherence time
     * T_c >> T_s => slow fading
     * T_c < T_s  => fast fading
     *
     * Practical threshold: T_c > 10*T_s => slow fading */
    if (coherence_time_s > 10.0 * symbol_duration_s) {
        return TIMEVAR_SLOW;
    } else {
        return TIMEVAR_FAST;
    }
}
