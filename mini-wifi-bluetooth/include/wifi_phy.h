/**
 * @file wifi_phy.h
 * @brief WiFi PHY Layer — OFDM Modulation, Channel Coding, MIMO (L2,L3,L5)
 *
 * Implements PHY-layer algorithms for IEEE 802.11a/g/n/ac/ax:
 *   - OFDM symbol construction (64/128/256/512-point FFT)
 *   - Pilot tone insertion and channel estimation
 *   - Convolutional encoding / Viterbi decoding
 *   - LDPC encoding (802.11n/ac/ax)
 *   - MIMO spatial streams and beamforming
 *   - Transmit power / EVM measurement
 *
 * Reference: IEEE Std 802.11-2020, Clause 17 (OFDM PHY)
 * Reference: Perahia, E. & Stacey, R., "Next Generation Wireless LANs",
 *            2nd ed., Cambridge University Press, 2013.
 */
#ifndef WIFI_PHY_H
#define WIFI_PHY_H

#include "wifi_bt_types.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * OFDM Symbol Construction (L3 Mathematical Structure)
 * ========================================================================== */

/**
 * @brief Initialize OFDM parameters for a given bandwidth
 *
 * IEEE 802.11 OFDM numerology:
 *   | BW (MHz) | N_FFT | N_DATA | N_PILOT | N_CP | Δf (kHz) | T_sym (µs) |
 *   |----------|-------|--------|---------|------|----------|-----------|
 *   |   20     |  64   |   48   |    4    |  16  |  312.5   |   4.0     |
 *   |   40     | 128   |  108   |    6    |  32  |  312.5   |   3.6     |
 *   |   80     | 256   |  234   |    8    |  64  |  312.5   |   3.6     |
 *   |  160     | 512   |  468   |   16    | 128  |  312.5   |   3.6     |
 *
 * @param params    OFDM parameters struct to fill
 * @param bw_mhz    Channel bandwidth (20/40/80/160)
 * @return 0 on success, -1 on invalid bandwidth
 *
 * Complexity: O(1)
 * Theorem: Subcarrier spacing Δf = BW / N_FFT ensures orthogonality
 *          (derived from Nyquist orthogonality: ∫ e^{j2πfₖt}·e^{-j2πfⱼt} dt = 0)
 */
int ofdm_params_init(ofdm_params_t *params, double bw_mhz);

/**
 * @brief Generate OFDM subcarrier mapping for an 802.11 signal
 *
 * Allocates logical subcarrier indices for data, pilots, DC null,
 * and guard bands. The DC subcarrier (k=0) is always nulled.
 *
 * 802.11a/g (20 MHz, 64-FFT):
 *   - Data: indices ±1..±26 excluding pilots
 *   - Pilots: ±7, ±21
 *   - DC null: index 0
 *   - Guard: -32..-27, 0, +28..+31 (for 20 MHz)
 *
 * @param map       Subcarrier map struct to fill
 * @param bw_mhz    Channel bandwidth
 * @return 0 on success, -1 on failure
 *
 * Complexity: O(N_FFT)
 */
int ofdm_subcarrier_map_init(ofdm_subcarrier_map_t *map, double bw_mhz);
void ofdm_subcarrier_map_free(ofdm_subcarrier_map_t *map);

/**
 * @brief Generate 802.11 legacy long training sequence (LTS)
 *
 * The LTS is used for channel estimation and fine frequency offset
 * correction. It is a known BPSK sequence in the frequency domain:
 *   L_{-26,26} = {1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,0,
 *                  1,-1,-1,1,1,-1,1,-1,1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,1,-1,1,1,1,1}
 *
 * @param lts       Output buffer (at least n_fft complex pairs)
 * @param n_fft     FFT size
 * @return 0 on success
 *
 * Complexity: O(N_FFT)
 */
int ofdm_generate_lts(double *lts_real, double *lts_imag, int n_fft);

/**
 * @brief Insert pilot tones into frequency-domain OFDM symbol
 *
 * 802.11 pilots use a scrambled BPSK sequence with polarity determined
 * by a 127-bit PRBS. Pilot values are ±1.
 *
 * Pilot scrambling sequence is initialized with all-ones (7-bit shift register).
 * The polynomial is: S(x) = x⁷ + x⁴ + 1 (127-bit sequence)
 *
 * @param freq_symbol   Frequency-domain data (size n_fft)
 * @param pilot_indices Array of pilot subcarrier indices
 * @param n_pilots      Number of pilot subcarriers
 * @param symbol_index  OFDM symbol index (0-based in packet)
 * @return 0 on success
 *
 * Complexity: O(N_pilots)
 * Reference: IEEE 802.11-2020 §17.3.5.8 Pilot subcarriers
 */
int ofdm_insert_pilots(double *freq_symbol, const int *pilot_indices,
                       int n_pilots, int symbol_index);

/**
 * @brief Construct a complete OFDM time-domain symbol via IFFT
 *
 * Steps:
 *   1. Map constellation points to data subcarriers
 *   2. Insert pilots
 *   3. Null DC and guard subcarriers
 *   4. Compute IFFT (frequency → time domain)
 *   5. Prepend cyclic prefix (copy last N_CP samples to front)
 *
 * Output length = N_FFT + N_CP time-domain complex samples
 *
 * @param time_symbol      Output time-domain samples (real/imag interleaved)
 * @param data_symbols     Input constellation points (N_data)
 * @param map              Subcarrier map
 * @param params           OFDM parameters
 * @param symbol_index     OFDM symbol index in packet
 * @return Number of time-domain samples (N_FFT + N_CP), or -1 on error
 *
 * Complexity: O(N log N) dominated by IFFT
 */
int ofdm_build_symbol(double *time_symbol,
                      const double *data_symbols,
                      const ofdm_subcarrier_map_t *map,
                      const ofdm_params_t *params,
                      int symbol_index);

/* ==========================================================================
 * Constellation Mapping (L2 Core Concept)
 * ========================================================================== */

/** Supported modulation schemes */
typedef enum {
    MOD_BPSK     = 0,       /**< Binary PSK, 1 bit/symbol */
    MOD_QPSK     = 1,       /**< Quadrature PSK, 2 bits/symbol */
    MOD_16QAM    = 2,       /**< 16-QAM, 4 bits/symbol */
    MOD_64QAM    = 3,       /**< 64-QAM, 6 bits/symbol */
    MOD_256QAM   = 4        /**< 256-QAM, 8 bits/symbol (802.11ax) */
} wifi_modulation_t;

/** QAM normalization constants (for unit average power) */
#define QPSK_NORM      0.7071067811865475   /* 1/sqrt(2) */
#define QAM16_NORM     0.31622776601683794  /* 1/sqrt(10) */
#define QAM64_NORM     0.1543033499620919   /* 1/sqrt(42) */
#define QAM256_NORM    0.07669649888473704  /* 1/sqrt(170) */

/**
 * @brief Map data bits to constellation point
 *
 * Supports BPSK, QPSK, 16-QAM, 64-QAM, 256-QAM with Gray coding.
 * Symbols are normalized to unit average power.
 *
 * Gray coding minimizes bit errors for adjacent constellation points.
 *
 * Normalization factors (for unit average power):
 *   - BPSK: 1
 *   - QPSK: 1/√2
 *   - 16-QAM: 1/√10
 *   - 64-QAM: 1/√42
 *   - 256-QAM: 1/√170
 *
 * @param i_real    Output I (in-phase) component
 * @param q_imag    Output Q (quadrature) component
 * @param bits      Input bits (packed, LSB first)
 * @param mod       Modulation type
 * @return Number of bits consumed, or -1 on error
 *
 * Complexity: O(1)
 */
int constellation_map(double *i_real, double *q_imag,
                      uint32_t bits, wifi_modulation_t mod);

/**
 * @brief Soft-decision demapping (LLR computation)
 *
 * Computes Log-Likelihood Ratios for each bit given a received
 * constellation point, assuming AWGN with known noise variance.
 *
 * LLR(b_k) = ln( P(b_k=0 | r) / P(b_k=1 | r) )
 *          = ln( Σ_{s: b_k=0} exp(-|r-s|²/σ²) ) - ln( Σ_{s: b_k=1} exp(-|r-s|²/σ²) )
 *
 * Uses max-log approximation: LLR ≈ (min|r-s₁|² - min|r-s₀|²) / σ²
 *
 * @param llr_out   Output LLR array
 * @param r_i       Received I component
 * @param r_q       Received Q component
 * @param mod       Modulation type
 * @param noise_var Noise variance σ²
 * @return Number of LLR values produced
 */
int constellation_demap_soft(double *llr_out, double r_i, double r_q,
                             wifi_modulation_t mod, double noise_var);

/**
 * @brief Constellation hard-decision demodulation
 *
 * Finds nearest constellation point to received (r_i, r_q) and returns
 * the decoded bits.
 *
 * @param bits_out  Output decoded bits (packed)
 * @param r_i       Received I
 * @param r_q       Received Q
 * @param mod       Modulation type
 * @return Number of decoded bits, or -1
 */
int constellation_demap_hard(uint32_t *bits_out, double r_i, double r_q,
                             wifi_modulation_t mod);

/* ==========================================================================
 * Convolutional Encoding / Viterbi Decoding (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief IEEE 802.11 convolutional encoder (rate 1/2, constraint length 7)
 *
 * Generator polynomials (octal):
 *   G₀ = 133₈  (x⁶ + x⁵ + x³ + x² + 1)
 *   G₁ = 171₈  (x⁶ + x⁵ + x⁴ + x³ + 1)
 *
 * Output order: A₁ B₁ A₂ B₂ ... (puncturing applied per MCS)
 * Initial state = 0, flushed to 0 (6 tail bits appended).
 *
 * @param output    Output encoded bits (interleaved A/B)
 * @param input     Input data bits
 * @param n_bits    Number of input bits (before tail)
 * @return Number of output bits (2*(n_bits+6)), or -1 on error
 *
 * Complexity: O(N)
 * Reference: IEEE 802.11-2020 §17.3.5.6
 */
int conv_encode(uint8_t *output, const uint8_t *input, int n_bits);

/**
 * @brief Viterbi decoder (hard-decision, traceback depth 36)
 *
 * Implements the Viterbi algorithm for decoding rate 1/2 convolutional
 * code with constraint length K=7. Uses:
 *   - Trellis: 2^(K-1) = 64 states
 *   - Branch metric: Hamming distance
 *   - Traceback depth: 5*K = 35 → 36 (standard)
 *
 * Algorithm steps:
 *   1. Initialize path metrics (state 0 = 0, others = ∞)
 *   2. For each received symbol pair:
 *      a. For each state: compute 2 incoming branch metrics
 *      b. Add-Compare-Select (ACS): keep best predecessor
 *   3. Traceback from best final state to recover bits
 *
 * @param decoded   Output decoded bits
 * @param encoded   Input encoded bits (A/B interleaved)
 * @param n_encoded Number of encoded bits (must be even)
 * @return Number of decoded bits, or -1 on error
 *
 * Complexity: O(2^K · N) — O(64·N) for K=7
 * Reference: Viterbi, A.J., "Error Bounds for Convolutional Codes",
 *            IEEE Trans. Info. Theory, 1967.
 */
int viterbi_decode(uint8_t *decoded, const uint8_t *encoded, int n_encoded);

/**
 * @brief 802.11 rate-dependent puncturing for higher code rates
 *
 * Puncturing patterns for rates 2/3, 3/4 (standard 802.11):
 *   - Rate 1/2: no puncturing (source stream: A₁ B₁ A₂ B₂ ...)
 *   - Rate 2/3: puncture B₂  → pattern [A₁ B₁ A₂]
 *   - Rate 3/4: puncture A₂ B₁ → pattern [A₁ B₁ A₂ B₂] → [A₁ B₁ A₃ B₃]
 *
 * @param punctured  Output punctured bits
 * @param encoded    Input rate-1/2 encoded bits
 * @param n_encoded  Number of encoded bits
 * @param rate_num   Code rate numerator (2 for 2/3, 3 for 3/4)
 * @param rate_den   Code rate denominator (3 for 2/3, 4 for 3/4)
 * @return Number of punctured output bits, or -1 on error
 */
int conv_puncture(uint8_t *punctured, const uint8_t *encoded,
                  int n_encoded, int rate_num, int rate_den);

/**
 * @brief Depuncture received bits (insert erasures for Viterbi input)
 *
 * Inserts dummy (zero) bits at punctured positions for Viterbi decoder.
 *
 * @param depunctured Output depunctured bits
 * @param received    Received punctured bits
 * @param n_received  Number of received bits
 * @param rate_num    Code rate numerator
 * @param rate_den    Code rate denominator
 * @return Number of depunctured bits, or -1 on error
 */
int conv_depuncture(uint8_t *depunctured, const uint8_t *received,
                    int n_received, int rate_num, int rate_den);

/* ==========================================================================
 * Interleaver / Deinterleaver (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief 802.11a/g block interleaver
 *
 * One-stage block interleaver with block size N_CBPS (coded bits per
 * OFDM symbol). 802.11n/ac use a two-stage interleaver.
 *
 * First permutation:
 *   i = (N_CBPS / 16) * (k mod 16) + floor(k / 16)
 *   for k = 0, 1, ..., N_CBPS - 1
 *
 * @param interleaved Output interleaved bits
 * @param input       Input coded bits
 * @param n_cbps      Coded bits per OFDM symbol
 * @return 0 on success, -1 on error
 *
 * Complexity: O(N_CBPS)
 */
int interleaver_80211a(uint8_t *interleaved, const uint8_t *input, int n_cbps);

/**
 * @brief 802.11a/g block deinterleaver (inverse permutation)
 *
 * Inverse permutation:
 *   k = 16*i - (N_CBPS - 1) * floor(16*i / N_CBPS)
 *
 * @param deinterleaved Output deinterleaved bits
 * @param received       Received interleaved bits
 * @param n_cbps         Coded bits per OFDM symbol
 * @return 0 on success
 *
 * Complexity: O(N_CBPS)
 */
int deinterleaver_80211a(uint8_t *deinterleaved, const uint8_t *received, int n_cbps);

/* ==========================================================================
 * LDPC Encoding (L5 Algorithm — 802.11n/ac/ax)
 * ========================================================================== */

/**
 * @brief IEEE 802.11 LDPC parity check matrix info
 *
 * 802.11 uses quasi-cyclic (QC) LDPC codes with block lengths
 * 648, 1296, and 1944 bits. Code rates: 1/2, 2/3, 3/4, 5/6.
 */
typedef struct {
    int n_codeword;      /**< Codeword length (648/1296/1944) */
    int n_info;          /**< Information bits */
    int z_factor;        /**< Expansion factor Z = n_codeword/24 */
    int rate_num;        /**< Code rate numerator */
    int rate_den;        /**< Code rate denominator */
} ldpc_params_t;

/**
 * @brief Initialize LDPC parameters for given codeword length and rate
 *
 * @param params      LDPC parameters to fill
 * @param n_codeword  Codeword length (648, 1296, or 1944)
 * @param rate_num    Rate numerator
 * @param rate_den    Rate denominator
 * @return 0 on success, -1 on invalid parameters
 */
int ldpc_params_init(ldpc_params_t *params, int n_codeword,
                     int rate_num, int rate_den);

/**
 * @brief Encode data using 802.11 LDPC
 *
 * LDPC encoding: c = [s | p] where s = systematic bits, p = parity bits.
 * Parity bits computed from H*[s | p]^T = 0.
 *
 * Uses Richardson-Urbanke efficient encoding via parity check matrix
 * decomposition into lower-triangular form.
 *
 * @param codeword  Output codeword (n_codeword bits)
 * @param info      Input information bits (n_info bits)
 * @param params    LDPC parameters
 * @return 0 on success
 *
 * Complexity: O(n_codeword * Z)
 * Reference: IEEE 802.11-2020 §19.3.11.7 LDPC encoding
 */
int ldpc_encode(uint8_t *codeword, const uint8_t *info, const ldpc_params_t *params);

/* ==========================================================================
 * STBC / Spatial Streams / MIMO (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Alamouti Space-Time Block Code (STBC) encoding for 2x1 MISO
 *
 * For 2 transmit antennas, encodes symbols s₀, s₁ as:
 *   time 0: [s₀    s₁   ]  ← antenna 0, 1
 *   time 1: [-s₁*  s₀*  ]  ← antenna 0, 1
 *
 * At the receiver, with channel estimates h₀, h₁:
 *   ŝ₀ = h₀*·r₀ + h₁·r₁*
 *   ŝ₁ = h₁*·r₀ - h₀·r₁*
 *
 * Provides 2nd-order diversity with rate 1 (no bandwidth penalty).
 *
 * @param stbc_out   Output: 4 values [tx0_t0, tx1_t0, tx0_t1, tx1_t1]
 * @param s0_real    Input symbol 0 real part
 * @param s0_imag    Input symbol 0 imag part
 * @param s1_real    Input symbol 1 real part
 * @param s1_imag    Input symbol 1 imag part
 *
 * Complexity: O(1)
 * Reference: Alamouti, S.M., "A Simple Transmit Diversity Technique
 *            for Wireless Comm.", IEEE JSAC, 1998.
 */
void stbc_alamouti_encode(double stbc_out[4], double s0_real, double s0_imag,
                          double s1_real, double s1_imag);

/**
 * @brief Alamouti STBC decoding (combining)
 *
 * Recovers transmitted symbols from received signals using channel
 * estimates, achieving full diversity gain.
 *
 * @param s0_r  Output symbol 0 real
 * @param s0_i  Output symbol 0 imag
 * @param s1_r  Output symbol 1 real
 * @param s1_i  Output symbol 1 imag
 * @param r0_r  Received signal at time 0, real
 * @param r0_i  Received signal at time 0, imag
 * @param r1_r  Received signal at time 1, real
 * @param r1_i  Received signal at time 1, imag
 * @param h0_r  Channel estimate for antenna 0, real
 * @param h0_i  Channel estimate for antenna 0, imag
 * @param h1_r  Channel estimate for antenna 1, real
 * @param h1_i  Channel estimate for antenna 1, imag
 */
void stbc_alamouti_decode(double *s0_r, double *s0_i,
                          double *s1_r, double *s1_i,
                          double r0_r, double r0_i,
                          double r1_r, double r1_i,
                          double h0_r, double h0_i,
                          double h1_r, double h1_i);

/* ==========================================================================
 * EVM / Transmitter Quality Metrics (L1 Definition)
 * ========================================================================== */

/**
 * @brief Calculate Error Vector Magnitude (EVM)
 *
 * EVM measures the deviation of actual transmitted symbols from ideal
 * constellation points. Defined in IEEE 802.11 §17.3.10.7.2:
 *
 *   EVM_RMS = sqrt( Σ|s_meas - s_ideal|² / (N * P_avg) ) × 100%
 *
 * where P_avg is the average constellation power.
 *
 * @param measured     Measured symbol array (real/imag pairs)
 * @param ideal        Ideal symbol array (real/imag pairs)
 * @param n_symbols    Number of symbols
 * @param modulation   Modulation type (for P_avg)
 * @return EVM RMS in percent, or -1.0 on error
 *
 * Complexity: O(N)
 */
double compute_evm(const double *measured, const double *ideal,
                   int n_symbols, wifi_modulation_t modulation);

/**
 * @brief Calculate transmitter constellation error
 *
 * Returns per-subcarrier EVM for diagnostic purposes.
 *
 * @param per_sc_evm   Output per-subcarrier EVM array (%)
 * @param max_sc       Max subcarriers to report
 * @param measured     Measured frequency-domain symbols
 * @param ideal        Ideal frequency-domain symbols
 * @param n_sc         Number of subcarriers
 * @return Average EVM in percent
 */
double compute_evm_per_subcarrier(double *per_sc_evm, int max_sc,
                                  const double *measured,
                                  const double *ideal, int n_sc);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_PHY_H */
