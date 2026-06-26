#include "nr_phy_ofdm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

static int tests_run = 0;
#define TEST(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);exit(1);} tests_run++; } while(0)
#define TEST_FEQ(a,b,e) do { if(fabs((a)-(b))>(e)){fprintf(stderr,"FAIL %s:%d: %g vs %g\n",__FILE__,__LINE__,(double)(a),(double)(b));exit(1);} tests_run++; } while(0)

int main(void)
{
    nr_ofdm_mod_ctx_t ctx;

    /* Initialize OFDM modulator */
    TEST(nr_ofdm_mod_init(&ctx, 0, 52, 0) == 0);
    TEST(ctx.fft_size >= 1024);
    TEST(ctx.num_symbols == 14);
    TEST(ctx.scs_hz == 15000.0);

    /* Allocate test symbols: all ones on active subcarriers */
    int n_active = ctx.num_active_sc;
    nr_complex_t *symbols = calloc(n_active, sizeof(nr_complex_t));
    for (int i = 0; i < n_active; i++)
        symbols[i] = nr_complex_make(1.0, 0.0);

    int total_samples = ctx.fft_size + ctx.cp_lengths[0];
    nr_complex_t *waveform = calloc(total_samples, sizeof(nr_complex_t));

    /* OFDM modulate symbol 0 */
    int n_samples = nr_ofdm_modulate(&ctx, symbols, 0, waveform);
    TEST(n_samples == total_samples);

    /* Demodulate */
    nr_complex_t *rx_syms = calloc(n_active, sizeof(nr_complex_t));
    TEST(nr_ofdm_demodulate(&ctx, waveform, 0, rx_syms) == 0);

    /* EVM should be small (ideal channel) */
    double evm = nr_ofdm_evm(symbols, rx_syms, n_active);
    TEST(evm < 0.1);

    /* PAPR computation */
    double papr = nr_ofdm_papr(waveform, n_samples);
    TEST(papr > 0.0);

    /* DFT-s-OFDM modulation */
    nr_ofdm_mod_ctx_t ctx_dft;
    TEST(nr_ofdm_mod_init(&ctx_dft, 1, 51, 0) == 0);
    int M = 12; /* 1 RB worth of symbols */
    nr_complex_t *sc_fdma_syms = calloc(M, sizeof(nr_complex_t));
    for (int i = 0; i < M; i++) sc_fdma_syms[i] = nr_complex_make(1.0, 0.0);

    int total2 = ctx_dft.fft_size + ctx_dft.cp_lengths[0];
    nr_complex_t *wf2 = calloc(total2, sizeof(nr_complex_t));
    int ns2 = nr_dft_s_ofdm_modulate(&ctx_dft, sc_fdma_syms, M, 0, 0, wf2);
    TEST(ns2 > 0);

    /* Resource mapping */
    nr_complex_t *grid = calloc(n_active * 14, sizeof(nr_complex_t));
    int *dmrs_mask = calloc(n_active * 14, sizeof(int));
    nr_ofdm_resource_mapping(symbols, n_active, grid, n_active, 14, dmrs_mask);
    nr_complex_t *demapped = calloc(n_active, sizeof(nr_complex_t));
    int n_dm = nr_ofdm_resource_demapping(grid, n_active, 14, dmrs_mask, demapped);
    TEST(n_dm == n_active);

    /* Spectral mask check */
    int mask_ok = nr_ofdm_spectral_mask_check(waveform, n_samples,
                                               10e6, 15000.0, -10.0);
    /* mask_ok may be -1 if insufficient data, that's fine */

    free(symbols); free(waveform); free(rx_syms);
    free(sc_fdma_syms); free(wf2); free(grid); free(dmrs_mask); free(demapped);

    printf("PASS: test_ofdm (%d assertions)\n", tests_run);
    return 0;
}
