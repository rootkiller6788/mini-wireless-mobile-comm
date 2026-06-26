/**
 * @file lora_demo.c
 * @brief LoRa PHY demonstration -- chirp generation, modulation, demodulation, airtime
 * L6: End-to-end LoRa packet transmission simulation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "lora_phy.h"

int main(void) {
    printf("=== LoRa PHY Demo ===\n\n");

    lora_phy_params_t params;
    lora_phy_params_init_default(&params);

    printf("Configuration:\n");
    printf("  SF: %d\n", (int)params.sf);
    printf("  BW: %d Hz\n", (int)params.bw);
    printf("  CR: 4/%d\n", (int)(4 + params.cr));
    printf("  Carrier: %.3f MHz\n", params.carrier_freq / 1e6);
    printf("  Chips/symbol: %u\n", params.num_chips);
    printf("  Symbol period: %.3f ms\n", params.symbol_period * 1000.0);
    printf("  Bit rate: %.2f bps\n", params.bit_rate);

    printf("\n[L4] Receiver sensitivity:\n");
    for (int sf = 7; sf <= 12; sf++) {
        double sens = lora_receiver_sensitivity((lora_spreading_factor_t)sf,
                                                 LORA_BW_125_KHZ, 6.0);
        double Gp = lora_processing_gain_db((lora_spreading_factor_t)sf);
        printf("  SF%d: Sensitivity=%.1f dBm, ProcGain=%.1f dB\n", sf, sens, Gp);
    }

    printf("\n[L3] Chirp sample generation (SF7, first 10 samples of symbol 0):\n");
    for (int i = 0; i < 10; i++) {
        double complex s = lora_chirp_sample(&params, 0, (uint32_t)i, 125000.0);
        printf("  n=%d: %.4f + j%.4f  |s|=%.4f\n",
               i, creal(s), cimag(s), cabs(s));
    }

    printf("\n[L5] Hamming(7,4) FEC demo:\n");
    for (int nib = 0; nib < 4; nib++) {
        uint8_t cw[8], dec;
        int len = lora_hamming_encode((uint8_t)nib, LORA_CR_4_7, cw);
        lora_hamming_decode(cw, LORA_CR_4_7, &dec);
        printf("  nibble=%d -> codeword=", nib);
        for (int i = 0; i < len; i++) printf("%d", cw[i]);
        printf(" -> decoded=%d\n", dec);
    }

    printf("\n[L5] CRC-16 demo:\n");
    const char *test_str = "LoRa";
    uint16_t crc = lora_crc16((const uint8_t *)test_str, 4);
    printf("  CRC-16 of '%s': 0x%04X\n", test_str, crc);

    printf("\n[L6] Packet airtime table (BW125, CR4/5, 20 bytes payload):\n");
    printf("  %-6s %-12s %-12s\n", "SF", "BitRate(bps)", "Airtime(ms)");
    for (int sf = 7; sf <= 12; sf++) {
        params.sf = (lora_spreading_factor_t)sf;
        params.num_chips = (uint32_t)1 << (uint32_t)sf;
        params.symbol_period = (double)params.num_chips / (double)params.bw;
        params.bit_rate = lora_bit_rate(params.sf, params.bw, params.cr);
        params.payload_len = 20;
        double airtime = lora_packet_airtime(&params) * 1000.0;
        printf("  SF%-4d %-12.1f %-12.1f\n", sf, params.bit_rate, airtime);
    }

    printf("\n[L5] Demodulation test: encode symbol 73, decode via FFT:\n");
    params.sf = LORA_SF7;
    params.num_chips = 128;
    params.symbol_period = 128.0 / 125000.0;
    double complex rx[128];
    for (uint32_t n = 0; n < 128; n++)
        rx[n] = lora_chirp_sample(&params, 73, n, 125000.0);
    int sym = lora_demodulate_symbol_fft(&params, rx, 128);
    printf("  Transmitted: 73, Demodulated: %d %s\n", sym,
           (sym == 73) ? "CORRECT" : "ERROR");

    printf("\nDemo complete.\n");
    return 0;
}
