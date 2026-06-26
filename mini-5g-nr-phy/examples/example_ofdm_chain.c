/**
 * example_ofdm_chain.c — Complete OFDM TX/RX chain for 5G NR
 *
 * Demonstrates: numerology setup → OFDM modulation → channel → OFDM demodulation
 * This is the fundamental PHY processing chain for NR downlink.
 */
#include "nr_phy_common.h"
#include "nr_phy_ofdm.h"
#include "nr_phy_channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(void)
{
    printf("=== 5G NR OFDM TX/RX Chain Example ===\n\n");

    /* Step 1: Configure numerology (mu=1, 30 kHz SCS, 100 MHz BW, 273 PRBs) */
    nr_numerology_t num;
    nr_numerology_init(&num, 1, 0);
    printf("Numerology: mu=%d, SCS=%.0f kHz, %d slots/subframe, %d symbols/slot\n",
           num.mu, num.scs_khz, num.slots_per_subframe, num.symbols_per_slot);

    /* Step 2: Initialize OFDM modulator */
    nr_ofdm_mod_ctx_t ofdm;
    if (nr_ofdm_mod_init(&ofdm, 1, 273, 0) != 0) {
        fprintf(stderr, "Failed to init OFDM modulator\n");
        return 1;
    }
    printf("OFDM: FFT size=%d, active SC=%d, sampling rate=%.3f MHz\n",
           ofdm.fft_size, ofdm.num_active_sc, ofdm.sampling_rate / 1e6);

    /* Step 3: Generate random QPSK symbols */
    int n_active = ofdm.num_active_sc;
    nr_complex_t *tx_syms = malloc(n_active * sizeof(nr_complex_t));
    srand((unsigned)time(NULL));
    for (int i = 0; i < n_active; i++) {
        double re = (rand() % 2) ? 1.0 : -1.0;
        double im = (rand() % 2) ? 1.0 : -1.0;
        tx_syms[i] = nr_complex_make(re / sqrt(2.0), im / sqrt(2.0));
    }

    /* Step 4: OFDM modulate one symbol */
    int total_samples = ofdm.fft_size + ofdm.cp_lengths[0];
    nr_complex_t *waveform = calloc(total_samples, sizeof(nr_complex_t));
    int n_out = nr_ofdm_modulate(&ofdm, tx_syms, 0, waveform);
    printf("Modulated: %d time-domain samples (FFT=%d + CP=%d)\n",
           n_out, ofdm.fft_size, ofdm.cp_lengths[0]);

    /* Step 5: Compute PAPR */
    double papr = nr_ofdm_papr(waveform, n_out);
    printf("PAPR = %.1f dB\n", papr);

    /* Step 6: Pass through a simple channel (TDL-A) */
    nr_tdl_channel_t chan;
    nr_tdl_init(&chan, 'A', 30.0, 3.5, 3.0, 0);
    printf("Channel: TDL-A, delay spread=%.1f ns, Doppler=%.1f Hz\n",
           chan.delay_spread_ns, chan.doppler_shift_hz);

    /* Apply channel */
    int y_len = n_out + 5;
    nr_complex_t *rx_waveform = calloc(y_len, sizeof(nr_complex_t));
    /* Generate tap coefficients at t=0 */
    nr_complex_t *tap_h = malloc(chan.num_taps * sizeof(nr_complex_t));
    nr_tdl_generate(&chan, 0.0, tap_h);

    nr_complex_t *noise = calloc(y_len, sizeof(nr_complex_t));
    for (int i = 0; i < y_len; i++) {
        noise[i].re = ((double)rand()/RAND_MAX - 0.5) * 0.02;
        noise[i].im = ((double)rand()/RAND_MAX - 0.5) * 0.02;
    }
    int n_rx = nr_tdl_apply(&chan, waveform, n_out, noise, rx_waveform);
    printf("Received: %d samples after channel\n", n_rx);

    /* Step 7: OFDM demodulate */
    nr_complex_t *rx_syms = calloc(n_active, sizeof(nr_complex_t));
    if (nr_ofdm_demodulate(&ofdm, rx_waveform, 0, rx_syms) == 0) {
        /* Step 8: Compute EVM */
        double evm = nr_ofdm_evm(tx_syms, rx_syms, n_active);
        printf("EVM = %.1f%%\n", evm * 100.0);
    }

    /* Step 9: SNR calculation */
    double snr_lin;
    double snr_db = nr_snr_dbm(30.0, nr_pathloss_db(NR_PATHLOSS_UMA, 500.0,
                                                      3.5, 25.0, 1.5, 1),
                                15.0, 2.0, 20e6, 5.0, &snr_lin);
    printf("SNR at 500m (UMa): %.1f dB\n", snr_db);
    double capacity = nr_channel_capacity(20e6, snr_lin);
    printf("Shannon capacity: %.2f Mbps\n", capacity / 1e6);

    free(tx_syms); free(waveform); free(tap_h);
    free(noise); free(rx_waveform); free(rx_syms);

    printf("\n=== OFDM Chain Complete ===\n");
    return 0;
}
