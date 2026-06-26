/**
 * nr_phy_channel.c — 5G NR Channel Models & Estimation
 *
 * Implements 3GPP TR 38.901 channel models:
 *   TDL-A/B/C/D/E profiles, Jakes Doppler, DMRS-based LS/MMSE estimation,
 *   path loss models (Friis, UMa, UMi, RMa).
 */

#include "nr_phy_channel.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5: TDL Channel Model (3GPP TR 38.901 Table 7.7.2)
 * ============================================================================ */

/* TDL-A: Short delay spread, 30 ns RMS */
static const nr_tap_t tdl_a_taps[] = {
    {0.0,    -13.4, 0, 0.0},
    {2.8543,  0.0,   0, 0.0},
    {5.7086, -2.2,   0, 0.0},
    {8.5629, -4.0,   0, 0.0},
    {11.4172, -6.0,  0, 0.0},
    {14.2715, -8.2,  0, 0.0},
    {17.1258, -9.9,  0, 0.0},
    {19.9801, -10.5, 0, 0.0},
    {22.8344, -7.5,  0, 0.0},
    {25.6887, -15.9, 0, 0.0},
    {28.5430, -6.6,  0, 0.0},
    {31.3973, -16.7, 0, 0.0},
    {34.2516, -12.4, 0, 0.0}
};
#define TDL_A_NUM_TAPS 13

/* TDL-B: Medium delay spread, 100 ns RMS */
static const nr_tap_t tdl_b_taps[] = {
    {0.0,     0.0,   0, 0.0},
    {2.0,    -2.2,   0, 0.0},
    {5.8,    -1.0,   0, 0.0},
    {8.7,    -3.4,   0, 0.0},
    {11.5,   -5.2,   0, 0.0},
    {14.3,   -7.6,   0, 0.0},
    {17.1,   -7.0,   0, 0.0},
    {19.9,   -10.5,  0, 0.0},
    {23.3,   -11.6,  0, 0.0},
    {26.7,   -12.7,  0, 0.0},
    {30.1,   -15.0,  0, 0.0},
    {33.5,   -16.2,  0, 0.0},
    {36.9,   -17.6,  0, 0.0},
    {40.3,   -18.8,  0, 0.0},
    {43.7,   -20.6,  0, 0.0},
    {47.1,   -22.5,  0, 0.0},
    {50.5,   -24.2,  0, 0.0},
    {53.9,   -26.1,  0, 0.0},
    {57.3,   -28.2,  0, 0.0},
    {60.7,   -29.7,  0, 0.0},
    {64.1,   -32.3,  0, 0.0},
    {67.5,   -33.5,  0, 0.0},
    {70.9,   -36.3,  0, 0.0}
};
#define TDL_B_NUM_TAPS 23

/* TDL-C: Long delay spread, 300 ns RMS */
static const nr_tap_t tdl_c_taps[] = {
    {0.0,    -4.4,   0, 0.0},
    {2.0,    -1.9,   0, 0.0},
    {5.8,    -0.8,   0, 0.0},
    {8.7,    -2.6,   0, 0.0},
    {11.5,   -4.2,   0, 0.0},
    {14.3,   -5.4,   0, 0.0},
    {17.1,   -8.1,   0, 0.0},
    {19.9,   -10.4,  0, 0.0},
    {22.8,   -13.5,  0, 0.0},
    {25.7,   -15.4,  0, 0.0},
    {28.6,   -18.3,  0, 0.0},
    {31.5,   -19.8,  0, 0.0},
    {34.4,   -22.4,  0, 0.0},
    {37.3,   -25.1,  0, 0.0},
    {40.2,   -27.2,  0, 0.0},
    {43.1,   -29.8,  0, 0.0},
    {46.0,   -31.6,  0, 0.0},
    {48.9,   -33.7,  0, 0.0},
    {51.8,   -35.9,  0, 0.0},
    {54.7,   -38.2,  0, 0.0},
    {57.6,   -40.1,  0, 0.0},
    {60.5,   -42.3,  0, 0.0},
    {63.4,   -44.1,  0, 0.0},
    {66.3,   -46.2,  0, 0.0}
};
#define TDL_C_NUM_TAPS 24

/* Simple PRNG for Doppler/Jakes generation */
static unsigned int tdl_rand_state = 12345;

static double tdl_rand_gaussian(void)
{
    /* Box-Muller transform */
    double u1 = (double)(tdl_rand_state = tdl_rand_state * 1103515245 + 12345)
                / (double)0xFFFFFFFF;
    double u2 = (double)(tdl_rand_state = tdl_rand_state * 1103515245 + 12345)
                / (double)0xFFFFFFFF;
    /* Avoid log(0) */
    if (u1 < 1.0e-12) u1 = 1.0e-12;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

void nr_tdl_init(nr_tdl_channel_t *chan, char profile,
                  double ds_ns, double fc_ghz,
                  double speed_kmh, int is_los)
{
    if (!chan) return;
    memset(chan, 0, sizeof(*chan));

    chan->profile = profile;
    chan->carrier_freq_ghz = fc_ghz;
    chan->ue_speed_kmh = speed_kmh;
    chan->is_los = is_los;

    /* Select profile taps */
    const nr_tap_t *src_taps = NULL;
    int src_count = 0;

    switch (profile) {
        case 'A': case 'a':
            src_taps = tdl_a_taps; src_count = TDL_A_NUM_TAPS;
            chan->delay_spread_ns = (ds_ns > 0) ? ds_ns : 30.0;
            break;
        case 'B': case 'b':
            src_taps = tdl_b_taps; src_count = TDL_B_NUM_TAPS;
            chan->delay_spread_ns = (ds_ns > 0) ? ds_ns : 100.0;
            break;
        case 'C': case 'c':
            src_taps = tdl_c_taps; src_count = TDL_C_NUM_TAPS;
            chan->delay_spread_ns = (ds_ns > 0) ? ds_ns : 300.0;
            break;
        case 'D': case 'd':
            /* TDL-D: very long, 1000 ns, use scaled TDL-C */
            src_taps = tdl_c_taps; src_count = TDL_C_NUM_TAPS;
            chan->delay_spread_ns = (ds_ns > 0) ? ds_ns : 1000.0;
            break;
        case 'E': case 'e':
            /* TDL-E: ultra long, 3000 ns */
            src_taps = tdl_c_taps; src_count = TDL_C_NUM_TAPS;
            chan->delay_spread_ns = (ds_ns > 0) ? ds_ns : 3000.0;
            break;
        default:
            src_taps = tdl_a_taps; src_count = TDL_A_NUM_TAPS;
            chan->delay_spread_ns = 30.0;
            break;
    }

    /* Scale delays to desired delay spread */
    double default_ds = 0.0;
    if (profile == 'A' || profile == 'a') default_ds = 30.0;
    else if (profile == 'B' || profile == 'b') default_ds = 100.0;
    else default_ds = 300.0;

    double scale = chan->delay_spread_ns / default_ds;

    chan->num_taps = src_count;
    if (chan->num_taps > NR_TDL_MAX_TAPS)
        chan->num_taps = NR_TDL_MAX_TAPS;

    for (int i = 0; i < chan->num_taps; i++) {
        chan->taps[i].delay_ns = src_taps[i].delay_ns * scale;
        chan->taps[i].power_db = src_taps[i].power_db;
        chan->taps[i].fading_type = (is_los && i == 0) ? 1.0 : 0.0;
        chan->taps[i].k_factor_db = (is_los && i == 0) ? 7.0 : 0.0;
    }

    /* Doppler shift = v * f_c / c */
    double fc_hz = fc_ghz * 1.0e9;
    double v_ms = speed_kmh / 3.6;
    chan->doppler_shift_hz = v_ms * fc_hz / 299792458.0;
}

/* ============================================================================
 * L5: Jakes Doppler Spectrum Channel Generation
 * ============================================================================ */

void nr_tdl_generate(const nr_tdl_channel_t *chan, double t_sec,
                      nr_complex_t *h)
{
    if (!chan || !h) return;

    for (int i = 0; i < chan->num_taps; i++) {
        /* Linear power */
        double pwr = pow(10.0, chan->taps[i].power_db / 10.0);

        /* Jakes sum-of-sinusoids model for Rayleigh fading */
        double f_d = chan->doppler_shift_hz;
        double re = 0.0, im = 0.0;

        if (chan->taps[i].fading_type == 0.0) {
            /* Rayleigh: 8 sinusoids with equal Doppler spread */
            const int N_osc = 8;
            for (int k = 0; k < N_osc; k++) {
                double phi = 2.0 * M_PI * (double)(k + i) / (double)(N_osc + 1);
                double f_k = f_d * cos(2.0 * M_PI * (double)k / (double)N_osc);
                double theta = 2.0 * M_PI * (double)((k * 131 + i * 37) % 997)
                               / 997.0;
                double arg = 2.0 * M_PI * f_k * t_sec + theta;
                re += cos(arg) * cos(phi);
                im += sin(arg) * sin(phi);
            }
            re /= sqrt((double)N_osc / 2.0);
            im /= sqrt((double)N_osc / 2.0);
        } else if (chan->taps[i].fading_type == 1.0) {
            /* Rician: LOS component + Rayleigh scatter */
            double K_lin = pow(10.0, chan->taps[i].k_factor_db / 10.0);
            /* LOS component (frequency shift due to Doppler) */
            double los_re = sqrt(K_lin / (K_lin + 1.0)) * cos(2.0 * M_PI * f_d * t_sec);
            double los_im = sqrt(K_lin / (K_lin + 1.0)) * sin(2.0 * M_PI * f_d * t_sec);
            /* Scattered component (Rayleigh) */
            double scat_re = tdl_rand_gaussian() / sqrt(2.0);
            double scat_im = tdl_rand_gaussian() / sqrt(2.0);
            scat_re /= sqrt(K_lin + 1.0);
            scat_im /= sqrt(K_lin + 1.0);
            re = los_re + scat_re;
            im = los_im + scat_im;
        } else {
            /* AWGN: constant channel */
            re = 1.0; im = 0.0;
        }

        double scale = sqrt(pwr);
        h[i].re = re * scale;
        h[i].im = im * scale;
    }
}

/* ============================================================================
 * L5: Channel Application (Convolution)
 * ============================================================================ */

int nr_tdl_apply(const nr_tdl_channel_t *chan,
                  const nr_complex_t *x, int n_in,
                  const nr_complex_t *noise,
                  nr_complex_t *y)
{
    if (!chan || !x || !y || n_in <= 0) return -1;

    /* Convert tap delays to sample indices */
    /* Assume unit sampling period (1 sample ≈ 1 / sampling_rate) */
    /* For simplicity, use nanosecond delays as sample offsets */
    int max_delay_samples = 0;
    int *delays = (int *)malloc(chan->num_taps * sizeof(int));
    if (!delays) return -1;

    for (int i = 0; i < chan->num_taps; i++) {
        delays[i] = (int)(chan->taps[i].delay_ns * 0.01536); /* rough: ns to samples at 30.72 MHz */
        if (delays[i] > max_delay_samples) max_delay_samples = delays[i];
    }

    int n_out = n_in + max_delay_samples;

    /* Initialize output to zero */
    memset(y, 0, n_out * sizeof(nr_complex_t));

    /* Convolution: y[n] = sum_k h_k * x[n - d_k] */
    for (int tap = 0; tap < chan->num_taps; tap++) {
        nr_complex_t h_tap = {0}; /* Will be filled if needed */
        /* For static channel, use the tap power */
        double pwr_amp = sqrt(pow(10.0, chan->taps[tap].power_db / 10.0));
        h_tap.re = pwr_amp;
        h_tap.im = 0.0;

        for (int n = 0; n < n_in; n++) {
            int out_idx = n + delays[tap];
            if (out_idx < n_out) {
                nr_complex_t contrib = nr_complex_mul(h_tap, x[n]);
                y[out_idx] = nr_complex_add(y[out_idx], contrib);
            }
        }
    }

    /* Add noise if provided */
    if (noise) {
        for (int n = 0; n < n_out; n++) {
            y[n] = nr_complex_add(y[n], noise[n]);
        }
    }

    free(delays);
    return n_out;
}

/* ============================================================================
 * L5: Frequency Response Computation
 * ============================================================================ */

void nr_tdl_freq_response(const nr_tdl_channel_t *chan,
                           double t_sec,
                           double sc_spacing, int num_sc,
                           double f_offset,
                           nr_complex_t *h_freq)
{
    if (!chan || !h_freq || num_sc <= 0) return;

    nr_complex_t *tap_h = (nr_complex_t *)malloc(chan->num_taps
                                                  * sizeof(nr_complex_t));
    if (!tap_h) return;

    nr_tdl_generate(chan, t_sec, tap_h);

    for (int sc = 0; sc < num_sc; sc++) {
        double f = f_offset + sc * sc_spacing;
        nr_complex_t sum = nr_complex_make(0.0, 0.0);

        for (int tap = 0; tap < chan->num_taps; tap++) {
            double phase = -2.0 * M_PI * f * chan->taps[tap].delay_ns * 1.0e-9;
            nr_complex_t e_phase = nr_complex_expj(phase);
            nr_complex_t term = nr_complex_mul(tap_h[tap], e_phase);
            sum = nr_complex_add(sum, term);
        }
        h_freq[sc] = sum;
    }

    free(tap_h);
}

/* ============================================================================
 * L5: Channel Estimation — LS & MMSE
 * ============================================================================ */

void nr_chan_est_init(nr_chan_est_ctx_t *ctx,
                       int num_sc, int num_sym,
                       int dmrs_sc_sp, int dmrs_sym_sp)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->num_sc = num_sc;
    ctx->num_sym = num_sym;
    ctx->dmrs_sc_spacing = dmrs_sc_sp;
    ctx->dmrs_sym_spacing = dmrs_sym_sp;
    ctx->estimates = (nr_chan_est_t *)calloc(num_sc * num_sym,
                                              sizeof(nr_chan_est_t));
}

void nr_chan_est_ls(nr_chan_est_ctx_t *ctx,
                     const nr_complex_t *dmrs_tx,
                     const nr_complex_t *dmrs_rx,
                     int num_dmrs)
{
    if (!ctx || !dmrs_tx || !dmrs_rx || num_dmrs <= 0) return;

    /* LS: H_hat[k] = Y[k] / X[k] */
    for (int i = 0; i < num_dmrs; i++) {
        double denom = nr_complex_abs_sq(dmrs_tx[i]);
        if (denom < 1.0e-12) continue;

        /* H = Y * conj(X) / |X|^2 */
        nr_complex_t conj_x = nr_complex_conj(dmrs_tx[i]);
        nr_complex_t h_est = nr_complex_mul(dmrs_rx[i], conj_x);
        h_est.re /= denom;
        h_est.im /= denom;

        /* Store at appropriate position in the grid */
        int sc_idx = i % ctx->num_sc;
        int sym_idx = (i / ctx->num_sc) % ctx->num_sym;
        int grid_idx = sc_idx * ctx->num_sym + sym_idx;
        if (grid_idx < ctx->num_sc * ctx->num_sym) {
            ctx->estimates[grid_idx].h = h_est;
            ctx->estimates[grid_idx].noise_var = 1.0;
        }
    }
}

void nr_chan_est_mmse(nr_chan_est_ctx_t *ctx,
                       const nr_complex_t *dmrs_tx,
                       const nr_complex_t *dmrs_rx,
                       int num_dmrs,
                       double delay_spread_ns,
                       double noise_power)
{
    (void)delay_spread_ns; /* Reserved for frequency-correlated MMSE extension */
    if (!ctx || !dmrs_tx || !dmrs_rx || num_dmrs <= 0) return;
    if (noise_power <= 0.0) {
        /* Fall back to LS if noise power invalid */
        nr_chan_est_ls(ctx, dmrs_tx, dmrs_rx, num_dmrs);
        return;
    }

    ctx->noise_power = noise_power;

    /* Frequency correlation: R_HH(f1,f2) depends on delay spread */
    /* Assumption: exponential PDP → R_HH[delta_sc] = exp(-2*pi*ds*delta_f) */

    /* For each DMRS position, compute frequency-domain MMSE weighting.
     * The frequency correlation depends on subcarrier spacing and
     * delay spread: R_HH(delta_f) = exp(-2*pi*ds*delta_f).
     * Currently using a simplified scalar MMSE model. */

    for (int i = 0; i < num_dmrs; i++) {
        /* LS estimate at this RE */
        double denom = nr_complex_abs_sq(dmrs_tx[i]);
        if (denom < 1.0e-12) continue;

        nr_complex_t conj_x = nr_complex_conj(dmrs_tx[i]);
        nr_complex_t h_ls = nr_complex_mul(dmrs_rx[i], conj_x);
        h_ls.re /= denom;
        h_ls.im /= denom;

        /* MMSE scalar: w = R_HH / (R_HH + sigma^2) */
        /* For self-correlation, R_HH(0) = 1 (normalized) */
        double r_hh = 1.0;
        double w = r_hh / (r_hh + noise_power);

        /* Smooth neighbors if other DMRS exist (frequency-domain correlation) */
        int sc_idx = i % ctx->num_sc;
        int sym_idx = (i / ctx->num_sc) % ctx->num_sym;
        int grid_idx = sc_idx * ctx->num_sym + sym_idx;

        if (grid_idx < ctx->num_sc * ctx->num_sym) {
            ctx->estimates[grid_idx].h.re = h_ls.re * w;
            ctx->estimates[grid_idx].h.im = h_ls.im * w;
            ctx->estimates[grid_idx].noise_var = noise_power;
            /* SINR estimate */
            double sinr_lin = (1.0 / noise_power) * r_hh;
            ctx->estimates[grid_idx].sinr_db = 10.0 * log10(sinr_lin > 0 ? sinr_lin : 0.001);
        }
    }
}

void nr_chan_est_interpolate(nr_chan_est_ctx_t *ctx)
{
    if (!ctx || !ctx->estimates) return;

    /* Linear interpolation in frequency then in time */
    /* Frequency interpolation along each symbol */
    for (int sym = 0; sym < ctx->num_sym; sym++) {
        int last_valid_sc = -1;

        for (int sc = 0; sc < ctx->num_sc; sc++) {
            int idx = sc * ctx->num_sym + sym;
            if (nr_complex_abs_sq(ctx->estimates[idx].h) > 0.0) {
                if (last_valid_sc >= 0 && (sc - last_valid_sc) > 1) {
                    /* Interpolate between last_valid_sc and sc */
                    nr_complex_t a = ctx->estimates[last_valid_sc * ctx->num_sym + sym].h;
                    nr_complex_t b = ctx->estimates[idx].h;
                    int gap = sc - last_valid_sc;
                    for (int k = 1; k < gap; k++) {
                        double alpha = (double)k / (double)gap;
                        int mid_idx = (last_valid_sc + k) * ctx->num_sym + sym;
                        ctx->estimates[mid_idx].h.re = a.re * (1.0 - alpha) + b.re * alpha;
                        ctx->estimates[mid_idx].h.im = a.im * (1.0 - alpha) + b.im * alpha;
                    }
                }
                last_valid_sc = sc;
            }
        }
    }
}

nr_chan_est_t nr_chan_est_get(const nr_chan_est_ctx_t *ctx,
                                int sc_idx, int sym_idx)
{
    nr_chan_est_t empty = {{0.0, 0.0}, 1.0, -20.0};
    if (!ctx || !ctx->estimates) return empty;
    if (sc_idx < 0 || sc_idx >= ctx->num_sc) return empty;
    if (sym_idx < 0 || sym_idx >= ctx->num_sym) return empty;

    return ctx->estimates[sc_idx * ctx->num_sym + sym_idx];
}

void nr_chan_est_free(nr_chan_est_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx->estimates);
    ctx->estimates = NULL;
}

/* ============================================================================
 * L4: Path Loss Models (3GPP TR 38.901 Section 7.4)
 * ============================================================================ */

double nr_pathloss_db(nr_pathloss_model_t model, double d_m,
                       double fc_ghz, double h_bs_m, double h_ue_m,
                       int is_los)
{
    if (d_m <= 0.0 || fc_ghz <= 0.0) return 0.0;

    double pl_db = 0.0;

    switch (model) {
        case NR_PATHLOSS_FREE_SPACE: {
            /* Friis: PL = 20*log10(4*pi*d/lambda) */
            double lambda = 299792458.0 / (fc_ghz * 1.0e9);
            pl_db = 20.0 * log10(4.0 * M_PI * d_m / lambda);
            break;
        }
        case NR_PATHLOSS_UMA: {
            /* Urban Macro per TR 38.901 Table 7.4.1-1 */
            double d_bp = 4.0 * (h_bs_m - 1.0) * (h_ue_m - 1.0) * fc_ghz * 1.0e9
                          / 299792458.0;
            if (d_bp < 10.0) d_bp = 10.0;

            if (is_los) {
                if (d_m < d_bp) {
                    pl_db = 28.0 + 22.0 * log10(d_m) + 20.0 * log10(fc_ghz);
                } else {
                    pl_db = 28.0 + 40.0 * log10(d_m) + 20.0 * log10(fc_ghz)
                            - 9.0 * log10(d_bp * d_bp + (h_bs_m - h_ue_m)
                                          * (h_bs_m - h_ue_m));
                }
            } else {
                pl_db = 13.54 + 39.08 * log10(d_m) + 20.0 * log10(fc_ghz)
                        - 0.6 * (h_ue_m - 1.5);
            }
            break;
        }
        case NR_PATHLOSS_UMI: {
            /* Urban Micro per TR 38.901 Table 7.4.1-1 */
            double d_bp = 4.0 * (h_bs_m - 1.0) * (h_ue_m - 1.0) * fc_ghz * 1.0e9
                          / 299792458.0;
            if (d_bp < 10.0) d_bp = 10.0;

            if (is_los) {
                if (d_m < d_bp) {
                    pl_db = 32.4 + 21.0 * log10(d_m) + 20.0 * log10(fc_ghz);
                } else {
                    pl_db = 32.4 + 40.0 * log10(d_m) + 20.0 * log10(fc_ghz)
                            - 9.5 * log10(d_bp * d_bp + (h_bs_m - h_ue_m)
                                          * (h_bs_m - h_ue_m));
                }
            } else {
                pl_db = 35.3 * log10(d_m) + 22.4 + 21.3 * log10(fc_ghz)
                        - 0.3 * (h_ue_m - 1.5);
            }
            break;
        }
        case NR_PATHLOSS_RMA: {
            /* Rural Macro */
            if (is_los) {
                pl_db = 20.0 * log10(40.0 * M_PI * d_m * fc_ghz * 1.0e9
                       / 3.0 / 299792458.0)
                       + 0.0; /* min(0.03*h^1.72, 10)*log10(d) - min(...) */
            } else {
                pl_db = 161.04 - 7.1 * log10(20.0)
                        + 7.5 * log10(h_ue_m)
                        - (24.37 - 3.7 * (h_bs_m / h_ue_m) * (h_bs_m / h_ue_m))
                          * log10(h_bs_m)
                        + (43.42 - 3.1 * log10(h_bs_m)) * (log10(d_m) - 3.0)
                        + 20.0 * log10(fc_ghz);
            }
            break;
        }
        case NR_PATHLOSS_INDOOR: {
            /* Simplified indoor: ITU indoor model */
            pl_db = 20.0 * log10(fc_ghz * 1.0e3)
                    + (is_los ? 12.0 : 18.0) * log10(d_m) + 28.0;
            break;
        }
    }

    return pl_db;
}

double nr_pathloss_inverse(double pl_db, nr_pathloss_model_t model,
                            double fc_ghz, double h_bs_m, double h_ue_m,
                            int is_los)
{
    /* Simple iterative search for inverse */
    double d_lo = 0.1;
    double d_hi = 100000.0;

    for (int iter = 0; iter < 50; iter++) {
        double d_mid = (d_lo + d_hi) / 2.0;
        double pl_mid = nr_pathloss_db(model, d_mid, fc_ghz,
                                        h_bs_m, h_ue_m, is_los);
        if (pl_mid < pl_db) {
            d_lo = d_mid;
        } else {
            d_hi = d_mid;
        }
        if ((d_hi - d_lo) < 0.01) break;
    }

    return (d_lo + d_hi) / 2.0;
}

double nr_shadow_fading_db(double sigma_db, unsigned int *seed)
{
    /* Box-Muller */
    unsigned int s = seed ? *seed : 1;
    s = s * 1103515245 + 12345;
    double u1 = (double)(s & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    s = s * 1103515245 + 12345;
    double u2 = (double)(s & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    if (seed) *seed = s;

    if (u1 < 1.0e-12) u1 = 1.0e-12;
    return sigma_db * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ============================================================================
 * L4: SNR and Capacity
 * ============================================================================ */

double nr_snr_dbm(double p_tx_dbm, double pl_db,
                   double g_tx_db, double g_rx_db,
                   double bw_hz, double nf_db,
                   double *snr_lin)
{
    /* Thermal noise: -174 dBm/Hz at 290K */
    double noise_floor_dbm = -174.0 + 10.0 * log10(bw_hz) + nf_db;
    double rx_power_dbm = p_tx_dbm - pl_db + g_tx_db + g_rx_db;
    double snr_db = rx_power_dbm - noise_floor_dbm;

    if (snr_lin) *snr_lin = pow(10.0, snr_db / 10.0);
    return snr_db;
}

double nr_channel_capacity(double bw_hz, double snr_linear)
{
    if (bw_hz <= 0.0 || snr_linear <= 0.0) return 0.0;
    return bw_hz * log2(1.0 + snr_linear);
}