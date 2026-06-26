/**
 * @file lora_phy.h
 * @brief LoRa Physical Layer -- Chirp Spread Spectrum (CSS) modulation definitions
 *
 * Knowledge Coverage:
 *   L1 -- Spreading Factor (SF), Bandwidth (BW), Coding Rate (CR),
 *        Symbol Rate, Chip Rate, preamble, sync word
 *   L2 -- Chirp modulation/demodulation principle, orthogonal SFs
 *   L3 -- Complex baseband chirp signal: s(t) = exp(j*2pi*(f0 + k*t)*t)
 *        Instantaneous frequency slope mu = BW^2 / 2^SF
 *   L4 -- Shannon-Hartley applied to CSS, Processing gain, Sensitivity formula
 *   L5 -- FFT-based dechirping, Hamming(7,4) FEC, CRC-16, whitening
 *   L6 -- Packet time-on-air canonical calculation
 *
 * References:
 *   - Semtech SX1276/77/78/79 datasheet, AN1200.22 LoRa Modulation Basics
 *   - O. Seller & N. Sornin, "LoRa: Chirp Spread Spectrum Modulation"
 *   - Vangelista, "Frequency Shift Chirp Modulation: The LoRa Modulation"
 *   - LoRaWAN Specification 1.0.4 (LoRa Alliance)
 *
 * Curriculum Mapping:
 *   - Stanford EE359: Wireless Communications -- Spread Spectrum
 *   - MIT 6.450: Digital Communications -- Chirp signals
 *   - Berkeley EE123: Digital Signal Processing -- FFT-based demod
 *   - TU Munich: Signal Processing -- Time-frequency analysis
 *   - ETH 227-0436: Communications -- LPWAN modulation
 *
 * @license MIT
 */

#ifndef LORA_PHY_H
#define LORA_PHY_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <complex.h>

/* C11 CMPLX fallback */
#ifndef CMPLX
#define CMPLX(r, i) ((double complex)((r) + (i) * _Complex_I))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   L1: Core Definitions -- Spreading Factor, Bandwidth, Coding Rate
   ============================================================================ */

/**
 * LoRa Spreading Factor (SF)
 *
 * Definition (L1): SF = log2(N_chips), number of chips per symbol.
 * Each symbol carries SF bits. Higher SF gives longer airtime and better sensitivity.
 *
 * Sensitivity improvement per SF step: approx 2.5 dB
 * Time-on-air multiplier per SF step: x2
 */
typedef enum {
    LORA_SF6  = 6,   /**< 2^6 = 64 chips/symbol, implicit header only */
    LORA_SF7  = 7,   /**< 2^7 = 128 chips/symbol */
    LORA_SF8  = 8,   /**< 2^8 = 256 chips/symbol */
    LORA_SF9  = 9,   /**< 2^9 = 512 chips/symbol */
    LORA_SF10 = 10,  /**< 2^10 = 1024 chips/symbol */
    LORA_SF11 = 11,  /**< 2^11 = 2048 chips/symbol */
    LORA_SF12 = 12,  /**< 2^12 = 4096 chips/symbol -- max sensitivity */
} lora_spreading_factor_t;

/**
 * LoRa Bandwidth (BW)
 *
 * Definition (L1): Total sweep bandwidth of the chirp signal.
 * Chip rate R_c = BW chips/s. Symbol rate R_s = BW / 2^SF symbols/s.
 */
typedef enum {
    LORA_BW_7_8_KHZ   = 7800,   /**< 7.8 kHz */
    LORA_BW_10_4_KHZ  = 10400,  /**< 10.4 kHz */
    LORA_BW_15_6_KHZ  = 15600,  /**< 15.6 kHz */
    LORA_BW_20_8_KHZ  = 20800,  /**< 20.8 kHz */
    LORA_BW_31_25_KHZ = 31250,  /**< 31.25 kHz */
    LORA_BW_41_7_KHZ  = 41700,  /**< 41.7 kHz */
    LORA_BW_62_5_KHZ  = 62500,  /**< 62.5 kHz */
    LORA_BW_125_KHZ   = 125000, /**< 125 kHz -- default LoRaWAN BW */
    LORA_BW_250_KHZ   = 250000, /**< 250 kHz */
    LORA_BW_500_KHZ   = 500000, /**< 500 kHz */
} lora_bandwidth_t;

/**
 * LoRa Coding Rate (CR)
 *
 * Definition (L1): FEC rate = 4/(4+CR).
 * CR=1: 4/5, CR=2: 4/6 (2/3), CR=3: 4/7, CR=4: 4/8 (1/2).
 * Higher CR adds more redundancy for better error correction.
 */
typedef enum {
    LORA_CR_4_5 = 1,  /**< Coding rate 4/5 */
    LORA_CR_4_6 = 2,  /**< Coding rate 4/6 */
    LORA_CR_4_7 = 3,  /**< Coding rate 4/7 */
    LORA_CR_4_8 = 4,  /**< Coding rate 4/8 */
} lora_coding_rate_t;

/* Preamble and sync constants */
#define LORA_PREAMBLE_MIN_SYMBOLS  6
#define LORA_PREAMBLE_DEFAULT       8
#define LORA_SYNC_PUBLIC           0x34
#define LORA_SYNC_PRIVATE          0x12

/* ============================================================================
   L2: Core Concepts -- Chirp Spread Spectrum Signal Model
   ============================================================================ */

/**
 * LoRa PHY parameters -- complete configuration for a LoRa transmission.
 *
 * L3 Mathematical Structure:
 *   Baseband chirp: s_bb(t) = exp(j * pi * mu * t^2)
 *   where mu = BW / T_s = BW^2 / 2^SF (chirp rate in Hz/s)
 *
 *   The instantaneous frequency f(t) = mu * t sweeps linearly.
 *   For an up-chirp: f(t) goes from -BW/2 to +BW/2 over one symbol period.
 *   Symbol value k shifts the starting frequency: f_start = k * BW/2^SF.
 */
typedef struct {
    lora_spreading_factor_t sf;       /**< Spreading factor (SF7-SF12) */
    lora_bandwidth_t        bw;       /**< Bandwidth in Hz */
    lora_coding_rate_t      cr;       /**< Coding rate (FEC overhead) */
    double   carrier_freq;            /**< Center frequency in Hz (e.g. 868.1e6) */
    uint32_t chip_rate;               /**< R_c = BW chips/s */
    uint32_t num_chips;               /**< 2^SF chips per symbol */
    double   symbol_period;           /**< T_s = 2^SF / BW seconds */
    double   chirp_rate;              /**< mu = BW^2 / 2^SF Hz/s */
    double   bit_rate;                /**< Effective bit rate in bps */
    uint16_t preamble_len;            /**< Number of preamble symbols */
    uint8_t  sync_word;               /**< Sync word value (0x34 public) */
    int      enable_crc;              /**< Nonzero to enable CRC */
    int      implicit_header;         /**< Nonzero for implicit header mode */
    uint8_t  payload_len;             /**< Payload bytes (implicit header) */
} lora_phy_params_t;

/**
 * Chirp generator state -- phase accumulator for continuous chirp synthesis.
 */
typedef struct {
    double phase_accum;      /**< Instantaneous phase (radians) */
    double freq_instant;     /**< Current instantaneous frequency (Hz) */
    double phase_step;       /**< Per-sample phase increment */
    int    direction;        /**< +1 up-chirp, -1 down-chirp */
    uint32_t chip_idx;       /**< Current chip within symbol */
    uint32_t symbol_idx;     /**< Current symbol counter */
} lora_chirp_gen_t;

/* ============================================================================
   L3: Mathematical Structures -- Complex baseband processing
   ============================================================================ */

/**
 * Generate one sample of a LoRa chirp.
 *
 * Implements: s[n] = exp(j * (2*pi * f_start * n/Fs + pi * mu * (n/Fs)^2))
 *
 * f_start determines the symbol value: f_start = (symbol * BW) / 2^SF
 * The chirp wraps around: when f(t) exceeds BW/2, it jumps to -BW/2.
 *
 * L4 Theorem -- Orthogonality of SFs:
 *   Chirps with different SF are approximately orthogonal because
 *   the integral over one symbol period approaches zero:
 *     integral_0^T exp(j*pi*(mu1 - mu2)*t^2) dt ~ 0 for mu1 != mu2
 *   This enables concurrent reception of multiple SFs (LoRaWAN gateways).
 *
 * @param params   PHY parameters
 * @param symbol   Symbol value in [0, 2^SF-1]
 * @param chip_idx Chip index within symbol [0, 2^SF-1]
 * @param sample_rate Sampling frequency in Hz
 * @return Complex baseband sample value
 */
double complex lora_chirp_sample(const lora_phy_params_t *params,
                                  uint32_t symbol,
                                  uint32_t chip_idx,
                                  double sample_rate);

/**
 * Generate complete LoRa preamble.
 * Structure: [N_preamble up-chirps] [2 coded chirps for sync] [0.25 down-chirp]
 * Returns total samples generated, or -1 on error.
 */
int lora_generate_preamble(const lora_phy_params_t *params,
                            double complex *buffer,
                            size_t max_len);

/**
 * Demodulate a LoRa symbol using FFT-based dechirping.
 *
 * L5 Algorithm:
 *   1. Multiply received samples by conjugate of base down-chirp (dechirp)
 *   2. After dechirping, the signal becomes a pure tone at frequency k*BW/2^SF
 *   3. Take FFT -- bin k corresponds to symbol value k
 *   4. Find argmax |FFT[k]| -- this is the demodulated symbol
 *
 * This approach leverages the fact that CSS processing gain concentrates
 * energy into a single FFT bin, enabling reception below the noise floor.
 *
 * @param params    PHY parameters
 * @param samples   Received complex samples (must be 2^SF samples)
 * @param num_samples Number of samples (must equal 2^SF)
 * @return Demodulated symbol value, or -1 on error
 */
int lora_demodulate_symbol_fft(const lora_phy_params_t *params,
                                const double complex *samples,
                                size_t num_samples);

/* ============================================================================
   L4: Fundamental Laws -- Timing, Sensitivity, Processing Gain
   ============================================================================ */

/**
 * Symbol period: T_sym = 2^SF / BW seconds.
 * This is the fundamental time unit of LoRa modulation.
 */
double lora_symbol_period(lora_spreading_factor_t sf, lora_bandwidth_t bw);

/**
 * Chip rate: R_c = BW chips/second.
 * In LoRa CSS, the chip rate equals the bandwidth.
 */
double lora_chip_rate_hz(lora_bandwidth_t bw);

/**
 * Effective bit rate.
 *
 * R_b = SF * CR_rate * R_s
 *     = SF * (4/(4+CR)) * (BW / 2^SF)  bps
 *
 * Example: SF7, BW125, CR4/5 -> 7 * 4/5 * 125000/128 = 5468.75 bps
 */
double lora_bit_rate(lora_spreading_factor_t sf,
                     lora_bandwidth_t bw,
                     lora_coding_rate_t cr);

/**
 * Receiver sensitivity.
 *
 * S(dBm) = -174 + 10*log10(BW) + NF + SNR_min
 *
 * where:
 *   -174 dBm/Hz = kT_0, thermal noise power spectral density at 290K
 *   BW = receiver bandwidth in Hz
 *   NF = receiver noise figure (typically 6 dB for SX1276)
 *   SNR_min = minimum required SNR per SF:
 *     SF7: -7.5, SF8: -10, SF9: -12.5, SF10: -15, SF11: -17.5, SF12: -20 dB
 *
 * CSS processing gain = 10*log10(2^SF) allows reception at negative SNR.
 */
double lora_receiver_sensitivity(lora_spreading_factor_t sf,
                                  lora_bandwidth_t bw,
                                  double nf);

/**
 * Processing gain: G_p = 10 * log10(2^SF) dB.
 *
 * This is the SNR improvement from despreading the CSS signal.
 * For SF12: G_p = 10*log10(4096) = 36.1 dB.
 * A signal at -20 dB SNR becomes +16.1 dB after despreading.
 */
double lora_processing_gain_db(lora_spreading_factor_t sf);

/**
 * SNR after despreading.
 * SNR_out = SNR_in + G_p
 */
double lora_snr_after_processing(double snr_in_dB, lora_spreading_factor_t sf);

/* ============================================================================
   L5: Algorithms -- Modulation, Demodulation, FEC
   ============================================================================ */

/**
 * Initialize chirp generator for given symbol value and sweep direction.
 *
 * @param state     Output generator state
 * @param params    PHY parameters
 * @param symbol    Initial symbol value [0, 2^SF-1]
 * @param direction +1 for up-chirp, -1 for down-chirp
 */
void lora_chirp_gen_init(lora_chirp_gen_t *state,
                          const lora_phy_params_t *params,
                          uint32_t symbol,
                          int direction);

/**
 * Generate next chirp sample, advancing generator state.
 *
 * @param state  Generator state (updated in place)
 * @param params PHY parameters
 * @param fs     Sample rate in Hz (typically equal to BW)
 * @return Complex baseband sample
 */
double complex lora_chirp_gen_next(lora_chirp_gen_t *state,
                                    const lora_phy_params_t *params,
                                    double fs);

/**
 * Dechirp operation: multiply received samples by conjugate of base down-chirp.
 *
 * r_dechirp[n] = r[n] * conj(base_down_chirp[n])
 *
 * After dechirping, FFT peak detection recovers the symbol value.
 * This is the core demodulation step that provides CSS processing gain.
 *
 * @param params      PHY parameters
 * @param rx          Input samples (overwritten with dechirped result)
 * @param num_samples Number of samples
 * @return 0 on success, -1 on error
 */
int lora_dechirp(const lora_phy_params_t *params,
                  double complex *rx,
                  size_t num_samples);

/**
 * Hamming(7,4) systematic encoder for LoRa FEC.
 *
 * Encodes a 4-bit nibble into a 7-bit Hamming codeword.
 * Parity equations:
 *   p1 = d1 ^ d2 ^ d4  (covers bits 3,5,7)
 *   p2 = d1 ^ d3 ^ d4  (covers bits 3,6,7)
 *   p3 = d2 ^ d3 ^ d4  (covers bits 5,6,7)
 *
 * Codeword layout: [p1, p2, d1, p3, d2, d3, d4]
 *
 * The coding rate CR selects how many bits are actually transmitted:
 *   CR=1: 5 bits (codeword[0..4], rate 4/5)
 *   CR=2: 6 bits (codeword[0..5], rate 4/6)
 *   CR=3: 7 bits (codeword[0..6], rate 4/7)
 *   CR=4: 8 bits (codeword[0..7], rate 4/8, includes extra parity)
 *
 * @param nibble   4-bit data input (low nibble)
 * @param cr       Coding rate
 * @param codeword Output codeword buffer (at least 8 bytes)
 * @return Number of bits in output codeword (4+CR)
 */
int lora_hamming_encode(uint8_t nibble, lora_coding_rate_t cr,
                         uint8_t *codeword);

/**
 * Hamming(7,4) decode with single-bit error correction.
 *
 * Computes syndrome:
 *   s1 = p1 ^ d1 ^ d2 ^ d4
 *   s2 = p2 ^ d1 ^ d3 ^ d4
 *   s3 = p3 ^ d2 ^ d3 ^ d4
 *
 * If syndrome = 0: no error
 * If syndrome in {1..7}: flip bit at position syndrome
 * Otherwise: uncorrectable error
 *
 * @param codeword Received codeword (4+CR bits packed in bytes)
 * @param cr       Coding rate used during encoding
 * @param nibble   Output decoded 4-bit nibble
 * @return 0 = no errors, 1 = single error corrected, -1 = uncorrectable
 */
int lora_hamming_decode(const uint8_t *codeword, lora_coding_rate_t cr,
                         uint8_t *nibble);

/**
 * CRC-16-CCITT for LoRa payload integrity.
 * Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 * Initial value: 0x0000
 *
 * This CRC is appended to every LoRa packet payload.
 */
uint16_t lora_crc16(const uint8_t *data, size_t len);

/**
 * Data whitening/de-whitening for LoRa payload.
 *
 * Uses a 9-bit LFSR with polynomial x^9 + x^5 + 1.
 * Initial state: 0x1FF (all ones).
 * XORs LFSR output with data bytes to avoid long runs of 0s or 1s
 * that would degrade receiver synchronization.
 */
void lora_whiten(uint8_t *data, size_t len);

/* ============================================================================
   L6: Canonical Problem -- Packet Time-on-Air Calculation
   ============================================================================ */

/**
 * Calculate total packet time-on-air (canonical LoRa airtime problem).
 *
 * The packet consists of preamble and payload sections:
 *
 * T_preamble = (N_preamble + 4.25) * T_sym
 *
 * N_payload_sym = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH)
 *                   / (4*(SF - 2*DE))) * (CR + 4), 0)
 *
 * T_payload = N_payload_sym * T_sym
 *
 * where:
 *   PL = payload length in bytes
 *   SF = spreading factor (6-12)
 *   IH = 1 if implicit header mode, 0 otherwise
 *   DE = 1 if low data rate optimize enabled (SF11/12 with BW125), else 0
 *   CRC = 1 if CRC enabled, 0 otherwise
 *   CR = coding rate (1-4)
 *
 * This formula comes from the Semtech LoRa modem design and is
 * essential for duty cycle compliance and battery life estimation.
 *
 * @param params PHY parameters with payload_len set
 * @return Total packet time-on-air in seconds
 */
double lora_packet_airtime(const lora_phy_params_t *params);

/**
 * Calculate number of payload symbols for a given PHY configuration.
 * Internal component of the airtime calculation.
 */
int lora_payload_symbol_count(const lora_phy_params_t *params);

/**
 * Validate LoRa PHY parameter combination.
 *
 * Checks:
 *   - SF in valid range [6, 12]
 *   - SF6 requires implicit header mode
 *   - BW in valid set
 *   - CR in valid range [1, 4]
 *   - Preamble length >= minimum
 *
 * @param params PHY parameters to validate
 * @return 0 if valid, -1 if invalid
 */
int lora_phy_params_validate(const lora_phy_params_t *params);

/**
 * Initialize PHY parameters with sensible defaults.
 * Default: SF7, BW125kHz, CR4/5, 868.1 MHz, preamble 8,
 * public sync, CRC enabled, explicit header.
 */
void lora_phy_params_init_default(lora_phy_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* LORA_PHY_H */
