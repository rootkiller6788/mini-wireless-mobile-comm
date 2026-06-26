/**
 * nr_phy_ofdm.h — 5G NR OFDM Modulation / Demodulation
 *
 * Knowledge Coverage:
 *   L2 Core Concepts: CP-OFDM, DFT-s-OFDM, cyclic prefix, windowing
 *   L3 Math Structures: FFT/IFFT, DFT spreading
 *   L5 Algorithms: OFDM modulator/demodulator, DFT-s-OFDM
 *   L6 Canonical Problems: OFDM symbol generation per 3GPP TS 38.211
 *
 * Course: Stanford EE359, MIT 6.450, Berkeley EE123
 * Ref: 3GPP TS 38.211 Section 5.3
 */

#ifndef NR_PHY_OFDM_H
#define NR_PHY_OFDM_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** OFDM modulator context */
typedef struct {
    int     fft_size;           /* N_FFT */
    int     num_active_sc;      /* Active subcarriers excluding DC */
    int     cp_lengths[14];     /* CP length per symbol (samples) */
    int     num_symbols;        /* Symbols per slot */
    int     numerology_mu;
    int     cp_type;
    double  scs_hz;             /* Subcarrier spacing in Hz */
    double  sampling_rate;      /* FFT_size * scs_hz */
    /* Windowing */
    int     window_length;      /* Samples for raised-cosine window */
    double  window_beta;        /* Roll-off factor */
} nr_ofdm_mod_ctx_t;

/**
 * nr_ofdm_mod_init — Initialize OFDM modulator context
 *
 * Configures FFT size, CP lengths per symbol, and sampling rate
 * for the given numerology and bandwidth.
 *
 * L4: OFDM subcarrier orthogonality requires subcarrier spacing
 * Delta_f = 1/T_u where T_u is the useful symbol duration.
 * The CP length must exceed the channel delay spread.
 *
 * Complexity: O(1).
 */
int nr_ofdm_mod_init(nr_ofdm_mod_ctx_t *ctx, int mu, int num_prb, int cp_type);

/**
 * nr_ofdm_modulate — CP-OFDM modulation per 3GPP TS 38.211 5.3.1
 *
 * Steps:
 *   1. Map complex symbols to subcarriers (with DC null, guard bands)
 *   2. N_FFT-point IFFT
 *   3. Prepend cyclic prefix
 *   4. Apply windowing (raised-cosine) for spectral containment
 *
 * @param ctx          OFDM modulator context
 * @param symbols_in   Frequency-domain complex symbols (num_active_sc)
 * @param symbol_idx   Which OFDM symbol in the slot (0..13 for NCP)
 * @param waveform_out Time-domain samples (fft_size + cp_length)
 * @return Number of time-domain samples output
 */
int nr_ofdm_modulate(const nr_ofdm_mod_ctx_t *ctx,
                      const nr_complex_t *symbols_in,
                      int symbol_idx,
                      nr_complex_t *waveform_out);

/**
 * nr_ofdm_demodulate — CP-OFDM demodulation
 *
 * Inverse of modulation:
 *   1. Remove cyclic prefix
 *   2. N_FFT-point FFT
 *   3. Extract active subcarriers
 *
 * @return 0 on success
 */
int nr_ofdm_demodulate(const nr_ofdm_mod_ctx_t *ctx,
                        const nr_complex_t *waveform_in,
                        int symbol_idx,
                        nr_complex_t *symbols_out);

/**
 * nr_dft_s_ofdm_modulate — DFT-s-OFDM modulation (UL)
 *
 * 3GPP TS 38.211 Section 5.3.2.
 *
 * For uplink coverage-limited scenarios:
 *   1. M-point DFT spread the modulated symbols
 *   2. Subcarrier mapping (localized or interleaved)
 *   3. N-point IFFT (N >> M)
 *   4. Add CP
 *
 * DFT spreading reduces PAPR compared to CP-OFDM.
 *
 * @param ctx           OFDM modulator context
 * @param symbols_in    Time-domain symbols (M = allocated subcarriers)
 * @param M             Number of DFT-spread symbols
 * @param first_sc      First subcarrier index for mapping
 * @param symbol_idx    OFDM symbol index
 * @param waveform_out  Output waveform
 * @return Number of time-domain samples
 */
int nr_dft_s_ofdm_modulate(const nr_ofdm_mod_ctx_t *ctx,
                            const nr_complex_t *symbols_in,
                            int M, int first_sc, int symbol_idx,
                            nr_complex_t *waveform_out);

/**
 * nr_dft_s_ofdm_demodulate — DFT-s-OFDM demodulation
 */
int nr_dft_s_ofdm_demodulate(const nr_ofdm_mod_ctx_t *ctx,
                              const nr_complex_t *waveform_in,
                              int M, int first_sc, int symbol_idx,
                              nr_complex_t *symbols_out);

/**
 * nr_ofdm_resource_mapping — Map modulated symbols to RE grid
 *
 * 3GPP TS 38.211 Section 6.3.1: The RE grid is defined as
 * N_sc^RB * N_RB subcarriers in frequency by N_symb^slot symbols
 * in time. This function fills the grid from a linear symbol stream
 * according to frequency-first then time-first mapping.
 *
 * @param symbols       Linear stream of complex symbols
 * @param num_symbols   Total number of symbols to map
 * @param grid          Output: 2D RE grid [N_sc * N_prb][N_symb_slot]
 * @param n_sc          Subcarriers (12 * N_prb)
 * @param n_symb        OFDM symbols per slot
 * @param dmrs_mask     DMRS positions: 1 = DMRS, 0 = data
 */
void nr_ofdm_resource_mapping(const nr_complex_t *symbols,
                               int num_symbols,
                               nr_complex_t *grid,
                               int n_sc, int n_symb,
                               const int *dmrs_mask);

/**
 * nr_ofdm_resource_demapping — Extract data symbols from RE grid
 *
 * Reverse of resource mapping: skips DMRS REs and extracts
 * data symbols in order.
 *
 * @return Number of symbols extracted
 */
int nr_ofdm_resource_demapping(const nr_complex_t *grid,
                                int n_sc, int n_symb,
                                const int *dmrs_mask,
                                nr_complex_t *symbols_out);

/**
 * nr_ofdm_peak_to_average_power_ratio — Compute PAPR in dB
 *
 * PAPR = 10 * log10( max|x|^2 / E[|x|^2] )
 *
 * DFT-s-OFDM significantly reduces PAPR vs CP-OFDM.
 * This is critical for UE power amplifier efficiency.
 *
 * @param waveform  Time-domain samples
 * @param len       Number of samples
 * @return PAPR in dB
 */
double nr_ofdm_papr(const nr_complex_t *waveform, int len);

/**
 * nr_ofdm_spectral_mask_check — Verify spectral emission mask
 *
 * 3GPP TS 38.101 defines the spectral emission mask to limit
 * adjacent channel interference. This function computes the
 * power spectral density and checks against the mask.
 *
 * @param waveform    Time-domain samples (one slot)
 * @param len         Total samples
 * @param bw_hz       Channel bandwidth (Hz)
 * @param scs_hz      Subcarrier spacing
 * @param max_dbc     Max allowed out-of-band emission (dBc)
 * @return 0 = passes, -1 = fails
 */
int nr_ofdm_spectral_mask_check(const nr_complex_t *waveform, int len,
                                 double bw_hz, double scs_hz, double max_dbc);

/**
 * nr_ofdm_evm — Compute Error Vector Magnitude
 *
 * EVM is a key transmitter quality metric per 3GPP TS 38.101.
 * EVM = sqrt(mean(|symbol_rx - symbol_tx|^2) / mean(|symbol_tx|^2))
 *
 * @param symbols_tx  Transmitted (ideal) symbols
 * @param symbols_rx  Received (impaired) symbols
 * @param len         Number of symbols
 * @return EVM as ratio (0 = perfect), or -1 on error
 */
double nr_ofdm_evm(const nr_complex_t *symbols_tx,
                    const nr_complex_t *symbols_rx,
                    int len);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_OFDM_H */
