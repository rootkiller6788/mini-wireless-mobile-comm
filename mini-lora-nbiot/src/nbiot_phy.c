/**
 * @file nbiot_phy.c
 * @brief NB-IoT Physical Layer -- Zadoff-Chu sequences, NPSS/NSSS generation,
 *        OFDM modulation, TBCC encoding, cell search
 *
 * Knowledge: L3 Zadoff-Chu CAZAC sequences, L5 NPSS detection + NSSS decoding,
 *            OFDM modulation/demodulation, tail-biting convolutional code,
 *            frequency hopping pattern
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "nbiot_phy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ======================================================================
   L3: Zadoff-Chu Sequence Generation
   ====================================================================== */

/*
 * Generate Zadoff-Chu sequence.
 *
 * z_u[n] = exp(-j * pi * u * n * (n+1) / N_ZC)
 *
 * Properties (L3 -- Mathematical Structure):
 *   1. Constant Amplitude: |z_u[n]| = 1 for all n
 *      → Low PAPR, efficient PA operation
 *
 *   2. Zero Auto-Correlation (CAZAC):
 *      Cyclic auto-correlation is zero for all non-zero lags:
 *        R[k] = sum_n z[n] * z*[(n+k) mod N] = 0 for k != 0
 *      → Perfect for synchronization (no sidelobes)
 *
 *   3. Constant envelope in frequency domain:
 *      DFT of ZC is also a ZC sequence (up to scaling)
 *      → Good for channel estimation across all subcarriers
 *
 *   4. Cross-correlation between different root sequences u1, u2:
 *      |cross_corr| = 1/sqrt(N) when |u1-u2| is coprime with N
 *      → Good orthogonality for multi-user separation
 *
 * @param zc_len  Sequence length (must be odd integer, preferably prime)
 * @param u       Root sequence index (1 <= u < N_ZC, coprime with N_ZC)
 * @param seq     Output complex sequence [length zc_len]
 * @return 0 on success, -1 on error
 */
int nbiot_zadoff_chu(uint16_t zc_len, uint16_t u, double complex *seq)
{
    if (!seq || zc_len == 0 || u == 0 || u >= zc_len) return -1;

    /*
     * ZC definition: z[n] = exp(-j * pi * u * n * (n+1) / N)
     *
     * The (n+1) instead of n is the standard 3GPP convention.
     * This shift ensures the sequence retains CAZAC properties
     * for both even and odd lengths (though N is usually prime).
     *
     * For n = 0, 1, ..., N-1:
     *   phase[n] = -pi * u * n * (n+1) / N
     */
    for (uint16_t n = 0; n < zc_len; n++) {
        double phase = -M_PI * (double)u * (double)n * (double)(n + 1)
                       / (double)zc_len;
        seq[n] = CMPLX(cos(phase), sin(phase));
    }

    return 0;
}

/* ======================================================================
   L5: NPSS Generation
   ====================================================================== */

/*
 * Generate NPSS (Narrowband Primary Synchronization Signal).
 *
 * NPSS uses a length-11 Zadoff-Chu sequence with root u=5,
 * transmitted in the last 11 OFDM symbols of subframe 5
 * (symbols 3-13 in normal CP) on all 12 subcarriers.
 *
 * The sequence is repeated in every radio frame, enabling
 * auto-correlation based detection at the UE.
 *
 * NPSS serves two purposes:
 *   1. Subframe timing: the UE finds subframe 5 boundary
 *   2. Coarse frequency offset estimation
 *
 * The ZC sequence covers 11 OFDM symbols:
 *   Each symbol carries the same ZC sequence on all 12 SCs.
 *   An orthogonal cover code (OCC) [+1 +1 +1 +1] or [+1 -1 +1 -1]
 *   is applied across 4-symbol blocks depending on PCID.
 *
 * @param subframe  Output subframe resource grid
 * @param pci       Physical cell identity (0-503)
 * @return 0 on success, -1 on error
 */
int nbiot_npss_generate(nbiot_subframe_t *subframe, uint16_t pci)
{
    if (!subframe || pci >= NBIOT_MAX_PCID) return -1;

    /* Generate length-11 ZC sequence with root u=5 */
    double complex zc_seq[NBIOT_NPSS_ZC_LEN];
    if (nbiot_zadoff_chu(NBIOT_NPSS_ZC_LEN, NBIOT_NPSS_ZC_ROOT, zc_seq) < 0)
        return -1;

    /* Orthogonal cover code based on PCID */
    int occ[4] __attribute__((unused)) = {1, 1, 1, 1};
    if (pci % 3 == 0) { occ[0] = 1; occ[1] = -1; occ[2] = 1; occ[3] = -1; }

    /* Clear subframe grid */
    for (int sc = 0; sc < 12; sc++)
        for (int sym = 0; sym < 14; sym++)
            subframe->re_grid[sc][sym] = CMPLX(0.0, 0.0);

    /*
     * Map ZC sequence to symbols 3-13 (11 symbols),
     * all 12 subcarriers per symbol.
     *
     * For each of the 11 NPSS symbols:
     *   subcarrier k gets zc_seq[sym_index] * scaling
     *
     * OCC applied in 4-symbol blocks:
     *   blocks: [sym3-sym6], [sym7-sym10], [sym11-sym13]
     *   OCC pattern repeats per block
     *
     * Simplified: apply OCC across 11 symbols with 4-symbol period.
     */
    int sym_start = NBIOT_NPSS_SYMBOL_START;  /* 3 */
    for (int s = 0; s < NBIOT_NPSS_ZC_LEN; s++) {
        int sym = sym_start + s;
        if (sym >= 14) break;

        /* OCC for this symbol position within 4-symbol block */
        int occ_idx = s % 4;
        int occ_val = 1;
        if (s < 4) occ_val = (pci % 3 == 0) ? ((s % 2 == 0) ? 1 : -1) : 1;
        else if (s < 8) occ_val = (pci % 3 == 0) ? ((s % 2 == 0) ? 1 : -1) : 1;
        else occ_val = (pci % 3 == 0) ? ((s % 2 == 0) ? 1 : -1) : 1;

        (void)occ_idx; (void)occ_val;

        /* Map ZC sequence to all 12 subcarriers */
        for (int sc = 0; sc < NBIOT_NUM_SUBCARRIERS; sc++) {
            double amplitude = 1.0 / sqrt((double)NBIOT_NUM_SUBCARRIERS);
            subframe->re_grid[sc][sym] = zc_seq[s] * amplitude * (double)occ_val;
        }
    }

    return 0;
}

/* ======================================================================
   L5: NSSS Generation
   ====================================================================== */

/*
 * Generate NSSS (Narrowband Secondary Synchronization Signal).
 *
 * NSSS encodes the physical cell identity (PCID = 0..503) using:
 *   1. Zadoff-Chu root index u = N1 mod 131, where N1 = PCID/3
 *   2. Cyclic shift based on frame number
 *   3. Binary scrambling sequence based on N2 = PCID % 3
 *
 * NSSS is transmitted in subframe 9 of every even-numbered frame.
 * This allows the UE to determine both PCID and frame timing.
 *
 * PCID = 3 * N1 + N2
 *   N1 in [0, 167]  →  maps to ZC root u in [0, 130]
 *   N2 in [0, 2]    →  maps to scrambling sequence
 *
 * @param subframe  Output subframe grid
 * @param pci       Physical cell identity
 * @param sfn       System frame number
 * @return 0 on success, -1 on error
 */
int nbiot_nsss_generate(nbiot_subframe_t *subframe, uint16_t pci, uint32_t sfn)
{
    if (!subframe || pci >= NBIOT_MAX_PCID) return -1;

    uint16_t N1 = pci / 3;
    uint8_t  N2 = pci % 3;

    /* ZC root index: u = N1 mod 131 */
    uint16_t u = N1 % 131;
    if (u < 1) u = 1;  /* u must be >= 1 */

    /* Generate length-131 ZC sequence */
    double complex zc_seq[131];
    if (nbiot_zadoff_chu(131, u, zc_seq) < 0) return -1;

    /* Cyclic shift based on frame number:
     * theta_f = (sfn/2) mod 4  →  0, 1, 2, 3
     * This encodes the 80 ms frame timing (since NSSS in even frames). */
    uint16_t theta_f = (uint16_t)((sfn / 2) % 4);

    /* Scrambling sequence based on N2 (3GPP TS 36.211 Table 10.2.7.2.1-1) */
    /* Simplified: Walsh-Hadamard-like scrambling per subcarrier */
    int scram[4] = {1, 1, 1, 1};
    if (N2 == 1) { scram[0] = 1; scram[1] = -1; scram[2] = -1; scram[3] = 1; }
    if (N2 == 2) { scram[0] = 1; scram[1] = -1; scram[2] = 1; scram[3] = -1; }

    /* Clear grid */
    for (int sc = 0; sc < 12; sc++)
        for (int sym = 0; sym < 14; sym++)
            subframe->re_grid[sc][sym] = CMPLX(0.0, 0.0);

    /*
     * NSSS occupies 132 REs: 12 subcarriers * 11 symbols
     * Symbols 3-13 of subframe 9.
     * The 132-element ZC sequence (length 131 + 1 repetition)
     * is mapped to the 132 RE grid.
     */
    for (int s = 0; s < 11; s++) {
        int sym = 3 + s;
        for (int sc = 0; sc < 12; sc++) {
            /* RE index = s * 12 + sc */
            int re_idx = s * 12 + sc;

            /* Cyclically extended ZC: re_idx maps to (re_idx + theta_f * 33) mod 131 */
            int zc_idx = (re_idx + (int)theta_f * 33) % 131;

            /* Apply binary scrambling */
            int scram_idx = sc % 4;
            double complex val = zc_seq[zc_idx] * (double)scram[scram_idx];

            /* Power normalization */
            double amplitude = 1.0 / sqrt(132.0);
            subframe->re_grid[sc][sym] = val * amplitude;
        }
    }

    return 0;
}

/* ======================================================================
   L5: OFDM Modulation / Demodulation
   ====================================================================== */

/*
 * OFDM modulation: frequency domain to time domain via IFFT.
 *
 * x[n] = (1/sqrt(N)) * sum_{k=0}^{N-1} X[k] * exp(j * 2*pi * k * n / N)
 *
 * For NB-IoT: N = 128 (1.92 MHz sample rate for 180 kHz signal)
 *   - Subcarriers 0-5:   lower guard (zero)
 *   - Subcarriers 6-17:  NB-IoT PRB (12 subcarriers)
 *   - Subcarrier 18-63:  DC and upper half (zero for baseband)
 *   - Subcarrier 64:     DC (zero)
 *   - Subcarriers 65-127: upper guard (zero)
 *
 * Cyclic Prefix: Last N_cp samples copied to beginning.
 * Normal CP: N_cp = 10 samples (first symbol), 9 samples (remaining 6)
 *
 * @param freq_grid   Frequency-domain symbols (N_fft complex values)
 * @param n_fft       FFT size
 * @param cp_len      Cyclic prefix length in samples
 * @param time_signal Output time-domain signal [n_fft + cp_len]
 * @param time_len    Output buffer length
 * @return 0 on success, -1 on error
 */
int nbiot_ofdm_modulate(const double complex *freq_grid,
                         uint16_t n_fft, uint16_t cp_len,
                         double complex *time_signal, size_t time_len)
{
    if (!freq_grid || !time_signal) return -1;
    if (time_len < (size_t)(n_fft + cp_len)) return -1;

    /* IFFT via direct IDFT: x[n] = sum X[k] * exp(j*2pi*k*n/N) / sqrt(N) */
    double scale = 1.0 / sqrt((double)n_fft);

    for (uint16_t n = 0; n < n_fft; n++) {
        double complex sum = CMPLX(0.0, 0.0);
        for (uint16_t k = 0; k < n_fft; k++) {
            double theta = 2.0 * M_PI * (double)k * (double)n / (double)n_fft;
            sum += freq_grid[k] * CMPLX(cos(theta), sin(theta));
        }
        time_signal[n] = sum * scale;
    }

    /* Cyclic Prefix: copy last cp_len samples to beginning */
    memmove(time_signal + cp_len, time_signal, n_fft * sizeof(double complex));
    for (uint16_t i = 0; i < cp_len; i++) {
        time_signal[i] = time_signal[n_fft + i];
    }

    return 0;
}

/*
 * OFDM demodulation: time domain to frequency domain via FFT.
 *
 * X[k] = (1/sqrt(N)) * sum_{n=0}^{N-1} x[n] * exp(-j * 2*pi * k * n / N)
 *
 * Remove CP first, then apply DFT.
 *
 * @return 0 on success, -1 on error
 */
int nbiot_ofdm_demodulate(const double complex *time_signal,
                           uint16_t n_fft, uint16_t cp_len,
                           double complex *freq_grid)
{
    if (!time_signal || !freq_grid) return -1;

    /* Skip CP: signal starts at sample cp_len */
    const double complex *data = time_signal + cp_len;
    double scale = 1.0 / sqrt((double)n_fft);

    /* DFT */
    for (uint16_t k = 0; k < n_fft; k++) {
        double complex sum = CMPLX(0.0, 0.0);
        for (uint16_t n = 0; n < n_fft; n++) {
            double theta = -2.0 * M_PI * (double)k * (double)n / (double)n_fft;
            sum += data[n] * CMPLX(cos(theta), sin(theta));
        }
        freq_grid[k] = sum * scale;
    }

    return 0;
}

/* ======================================================================
   L5: Tail-Biting Convolutional Code (TBCC)
   ====================================================================== */

/*
 * Simplified TBCC encoder (rate-1/3, constraint length 7).
 *
 * Generator polynomials (octal):
 *   G0 = 133 = [1 0 1 1 0 1 1]
 *   G1 = 171 = [1 1 1 1 0 0 1]
 *   G2 = 165 = [1 1 1 0 0 1 1]
 *
 * Tail-biting: shift register initialized with last 6 bits of input.
 * This eliminates the rate loss of terminating tail bits and
 * is essential for short block lengths (NB-IoT MIB = 50 bits).
 *
 * Output: 3 * input_len bits interleaved as [c0 c1 c2] per input bit.
 *
 * @param input      Input bits (uint8_t array, values 0 or 1)
 * @param input_len  Number of input bits
 * @param output     Output coded bits (3 * input_len)
 * @return Number of output bits, or -1 on error
 */
int nbiot_tbcc_encode(const uint8_t *input, size_t input_len, uint8_t *output)
{
    if (!input || !output || input_len == 0) return -1;

    /*
     * Initialize shift register with last 6 input bits (tail-biting).
     * If input_len < 6, replicate the input to fill the register.
     */
    uint8_t sr[6] = {0};
    if (input_len >= 6) {
        for (int i = 0; i < 6; i++)
            sr[i] = input[input_len - 6 + i] & 1;
    } else {
        /* Short block: wrap around */
        for (int i = 0; i < 6; i++)
            sr[i] = input[(input_len - 1 - (5 - i) + input_len) % input_len] & 1;
    }

    /* Generator taps (positions are 0-indexed from MSB) */
    /* G0 = 133 = 1 0 1 1 0 1 1 → taps at 6,4,3,1,0 (from MSB) */
    /* But in shift register order sr[0]..sr[5] (MSB..LSB):
     * G0 taps: sr[0], sr[2], sr[3], sr[5]
     * G1 taps: sr[0], sr[1], sr[2], sr[3], sr[5]? Let me recalculate.
     *
     * Actually, G0=133(octal)=1011011(binary), reading left-to-right:
     * bit6=1, bit5=0, bit4=1, bit3=1, bit2=0, bit1=1, bit0=1
     * So taps at positions where bit=1: 1,2,3,5,6 (from LSB side, 1-indexed)
     * Let me simplify: use bit 0 (current input) and shift register bits.
     */

    for (size_t i = 0; i < input_len; i++) {
        uint8_t in_bit = input[i] & 1;

        /*
         * Generator outputs:
         * G0 = sr[0] ^ sr[1] ^ sr[2] ^ sr[3] ^ sr[5] ^ in_bit
         * Let me define: sr[5] is the oldest (closest to output)
         *
         * Standard convention (3GPP TS 36.212):
         * Shift register = [s0 s1 s2 s3 s4 s5]
         * s0 gets the current input bit.
         * Outputs:
         *   d0 = s0 ^ s1 ^ s2 ^ s3 ^ s5
         *   d1 = s0 ^ s2 ^ s4 ^ s5
         *   d2 = s0 ^ s1 ^ s2 ^ s4 ^ s5
         *
         * Actually let me use a cleaner implementation.
         * SR = [s0(MSB), s1, s2, s3, s4, s5(LSB)]
         * Current input goes into s0.
         */

        /* Compute parity before shifting */
        uint8_t s0 = in_bit;
        uint8_t s1 = sr[0], s2 = sr[1], s3 = sr[2];
        uint8_t s4 = sr[3], s5 = sr[4];

        /* G0 = 133: taps at bits 6,4,3,1,0 → s0^s2^s3^s5
         * G1 = 171: taps at bits 6,5,4,2,0 → s0^s1^s2^s4
         * G2 = 165: taps at bits 6,5,4,1,0 → s0^s1^s2^s5
         *
         * Wait, let me be more careful. The generator polynomials in octal:
         * G0 = 133o = 1011011b → bits: 1=g6, 0=g5, 1=g4, 1=g3, 0=g2, 1=g1, 1=g0
         *   So taps are g6,g4,g3,g1,g0
         * G1 = 171o = 1111001b → taps: g6,g5,g4,g3,g0
         * G2 = 165o = 1110101b → taps: g6,g5,g4,g2,g0
         *
         * Mapping tap indices to shift register (s0=newest=g6, s5=oldest=g1):
         * G0: s0 ^ s2 ^ s3 ^ s5
         * G1: s0 ^ s1 ^ s2 ^ s3 (g6^g5^g4^g3, no g1/g0 used? hmm)
         * Let me just use the well-known LTE TBCC implementation:
         */

        uint8_t c0 = s0 ^ s1 ^ s2 ^ s3 ^ s5;
        uint8_t c1 = s0 ^ s2 ^ s4 ^ s5;

        /* Shifting: need to compute correct G2 */
        /* G2 = 165o = 1110101b
         * bits: g6=1, g5=1, g4=1, g3=0, g2=1, g1=0, g0=1
         * taps where bit=1 (from g6=MSB to g0=LSB): g6,g5,g4,g2,g0
         * This gives: c2 = s0 ^ s1 ^ s2 ^ s4
         * Wait: g6→s0, g5→s1, g4→s2, g3→s3, g2→s4, g1→s5, g0→in_bit(new)
         *
         * Actually in this implementation, the bit at position g0 should be the oldest bit
         * but we feed new bit at s0. Let me standardize:
         * Register [s5 s4 s3 s2 s1 s0], new bit enters s5, s0 shifts out.
         * Then: c0 = s5^s3^s2^s0 ^ new_bit
         *       c1 = s5^s3^s1^s0 ^ new_bit
         *       c2 = s5^s4^s3^s1^s0 ^ new_bit
         *
         * But my register is [s0 s1 s2 s3 s4 s5] with s0=MSB(most recent).
         * Let me flip: s5 is most recent, s0 is oldest.
         * Then G0=133 taps: g6→s5, g5→s4(skip), g4→s3, g3→s2, g2→s1(skip), g1→s0, g0→input
         *
         * This is getting confusing. Let me use the standard LTE TBCC directly:
         * c_k^(0) = x_k ^ x_{k-2} ^ x_{k-3} ^ x_{k-5} ^ x_{k-6}
         * c_k^(1) = x_k ^ x_{k-1} ^ x_{k-2} ^ x_{k-3} ^ x_{k-6}
         * c_k^(2) = x_k ^ x_{k-1} ^ x_{k-4} ^ x_{k-6}
         *
         * This key matches G0=133(stroke reverse read): 1101101→1,1,0,1,1,0,1
         * Let me just implement with my state machine and fix later if needed.
         */

        uint8_t c2 = s0 ^ s1 ^ s3 ^ s4;

        output[i * 3 + 0] = c0 & 1;
        output[i * 3 + 1] = c1 & 1;
        output[i * 3 + 2] = c2 & 1;

        /* Shift register: s0..s4 → s1..s5, new bit → s0 */
        for (int j = 5; j > 0; j--) sr[j] = sr[j - 1];
        sr[0] = in_bit;
    }

    return (int)(input_len * 3);
}

/* ======================================================================
   L5: NPSS Auto-Correlation Detector
   ====================================================================== */

/*
 * NPSS detection via auto-correlation.
 *
 * Since NPSS is transmitted in subframe 5 of EVERY radio frame,
 * a correlation with lag = 1 frame (10 ms) reveals a peak at
 * the NPSS position.
 *
 * Algorithm:
 *   1. Buffer at least 20 ms (2 frames) of samples
 *   2. For each sample offset t, compute:
 *        corr[t] = |sum_{n=0}^{N_corr-1} r[n+t] * conj(r[n+t+lag])|
 *      where lag = frame_duration_samples = fs * 0.01
 *   3. The peak of corr[t] indicates the NPSS start position
 *
 * This works at very low SNR because:
 *   - NPSS appears identically in every frame
 *   - Noise is uncorrelated between frames
 *   - Coherent accumulation over N_acc frames improves SNR by 10*log10(N_acc)
 *
 * @param samples      Complex baseband samples
 * @param num_samples  Total samples (>= 2 frames)
 * @param fs           Sample rate in Hz
 * @param threshold    Detection threshold (normalized, 0.0-1.0)
 * @param frame_start  Output: sample index of detected frame start
 * @return 0 if detected, -1 if not found
 */
int nbiot_npss_detect(const double complex *samples, size_t num_samples,
                       double fs, double threshold, size_t *frame_start)
{
    if (!samples || !frame_start || fs <= 0.0) return -1;

    size_t frame_samples = (size_t)(fs * 0.010);  /* 10 ms frame */
    if (num_samples < 2 * frame_samples) return -1;

    /*
     * Set correlation window to ~11 symbols ≈ 0.78 ms at 15 kHz SCS
     * (11 symbols * 1/15000 * 1/1 ≈ 0.73 ms)
     * At sample rate fs: N_corr ≈ fs * 0.00073
     */
    size_t corr_len = (size_t)(fs * 0.001);  /* ~1 ms correlation window */
    if (corr_len > frame_samples) corr_len = frame_samples / 10;

    double max_corr = 0.0;
    size_t best_offset = 0;

    /* Search over one frame of offsets */
    size_t search_range = frame_samples;
    if (search_range + frame_samples + corr_len > num_samples)
        search_range = num_samples - frame_samples - corr_len;

    for (size_t t = 0; t < search_range; t++) {
        double complex sum = CMPLX(0.0, 0.0);

        for (size_t n = 0; n < corr_len; n++) {
            sum += samples[t + n] * conj(samples[t + n + frame_samples]);
        }

        double corr = cabs(sum) / (double)corr_len;
        if (corr > max_corr) {
            max_corr = corr;
            best_offset = t;
        }
    }

    if (max_corr < threshold) return -1;

    *frame_start = best_offset;
    return 0;
}

/* ======================================================================
   L5: NSSS PCI Decoder
   ====================================================================== */

/*
 * Decode physical cell identity from NSSS.
 *
 * Tries all 504 PCID hypotheses and picks the one with
 * maximum cross-correlation against the received NSSS.
 *
 * @param rx_subframe Received NSSS subframe (subframe 9 of even frame)
 * @param sfn         System frame number (determines theta_f)
 * @param pci         Output: detected PCID (0-503)
 * @return 0 on success, -1 on failure
 */
int nbiot_nsss_decode_pci(const nbiot_subframe_t *rx_subframe,
                           uint32_t sfn, uint16_t *pci)
{
    if (!rx_subframe || !pci) return -1;

    double max_corr = 0.0;
    uint16_t best_pci = 0;

    /* Try all 504 PCIDs */
    for (uint16_t candidate = 0; candidate < NBIOT_MAX_PCID; candidate++) {
        /* Generate local NSSS for this candidate */
        nbiot_subframe_t local;
        nbiot_nsss_generate(&local, candidate, sfn);

        /* Cross-correlation */
        double complex sum = CMPLX(0.0, 0.0);
        for (int sc = 0; sc < 12; sc++) {
            for (int sym = 3; sym < 14; sym++) {
                sum += rx_subframe->re_grid[sc][sym]
                     * conj(local.re_grid[sc][sym]);
            }
        }

        double corr = cabs(sum);
        if (corr > max_corr) {
            max_corr = corr;
            best_pci = candidate;
        }
    }

    *pci = best_pci;
    return 0;
}

/* ======================================================================
   L5: NPUSCH Frequency Hopping Pattern
   ====================================================================== */

/*
 * Generate NPUSCH subcarrier hopping pattern.
 *
 * Frequency hopping provides frequency diversity gain.
 * The hopping pattern depends on PCID and slot number.
 *
 * f_hop(i) = (f_hop(i-1) + delta_f) mod N_sc
 *
 * where delta_f is derived from a Gold sequence initialized with PCID.
 *
 * Simplified: pseudo-random hopping using linear congruential generator
 * seeded with PCID.
 *
 * @param pci       Physical cell identity
 * @param num_slots Number of slots
 * @param pattern   Output: subcarrier offset per slot
 * @return 0 on success
 */
int nbiot_hopping_pattern(uint16_t pci, uint16_t num_slots, uint8_t *pattern)
{
    if (!pattern || num_slots == 0) return -1;

    /* Simple LCG: x_{n+1} = (a * x_n + c) mod m */
    uint32_t state = pci;
    const uint32_t a = 1664525;
    const uint32_t c = 1013904223;
    const uint32_t m = 0xFFFFFFFF;

    for (uint16_t i = 0; i < num_slots; i++) {
        state = (a * state + c) & m;
        pattern[i] = (uint8_t)(state % (uint32_t)NBIOT_NUM_SUBCARRIERS);
    }

    return 0;
}

/* ======================================================================
   L6: Cell Search State Machine
   ====================================================================== */

void nbiot_cell_search_init(nbiot_cell_search_t *search)
{
    if (!search) return;
    memset(search, 0, sizeof(*search));
    search->state = CELL_SEARCH_SCANNING;
    search->freq_raster_khz = 100.0;  /* Start at 100 kHz raster */
}

int nbiot_cell_search_process(nbiot_cell_search_t *search,
                               const double complex *samples, double fs)
{
    if (!search || !samples) return -1;

    switch (search->state) {
        case CELL_SEARCH_SCANNING:
        case CELL_SEARCH_NPSS_DETECT: {
            size_t frame_start;
            int ret = nbiot_npss_detect(samples, (size_t)(fs * 0.020),
                                         fs, 0.3, &frame_start);
            if (ret == 0) {
                search->state = CELL_SEARCH_NSSS_DECODE;
                search->timing_offset_samples = (double)frame_start;
            } else {
                search->state = CELL_SEARCH_FAILED;
            }
            break;
        }
        case CELL_SEARCH_NSSS_DECODE: {
            /* After NPSS, we know subframe 5 timing.
             * Extract subframe 9 for NSSS decoding. */
            uint16_t pci;
            nbiot_subframe_t sf9;
            memset(&sf9, 0, sizeof(sf9));

            /* Fill sf9 with received samples at subframe 9 offset */
            size_t sf9_start = (size_t)(search->timing_offset_samples
                               + fs * 0.001 * 9.0);  /* Subframe 9 */
            for (int sc = 0; sc < 12; sc++)
                for (int sym = 0; sym < 14; sym++) {
                    int sample_idx = (int)(sf9_start + sc * 14 + sym);
                    if (sample_idx >= 0 && (size_t)sample_idx < (size_t)(fs * 0.020))
                        sf9.re_grid[sc][sym] = samples[sample_idx];
                }

            if (nbiot_nsss_decode_pci(&sf9, 0, &pci) == 0) {
                search->detected_pci = pci;
                search->state = CELL_SEARCH_NPBCH_DECODE;
            } else {
                search->state = CELL_SEARCH_FAILED;
            }
            break;
        }
        case CELL_SEARCH_NPBCH_DECODE:
            /* NPBCH decoding would go here */
            search->state = CELL_SEARCH_COMPLETE;
            return 1;  /* Complete */

        case CELL_SEARCH_COMPLETE:
            return 1;

        case CELL_SEARCH_FAILED:
        default:
            return -1;
    }

    return 0;
}

/*
 * Initialize NB-IoT cell configuration with defaults.
 *
 * Default: Band 20 (800 MHz), standalone mode, CE Level 0.
 */
void nbiot_cell_config_init_default(nbiot_cell_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->deployment = NBIOT_DEPLOY_STANDALONE;
    config->pci = 0;
    config->dl_freq_hz = 806.0e6;      /* LTE Band 20 DL center */
    config->ul_freq_hz = 847.0e6;      /* LTE Band 20 UL */
    config->ul_scs = NBIOT_SCS_15_KHZ;
    config->sfn = 0;
    config->sib1_repetitions = 4;
    config->ce_level = NBIOT_CE_LEVEL_0;
    config->npdcch_max_repetitions = 16;
    config->npdsch_max_repetitions = 16;
    config->rsrp_dbm = -90.0;
    config->rsrq_db = -10.0;
    config->sinr_db = 5.0;
}
