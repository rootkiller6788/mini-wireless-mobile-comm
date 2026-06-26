/**
 * nr_phy_ofdm.c — 5G NR OFDM Modulation / Demodulation Implementation
 *
 * Implements 3GPP TS 38.211 Section 5.3:
 *   - CP-OFDM modulator/demodulator with IFFT/FFT
 *   - DFT-s-OFDM for uplink
 *   - Cyclic prefix insertion/removal
 *   - Resource element mapping
 *
 * Key mathematical engines:
 *   - Radix-2 decimation-in-time FFT (Cooley-Tukey 1965)
 *   - DFT spreading for SC-FDMA waveform
 */

#include "nr_phy_ofdm.h"
#include <stdlib.h>
#include <string.h>

/*
 * ============================================================================
 * L5: Radix-2 Decimation-In-Time FFT (Cooley & Tukey, 1965)
 *
 * Fundamental building block for all OFDM modulation.
 * X[k] = sum_{n=0}^{N-1} x[n] * exp(-j*2*pi*k*n/N)
 *
 * Complexity: O(N log2 N)
 * ============================================================================
 */

/* Bit-reverse permutation for in-place FFT */
static void bit_reverse_copy(nr_complex_t *a, int n)
{
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            nr_complex_t tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
        }
        int k = n >> 1;
        while (k <= j) { j -= k; k >>= 1; }
        j += k;
    }
}

void nr_fft(nr_complex_t *x, int n, int inverse)
{
    if (!x || n < 2) return;

    /* Bit-reverse ordering */
    bit_reverse_copy(x, n);

    /* Butterfly stages */
    for (int len = 2; len <= n; len <<= 1) {
        double angle = 2.0 * M_PI / len;
        if (inverse) angle = -angle;

        nr_complex_t wlen = nr_complex_expj(angle);

        for (int i = 0; i < n; i += len) {
            nr_complex_t w = nr_complex_make(1.0, 0.0);
            for (int j = 0; j < len / 2; j++) {
                nr_complex_t u = x[i + j];
                nr_complex_t t = nr_complex_mul(w, x[i + j + len / 2]);
                x[i + j] = nr_complex_add(u, t);
                x[i + j + len / 2] = nr_complex_sub(u, t);
                w = nr_complex_mul(w, wlen);
            }
        }
    }

    /* Scale for IFFT */
    if (inverse) {
        double ninv = 1.0 / (double)n;
        for (int i = 0; i < n; i++) {
            x[i].re *= ninv;
            x[i].im *= ninv;
        }
    }
}

/* ============================================================================
 * L5: OFDM Modulator Initialization
 * ============================================================================ */

int nr_ofdm_mod_init(nr_ofdm_mod_ctx_t *ctx, int mu, int num_prb, int cp_type)
{
    if (!ctx) return -1;
    if (mu < 0 || mu > NR_MAX_MU || num_prb <= 0) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->numerology_mu = mu;
    ctx->cp_type = cp_type;
    ctx->num_symbols = (cp_type == 0) ? NR_SYMBOLS_PER_SLOT_NCP
                                       : NR_SYMBOLS_PER_SLOT_ECP;
    ctx->scs_hz = nr_scs_khz(mu) * 1000.0;

    /* FFT size: must be power of 2 ≥ num_prb*12 */
    ctx->fft_size = nr_fft_size_min(mu, num_prb);
    if (ctx->fft_size <= 0) return -1;

    /* Number of active subcarriers (minus DC subcarrier for DL) */
    ctx->num_active_sc = num_prb * NR_NUM_SC_PER_RB;

    /* Sampling rate = fft_size * subcarrier spacing */
    ctx->sampling_rate = ctx->fft_size * ctx->scs_hz;

    /* CP lengths for each symbol */
    for (int s = 0; s < ctx->num_symbols; s++) {
        ctx->cp_lengths[s] = nr_cp_length(s, ctx->fft_size, mu);
    }

    /* Default windowing: no window (rectangular) */
    ctx->window_length = 0;
    ctx->window_beta = 0.0;

    return 0;
}

/* ============================================================================
 * L5: CP-OFDM Modulation (3GPP TS 38.211 Section 5.3.1)
 * ============================================================================ */

int nr_ofdm_modulate(const nr_ofdm_mod_ctx_t *ctx,
                      const nr_complex_t *symbols_in,
                      int symbol_idx,
                      nr_complex_t *waveform_out)
{
    if (!ctx || !symbols_in || !waveform_out) return -1;
    if (symbol_idx < 0 || symbol_idx >= ctx->num_symbols) return -1;
    if (ctx->fft_size <= 0) return -1;

    int n_fft = ctx->fft_size;
    int n_active = ctx->num_active_sc;
    int cp_len = ctx->cp_lengths[symbol_idx];

    /* Allocate frequency-domain grid (FFT-sized) with zeros */
    nr_complex_t *fd_grid = (nr_complex_t *)calloc(n_fft, sizeof(nr_complex_t));
    if (!fd_grid) return -1;

    /* Map active subcarriers (skip DC subcarrier for DL) */
    int half_active = n_active / 2;
    /* Negative frequencies: map to upper half of FFT */
    for (int i = 0; i < half_active; i++) {
        /* subcarrier index relative to DC: -half_active + i */
        int fft_idx = (n_fft - half_active + i) % n_fft;
        fd_grid[fft_idx] = symbols_in[i];
    }

    /* DC subcarrier */
    fd_grid[half_active] = nr_complex_make(0.0, 0.0);

    /* Positive frequencies: map to lower half of FFT */
    for (int i = 0; i < half_active; i++) {
        int fft_idx = half_active + 1 + i;
        fd_grid[fft_idx] = symbols_in[half_active + i];
    }

    /* IFFT: frequency → time domain */
    nr_fft(fd_grid, n_fft, 1); /* inverse FFT */

    /* Cyclic prefix: copy last cp_len samples to the beginning */
    for (int i = 0; i < cp_len; i++) {
        waveform_out[i] = fd_grid[n_fft - cp_len + i];
    }
    /* Copy FFT output */
    memcpy(waveform_out + cp_len, fd_grid, n_fft * sizeof(nr_complex_t));

    /* Apply raised-cosine window if configured */
    if (ctx->window_length > 0) {
        int wlen = ctx->window_length;
        int total = n_fft + cp_len;
        /* Leading edge window */
        for (int i = 0; i < wlen && i < total; i++) {
            double w = 0.5 * (1.0 - cos(M_PI * (double)i / (double)wlen));
            waveform_out[i].re *= w;
            waveform_out[i].im *= w;
        }
        /* Trailing edge window */
        for (int i = total - wlen; i < total; i++) {
            int wi = total - i;
            double w = 0.5 * (1.0 - cos(M_PI * (double)wi / (double)wlen));
            waveform_out[i].re *= w;
            waveform_out[i].im *= w;
        }
    }

    free(fd_grid);
    return n_fft + cp_len;
}

/* ============================================================================
 * L5: CP-OFDM Demodulation
 * ============================================================================ */

int nr_ofdm_demodulate(const nr_ofdm_mod_ctx_t *ctx,
                        const nr_complex_t *waveform_in,
                        int symbol_idx,
                        nr_complex_t *symbols_out)
{
    if (!ctx || !waveform_in || !symbols_out) return -1;
    if (symbol_idx < 0 || symbol_idx >= ctx->num_symbols) return -1;

    int n_fft = ctx->fft_size;
    int cp_len = ctx->cp_lengths[symbol_idx];
    int n_active = ctx->num_active_sc;

    /* Remove cyclic prefix: skip first cp_len samples */
    nr_complex_t *td = (nr_complex_t *)malloc(n_fft * sizeof(nr_complex_t));
    if (!td) return -1;

    memcpy(td, waveform_in + cp_len, n_fft * sizeof(nr_complex_t));

    /* FFT: time → frequency domain */
    nr_fft(td, n_fft, 0); /* forward FFT */

    /* Extract active subcarriers */
    int half_active = n_active / 2;
    /* Negative frequencies */
    for (int i = 0; i < half_active; i++) {
        int fft_idx = (n_fft - half_active + i) % n_fft;
        symbols_out[i] = td[fft_idx];
    }
    /* Positive frequencies */
    for (int i = 0; i < half_active; i++) {
        int fft_idx = half_active + 1 + i;
        symbols_out[half_active + i] = td[fft_idx];
    }

    free(td);
    return 0;
}

/* ============================================================================
 * L5: DFT-s-OFDM (SC-FDMA) Modulation — 3GPP TS 38.211 Section 5.3.2
 *
 * For uplink, DFT spreading before OFDM modulation reduces PAPR.
 * The key formula:
 *   y = F_N * M * F_M^H * x
 * where F_N = N-point FFT, F_M^H = M-point IDFT (spreading),
 * M = mapping matrix (subcarrier assignment).
 * ============================================================================ */

int nr_dft_s_ofdm_modulate(const nr_ofdm_mod_ctx_t *ctx,
                            const nr_complex_t *symbols_in,
                            int M, int first_sc, int symbol_idx,
                            nr_complex_t *waveform_out)
{
    if (!ctx || !symbols_in || !waveform_out) return -1;
    if (M <= 0 || M > ctx->num_active_sc || first_sc < 0) return -1;
    if (first_sc + M > ctx->num_active_sc) return -1;

    int n_fft = ctx->fft_size;
    int cp_len = ctx->cp_lengths[symbol_idx];

    /* Step 1: M-point DFT spreading */
    nr_complex_t *spread = (nr_complex_t *)malloc(M * sizeof(nr_complex_t));
    memcpy(spread, symbols_in, M * sizeof(nr_complex_t));
    nr_fft(spread, M, 0); /* forward DFT: M-point */

    /* Step 2: Subcarrier mapping (localized) to N_FFT grid */
    nr_complex_t *fd_grid = (nr_complex_t *)calloc(n_fft, sizeof(nr_complex_t));

    int half_active = ctx->num_active_sc / 2;
    for (int i = 0; i < M; i++) {
        int sc = first_sc + i;
        int fft_idx;
        if (sc < half_active) {
            fft_idx = (n_fft - half_active + sc) % n_fft;
        } else {
            fft_idx = (sc - half_active + half_active + 1) % n_fft;
        }
        fd_grid[fft_idx] = spread[i];
    }

    /* Step 3: N_FFT-point IFFT */
    nr_fft(fd_grid, n_fft, 1);

    /* Step 4: Add CP */
    for (int i = 0; i < cp_len; i++) {
        waveform_out[i] = fd_grid[n_fft - cp_len + i];
    }
    memcpy(waveform_out + cp_len, fd_grid, n_fft * sizeof(nr_complex_t));

    free(spread);
    free(fd_grid);
    return n_fft + cp_len;
}

int nr_dft_s_ofdm_demodulate(const nr_ofdm_mod_ctx_t *ctx,
                              const nr_complex_t *waveform_in,
                              int M, int first_sc, int symbol_idx,
                              nr_complex_t *symbols_out)
{
    if (!ctx || !waveform_in || !symbols_out) return -1;
    if (M <= 0 || first_sc < 0) return -1;

    int n_fft = ctx->fft_size;
    int cp_len = ctx->cp_lengths[symbol_idx];

    /* Remove CP and FFT */
    nr_complex_t *fd_grid = (nr_complex_t *)malloc(n_fft * sizeof(nr_complex_t));
    memcpy(fd_grid, waveform_in + cp_len, n_fft * sizeof(nr_complex_t));
    nr_fft(fd_grid, n_fft, 0);

    /* Extract subcarriers */
    nr_complex_t *extracted = (nr_complex_t *)calloc(M, sizeof(nr_complex_t));
    int half_active = ctx->num_active_sc / 2;
    for (int i = 0; i < M; i++) {
        int sc = first_sc + i;
        int fft_idx;
        if (sc < half_active) {
            fft_idx = (n_fft - half_active + sc) % n_fft;
        } else {
            fft_idx = (sc - half_active + half_active + 1) % n_fft;
        }
        extracted[i] = fd_grid[fft_idx];
    }

    /* IDFT to undo spreading */
    nr_fft(extracted, M, 1); /* inverse DFT: M-point */
    memcpy(symbols_out, extracted, M * sizeof(nr_complex_t));

    free(fd_grid);
    free(extracted);
    return 0;
}

/* ============================================================================
 * L5: Resource Element Mapping/Demapping
 * ============================================================================ */

void nr_ofdm_resource_mapping(const nr_complex_t *symbols,
                               int num_symbols,
                               nr_complex_t *grid,
                               int n_sc, int n_symb,
                               const int *dmrs_mask)
{
    if (!symbols || !grid || n_sc <= 0 || n_symb <= 0) return;

    /* Frequency-first, time-second mapping */
    /* grid[sc_idx * n_symb + sym_idx] */
    int sym_idx = 0;
    for (int s = 0; s < n_symb && sym_idx < num_symbols; s++) {
        for (int sc = 0; sc < n_sc && sym_idx < num_symbols; sc++) {
            /* Skip DMRS REs */
            if (dmrs_mask && dmrs_mask[sc * n_symb + s]) continue;
            grid[sc * n_symb + s] = symbols[sym_idx++];
        }
    }
}

int nr_ofdm_resource_demapping(const nr_complex_t *grid,
                                int n_sc, int n_symb,
                                const int *dmrs_mask,
                                nr_complex_t *symbols_out)
{
    if (!grid || !symbols_out || n_sc <= 0 || n_symb <= 0) return 0;

    int sym_idx = 0;
    for (int s = 0; s < n_symb; s++) {
        for (int sc = 0; sc < n_sc; sc++) {
            if (dmrs_mask && dmrs_mask[sc * n_symb + s]) continue;
            symbols_out[sym_idx++] = grid[sc * n_symb + s];
        }
    }
    return sym_idx;
}

/* ============================================================================
 * L5: PAPR Computation
 * ============================================================================ */

double nr_ofdm_papr(const nr_complex_t *waveform, int len)
{
    if (!waveform || len <= 0) return -1.0;

    double max_power = 0.0;
    double mean_power = 0.0;

    for (int i = 0; i < len; i++) {
        double p = nr_complex_abs_sq(waveform[i]);
        mean_power += p;
        if (p > max_power) max_power = p;
    }
    mean_power /= (double)len;

    if (mean_power <= 0.0) return 0.0;
    return 10.0 * log10(max_power / mean_power);
}

/* ============================================================================
 * L5: Spectral Mask Check
 * ============================================================================ */

int nr_ofdm_spectral_mask_check(const nr_complex_t *waveform, int len,
                                 double bw_hz, double scs_hz, double max_dbc)
{
    if (!waveform || len <= 0 || bw_hz <= 0) return -1;

    /* Simplified: compute in-band vs out-of-band power ratio */
    /* Use FFT to estimate PSD */
    int n_fft = 1;
    while (n_fft < len) n_fft <<= 1;

    nr_complex_t *fft_buf = (nr_complex_t *)calloc(n_fft, sizeof(nr_complex_t));
    if (!fft_buf) return -1;

    memcpy(fft_buf, waveform, len * sizeof(nr_complex_t));
    nr_fft(fft_buf, n_fft, 0);

    /* In-band: bins corresponding to bandwidth */
    int inband_bins = (int)(bw_hz / scs_hz);
    if (inband_bins > n_fft / 2) inband_bins = n_fft / 2;

    double inband_power = 0.0;
    double outband_power = 0.0;

    for (int i = 0; i < n_fft / 2; i++) {
        double p = nr_complex_abs_sq(fft_buf[i]);
        if (i < inband_bins) {
            inband_power += p;
        } else {
            outband_power += p;
        }
    }

    free(fft_buf);

    if (inband_power <= 0.0) return -1;
    double ratio_db = 10.0 * log10(outband_power / inband_power);

    return (ratio_db <= max_dbc) ? 0 : -1;
}

/* ============================================================================
 * L5: EVM Computation (3GPP TS 38.101)
 * ============================================================================ */

double nr_ofdm_evm(const nr_complex_t *symbols_tx,
                    const nr_complex_t *symbols_rx,
                    int len)
{
    if (!symbols_tx || !symbols_rx || len <= 0) return -1.0;

    double err_power = 0.0;
    double ref_power = 0.0;

    for (int i = 0; i < len; i++) {
        nr_complex_t err = nr_complex_sub(symbols_rx[i], symbols_tx[i]);
        err_power += nr_complex_abs_sq(err);
        ref_power += nr_complex_abs_sq(symbols_tx[i]);
    }

    if (ref_power <= 0.0) return 0.0;
    return sqrt(err_power / ref_power);
}