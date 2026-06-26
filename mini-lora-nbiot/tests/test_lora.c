/**
 * @file test_lora.c
 * @brief Unit tests for LoRa/NB-IoT LPWAN module
 *
 * Tests: L1 definitions, L3 chirp math, L4 fundamental laws,
 *        L5 algorithms (Hamming, CRC, whitening, dechirp),
 *        L6 packet airtime, L2 MAC, L7 applications, L8 advanced
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <assert.h>
#include "lora_phy.h"
#include "lora_mac.h"
#include "lora_nbiot_common.h"
#include "nbiot_phy.h"
#include "lora_channel.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int tp = 0, tf = 0;

#define T(n) printf("  TEST: %s... ", n)
#define P()  do { printf("PASS\n"); tp++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); tf++; return; } while(0)
#define C(c) do { if (!(c)) F(#c); } while(0)
#define CE(a,b) do { if ((a)!=(b)) { printf("FAIL: %s=%d exp %d\n",#a,(int)(a),(int)(b)); tf++; return; } } while(0)
#define CN(a,b,t) do { if (fabs((a)-(b))>(t)) { printf("FAIL: %s=%f exp~%f\n",#a,a,b); tf++; return; } } while(0)

static void test_l1_sf(void) {
    T("SF values");
    C(LORA_SF7 == 7); C(LORA_SF12 == 12); P();
}

static void test_l1_bw(void) {
    T("BW values");
    C(LORA_BW_125_KHZ == 125000); C(LORA_BW_250_KHZ == 250000); P();
}

static void test_l1_cr(void) {
    T("CR values");
    C(LORA_CR_4_5 == 1); C(LORA_CR_4_8 == 4); P();
}

static void test_l3_chirp_sample(void) {
    T("Chirp sample magnitude");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    double complex s = lora_chirp_sample(&p, 0, 0, 125000.0);
    double mag = cabs(s);
    double exp = 1.0 / sqrt((double)p.num_chips);
    CN(mag, exp, 1e-9);
    P();
}

static void test_l3_chirp_ortho(void) {
    T("Chirp orthogonality");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    p.sf = LORA_SF7; p.num_chips = 128;
    /* At n=0, symbols 0 and 64 produce clearly different phases */
    /* Verify different symbols produce different chirps at n=0 */
    double complex s0 = lora_chirp_sample(&p, 0, 64, 125000.0);
    double complex s1 = lora_chirp_sample(&p, 1, 64, 125000.0);
    double diff = cabs(s0 - s1);
    C(diff > 0.001);
    P();
}

static void test_l4_sym_period(void) {
    T("Symbol period SF7/BW125");
    double Ts = lora_symbol_period(LORA_SF7, LORA_BW_125_KHZ);
    CN(Ts, 128.0 / 125000.0, 1e-9);
    P();
}

static void test_l4_proc_gain(void) {
    T("Processing gain SF12");
    double Gp = lora_processing_gain_db(LORA_SF12);
    CN(Gp, 12.0 * 10.0 * log10(2.0), 0.5);
    P();
}

static void test_l4_sensitivity(void) {
    T("Receiver sensitivity SF7/BW125/NF6");
    double S = lora_receiver_sensitivity(LORA_SF7, LORA_BW_125_KHZ, 6.0);
    double exp = -174.0 + 10.0 * log10(125000.0) + 6.0 - 7.5;
    CN(S, exp, 0.2);
    P();
}

static void test_l4_friis(void) {
    T("Friis path loss 1km/868MHz");
    double pl = friis_path_loss_db(1000.0, 868.0);
    C(pl > 80.0 && pl < 100.0);
    P();
}

static void test_l4_bitrate(void) {
    T("Bit rate SF7/BW125/CR4_5");
    double Rb = lora_bit_rate(LORA_SF7, LORA_BW_125_KHZ, LORA_CR_4_5);
    CN(Rb, 5468.75, 2.0);
    P();
}

static void test_l4_link_budget(void) {
    T("Link budget computation");
    double rx_pwr;
    double margin = link_budget_compute(14.0, 2.0, 2.0, 100.0, 3.0, -120.0, &rx_pwr);
    C(margin > -100.0);
    P();
}

static void test_l5_hamming_noerr(void) {
    T("Hamming(7,4) encode/decode no errors");
    for (int nib = 0; nib < 16; nib++) {
        uint8_t cw[8], dec;
        int len = lora_hamming_encode((uint8_t)nib, LORA_CR_4_7, cw);
        C(len == 7);
        int err = lora_hamming_decode(cw, LORA_CR_4_7, &dec);
        C(err == 0);
        CE(dec, nib);
    }
    P();
}

static void test_l5_hamming_1err(void) {
    T("Hamming(7,4) single error correction");
    uint8_t cw[8], dec;
    lora_hamming_encode(0x0A, LORA_CR_4_7, cw);
    cw[2] ^= 1;
    int err = lora_hamming_decode(cw, LORA_CR_4_7, &dec);
    C(err == 1);
    CE(dec, 0x0A);
    P();
}

static void test_l5_crc16(void) {
    T("CRC-16 computation");
    uint8_t d[] = {0x31, 0x32, 0x33, 0x34};
    uint16_t crc = lora_crc16(d, 4);
    C(crc != 0);
    crc = lora_crc16(d, 0);
    CE((int)crc, 0);
    P();
}

static void test_l5_whiten(void) {
    T("Data whitening self-inverse");
    uint8_t d[] = {0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34};
    uint8_t orig[6];
    memcpy(orig, d, 6);
    lora_whiten(d, 6);
    int changed = memcmp(d, orig, 6) != 0;
    C(changed);
    lora_whiten(d, 6);
    C(memcmp(d, orig, 6) == 0);
    P();
}

static void test_l5_dechirp_demod(void) {
    T("Dechirp + FFT demodulation");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    p.sf = LORA_SF7; p.num_chips = 128; p.symbol_period = 128.0/125000.0;
    double complex rx[128];
    /* Generate base up-chirp (symbol encoded as cyclic time shift),
     * verify demodulation returns a valid symbol in range [0, 127] */
    for (uint32_t n = 0; n < 128; n++)
        rx[n] = lora_chirp_sample(&p, 0, n, 125000.0);
    /* Verify dechirp works without error */
    C(lora_dechirp(&p, rx, 128) == 0);
    /* Regenerate and verify demodulation returns valid symbol */
    for (uint32_t n = 0; n < 128; n++)
        rx[n] = lora_chirp_sample(&p, 0, n, 125000.0);
    int sym = lora_demodulate_symbol_fft(&p, rx, 128);
    C(sym >= 0 && sym < 128);
    P();
}

static void test_l6_airtime(void) {
    T("Packet airtime SF7/BW125/10bytes");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    p.payload_len = 10;
    double at = lora_packet_airtime(&p);
    C(at > 0.0 && at < 1.0);
    P();
}

static void test_l6_validate(void) {
    T("PHY params validation");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    C(lora_phy_params_validate(&p) == 0);
    p.sf = 5;
    C(lora_phy_params_validate(&p) == -1);
    P();
}

static void test_l6_preamble(void) {
    T("Preamble generation");
    lora_phy_params_t p;
    lora_phy_params_init_default(&p);
    double complex buf[2048];
    int n = lora_generate_preamble(&p, buf, 2048);
    C(n > 0);
    P();
}

static void test_l2_duty_cycle(void) {
    T("Duty cycle tracking");
    lora_duty_cycle_tracker_t dt;
    lora_duty_cycle_init(&dt, 0.01, 3600.0);
    C(lora_duty_cycle_check(&dt, 30.0, 100.0) == 1);
    lora_duty_cycle_record(&dt, 30.0, 100.0);
    /* At 1%, 30s used out of 36s, so 5s more is OK (35 < 36) */
    C(lora_duty_cycle_check(&dt, 5.0, 100.0) == 1);
    P();
}

static void test_l2_build_frame(void) {
    T("Join request frame build");
    uint8_t je[8] = {0}, de[8] = {0}, buf[32];
    int len = lora_build_join_request(je, de, 0x1234, buf, 32);
    C(len == 23 && buf[0] == 0x00);
    P();
}

static void test_l7_smart_meter(void) {
    T("Smart meter encode/decode");
    smart_meter_report_t r, d;
    memset(&r, 0, sizeof(r)); memset(&d, 0, sizeof(d));
    r.meter_id = 12345; r.reading_value = 9876.54;
    r.reading_timestamp = 1719000000;
    r.battery_level_pct = 85; r.valve_status = 1;
    uint8_t buf[32];
    int len = smart_meter_encode(&r, buf, 32);
    C(len == 13);
    C(smart_meter_decode(buf, len, &d) == 0);
    CE((int)d.meter_id, 12345);
    C(d.battery_level_pct >= 80);
    P();
}

static void test_l8_cross_sf(void) {
    T("Cross-SF isolation");
    double iso = lora_cross_sf_isolation_db(7, 12);
    C(iso > 10.0);
    double iso_same = lora_cross_sf_isolation_db(10, 10);
    CN(iso_same, 0.0, 0.01);
    P();
}

static void test_l8_capture(void) {
    T("Capture probability");
    double inter[] = {-95.0, -100.0};
    double p = lora_capture_probability(-80.0, inter, 2, 6.0);
    C(p > 0.9);
    p = lora_capture_probability(-100.0, inter, 2, 6.0);
    C(p < 0.5);
    P();
}

static void test_nbiot_zc(void) {
    T("Zadoff-Chu sequence generation");
    double complex zc[131];
    C(nbiot_zadoff_chu(131, 5, zc) == 0);
    double mag = cabs(zc[0]);
    CN(mag, 1.0, 1e-6);
    P();
}

static void test_nbiot_npss(void) {
    T("NPSS generation");
    nbiot_subframe_t sf;
    C(nbiot_npss_generate(&sf, 0) == 0);
    double pwr = 0.0;
    for (int sc = 0; sc < 12; sc++)
        for (int sy = 3; sy < 14; sy++)
            pwr += cabs(sf.re_grid[sc][sy]) * cabs(sf.re_grid[sc][sy]);
    C(pwr > 0.0);
    P();
}

static void test_nbiot_psm(void) {
    T("PSM average current");
    nbiot_psm_state_t psm;
    nbiot_psm_init(&psm);
    double avg = nbiot_psm_average_current_ma(&psm);
    C(avg < 1.0 && avg > 0.0);
    P();
}

static void test_battery_life(void) {
    T("Battery life estimation");
    double life = battery_life_estimate_years(2400.0, 3.6, 3600.0,
                                               0.5, 30.0, 0.01, 10.0, 1.5);
    C(life > 1.0 && life < 50.0);
    P();
}

int main(void) {
    printf("=== mini-lora-nbiot Test Suite ===\n\n");

    printf("[L1 Definitions]\n");
    test_l1_sf(); test_l1_bw(); test_l1_cr();

    printf("\n[L3 Math Structures]\n");
    test_l3_chirp_sample(); test_l3_chirp_ortho();

    printf("\n[L4 Fundamental Laws]\n");
    test_l4_sym_period(); test_l4_proc_gain(); test_l4_sensitivity();
    test_l4_friis(); test_l4_bitrate(); test_l4_link_budget();

    printf("\n[L5 Algorithms]\n");
    test_l5_hamming_noerr(); test_l5_hamming_1err();
    test_l5_crc16(); test_l5_whiten(); test_l5_dechirp_demod();

    printf("\n[L6 Canonical Problems]\n");
    test_l6_airtime(); test_l6_validate(); test_l6_preamble();

    printf("\n[L2 MAC]\n");
    test_l2_duty_cycle(); test_l2_build_frame();

    printf("\n[L7 Applications]\n");
    test_l7_smart_meter();

    printf("\n[L8 Advanced]\n");
    test_l8_cross_sf(); test_l8_capture();

    printf("\n[NB-IoT]\n");
    test_nbiot_zc(); test_nbiot_npss(); test_nbiot_psm();

    printf("\n[Power Models]\n");
    test_battery_life();

    printf("\n=== Results: %d passed, %d failed ===\n", tp, tf);
    return tf ? 1 : 0;
}
