/**
 * @file lora_modem.c
 * @brief LoRa CSS Modulator/Demodulator -- complete PHY implementation
 *
 * Knowledge: L3 complex baseband chirp signal synthesis,
 *            L5 FFT dechirp demodulation, Hamming(7,4) FEC,
 *            CRC-16, data whitening, packet airtime calculation
 *            L6 preamble generation, parameter validation
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "lora_phy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Normalize phase to [-pi, pi] for numerical stability */
static double norm_phase(double ph) {
    ph = fmod(ph, 2.0 * M_PI);
    if (ph > M_PI)  ph -= 2.0 * M_PI;
    if (ph < -M_PI) ph += 2.0 * M_PI;
    return ph;
}

/*
 * Base down-chirp: d[n] = exp(-j * pi * n^2 / N)
 * This is the conjugate of the standard up-chirp.
 * Used for dechirping: multiplying received signal by d* converts
 * the modulated chirp into a pure tone for FFT detection.
 */
static double complex base_down_chirp(uint32_t n, uint32_t N) {
    double ph = -M_PI * (double)(n * n) / (double)N;
    return CMPLX(cos(ph), sin(ph));
}

/*
 * Base up-chirp: u[n] = exp(+j * pi * n^2 / N)
 * Unmodulated up-chirp used in preamble for receiver synchronization.
 */
static double complex base_up_chirp(uint32_t n, uint32_t N) {
    double ph = M_PI * (double)(n * n) / (double)N;
    return CMPLX(cos(ph), sin(ph));
}

/* ======================================================================
   L3: Complex Baseband Chirp Generation
   ====================================================================== */

/*
 * Generate one sample of a LoRa modulated chirp.
 *
 * Mathematical model:
 *   s[n] = exp(j * pi * (2*k*n + n^2) / N)
 *
 * where:
 *   k = symbol value in [0, N-1], carrying SF bits
 *   N = 2^SF = number of chips per symbol
 *   n = chip index in [0, N-1]
 *
 * The instantaneous frequency profile f[n] creates a sawtooth:
 *   f[n] = (BW/N) * ((k + n) mod N) - BW/2
 *
 * When f[n] reaches +BW/2, it wraps to -BW/2. This cyclic frequency
 * shift is the defining feature of Chirp Spread Spectrum.
 *
 * The frequency wrapping is encoded in the "(symbol + n) mod N" term
 * which cyclically shifts the base chirp's time-frequency trajectory.
 *
 * @param symbol    Symbol value [0, 2^SF-1]
 * @param chip_idx  Sample index within symbol [0, 2^SF-1]
 * @param sample_rate Not used (critical sampling assumed)
 * @return Complex baseband sample with unity average power per symbol
 */
double complex lora_chirp_sample(const lora_phy_params_t *params,
                                  uint32_t symbol, uint32_t chip_idx,
                                  double sample_rate)
{
    (void)sample_rate;
    if (!params) return CMPLX(0.0, 0.0);
    if (chip_idx >= params->num_chips) return CMPLX(0.0, 0.0);

    uint32_t N = params->num_chips;
    uint32_t n = chip_idx;

    /*
     * LoRa CSS Modulation: Symbol value k is encoded by cyclically
     * shifting the base up-chirp in time by k samples.
     *
     * Base up-chirp:   u[n] = exp(j * pi * n^2 / N)
     * Modulated chirp: s[n] = u[(n + k) mod N]
     *                       = exp(j * pi * ((n+k) mod N)^2 / N)
     *
     * This time-domain cyclic shift produces a frequency-domain
     * shift after dechirping. Specifically:
     *   Dechirp: r[n] = s[n] * conj(down_chirp[n]) = s[n] * exp(-j*pi*n^2/N)
     *   = exp(j*pi*(((n+k) mod N)^2 - n^2)/N)
     *
     * For most n: ((n+k) mod N)^2 = (n+k)^2 = n^2 + 2kn + k^2
     *   → r[n] = exp(j*pi*(2kn + k^2)/N) = exp(j*2pi*k*n/N) * const
     *   → Pure tone at frequency k/N after dechirping!
     *
     * The DFT then peaks at bin k, recovering the symbol value.
     */
    uint32_t idx = (n + symbol) % N;
    double phase = M_PI * (double)(idx * idx) / (double)N;
    phase = norm_phase(phase);

    /* Unity average power per symbol: E[|s|^2] = 1/N * sum |s[n]|^2 = 1/N */
    double amplitude = 1.0 / sqrt((double)N);

    return CMPLX(amplitude * cos(phase), amplitude * sin(phase));
}

/* ======================================================================
   L3: Preamble Generation
   ====================================================================== */

/*
 * Generate complete LoRa preamble including sync word and SFD.
 *
 * Preamble structure (LoRaWAN Spec 1.0.4):
 *   [N_preamble unmodulated up-chirps]
 *   [2 modulated sync word chirps]
 *   [2.25 down-chirp symbols as SFD]
 *
 * Total preamble duration: (N_preamble + 4.25) * T_sym
 *
 * The preamble serves three purposes:
 *   1. Signal detection: the periodic up-chirps enable energy detection
 *      and auto-correlation at very low SNR.
 *   2. Synchronization: the chirp edges provide symbol timing.
 *   3. Frequency offset estimation: the preamble phase slope gives
 *      coarse carrier frequency offset (CFO).
 *
 * @param buffer  Pre-allocated complex sample buffer
 * @param max_len Maximum samples to write
 * @return Number of samples written, or -1 on error
 */
int lora_generate_preamble(const lora_phy_params_t *params,
                            double complex *buffer, size_t max_len)
{
    if (!params || !buffer) return -1;
    uint32_t N = params->num_chips;
    if (N == 0) return -1;

    uint32_t total_chips = (uint32_t)params->preamble_len * N
                         + (uint32_t)(2.25 * N);
    if (total_chips > max_len) return -1;

    uint32_t idx = 0;

    /* Section 1: N_preamble unmodulated up-chirps.
     * The receiver uses auto-correlation across these identical symbols
     * to detect the signal and estimate coarse timing. */
    for (uint32_t s = 0; s < params->preamble_len; s++) {
        for (uint32_t n = 0; n < N; n++)
            buffer[idx + n] = base_up_chirp(n, N);
        idx += N;
    }

    /* Section 2: Sync word (2 modulated chirps).
     * The sync word value (0x34 = public LoRaWAN) is transmitted
     * as two consecutive chirp symbols. This provides network
     * separation and fine symbol timing alignment. */
    uint32_t sv = params->sync_word;
    for (uint32_t s = 0; s < 2; s++) {
        for (uint32_t n = 0; n < N; n++)
            buffer[idx + n] = lora_chirp_sample(params, sv, n, params->chip_rate);
        idx += N;
    }

    /* Section 3: SFD (Start Frame Delimiter) -- 0.25 down-chirps.
     * The reversal of chirp direction marks the end of preamble.
     * Down-chirps sweep from +BW/2 to -BW/2, opposite to up-chirps. */
    for (uint32_t n = 0; n < N / 4; n++)
        buffer[idx + n] = base_down_chirp(n, N);
    idx += N / 4;

    return (int)idx;
}

/* ======================================================================
   L5: FFT-based Dechirp Demodulation
   ====================================================================== */

/* Squared complex magnitude -- avoids sqrt for peak detection */
static double cmag_sq(double complex z) {
    double r = creal(z), i = cimag(z);
    return r * r + i * i;
}

/*
 * Demodulate a LoRa symbol using dechirping + DFT peak detection.
 *
 * Algorithm (3 steps):
 *
 * Step 1 -- Dechirp:
 *   Multiply received samples by conjugate of base down-chirp:
 *     dc[n] = r[n] * conj(d[n])
 *           = r[n] * exp(+j * pi * n^2 / N)
 *
 *   If r[n] contains symbol k: r[n] = exp(j*pi*(2k*n + n^2)/N) + noise
 *   Then dc[n] = exp(j*2pi*k*n/N) + noise * exp(j*pi*n^2/N)
 *              = pure tone at frequency k/N + rotated noise
 *
 *   Since rotating AWGN preserves its statistics, we now have
 *   a pure complex exponential in AWGN -- the classic frequency
 *   estimation problem.
 *
 * Step 2 -- DFT:
 *   Compute X[k] = sum_{n=0}^{N-1} dc[n] * exp(-j*2pi*k*n/N)
 *
 *   This is a coherent integration: the signal component adds
 *   constructively (N fold), while noise adds incoherently (sqrt(N) fold).
 *   Hence SNR improvement = 10*log10(N) = processing gain.
 *
 * Step 3 -- Peak detection:
 *   symbol_hat = argmax_{k} |X[k]|
 *
 *   The DFT bin k directly corresponds to the transmitted symbol.
 *   This is because the dechirping removes the quadratic phase,
 *   leaving only the linear frequency term encoding the symbol.
 *
 * Complexity: O(N^2) for this educational DFT implementation.
 * Production uses FFT at O(N log N).
 *
 * @return Demodulated symbol in [0, 2^SF-1], or -1 on error
 */
int lora_demodulate_symbol_fft(const lora_phy_params_t *params,
                                const double complex *samples,
                                size_t num_samples)
{
    if (!params || !samples) return -1;
    if (num_samples != params->num_chips) return -1;

    uint32_t N = params->num_chips;
    if (N == 0 || N > 4096) return -1;

    /* Step 1: Dechirp -- convert chirp to tone */
    double complex dc[4096];
    for (uint32_t n = 0; n < N; n++)
        dc[n] = samples[n] * conj(base_down_chirp(n, N));

    /* Step 2 + 3: DFT + peak detection */
    double mag_max = 0.0;
    int    peak_bin = -1;

    for (uint32_t k = 0; k < N; k++) {
        double complex Xk = CMPLX(0.0, 0.0);
        double theta_step = -2.0 * M_PI * (double)k / (double)N;

        /* DFT accumulation */
        for (uint32_t n = 0; n < N; n++) {
            double theta = theta_step * (double)n;
            Xk += dc[n] * CMPLX(cos(theta), sin(theta));
        }

        double msq = cmag_sq(Xk);
        if (msq > mag_max) {
            mag_max = msq;
            peak_bin = (int)k;
        }
    }

    return peak_bin;
}

/*
 * Standalone dechirp operation (without DFT).
 * Multiplies received samples by conjugate of base down-chirp
 * in-place, preparing them for FFT detection.
 *
 * r[n] = r[n] * conj(base_down_chirp[n])
 */
int lora_dechirp(const lora_phy_params_t *params,
                  double complex *rx, size_t num_samples)
{
    if (!params || !rx) return -1;
    if (num_samples != params->num_chips) return -1;

    uint32_t N = params->num_chips;
    for (uint32_t n = 0; n < N; n++)
        /* dechirp: multiply by conj(base_up_chirp) = base_down_chirp */
        rx[n] = rx[n] * base_down_chirp(n, N);
    return 0;
}

/* ======================================================================
   L5: Streaming Chirp Generator
   ====================================================================== */

/*
 * Initialize chirp generator state for streaming synthesis.
 *
 * Sets the starting instantaneous frequency based on:
 *   - symbol value determines initial frequency offset
 *   - direction: +1 for up-chirp (freq increases), -1 for down-chirp
 *
 * For symbol k, the starting frequency is:
 *   f_start = k * BW / 2^SF - BW/2   (up-chirp)
 *   f_start = +BW/2                   (down-chirp, sweeps downward from top)
 */
void lora_chirp_gen_init(lora_chirp_gen_t *state,
                          const lora_phy_params_t *params,
                          uint32_t symbol, int direction)
{
    if (!state || !params) return;

    double f0 = ((double)symbol / (double)params->num_chips)
                * (double)params->bw - (double)params->bw / 2.0;
    if (direction < 0) f0 = (double)params->bw / 2.0;

    state->phase_accum  = 0.0;
    state->freq_instant = f0;
    state->direction    = direction;
    state->chip_idx     = 0;
    state->symbol_idx   = 0;
    state->phase_step   = 0.0;
}

/*
 * Generate next chirp sample and advance state.
 *
 * Uses a phase accumulator with linear frequency sweep:
 *   phi(t + dt) = phi(t) + 2*pi * f(t) * dt
 *   f(t + dt)   = f(t) + mu * direction * dt
 *
 * where mu = chirp rate (BW / T_sym) in Hz/s.
 *
 * Frequency wrapping: when f exceeds BW/2 or goes below -BW/2,
 * it wraps to the opposite band edge. This creates the sawtooth
 * frequency profile characteristic of CSS modulation.
 */
double complex lora_chirp_gen_next(lora_chirp_gen_t *state,
                                    const lora_phy_params_t *params,
                                    double fs)
{
    if (!state || !params) return CMPLX(0.0, 0.0);

    double T_s = 1.0 / fs;
    double mu  = params->chirp_rate;
    double bw  = (double)params->bw;

    /* Current phase and frequency */
    double ph = state->phase_accum;
    double fr = state->freq_instant;

    /* Output current sample */
    double complex smp = CMPLX(cos(ph), sin(ph));

    /* Phase update: delta_phi = 2*pi * f * T_s */
    state->phase_accum += 2.0 * M_PI * fr * T_s;
    state->phase_accum = norm_phase(state->phase_accum);

    /* Frequency update: delta_f = mu * direction * T_s */
    fr += mu * (double)state->direction * T_s;

    /* Frequency wrapping at band edges */
    if (fr > bw / 2.0)      fr -= bw;
    else if (fr < -bw / 2.0) fr += bw;

    state->freq_instant = fr;
    state->chip_idx++;

    /* Symbol boundary crossing */
    if (state->chip_idx >= params->num_chips) {
        state->chip_idx = 0;
        state->symbol_idx++;
    }

    return smp;
}

/* ======================================================================
   L5: Hamming(7,4) Forward Error Correction
   ====================================================================== */

/*
 * Systematic Hamming(7,4) encoder.
 *
 * Codeword layout: [p1, p2, d1, p3, d2, d3, d4]
 *
 * Parity equations (from parity-check matrix):
 *   p1 = d1 XOR d2 XOR d4   (covers positions 3,5,7)
 *   p2 = d1 XOR d3 XOR d4   (covers positions 3,6,7)
 *   p3 = d2 XOR d3 XOR d4   (covers positions 5,6,7)
 *
 * Minimum Hamming distance = 3, enabling:
 *   - Single-bit error correction
 *   - Double-bit error detection
 *
 * CR-dependent output length:
 *   CR=1 (4/5): 5 bits transmitted (drop 2 parity)
 *   CR=2 (4/6): 6 bits (drop 1 parity)
 *   CR=3 (4/7): 7 bits (full Hamming codeword)
 *   CR=4 (4/8): 8 bits (adds overall parity for double-error detection)
 *
 * @return Number of output bits (4 + CR), or -1 on error
 */
int lora_hamming_encode(uint8_t nibble, lora_coding_rate_t cr,
                         uint8_t *codeword)
{
    if (!codeword) return -1;

    /* Extract 4 data bits from low nibble */
    uint8_t d1 = (nibble >> 3) & 1;
    uint8_t d2 = (nibble >> 2) & 1;
    uint8_t d3 = (nibble >> 1) & 1;
    uint8_t d4 =  nibble       & 1;

    /* Compute parity bits */
    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p3 = d2 ^ d3 ^ d4;

    /* Build systematic codeword */
    codeword[0] = p1;
    codeword[1] = p2;
    codeword[2] = d1;
    codeword[3] = p3;
    codeword[4] = d2;
    codeword[5] = d3;
    codeword[6] = d4;

    switch (cr) {
        case LORA_CR_4_5: return 5;
        case LORA_CR_4_6: return 6;
        case LORA_CR_4_7: return 7;
        case LORA_CR_4_8:
            /* Extra overall parity for rate 1/2 */
            codeword[7] = p1 ^ p2 ^ p3 ^ d1 ^ d2 ^ d3 ^ d4;
            return 8;
        default: return -1;
    }
}

/*
 * Hamming(7,4) decoder with single-bit error correction.
 *
 * Syndrome calculation:
 *   s1 = p1 XOR d1 XOR d2 XOR d4
 *   s2 = p2 XOR d1 XOR d3 XOR d4
 *   s3 = p3 XOR d2 XOR d3 XOR d4
 *
 * Syndrome-to-position mapping:
 *   [s1 s2 s3] bits decode to error position 1-7:
 *   001=bit1(p1), 010=bit2(p2), 011=bit3(d1), 100=bit4(p3),
 *   101=bit5(d2), 110=bit6(d3), 111=bit7(d4)
 *   [000] = no error
 *
 * For rates with missing parity bits (CR=4/5, 4/6), missing data
 * bits are estimated through parity equation consistency.
 *
 * @return 0=no error, 1=single error corrected, -1=uncorrectable
 */
int lora_hamming_decode(const uint8_t *codeword, lora_coding_rate_t cr,
                         uint8_t *nibble)
{
    if (!codeword || !nibble) return -1;

    uint8_t p1, p2, d1, p3, d2, d3, d4;

    /* Extract bits based on coding rate */
    if (cr >= LORA_CR_4_7) {
        /* Full 7-bit codeword available */
        p1 = codeword[0] & 1; p2 = codeword[1] & 1; d1 = codeword[2] & 1;
        p3 = codeword[3] & 1; d2 = codeword[4] & 1; d3 = codeword[5] & 1;
        d4 = codeword[6] & 1;
    } else if (cr == LORA_CR_4_6) {
        /* 6 bits: d4 missing from direct observation */
        p1 = codeword[0] & 1; p2 = codeword[1] & 1; d1 = codeword[2] & 1;
        p3 = codeword[3] & 1; d2 = codeword[4] & 1; d3 = codeword[5] & 1;
        /* Estimate d4 from parity: p1 = d1^d2^d4 => d4 = p1^d1^d2
         *                       p2 = d1^d3^d4 => d4 = p2^d1^d3
         * If both estimates agree, use that value. */
        uint8_t d4a = p1 ^ d1 ^ d2;
        uint8_t d4b = p2 ^ d1 ^ d3;
        d4 = (d4a == d4b) ? d4a : 0;
    } else {
        /* CR=4/5: 5 bits. d3 and d4 unknown.
         * Brute-force 4 combinations, pick one minimizing parity errors. */
        p1 = codeword[0] & 1; p2 = codeword[1] & 1; d1 = codeword[2] & 1;
        p3 = codeword[3] & 1; d2 = codeword[4] & 1;
        int best_err = 99, best_d3 = 0, best_d4 = 0;
        for (uint8_t t3 = 0; t3 <= 1; t3++) {
            for (uint8_t t4 = 0; t4 <= 1; t4++) {
                int e = (p1 != (d1 ^ d2 ^ t4))
                      + (p2 != (d1 ^ t3 ^ t4))
                      + (p3 != (d2 ^ t3 ^ t4));
                if (e < best_err) {
                    best_err = e; best_d3 = t3; best_d4 = t4;
                }
            }
        }
        d3 = (uint8_t)best_d3; d4 = (uint8_t)best_d4;
        if (best_err > 2) return -1;  /* Too many mismatches */
    }

    /* Syndrome computation */
    int s1 = p1 ^ d1 ^ d2 ^ d4;
    int s2 = p2 ^ d1 ^ d3 ^ d4;
    int s3 = p3 ^ d2 ^ d3 ^ d4;
    int syndrome = (s1 << 0) | (s2 << 1) | (s3 << 2);

    int corrected = 0;
    if (syndrome != 0) {
        /* Flip the bit at position indicated by syndrome */
        switch (syndrome) {
            case 1: p1 ^= 1; break; case 2: p2 ^= 1; break;
            case 3: d1 ^= 1; break; case 4: p3 ^= 1; break;
            case 5: d2 ^= 1; break; case 6: d3 ^= 1; break;
            case 7: d4 ^= 1; break;
            default: return -1;  /* Invalid syndrome */
        }
        corrected = 1;
    }

    /* Reconstruct nibble */
    *nibble = (d1 << 3) | (d2 << 2) | (d3 << 1) | d4;
    return corrected;
}

/* ======================================================================
   L5: CRC-16-CCITT
   ====================================================================== */

/*
 * CRC-16-CCITT calculation.
 *
 * Polynomial: x^16 + x^12 + x^5 + 1 = 0x1021
 * Initial value: 0x0000
 * No final XOR.
 *
 * This is the standard CRC used in LoRa packets for payload
 * integrity verification. The transmitter appends the CRC to
 * the payload; the receiver recomputes and compares.
 *
 * Bit-by-bit LFSR implementation for clarity:
 *   For each input byte (MSB-first):
 *     XOR MSB of byte into MSB of CRC
 *     For each of 8 bits:
 *       If MSB of CRC is 1: shift left and XOR with polynomial
 *       Else: shift left
 */
uint16_t lora_crc16(const uint8_t *data, size_t len)
{
    if (!data) return 0;

    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

/* ======================================================================
   L5: Data Whitening
   ====================================================================== */

/*
 * Data whitening/de-whitening for LoRa payload.
 *
 * Uses a 9-bit Linear Feedback Shift Register (LFSR):
 *   Polynomial: x^9 + x^5 + 1
 *   Initial state: 0x1FF (all nine bits set)
 *
 * Whitening XORs the LFSR output with data bits to prevent
 * long runs of identical bits. Long runs of 0s or 1s cause:
 *   - Loss of clock recovery in the receiver
 *   - Spectral lines in the transmitted signal
 *   - Reduced preamble detection reliability
 *
 * The LFSR generates a pseudo-random sequence with period
 * 2^9 - 1 = 511 bits. XOR whitening is self-inverse:
 *   whitened = data XOR lfsr_sequence
 *   data = whitened XOR lfsr_sequence  (same operation!)
 *
 * So the same function serves both TX whitening and RX de-whitening.
 */
void lora_whiten(uint8_t *data, size_t len)
{
    if (!data || len == 0) return;

    uint16_t lfsr = 0x1FF;  /* 9-bit LFSR, init all ones */

    for (size_t i = 0; i < len; i++) {
        uint8_t output_byte = 0;

        for (int bit = 0; bit < 8; bit++) {
            /* Whitening bit from LFSR MSB */
            uint8_t wb = (lfsr >> 8) & 1;

            /* Data bit (MSB first) */
            uint8_t db = (data[i] >> (7 - bit)) & 1;

            /* XOR and shift into output byte */
            output_byte = (output_byte << 1) | (db ^ wb);

            /* LFSR update: shift left, feedback at taps */
            uint16_t feedback = lfsr & 1;
            lfsr >>= 1;
            if (feedback) lfsr ^= (1 << 8);  /* Feedback to bit 8 */
        }

        data[i] = output_byte;
    }
}

/* ======================================================================
   L4: Fundamental Laws -- Timing, Sensitivity, Processing Gain
   ====================================================================== */

/*
 * Symbol period: T_sym = 2^SF / BW.
 *
 * This is the fundamental time unit of LoRa modulation.
 * Each symbol carries SF bits and takes 2^SF chips at
 * chip rate R_c = BW.
 */
double lora_symbol_period(lora_spreading_factor_t sf, lora_bandwidth_t bw)
{
    return (double)((uint32_t)1 << (uint32_t)sf) / (double)bw;
}

/* Chip rate equals the bandwidth in LoRa CSS */
double lora_chip_rate_hz(lora_bandwidth_t bw)
{
    return (double)bw;
}

/*
 * Effective bit rate after FEC.
 *
 * R_b = SF * code_rate * symbol_rate
 *     = SF * (4/(4+CR)) * (BW / 2^SF)  bits/second
 *
 * Derivation:
 *   - SF bits per symbol (before FEC)
 *   - Code rate = 4/(4+CR) accounts for parity overhead
 *   - Symbol rate = BW / 2^SF (symbols per second)
 *
 * Example: SF12, BW125, CR4/5:
 *   R_b = 12 * 0.8 * 125000/4096 = 292.97 bps
 */
double lora_bit_rate(lora_spreading_factor_t sf,
                     lora_bandwidth_t bw, lora_coding_rate_t cr)
{
    double code_rate = 4.0 / (4.0 + (double)cr);
    double sym_rate  = (double)bw / (double)((uint32_t)1 << (uint32_t)sf);
    return (double)sf * code_rate * sym_rate;
}

/*
 * Receiver sensitivity.
 *
 * S(dBm) = -174 + 10*log10(BW) + NF + SNR_min
 *
 * where:
 *   -174 dBm/Hz = thermal noise floor (k*T*1Hz at 290K)
 *   BW = receiver noise bandwidth (Hz)
 *   NF = receiver noise figure (dB)
 *   SNR_min = minimum SNR for given SF (dB)
 *
 * SNR_min values from Semtech SX1276 characterization:
 *   SF6=-5.0, SF7=-7.5, SF8=-10.0, SF9=-12.5,
 *   SF10=-15.0, SF11=-17.5, SF12=-20.0 dB
 *
 * These are achievable because CSS despreading provides
 * processing gain = 10*log10(2^SF), lifting the signal
 * above the noise floor after dechirping.
 *
 * Example: SF12, BW125, NF=6:
 *   S = -174 + 10*log10(125000) + 6 + (-20)
 *     = -174 + 51.0 + 6 - 20 = -137 dBm
 */
double lora_receiver_sensitivity(lora_spreading_factor_t sf,
                                  lora_bandwidth_t bw, double nf)
{
    double snr_min;
    switch (sf) {
        case LORA_SF6:  snr_min = -5.0;  break;
        case LORA_SF7:  snr_min = -7.5;  break;
        case LORA_SF8:  snr_min = -10.0; break;
        case LORA_SF9:  snr_min = -12.5; break;
        case LORA_SF10: snr_min = -15.0; break;
        case LORA_SF11: snr_min = -17.5; break;
        case LORA_SF12: snr_min = -20.0; break;
        default:        snr_min = -7.5;  break;
    }
    return -174.0 + 10.0 * log10((double)bw) + nf + snr_min;
}

/*
 * CSS Processing Gain: G_p = 10 * log10(2^SF) = SF * 3.01 dB.
 *
 * This is the fundamental SNR improvement from despreading
 * the chirp signal. A signal below the thermal noise floor
 * becomes detectable because the DFT concentrates its energy
 * into a single frequency bin.
 *
 * For SF12: G_p = 12 * 3.01 = 36.12 dB
 * A signal at -20 dB SNR at the receiver input becomes
 * +16.12 dB after dechirping + DFT -- clearly detectable.
 */
double lora_processing_gain_db(lora_spreading_factor_t sf)
{
    return (double)sf * 10.0 * log10(2.0);
}

/* SNR after despreading = SNR_in + processing_gain */
double lora_snr_after_processing(double snr_in_dB, lora_spreading_factor_t sf)
{
    return snr_in_dB + lora_processing_gain_db(sf);
}

/* ======================================================================
   L6: Packet Time-on-Air (Canonical LoRa Problem)
   ====================================================================== */

/*
 * Total packet time-on-air.
 *
 * T_total = T_preamble + T_payload
 *
 * T_preamble = (N_preamble + 4.25) * T_sym
 *   - 4.25 accounts for sync word (2 sym) + SFD (2.25 sym)
 *
 * T_payload = N_payload_sym * T_sym
 *
 * This calculation is critical for:
 *   1. Duty cycle compliance (EU 1% sub-bands)
 *   2. Battery life estimation
 *   3. Network capacity planning
 *   4. Collision probability analysis
 */
double lora_packet_airtime(const lora_phy_params_t *params)
{
    if (!params) return -1.0;

    double Ts = params->symbol_period;
    double Tp = ((double)params->preamble_len + 4.25) * Ts;
    int ps = lora_payload_symbol_count(params);
    if (ps < 0) return -1.0;

    return Tp + (double)ps * Ts;
}

/*
 * Payload symbol count (Semtech formula, AN1200.13).
 *
 * N_payload = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH)
 *                        / (4*(SF - 2*DE))) * (CR + 4), 0)
 *
 * PL = payload bytes, SF = spreading factor, IH = implicit header flag,
 * DE = low data rate optimize flag, CRC = CRC enable flag, CR = coding rate.
 *
 * The "8" accounts for the explicit header symbols (if present).
 * The numerator encodes the total number of bits to transmit
 * (payload + CRC + header), expressed in terms of equivalent
 * information bits after accounting for FEC overhead.
 * The denominator accounts for the reduced coding efficiency
 * when low data rate optimization is active (DE=1 adds overhead).
 */
int lora_payload_symbol_count(const lora_phy_params_t *params)
{
    if (!params) return -1;

    int PL  = (int)params->payload_len;
    int SF  = (int)params->sf;
    int IH  = params->implicit_header ? 1 : 0;
    int CRC = params->enable_crc ? 1 : 0;
    int CR  = (int)params->cr;
    int DE  = (SF >= 11 && params->bw == LORA_BW_125_KHZ) ? 1 : 0;

    int denom = 4 * (SF - 2 * DE);
    if (denom <= 0) return -1;

    int numer = 8 * PL - 4 * SF + 28 + 16 * CRC - 20 * IH;
    int coded = (numer + denom - 1) / denom;  /* ceil division */
    if (coded < 0) coded = 0;

    return 8 + coded * (CR + 4);
}

/* ======================================================================
   L6: Parameter Validation and Initialization
   ====================================================================== */

/*
 * Validate LoRa PHY parameter combination.
 *
 * Constraints:
 *   - SF6-SF12 (SF6 only with implicit header)
 *   - BW must be from the defined set
 *   - CR must be 1-4
 *   - Preamble length >= minimum (6 symbols)
 *   - Carrier frequency must be positive
 *   - Derived parameters must be consistent
 *
 * Returns 0 if valid, -1 if invalid.
 */
int lora_phy_params_validate(const lora_phy_params_t *params)
{
    if (!params) return -1;

    /* Spreading factor range */
    if (params->sf < LORA_SF6 || params->sf > LORA_SF12) return -1;
    if (params->sf == LORA_SF6 && !params->implicit_header) return -1;

    /* Bandwidth valid set */
    switch (params->bw) {
        case LORA_BW_7_8_KHZ:   case LORA_BW_10_4_KHZ:  case LORA_BW_15_6_KHZ:
        case LORA_BW_20_8_KHZ:  case LORA_BW_31_25_KHZ: case LORA_BW_41_7_KHZ:
        case LORA_BW_62_5_KHZ:  case LORA_BW_125_KHZ:   case LORA_BW_250_KHZ:
        case LORA_BW_500_KHZ:   break;
        default: return -1;
    }

    /* Coding rate range */
    if (params->cr < LORA_CR_4_5 || params->cr > LORA_CR_4_8) return -1;

    /* Preamble minimum */
    if (params->preamble_len < LORA_PREAMBLE_MIN_SYMBOLS) return -1;

    /* Valid carrier frequency */
    if (params->carrier_freq <= 0.0) return -1;

    /* Derived parameter consistency */
    uint32_t expected_chips = (uint32_t)1 << (uint32_t)params->sf;
    if (params->num_chips != expected_chips) return -1;

    return 0;
}

/*
 * Initialize PHY parameters to sensible defaults.
 *
 * Default configuration:
 *   SF7, BW 125 kHz, CR 4/5, 868.1 MHz (EU g1 channel),
 *   8 preamble symbols, public sync word (0x34),
 *   CRC enabled, explicit header mode.
 *
 * This is the most common LoRaWAN EU868 configuration,
 * balancing data rate, range, and regulatory compliance.
 */
void lora_phy_params_init_default(lora_phy_params_t *params)
{
    if (!params) return;

    memset(params, 0, sizeof(*params));

    params->sf              = LORA_SF7;
    params->bw              = LORA_BW_125_KHZ;
    params->cr              = LORA_CR_4_5;
    params->carrier_freq    = 868100000.0;
    params->preamble_len    = LORA_PREAMBLE_DEFAULT;
    params->sync_word       = LORA_SYNC_PUBLIC;
    params->enable_crc      = 1;
    params->implicit_header = 0;
    params->payload_len     = 0;

    /* Derived parameters */
    params->num_chips     = (uint32_t)1 << (uint32_t)params->sf;
    params->chip_rate     = params->bw;
    params->symbol_period = (double)params->num_chips / (double)params->bw;
    params->chirp_rate    = (double)params->bw / params->symbol_period;
    params->bit_rate      = lora_bit_rate(params->sf, params->bw, params->cr);
}
