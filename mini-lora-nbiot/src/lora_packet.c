/**
 * @file lora_packet.c
 * @brief LoRa Packet Encoder/Decoder -- Frame construction, interleaving, deinterleaving
 *
 * Knowledge: L5 LoRa packet structure, gray indexing, diagonal interleaving,
 *            L6 packet encoding/decoding pipeline
 *
 * LoRa packet structure:
 *   [Preamble] [Sync] [Header (explicit)] [Payload] [CRC]
 *
 * The header contains: payload length, CR, CRC flag
 * Encoded with highest robustness (CR 4/8) to protect framing info.
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "lora_phy.h"
#include "lora_mac.h"

/* ======================================================================
   Gray Indexing
   ====================================================================== */

/*
 * Binary-to-Gray code conversion.
 *
 * Gray code ensures adjacent symbol values differ by exactly one bit.
 * This minimizes bit errors when noise causes symbol misdetection
 * to an adjacent FFT bin (the most common error in CSS demodulation).
 *
 * G = B XOR (B >> 1)
 *
 * Example: B=0(000) -> G=0(000), B=1(001) -> G=1(001),
 *          B=2(010) -> G=3(011), B=3(011) -> G=2(010)
 *
 * @param binary  Binary value (0 to 2^SF - 1)
 * @param sf      Spreading factor (number of bits)
 * @return Gray-coded value
 */
static uint32_t binary_to_gray(uint32_t binary, uint8_t sf)
{
    uint32_t mask = ((uint32_t)1 << sf) - 1;
    uint32_t gray = binary ^ (binary >> 1);
    return gray & mask;
}

/*
 * Gray-to-Binary conversion.
 *
 * B = G
 * for i = bits-2 down to 0: B[i] = B[i+1] XOR G[i]
 *
 * @param gray   Gray-coded value
 * @param sf     Spreading factor
 * @return Binary value
 */
static uint32_t gray_to_binary(uint32_t gray, uint8_t sf)
{
    uint32_t binary = gray;
    uint32_t mask = (uint32_t)1 << (sf - 1);
    while (mask > 1) {
        binary ^= (binary & mask) >> 1;
        mask >>= 1;
    }
    return binary;
}

/* ======================================================================
   Diagonal Interleaving
   ====================================================================== */

/*
 * LoRa diagonal interleaver.
 *
 * Interleaving spreads adjacent coded bits across different symbols
 * and different bit positions within symbols. This converts burst
 * errors (from fading or interference) into isolated errors that
 * the Hamming FEC can correct.
 *
 * Interleaving matrix dimensions:
 *   Rows = CR + 4 (coding rate parameter, 5-8)
 *   Cols = SF (spreading factor, number of bits per symbol)
 *
 * Data bits are written row-by-row into the matrix, then read
 * out diagonal-by-diagonal. This ensures adjacent input bits
 * are separated in time (different symbols) and position.
 *
 * Derived from the deinterleaver specification in Semtech patent.
 *
 * @param input     Input bits (coded payload)
 * @param input_len Number of input bits
 * @param cr        Coding rate (determines rows)
 * @param sf        Spreading factor (determines columns)
 * @param output    Interleaved output bits
 * @return Number of output bits
 */
int lora_diagonal_interleave(const uint8_t *input, size_t input_len,
                              lora_coding_rate_t cr __attribute__((unused)), uint8_t sf,
                              uint8_t *output)
{
    if (!input || !output) return -1;

    uint8_t rows __attribute__((unused)) = (uint8_t)cr + 4;
    uint8_t cols = sf;                /* 7-12 columns */

    size_t num_symbols = input_len / sf;
    if (input_len % sf != 0) num_symbols++;

    /* Zero-pad output */
    memset(output, 0, num_symbols * sf);

    /*
     * For each diagonal d (0 to cols-1):
     *   For each symbol s (0 to num_symbols-1):
     *     output[s * cols + d] = input[s * cols + f(d)]
     *
     * The mapping function f depends on the interleaver specification.
     * Simplified: write column-by-column, read diagonal.
     */
    for (size_t s = 0; s < num_symbols; s++) {
        for (uint8_t c = 0; c < cols; c++) {
            size_t in_idx = s * cols + c;
            if (in_idx >= input_len) continue;

            /* Diagonal mapping: output column = (c + s) mod cols */
            uint8_t out_col = (c + (uint8_t)(s % cols)) % cols;
            size_t out_idx = s * cols + out_col;
            output[out_idx] = input[in_idx] & 1;
        }
    }

    return (int)(num_symbols * sf);
}

/*
 * LoRa diagonal deinterleaver (inverse of interleave).
 *
 * Reads diagonally, writes row-by-row to recover original order.
 * This is applied at the receiver before FEC decoding.
 *
 * @param input     Interleaved bits (received symbols)
 * @param input_len Number of input bits
 * @param cr        Coding rate
 * @param sf        Spreading factor
 * @param output    Deinterleaved bits
 * @return Number of output bits
 */
int lora_diagonal_deinterleave(const uint8_t *input, size_t input_len,
                                lora_coding_rate_t cr __attribute__((unused)), uint8_t sf,
                                uint8_t *output)
{
    if (!input || !output) return -1;

    uint8_t cols = sf;
    size_t num_symbols = input_len / sf;

    memset(output, 0, input_len);

    for (size_t s = 0; s < num_symbols; s++) {
        for (uint8_t c = 0; c < cols; c++) {
            /* Inverse diagonal mapping */
            uint8_t out_col = (c + (uint8_t)(s % cols)) % cols;
            size_t in_idx = s * cols + out_col;
            size_t out_idx = s * cols + c;

            if (in_idx < input_len && out_idx < input_len)
                output[out_idx] = input[in_idx] & 1;
        }
    }

    return (int)(num_symbols * sf);
}

/* ======================================================================
   Packet Encoding Pipeline
   ====================================================================== */

/*
 * Encode a complete LoRa physical layer packet.
 *
 * Encoding pipeline:
 *   1. Compute CRC-16 over payload
 *   2. Add header (payload length, CR, CRC flag) if explicit mode
 *   3. Hamming(7,4) encode all nibbles (header + payload + CRC)
 *   4. Diagonal interleaving
 *   5. Data whitening (except preamble and sync)
 *   6. Gray indexing of each symbol
 *   7. Generate chirp samples for each symbol
 *
 * @param params       PHY parameters
 * @param payload      Application payload bytes
 * @param payload_len  Payload length
 * @param buffer       Output complex sample buffer
 * @param max_samples  Maximum output samples
 * @return Total samples generated, or -1 on error
 */
int lora_encode_packet(const lora_phy_params_t *params,
                        const uint8_t *payload, uint16_t payload_len,
                        double complex *buffer, size_t max_samples)
{
    if (!params || !buffer) return -1;
    if (payload_len > 255) return -1;  /* LoRa max payload */

    uint8_t SF = (uint8_t)params->sf;
    uint8_t CR = (uint8_t)params->cr;
    uint32_t N = params->num_chips;

    /*
     * Step 1: Build the raw bitstream.
     *
     * Format (explicit header):
     *   [Header: 8 bits payload_len | 3 bits CR | 1 bit CRC flag]
     *   [Header CRC: 8 bits]
     *   [Payload: payload_len bytes]
     *   [CRC-16: 2 bytes]
     *
     * Total raw bits before FEC:
     *   = (header_bits + payload_bits + crc_bits)
     *
     * After Hamming(7,4) with rate CR:
     *   coded_bits = raw_nibbles * (4 + CR)
     *
     * Number of symbols needed:
     *   num_symbols = ceil(coded_bits / SF)
     */

    /* Calculate raw data size */
    size_t header_nibbles = 4;   /* 2 bytes header = 4 nibbles */
    size_t payload_nibbles = ((size_t)payload_len * 8 + 3) / 4;  /* ceil(8*PL/4) */
    size_t crc_nibbles = 4;      /* 2 bytes CRC = 4 nibbles */
    size_t total_nibbles = header_nibbles + payload_nibbles + crc_nibbles;

    /* Allocate temporary buffers */
    uint8_t *raw_data = (uint8_t *)calloc(total_nibbles, 1);
    uint8_t *coded_bits = (uint8_t *)calloc(total_nibbles * 8, 1);
    uint8_t *interleaved = (uint8_t *)calloc(total_nibbles * 8, 1);

    if (!raw_data || !coded_bits || !interleaved) {
        free(raw_data); free(coded_bits); free(interleaved);
        return -1;
    }

    /* Build header nibbles */
    raw_data[0] = (payload_len >> 4) & 0x0F;
    raw_data[1] = payload_len & 0x0F;
    raw_data[2] = ((CR - 1) << 1) | (params->enable_crc ? 1 : 0);
    raw_data[3] = raw_data[0] ^ raw_data[1] ^ raw_data[2];  /* Header checksum */

    /* Copy payload nibbles */
    for (size_t i = 0; i < payload_len; i++) {
        raw_data[header_nibbles + 2*i]     = (payload[i] >> 4) & 0x0F;
        raw_data[header_nibbles + 2*i + 1] = payload[i] & 0x0F;
    }

    /* Compute and append CRC-16 over payload */
    uint16_t crc = lora_crc16(payload, payload_len);
    size_t crc_start = header_nibbles + payload_nibbles;
    raw_data[crc_start + 0] = (crc >> 12) & 0x0F;
    raw_data[crc_start + 1] = (crc >> 8)  & 0x0F;
    raw_data[crc_start + 2] = (crc >> 4)  & 0x0F;
    raw_data[crc_start + 3] = crc & 0x0F;

    /* Step 2: Hamming(7,4) encode each nibble */
    size_t coded_bit_count = 0;
    for (size_t i = 0; i < total_nibbles; i++) {
        uint8_t tmp[8];
        int bits = lora_hamming_encode(raw_data[i], params->cr, tmp);
        if (bits < 0) { free(raw_data); free(coded_bits); free(interleaved); return -1; }
        for (int b = 0; b < bits; b++)
            coded_bits[coded_bit_count++] = tmp[b];
    }

    /* Step 3: Diagonal interleaving */
    size_t il_count __attribute__((unused)) = (coded_bit_count + SF - 1) / SF * SF;  /* Pad to SF boundary */
    int il_bits = lora_diagonal_interleave(coded_bits, coded_bit_count,
                                            params->cr, SF, interleaved);
    if (il_bits < 0) { free(raw_data); free(coded_bits); free(interleaved); return -1; }

    /* Step 4: Data whitening */
    lora_whiten(interleaved, (il_bits + 7) / 8);

    /* Step 5: Generate symbols and chirp samples */
    size_t num_symbols = (size_t)il_bits / SF;
    if (il_bits % SF != 0) num_symbols++;

    /* Step 5a: Preamble first */
    int preamble_samples = lora_generate_preamble(params, buffer, max_samples);
    if (preamble_samples < 0) { free(raw_data); free(coded_bits); free(interleaved); return -1; }

    size_t sample_idx = (size_t)preamble_samples;

    /* Step 5b: Encode each data symbol */
    for (size_t s = 0; s < num_symbols; s++) {
        /* Extract SF bits for this symbol */
        uint32_t sym_bits = 0;
        for (uint8_t b = 0; b < SF; b++) {
            size_t bit_pos = s * SF + b;
            if (bit_pos < (size_t)il_bits)
                sym_bits = (sym_bits << 1) | (interleaved[bit_pos] & 1);
            else
                sym_bits = sym_bits << 1;  /* Zero pad */
        }

        /* Gray index the symbol bits */
        uint32_t gray_sym = binary_to_gray(sym_bits, SF);

        /* Generate chirp samples for this symbol */
        for (uint32_t n = 0; n < N; n++) {
            if (sample_idx >= max_samples) {
                free(raw_data); free(coded_bits); free(interleaved);
                return -1;
            }
            buffer[sample_idx++] = lora_chirp_sample(params, gray_sym, n,
                                                      (double)params->chip_rate);
        }
    }

    free(raw_data);
    free(coded_bits);
    free(interleaved);

    return (int)sample_idx;
}

/* ======================================================================
   Packet Decoding Pipeline
   ====================================================================== */

/*
 * Decode a LoRa physical layer packet from received samples.
 *
 * Decoding pipeline (inverse of encoding):
 *   1. Demodulate each symbol using FFT dechirping
 *   2. Gray-to-binary conversion
 *   3. Data de-whitening
 *   4. Diagonal deinterleaving
 *   5. Hamming(7,4) decode each nibble
 *   6. Extract header, payload, CRC
 *   7. Verify CRC
 *
 * @param params        PHY parameters
 * @param samples       Received complex samples (after preamble detection)
 * @param num_samples   Number of received samples
 * @param payload       Output: decoded payload
 * @param max_payload   Maximum payload buffer size
 * @param payload_len   Output: actual payload length
 * @return 0 on success (CRC verified), -1 on decoding error, -2 on CRC mismatch
 */
int lora_decode_packet(const lora_phy_params_t *params,
                        const double complex *samples, size_t num_samples,
                        uint8_t *payload, uint16_t max_payload,
                        uint16_t *payload_len)
{
    if (!params || !samples || !payload || !payload_len) return -1;

    uint8_t SF = (uint8_t)params->sf;
    uint32_t N = params->num_chips;

    /* Step 1: Demodulate symbols */
    size_t num_symbols = num_samples / N;
    if (num_symbols < 8) return -1;  /* Need at least header */

    uint8_t *symbols = (uint8_t *)calloc(num_symbols, 1);
    uint8_t *raw_bits = (uint8_t *)calloc(num_symbols * SF, 1);

    if (!symbols || !raw_bits) {
        free(symbols); free(raw_bits);
        return -1;
    }

    /* Demodulate each symbol via FFT dechirping */
    size_t bit_count = 0;
    for (size_t s = 0; s < num_symbols; s++) {
        int sym = lora_demodulate_symbol_fft(params, &samples[s * N], N);
        if (sym < 0) { free(symbols); free(raw_bits); return -1; }

        /* Gray-to-binary */
        uint32_t bin_sym = gray_to_binary((uint32_t)sym, SF);

        /* Extract SF bits (MSB first from binary representation) */
        for (int b = SF - 1; b >= 0; b--) {
            raw_bits[bit_count++] = (bin_sym >> b) & 1;
        }
    }

    /* Step 2: Data de-whitening */
    lora_whiten(raw_bits, (bit_count + 7) / 8);

    /* Step 3: Diagonal deinterleaving */
    uint8_t *deint_bits = (uint8_t *)calloc(bit_count, 1);
    if (!deint_bits) { free(symbols); free(raw_bits); return -1; }

    lora_diagonal_deinterleave(raw_bits, bit_count, params->cr, SF, deint_bits);

    /* Step 4: Hamming decode */
    size_t num_nibbles = bit_count / (size_t)(params->cr + 4);
    uint8_t *nibbles = (uint8_t *)calloc(num_nibbles, 1);
    if (!nibbles) { free(symbols); free(raw_bits); free(deint_bits); return -1; }

    for (size_t i = 0; i < num_nibbles; i++) {
        uint8_t cw_bits[8];
        int cw_len = (int)(params->cr + 4);
        for (int b = 0; b < cw_len; b++) {
            size_t idx = i * (size_t)cw_len + (size_t)b;
            cw_bits[b] = (idx < bit_count) ? deint_bits[idx] : 0;
        }
        lora_hamming_decode(cw_bits, params->cr, &nibbles[i]);
    }

    /* Step 5: Extract header */
    if (num_nibbles < 8) { free(symbols); free(raw_bits); free(deint_bits); free(nibbles); return -1; }

    uint8_t decoded_pl  = (nibbles[0] << 4) | nibbles[1];
    uint8_t decoded_cr __attribute__((unused)) = ((nibbles[2] >> 1) & 0x03) + 1;
    uint8_t decoded_crc = nibbles[2] & 0x01;
    uint8_t hdr_chk = nibbles[0] ^ nibbles[1] ^ nibbles[2];
    hdr_chk = hdr_chk & 0x0F;

    if (hdr_chk != nibbles[3]) {
        /* Header checksum failed */
        free(symbols); free(raw_bits); free(deint_bits); free(nibbles);
        return -1;
    }

    /* Step 6: Extract payload */
    if (decoded_pl > max_payload) {
        free(symbols); free(raw_bits); free(deint_bits); free(nibbles);
        return -1;
    }

    for (uint16_t i = 0; i < decoded_pl; i++) {
        payload[i] = (nibbles[4 + 2*i] << 4) | (nibbles[4 + 2*i + 1] & 0x0F);
    }

    // Move this variable declaration before the for loop
    /* Step 7: Verify CRC */
    size_t crc_nib_offset = 4 + (size_t)decoded_pl * 2;
    uint16_t decoded_crc_val = ((uint16_t)nibbles[crc_nib_offset] << 12)
                             | ((uint16_t)nibbles[crc_nib_offset + 1] << 8)
                             | ((uint16_t)nibbles[crc_nib_offset + 2] << 4)
                             | ((uint16_t)nibbles[crc_nib_offset + 3]);
    uint16_t computed_crc = lora_crc16(payload, decoded_pl);

    *payload_len = decoded_pl;

    free(symbols); free(raw_bits); free(deint_bits); free(nibbles);

    if (decoded_crc && decoded_crc_val != computed_crc)
        return -2;  /* CRC mismatch */

    return 0;
}

/* ======================================================================
   L6: Packet Convenience Functions
   ====================================================================== */

/*
 * Calculate the minimum buffer size needed for packet encoding.
 *
 * total_samples = (N_preamble + 4.25 + N_payload_sym) * 2^SF
 *
 * @param params PHY parameters
 * @return Required buffer size in complex samples
 */
size_t lora_packet_buffer_size(const lora_phy_params_t *params)
{
    if (!params) return 0;

    int ps = lora_payload_symbol_count(params);
    if (ps < 0) return 0;

    uint32_t N = params->num_chips;
    double total_syms = (double)params->preamble_len + 4.25 + (double)ps;
    return (size_t)(total_syms * (double)N);
}

/*
 * Extract basic information from decoded symbols without full decoding.
 *
 * Reads the first few symbols to determine payload length and CR,
 * which helps with memory allocation before full decode.
 *
 * @param params   PHY parameters
 * @param samples  First 8+ symbols of received samples
 * @param pl       Output: payload length
 * @param cr       Output: coding rate
 * @return 0 on success, -1 on error
 */
int lora_peek_header(const lora_phy_params_t *params,
                      const double complex *samples,
                      uint8_t *pl, lora_coding_rate_t *cr)
{
    if (!params || !samples || !pl || !cr) return -1;

    uint8_t SF = (uint8_t)params->sf;
    uint32_t N = params->num_chips;

    /* Demodulate first 8 symbols (enough for header) */
    uint32_t symbols[8];
    for (int s = 0; s < 8; s++) {
        int sym = lora_demodulate_symbol_fft(params, &samples[s * N], N);
        if (sym < 0) return -1;
        symbols[s] = (uint32_t)sym;
    }

    /* Convert first 8 symbols to bits, dewhiten, decode header nibbles */
    uint8_t raw_bits[8 * 12];  /* Max SF=12 */
    size_t bc = 0;
    for (int s = 0; s < 8; s++) {
        uint32_t bin = gray_to_binary(symbols[s], SF);
        for (int b = SF - 1; b >= 0; b--)
            raw_bits[bc++] = (bin >> b) & 1;
    }

    /* Rough estimate: assume SF bits per symbol, decode first 4 nibbles */
    uint8_t nibbles[4] = {0};
    size_t bits_per_nib = (size_t)(params->cr + 4);

    for (int i = 0; i < 4; i++) {
        uint8_t cw[8] = {0};
        for (int b = 0; b < (int)bits_per_nib && (size_t)(i * (int)bits_per_nib + b) < bc; b++) {
            cw[b] = raw_bits[i * (int)bits_per_nib + b];
        }
        lora_hamming_decode(cw, params->cr, &nibbles[i]);
    }

    *pl = (nibbles[0] << 4) | nibbles[1];
    *cr = (lora_coding_rate_t)(((nibbles[2] >> 1) & 0x03) + 1);

    return 0;
}
