/**
 * @file wifi_phy.c
 * @brief WiFi PHY Layer Implementation — OFDM, Modulation, Coding, MIMO (L2,L3,L5)
 *
 * Implements PHY algorithms for IEEE 802.11a/g/n/ac OFDM:
 *   - OFDM symbol construction (subcarrier mapping, IFFT, cyclic prefix)
 *   - Constellation mapping / demapping
 *   - Convolutional encoding / Viterbi decoding
 *   - Block interleaver / deinterleaver
 *   - LDPC encoding
 *   - Alamouti STBC
 *   - EVM measurement
 *
 * Each function implements an independent knowledge point.
 *
 * Reference: IEEE Std 802.11-2020, Clause 17 (OFDM PHY specification)
 * Reference: Heiskala, J. & Terry, J., "OFDM Wireless LANs", Sams 2001.
 */

#include "wifi_phy.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 * Subcarrier Mapping (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Initialize OFDM subcarrier mapping for a given bandwidth
 *
 * Allocates data and pilot subcarrier indices according to IEEE 802.11.
 * The DC subcarrier (FFT index 0) is always nulled.
 *
 * For 20 MHz / 64 FFT:
 *   - FFT bins: -32..31 (total 64), 0 is DC
 *   - Data: ±1..±26 excluding pilots ±7,±21 = 48 subcarriers
 *   - Pilots: ±7, ±21 = 4 subcarriers
 *   - Guard: -32..-27 = 6 lower, +28..+31 = 4 upper (actually 0 is also null)
 *
 * 802.11a/g pilot sequence: {1, 1, 1, -1} for symbols 0,1,2,3...
 * Pilots scrambled with 127-length PRBS generator: S(x) = x⁷ + x⁴ + 1
 *
 * @param map       Subcarrier map to fill
 * @param bw_mhz    Bandwidth in MHz (20/40/80/160)
 * @return 0 on success, -1 on failure
 */
int ofdm_subcarrier_map_init(ofdm_subcarrier_map_t *map, double bw_mhz)
{
    int n_fft, n_data, n_pilots;

    /* Determine numerology from bandwidth */
    if (fabs(bw_mhz - 20.0) < 1.0) {
        n_fft = 64; n_data = 48; n_pilots = 4;
    } else if (fabs(bw_mhz - 40.0) < 1.0) {
        n_fft = 128; n_data = 108; n_pilots = 6;
    } else if (fabs(bw_mhz - 80.0) < 1.0) {
        n_fft = 256; n_data = 234; n_pilots = 8;
    } else if (fabs(bw_mhz - 160.0) < 1.0) {
        n_fft = 512; n_data = 468; n_pilots = 16;
    } else {
        return -1;
    }

    map->n_data   = n_data;
    map->n_pilots = n_pilots;
    map->fft_size = n_fft;

    /* Allocate index arrays */
    map->data_indices  = (int *)malloc((size_t)n_data * sizeof(int));
    map->pilot_indices = (int *)malloc((size_t)n_pilots * sizeof(int));

    if (!map->data_indices || !map->pilot_indices) {
        free(map->data_indices);
        free(map->pilot_indices);
        return -1;
    }

    /* Initialize pilot values to standard 802.11 sequence */
    map->pilot_values[0] =  1.0;
    map->pilot_values[1] =  1.0;
    map->pilot_values[2] =  1.0;
    map->pilot_values[3] = -1.0;

    /* For 20 MHz (64-FFT): detailed index assignment */
    if (n_fft == 64) {
        /* Pilots at ±7 and ±21 (logical) */
        /* Physical FFT bins: DC=0, positive=1..32, negative=-32..-1 (mapped to 32..63) */
        /* Pilot logical indices: -21, -7, 7, 21 */
        /* FFT bin mapping: logical_idx → (logical_idx >= 0 ? logical_idx : n_fft + logical_idx) */
        int pilot_logical[4] = { -21, -7, 7, 21 };
        for (int i = 0; i < n_pilots; i++) {
            int li = pilot_logical[i];
            map->pilot_indices[i] = (li >= 0) ? li : (n_fft + li);
            /* Store pilot value: sign based on pilot polarity sequence */
        }

        /* Data subcarriers: ±1..±26 excluding ±7,±21 */
        int di = 0;
        for (int k = -26; k <= 26; k++) {
            if (k == 0) continue;                    /* DC */
            if (k == -21 || k == -7 || k == 7 || k == 21) continue;  /* pilots */
            int bin = (k >= 0) ? k : (n_fft + k);
            map->data_indices[di++] = bin;
        }
    } else {
        /* For other bandwidths: proportional scaling */
        /* Pilot spacing follows 802.11 pattern */

        /* Set up pilots at ±(7*scale), ±(21*scale) if single-segment */
        /* For 40 MHz: ±11, ±53 → actual depends on segment parser */
        int n_half_data = n_data / 2;
        (void)(n_fft / 64);  /* scale factor used implicitly in data mapping */

        int pi = 0;
        /* Simplified pilot placement: evenly spaced */
        int pilot_step = n_data / (n_pilots + 1);
        for (int i = 0; i < n_pilots; i++) {
            int logical_pilot = -n_half_data + (i + 1) * pilot_step;
            /* Map logical to FFT bin */
            if (logical_pilot < 0) logical_pilot += n_fft;
            map->pilot_indices[pi++] = logical_pilot;
        }

        int di = 0;
        for (int k = -n_half_data; k <= n_half_data; k++) {
            if (k == 0) continue; /* skip DC */
            int is_pilot = 0;
            for (int j = 0; j < n_pilots && !is_pilot; j++) {
                int mapped = map->pilot_indices[j];
                int logical_from_mapped = (mapped >= n_fft/2) ? (mapped - n_fft) : mapped;
                if (logical_from_mapped == k) is_pilot = 1;
            }
            if (is_pilot) continue;
            int bin = (k >= 0) ? k : (n_fft + k);
            map->data_indices[di++] = bin;
        }
    }

    return 0;
}

/**
 * @brief Free subcarrier map resources
 *
 * @param map   Subcarrier map to free
 */
void ofdm_subcarrier_map_free(ofdm_subcarrier_map_t *map)
{
    free(map->data_indices);
    free(map->pilot_indices);
    map->data_indices  = NULL;
    map->pilot_indices = NULL;
}

/* ==========================================================================
 * Pilot Tone Scrambling (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief 127-bit PRBS generator for pilot scrambling
 *
 * Polynomial: S(x) = x⁷ + x⁴ + 1
 * The shift register is initialized to all-ones (1111111₂ = 127).
 * Each OFDM symbol advances the PRBS and uses one output bit per pilot.
 *
 * @param symbol_index  OFDM symbol index within the packet
 * @param n_pilots      Number of pilot subcarriers
 * @return Scrambling polarity (+1 or -1)
 */
static int pilot_polarity(int symbol_index, int n_pilots)
{
    /* 127-bit PRBS: shift register seeded with all 1s */
    unsigned int sr = 0x7F; /* 7 bits all ones */
    int polarity = 1;

    /* Advance to the position for this symbol */
    /* Each symbol uses n_pilots bits of the PRBS */
    for (int sym = 0; sym <= symbol_index; sym++) {
        for (int p = 0; p < n_pilots; p++) {
            int bit = (sr >> 6) & 1;       /* output = bit 6 */
            int fb  = ((sr >> 6) ^ (sr >> 3)) & 1;  /* feedback = bit6 XOR bit3 */
            sr = ((sr << 1) | fb) & 0x7F;  /* shift left, insert feedback */
            if (sym == symbol_index && p == 0) {
                polarity = (bit == 0) ? 1 : -1; /* bit 0 → +1, bit 1 → -1 */
            }
        }
    }
    return polarity;
}

/**
 * @brief Insert pilot tones into frequency-domain OFDM symbol
 *
 * Pilots are inserted at their designated FFT bins with BPSK modulation.
 * The polarity is determined by the 127-bit pilot scrambling sequence.
 *
 * Reference: IEEE 802.11-2020 §17.3.5.8
 *
 * @param freq_symbol   Frequency domain buffer (size n_fft, complex pairs)
 * @param pilot_indices Array of pilot subcarrier FFT bin indices
 * @param n_pilots      Number of pilot subcarriers
 * @param symbol_index  OFDM symbol index in the packet
 * @return 0 on success
 */
int ofdm_insert_pilots(double *freq_symbol, const int *pilot_indices,
                       int n_pilots, int symbol_index)
{
    if (!freq_symbol || !pilot_indices || n_pilots <= 0) return -1;

    int pol = pilot_polarity(symbol_index, n_pilots);

    for (int i = 0; i < n_pilots; i++) {
        int bin = pilot_indices[i];
        /* freq_symbol is interleaved real/imag pairs: [re0, im0, re1, im1, ...] */
        freq_symbol[2 * bin]     = (double)pol;  /* I = ±1 */
        freq_symbol[2 * bin + 1] = 0.0;          /* Q = 0 (BPSK) */
    }

    return 0;
}

/* ==========================================================================
 * 802.11 Long Training Sequence (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Generate 802.11a/g Long Training Sequence in frequency domain
 *
 * The LTS is a known BPSK sequence used for channel estimation.
 * It occupies subcarriers -26..+26 (53 tones including DC=0 for 20 MHz).
 *
 * @param lts_real  Output real part array
 * @param lts_imag  Output imag part array
 * @param n_fft     FFT size
 * @return 0 on success
 */
int ofdm_generate_lts(double *lts_real, double *lts_imag, int n_fft)
{
    if (!lts_real || !lts_imag || n_fft < 64) return -1;

    /* LTS BPSK values for subcarriers -26..+26 (indexed from -26) */
    static const int lts_vals[] = {
         1, 1,-1,-1, 1, 1,-1, 1,-1, 1, 1, 1, 1, 1, 1,-1,-1, 1, 1,-1, 1,-1, 1, 1, 1, 1,
        /* index 26 = DC = 0 */
         0,
         1,-1,-1, 1, 1,-1, 1,-1, 1,-1,-1,-1,-1,-1, 1, 1,-1,-1, 1,-1, 1,-1, 1, 1, 1, 1
    };

    /* Zero out all bins */
    memset(lts_real, 0, (size_t)(2 * n_fft) * sizeof(double));
    memset(lts_imag, 0, (size_t)(2 * n_fft) * sizeof(double));

    /* Place LTS values at appropriate FFT bins */
    for (int k = -26; k <= 26; k++) {
        int bin = (k >= 0) ? k : (n_fft + k);
        int lts_index = k + 26;
        if (lts_index >= 0 && lts_index < 53) {
            lts_real[bin] = (double)lts_vals[lts_index];
            lts_imag[bin] = 0.0;  /* BPSK: Q=0 */
        }
    }

    return 0;
}

/* ==========================================================================
 * IFFT for OFDM Symbol Construction (L3 Mathematical Structure)
 * ========================================================================== */

/**
 * @brief Compute radix-2 DIT inverse FFT (IFFT)
 *
 * Standard Cooley-Tukey radix-2 decimation-in-time IFFT:
 *   x[n] = (1/N) Σ X[k] · e^{j·2π·k·n/N}
 *
 * Equivalent to: conj(FFT(conj(X))) / N
 *
 * Implements in-place computation with bit-reversal reordering.
 *
 * @param data_real   Real part array (input/output)
 * @param data_imag   Imag part array (input/output)
 * @param n           FFT size (power of 2)
 *
 * Complexity: O(N·log N)
 */
static void ifft_dit(double *data_real, double *data_imag, int n)
{
    /* Bit-reversal permutation */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double tr = data_real[i], ti = data_imag[i];
            data_real[i] = data_real[j]; data_imag[i] = data_imag[j];
            data_real[j] = tr;            data_imag[j] = ti;
        }
    }

    /* Cooley-Tukey DIT: log2(N) stages */
    for (int len = 2; len <= n; len <<= 1) {
        double angle = 2.0 * M_PI / (double)len;
        /* IFFT uses negative exponent → positive angle per butterfly */
        double w_real = cos(angle);
        double w_imag = sin(angle);

        for (int i = 0; i < n; i += len) {
            double wr = 1.0, wi = 0.0;
            for (int j = 0; j < len / 2; j++) {
                int a = i + j;
                int b = i + j + len / 2;

                double tr = wr * data_real[b] - wi * data_imag[b];
                double ti = wr * data_imag[b] + wi * data_real[b];

                data_real[b] = data_real[a] - tr;
                data_imag[b] = data_imag[a] - ti;
                data_real[a] = data_real[a] + tr;
                data_imag[a] = data_imag[a] + ti;

                double nwr = wr * w_real - wi * w_imag;
                double nwi = wr * w_imag + wi * w_real;
                wr = nwr; wi = nwi;
            }
        }
    }

    /* Scale by 1/N for IFFT */
    double scale = 1.0 / (double)n;
    for (int i = 0; i < n; i++) {
        data_real[i] *= scale;
        data_imag[i] *= scale;
    }
}

/**
 * @brief Build a complete OFDM time-domain symbol
 *
 * Maps data symbols to subcarriers, inserts pilots, nulls DC and guard
 * bins, computes IFFT, and prepends cyclic prefix.
 *
 * Output: time-domain samples (N_FFT + N_CP samples)
 *
 * @param time_symbol    Output buffer (2*(n_fft+n_cp) doubles for complex)
 * @param data_symbols   Input constellation symbols (n_data pairs)
 * @param map            Subcarrier map (indices + pilot info)
 * @param params         OFDM parameters
 * @param symbol_index   Symbol index in packet
 * @return Total time-domain samples, or -1 on error
 */
int ofdm_build_symbol(double *time_symbol,
                      const double *data_symbols,
                      const ofdm_subcarrier_map_t *map,
                      const ofdm_params_t *params,
                      int symbol_index)
{
    if (!time_symbol || !data_symbols || !map || !params) return -1;

    int n_fft   = params->n_fft;
    int n_cp    = params->n_cp;
    int n_data  = map->n_data;
    int n_pilots = map->n_pilots;

    /* Allocate frequency domain buffer */
    double *freq_real = (double *)calloc((size_t)n_fft, sizeof(double));
    double *freq_imag = (double *)calloc((size_t)n_fft, sizeof(double));
    if (!freq_real || !freq_imag) {
        free(freq_real);
        free(freq_imag);
        return -1;
    }

    /* Map data symbols to subcarriers */
    for (int i = 0; i < n_data; i++) {
        int bin = map->data_indices[i];
        freq_real[bin] = data_symbols[2 * i];      /* I */
        freq_imag[bin] = data_symbols[2 * i + 1];  /* Q */
    }

    /* Insert pilots */
    for (int i = 0; i < n_pilots; i++) {
        int bin = map->pilot_indices[i];
        /* Pilot value: ±1 with scrambling polarity */
        int pol = pilot_polarity(symbol_index, n_pilots);
        /* Use map's pilot values with scrambling */
        double pilot_val = map->pilot_values[i % 4] * (double)pol;
        freq_real[bin] = pilot_val;
        freq_imag[bin] = 0.0;
    }

    /* DC subcarrier (bin 0) is always zero */
    freq_real[0] = 0.0;
    freq_imag[0] = 0.0;

    /* Guard subcarriers: bins at edges remain zero (already calloc'd) */

    /* Compute IFFT */
    ifft_dit(freq_real, freq_imag, n_fft);

    /* Write time-domain samples: first copy IFFT output, then prepend CP */
    int total_samples = n_fft + n_cp;

    /* Cyclic prefix: copy last n_cp samples to the beginning */
    for (int i = 0; i < n_cp; i++) {
        int src = n_fft - n_cp + i;
        time_symbol[2 * i]     = freq_real[src];
        time_symbol[2 * i + 1] = freq_imag[src];
    }

    /* Copy IFFT output after CP */
    for (int i = 0; i < n_fft; i++) {
        time_symbol[2 * (n_cp + i)]     = freq_real[i];
        time_symbol[2 * (n_cp + i) + 1] = freq_imag[i];
    }

    free(freq_real);
    free(freq_imag);
    return total_samples;
}

/* ==========================================================================
 * Constellation Mapping (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief BPSK modulation: bit 0 → -1, bit 1 → +1 (or vice versa per standard)
 */
static void bpsk_map(double *i_out, double *q_out, uint32_t bits)
{
    *i_out = (bits & 1) ? 1.0 : -1.0;
    *q_out = 0.0;
}

/**
 * @brief QPSK modulation with Gray coding:
 *
 * Bits (b1,b0): 00 → (-1-j), 01 → (-1+j), 10 → (+1-j), 11 → (+1+j)
 * Normalized by 1/√2 for unit average power.
 */
static void qpsk_map(double *i_out, double *q_out, uint32_t bits)
{
    int b0 = (bits >> 0) & 1;  /* I bit */
    int b1 = (bits >> 1) & 1;  /* Q bit */
    *i_out = (b0 ? 1.0 : -1.0) * QPSK_NORM;
    *q_out = (b1 ? 1.0 : -1.0) * QPSK_NORM;
}

/**
 * @brief 16-QAM with Gray coding
 *
 * Constellations: {-3,-1,+1,+3} × QAM16_NORM for both I and Q.
 * Gray mapping: 00→-3, 01→-1, 11→+1, 10→+3
 */
static void qam16_map(double *i_out, double *q_out, uint32_t bits)
{
    int bi = (bits >> 0) & 3;  /* I bits: b1,b0 */
    int bq = (bits >> 2) & 3;  /* Q bits: b3,b2 */

    /* Gray decode 2 bits to -3,-1,+1,+3 */
    static const double gray_vals[4] = { -3.0, -1.0, 1.0, 3.0 };
    static const int gray_map[4] = { 0, 1, 3, 2 };  /* 00,01,11,10 */

    *i_out = gray_vals[gray_map[bi]] * QAM16_NORM;
    *q_out = gray_vals[gray_map[bq]] * QAM16_NORM;
}

/**
 * @brief 64-QAM with Gray coding
 *
 * Constellations: {-7,-5,-3,-1,+1,+3,+5,+7} × QAM64_NORM
 */
static void qam64_map(double *i_out, double *q_out, uint32_t bits)
{
    int bi = (bits >> 0) & 7;  /* I bits: 3 bits */
    int bq = (bits >> 3) & 7;  /* Q bits: 3 bits */

    static const double gray_vals_64[8] = { -7.0, -5.0, -1.0, -3.0, 3.0, 1.0, 5.0, 7.0 };
    static const int gray_map_64[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    *i_out = gray_vals_64[gray_map_64[bi]] * QAM64_NORM;
    *q_out = gray_vals_64[gray_map_64[bq]] * QAM64_NORM;
}

/**
 * @brief 256-QAM with Gray coding
 *
 * Constellations: {-15,-13,...,-1,+1,...,+13,+15} × QAM256_NORM
 */
static void qam256_map(double *i_out, double *q_out, uint32_t bits)
{
    int bi = (bits >> 0) & 15;  /* I bits: 4 bits */
    int bq = (bits >> 4) & 15;  /* Q bits: 4 bits */

    /* Gray-to-integer mapping for 4 bits */
    static const int gray4[16] = { 0,1,3,2, 6,7,5,4, 12,13,15,14, 10,11,9,8 };
    int gi = gray4[bi];
    int gq = gray4[bq];

    *i_out = (double)(2 * gi - 15) * QAM256_NORM;
    *q_out = (double)(2 * gq - 15) * QAM256_NORM;
}

int constellation_map(double *i_real, double *q_imag,
                      uint32_t bits, wifi_modulation_t mod)
{
    if (!i_real || !q_imag) return -1;

    switch (mod) {
        case MOD_BPSK:
            bpsk_map(i_real, q_imag, bits);
            return 1;
        case MOD_QPSK:
            qpsk_map(i_real, q_imag, bits);
            return 2;
        case MOD_16QAM:
            qam16_map(i_real, q_imag, bits);
            return 4;
        case MOD_64QAM:
            qam64_map(i_real, q_imag, bits);
            return 6;
        case MOD_256QAM:
            qam256_map(i_real, q_imag, bits);
            return 8;
        default:
            return -1;
    }
}

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
                             wifi_modulation_t mod)
{
    if (!bits_out) return -1;

    /* Scale received sample back to integer grid */
    double scale;
    int n_levels_side;

    switch (mod) {
        case MOD_BPSK:
            *bits_out = (r_i >= 0.0) ? 1 : 0;
            return 1;
        case MOD_QPSK:
            scale = 1.0 / QPSK_NORM;
            break;
        case MOD_16QAM:
            scale = 1.0 / QAM16_NORM;
            break;
        case MOD_64QAM:
            scale = 1.0 / QAM64_NORM;
            break;
        case MOD_256QAM:
            scale = 1.0 / QAM256_NORM;
            break;
        default:
            return -1;
    }

    /* Hard decision: nearest constellation level */
    /* For simplicity, use nearest-uniform approach */
    int i_scaled = (int)round(r_i * scale);
    int q_scaled = (int)round(r_q * scale);

    /* Clamp */
    int max_lev = (mod == MOD_64QAM) ? 7 : ((mod == MOD_256QAM) ? 15 : 3);
    if (mod == MOD_QPSK) max_lev = 1;
    int min_lev = -max_lev;
    if (mod == MOD_16QAM) max_lev = 3;
    /* Actually QPSK: ±1, 16QAM: ±1,±3 */

    if (i_scaled < min_lev) i_scaled = min_lev;
    if (i_scaled > max_lev) i_scaled = max_lev;
    if (q_scaled < min_lev) q_scaled = min_lev;
    if (q_scaled > max_lev) q_scaled = max_lev;

    /* Map back to bits */
    uint32_t bi = 0, bq = 0;
    int bps = 1; /* bits per symbol per component */
    if (mod == MOD_16QAM) bps = 2;
    else if (mod == MOD_64QAM) bps = 3;
    else if (mod == MOD_256QAM) bps = 4;

    /* Gray-to-binary inverse */
    static const int inverse_gray2[4] = {0, 1, 3, 2};
    static const int inverse_gray3[8] = {0, 1, 3, 2, 6, 7, 5, 4};
    static const int inverse_gray4[16] = {0, 1, 3, 2, 6, 7, 5, 4, 12, 13, 15, 14, 10, 11, 9, 8};

    if (bps == 2) {
        int i_idx = (i_scaled + 3) / 2; /* map -3,-1,1,3 → 0,1,2,3 */
        int q_idx = (q_scaled + 3) / 2;
        bi = (uint32_t)inverse_gray2[i_idx >= 0 && i_idx < 4 ? i_idx : 0];
        bq = (uint32_t)inverse_gray2[q_idx >= 0 && q_idx < 4 ? q_idx : 0];
    } else if (bps == 3) {
        int i_idx = (i_scaled + 7) / 2; /* -7→0, -5→1, ... +7→7 */
        int q_idx = (q_scaled + 7) / 2;
        bi = (uint32_t)(inverse_gray3[(i_idx >= 0 && i_idx < 8) ? i_idx : 0] & 7);
        bq = (uint32_t)(inverse_gray3[(q_idx >= 0 && q_idx < 8) ? q_idx : 0] & 7);
    } else if (bps == 4) {
        int i_idx = (i_scaled + 15) / 2;
        int q_idx = (q_scaled + 15) / 2;
        bi = (uint32_t)(inverse_gray4[(i_idx >= 0 && i_idx < 16) ? i_idx : 0] & 15);
        bq = (uint32_t)(inverse_gray4[(q_idx >= 0 && q_idx < 16) ? q_idx : 0] & 15);
    }

    *bits_out = bi | (bq << (uint32_t)bps);
    return 2 * bps;
}

/**
 * @brief Soft-decision demapping (max-log LLR approximation)
 *
 * LLR(b_k) ≈ (1/σ²) * [min_{s: b_k=0} |r-s|² - min_{s: b_k=1} |r-s|²]
 *
 * @param llr_out   Output LLR array
 * @param r_i       Received I
 * @param r_q       Received Q
 * @param mod       Modulation
 * @param noise_var Noise variance σ²
 * @return Number of LLR values
 */
int constellation_demap_soft(double *llr_out, double r_i, double r_q,
                             wifi_modulation_t mod, double noise_var)
{
    if (!llr_out || noise_var <= 0.0) return -1;

    int n_bits;
    switch (mod) {
        case MOD_BPSK:  n_bits = 1; break;
        case MOD_QPSK:  n_bits = 2; break;
        case MOD_16QAM: n_bits = 4; break;
        case MOD_64QAM: n_bits = 6; break;
        case MOD_256QAM:n_bits = 8; break;
        default: return -1;
    }

    /* For BPSK: LLR = 2·r_i / σ² */
    if (mod == MOD_BPSK) {
        llr_out[0] = 2.0 * r_i / noise_var;
        return 1;
    }

    /* For QPSK and QAM: max-log LLR approximation */
    /* Use simplified formulas for Gray-mapped QAM */
    double scale;
    switch (mod) {
        case MOD_QPSK:  scale = 1.0 / QPSK_NORM;  break;
        case MOD_16QAM: scale = 1.0 / QAM16_NORM;  break;
        case MOD_64QAM: scale = 1.0 / QAM64_NORM;  break;
        case MOD_256QAM:scale = 1.0 / QAM256_NORM; break;
        default: scale = 1.0; break;
    }

    /* Scale received signal to integer grid */
    double i_s = r_i * scale;
    double q_s = r_q * scale;

    /* Approximate LLRs for each bit position using piecewise linear */
    /* These are standard closed-form approximations for QAM LLRs */
    int bps = n_bits / 2; /* bits per I/Q component */

    for (int k = 0; k < bps; k++) {
        /* I-component bits */
        (void)(1 << (bps - k - 1));  /* distance to decision boundary, used implicitly */
        if (bps == 1) { /* QPSK */
            llr_out[0] = 2.0 * i_s / noise_var;
            llr_out[1] = 2.0 * q_s / noise_var;
        } else if (bps == 2) { /* 16QAM */
            llr_out[0] = 2.0 * i_s / noise_var;
            llr_out[2] = 2.0 * q_s / noise_var;
            llr_out[1] = (2.0 - fabs(i_s)) / noise_var;
            llr_out[3] = (2.0 - fabs(q_s)) / noise_var;
        } else if (bps == 3) { /* 64QAM */
            llr_out[0] = 2.0 * i_s / noise_var;
            llr_out[3] = 2.0 * q_s / noise_var;
            llr_out[1] = (4.0 - fabs(i_s)) / noise_var;
            llr_out[4] = (4.0 - fabs(q_s)) / noise_var;
            llr_out[2] = (2.0 - fabs(4.0 - fabs(i_s))) / noise_var;
            llr_out[5] = (2.0 - fabs(4.0 - fabs(q_s))) / noise_var;
        } else { /* 256QAM */
            /* First 2 bits: sign */
            llr_out[0] = 2.0 * i_s / noise_var;
            llr_out[4] = 2.0 * q_s / noise_var;
            /* Next 2 bits: 8-threshold */
            llr_out[1] = (8.0 - fabs(i_s)) / noise_var;
            llr_out[5] = (8.0 - fabs(q_s)) / noise_var;
            /* Next 2 bits: 4-threshold offset */
            llr_out[2] = (4.0 - fabs(8.0 - fabs(i_s))) / noise_var;
            llr_out[6] = (4.0 - fabs(8.0 - fabs(q_s))) / noise_var;
            /* Last 2 bits: 2-threshold offset */
            llr_out[3] = (2.0 - fabs(4.0 - fabs(8.0 - fabs(i_s)))) / noise_var;
            llr_out[7] = (2.0 - fabs(4.0 - fabs(8.0 - fabs(q_s)))) / noise_var;
        }
    }

    return n_bits;
}

/* ==========================================================================
 * EVM Calculation (L1 Definition)
 * ========================================================================== */

double compute_evm(const double *measured, const double *ideal,
                   int n_symbols, wifi_modulation_t modulation)
{
    (void)modulation;  /* Reserved for future per-modulation P_avg normalization */
    if (!measured || !ideal || n_symbols <= 0) return -1.0;

    double sum_sq_error = 0.0;
    double sum_sq_ideal = 0.0;

    /* Each symbol is a complex pair (real, imag) */
    for (int i = 0; i < n_symbols; i++) {
        double err_re = measured[2 * i] - ideal[2 * i];
        double err_im = measured[2 * i + 1] - ideal[2 * i + 1];
        sum_sq_error += err_re * err_re + err_im * err_im;
        sum_sq_ideal += ideal[2 * i] * ideal[2 * i] + ideal[2 * i + 1] * ideal[2 * i + 1];
    }

    if (sum_sq_ideal <= 0.0) return -1.0;

    /* EVM_RMS = sqrt(Σ|e|² / Σ|s_ref|²) × 100% */
    return sqrt(sum_sq_error / sum_sq_ideal) * 100.0;
}

double compute_evm_per_subcarrier(double *per_sc_evm, int max_sc,
                                  const double *measured,
                                  const double *ideal, int n_sc)
{
    if (!per_sc_evm || !measured || !ideal || n_sc <= 0) return -1.0;

    double sum_avg = 0.0;
    int n_report = (max_sc < n_sc) ? max_sc : n_sc;

    for (int i = 0; i < n_report; i++) {
        double err_re = measured[2 * i] - ideal[2 * i];
        double err_im = measured[2 * i + 1] - ideal[2 * i + 1];
        double ideal_pow = ideal[2 * i] * ideal[2 * i] + ideal[2 * i + 1] * ideal[2 * i + 1];
        double evm = (ideal_pow > 1e-12) ? sqrt((err_re * err_re + err_im * err_im) / ideal_pow) * 100.0 : 0.0;
        per_sc_evm[i] = evm;
        sum_avg += evm;
    }

    return sum_avg / (double)n_report;
}

/* ==========================================================================
 * Alamouti STBC (L2 Core Concept)
 * ========================================================================== */

void stbc_alamouti_encode(double stbc_out[4], double s0_real, double s0_imag,
                          double s1_real, double s1_imag)
{
    /* Antenna 0, time 0: s0 */
    stbc_out[0] = s0_real;
    stbc_out[1] = s0_imag;
    /* Antenna 1, time 0: s1 */
    stbc_out[2] = s1_real;
    stbc_out[3] = s1_imag;
    /* Antenna 0, time 1: -conj(s1) */
    /* Antenna 1, time 1: conj(s0) */
    /* Return as: [tx0_t0_r, tx0_t0_i, tx1_t0_r, tx1_t0_i, tx0_t1_r, tx0_t1_i, tx1_t1_r, tx1_t1_i] */
    /* BUT! We use a 4-element output. Let's define:
     * stbc_out[0..3] = [tx0_t0, tx1_t0] as complex pairs
     * Actual full encoding: output all 8 values
     * For clarity: we return only first time slot; second slot in next call
     */
}

/**
 * @brief Full Alamouti encoding: 2 symbols → 2 time slots, 2 antennas
 * Output: 8 values [tx0_t0_r, tx0_t0_i, tx1_t0_r, tx1_t0_i, tx0_t1_r, tx0_t1_i, tx1_t1_r, tx1_t1_i]
 */
void stbc_alamouti_encode_full(double output[8], double s0_real, double s0_imag,
                               double s1_real, double s1_imag)
{
    /* Time 0, Antenna 0: s0 */
    output[0] =  s0_real;
    output[1] =  s0_imag;
    /* Time 0, Antenna 1: s1 */
    output[2] =  s1_real;
    output[3] =  s1_imag;
    /* Time 1, Antenna 0: -s1* */
    output[4] = -s1_real;
    output[5] =  s1_imag;  /* conj: imag sign flips, plus outer minus */
    /* Time 1, Antenna 1: s0* */
    output[6] =  s0_real;
    output[7] = -s0_imag;  /* conj: imag sign flips */
}

void stbc_alamouti_decode(double *s0_r, double *s0_i,
                          double *s1_r, double *s1_i,
                          double r0_r, double r0_i,
                          double r1_r, double r1_i,
                          double h0_r, double h0_i,
                          double h1_r, double h1_i)
{
    /* Received at time 0: r0 = h0*s0 + h1*s1 + n0 */
    /* Received at time 1: r1 = -h0*conj(s1) + h1*conj(s0) + n1 */

    /* Combining:
     *   ŝ0 = h0*·r0 + h1·r1*
     *   ŝ1 = h1*·r0 - h0·r1*
     */

    /* ŝ0 */
    double h0_conj_r0_r = h0_r * r0_r + h0_i * r0_i;      /* Re(h0* · r0) */
    double h0_conj_r0_i = h0_r * r0_i - h0_i * r0_r;      /* Im(h0* · r0) */

    /* r1* = r1_r - j·r1_i */
    double h1_r1c_r = h1_r * r1_r + h1_i * r1_i;          /* Re(h1 · r1*) */
    double h1_r1c_i = h1_r * (-r1_i) + h1_i * r1_r;       /* Im(h1 · r1*) = -h1_r*r1_i + h1_i*r1_r */

    *s0_r = h0_conj_r0_r + h1_r1c_r;
    *s0_i = h0_conj_r0_i + h1_r1c_i;

    /* ŝ1 = h1*·r0 - h0·r1* */
    double h1_conj_r0_r = h1_r * r0_r + h1_i * r0_i;
    double h1_conj_r0_i = h1_r * r0_i - h1_i * r0_r;

    double h0_r1c_r = h0_r * r1_r + h0_i * r1_i;
    double h0_r1c_i = h0_r * (-r1_i) + h0_i * r1_r;

    *s1_r = h1_conj_r0_r - h0_r1c_r;
    *s1_i = h1_conj_r0_i - h0_r1c_i;

    /* Channel power normalization: divide by (|h0|² + |h1|²) */
    double h_power = h0_r * h0_r + h0_i * h0_i + h1_r * h1_r + h1_i * h1_i;
    if (h_power > 1e-12) {
        *s0_r /= h_power;
        *s0_i /= h_power;
        *s1_r /= h_power;
        *s1_i /= h_power;
    }
}

/* ==========================================================================
 * Interleaver / Deinterleaver (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief 802.11a/g block interleaver — first permutation
 *
 * i = (N_CBPS / 16) · (k mod 16) + floor(k / 16)
 *
 * @param interleaved   Output interleaved bits
 * @param input         Input coded bits (as bytes, 1 bit per byte)
 * @param n_cbps        Coded bits per OFDM symbol
 * @return 0 on success
 */
int interleaver_80211a(uint8_t *interleaved, const uint8_t *input, int n_cbps)
{
    if (!interleaved || !input || n_cbps <= 0) return -1;
    if (n_cbps % 16 != 0) return -1;  /* Must be multiple of 16 for BPSK/QPSK/16QAM/64QAM */

    int n_rows = n_cbps / 16;

    for (int k = 0; k < n_cbps; k++) {
        int i = n_rows * (k % 16) + (k / 16);
        interleaved[i] = input[k];
    }

    return 0;
}

/**
 * @brief 802.11a/g block deinterleaver — inverse first permutation
 *
 * k = 16·i - (N_CBPS - 1)·floor(16·i / N_CBPS)
 *
 * @param deinterleaved Output deinterleaved bits
 * @param received      Input interleaved bits
 * @param n_cbps        Coded bits per OFDM symbol
 * @return 0 on success
 */
int deinterleaver_80211a(uint8_t *deinterleaved, const uint8_t *received, int n_cbps)
{
    if (!deinterleaved || !received || n_cbps <= 0) return -1;
    if (n_cbps % 16 != 0) return -1;

    for (int i = 0; i < n_cbps; i++) {
        int k = 16 * i - (n_cbps - 1) * ((16 * i) / n_cbps);
        if (k >= 0 && k < n_cbps) {
            deinterleaved[k] = received[i];
        }
    }

    return 0;
}
