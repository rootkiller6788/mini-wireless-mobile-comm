#include "nr_phy_ofdm.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    printf("=== OFDM Mod/Demod Benchmark ===\n");
    int n_active = 3276;
    nr_ofdm_mod_ctx_t ctx;
    nr_ofdm_mod_init(&ctx, 1, 273, 0);

    nr_complex_t *syms = calloc(n_active, sizeof(nr_complex_t));
    for (int i = 0; i < n_active; i++)
        syms[i] = nr_complex_make(1.0, 0.0);

    int total = ctx.fft_size + ctx.cp_lengths[0];
    nr_complex_t *wf = calloc(total, sizeof(nr_complex_t));
    nr_complex_t *rx = calloc(n_active, sizeof(nr_complex_t));

    int n_iter = 1000;
    clock_t start = clock();
    for (int i = 0; i < n_iter; i++) {
        nr_ofdm_modulate(&ctx, syms, 0, wf);
        nr_ofdm_demodulate(&ctx, wf, 0, rx);
    }
    clock_t end = clock();

    double sec = (double)(end - start) / CLOCKS_PER_SEC;
    printf("FFT size: %d, iterations: %d\n", ctx.fft_size, n_iter);
    printf("Time: %.3f sec (%.1f us per mod/demod pair)\n",
           sec, sec * 1e6 / n_iter);

    free(syms); free(wf); free(rx);
    return 0;
}
