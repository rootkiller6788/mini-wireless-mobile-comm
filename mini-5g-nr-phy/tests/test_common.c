/**
 * test_common.c — Tests for 5G NR PHY common types and numerology
 */
#include "nr_phy_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

static int tests_run = 0;

#define TEST(expr) do { \
    if (!(expr)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } \
    tests_run++; \
} while(0)

#define TEST_FEQ(a, b, eps) do { \
    if (fabs((a) - (b)) > (eps)) { \
        fprintf(stderr, "FAIL: %s:%d: |%g - %g| > %g\n", __FILE__, __LINE__, (double)(a), (double)(b), (eps)); \
        exit(1); \
    } \
    tests_run++; \
} while(0)

int main(void)
{
    nr_numerology_t num;

    /* L2: Numerology initialization */
    TEST(nr_numerology_init(&num, 0, 0) == 0);
    TEST(num.mu == 0);
    TEST_FEQ(num.scs_khz, 15.0, 0.01);
    TEST(num.slots_per_subframe == 1);
    TEST(num.slots_per_frame == 10);
    TEST(num.symbols_per_slot == 14);

    TEST(nr_numerology_init(&num, 1, 0) == 0);
    TEST(num.mu == 1);
    TEST_FEQ(num.scs_khz, 30.0, 0.01);
    TEST(num.slots_per_subframe == 2);
    TEST(num.slots_per_frame == 20);

    TEST(nr_numerology_init(&num, 2, 0) == 0);
    TEST_FEQ(num.scs_khz, 60.0, 0.01);
    TEST(num.slots_per_subframe == 4);

    /* Invalid mu */
    TEST(nr_numerology_init(&num, -1, 0) == -1);
    TEST(nr_numerology_init(&num, 5, 0) == -1);

    /* Extended CP only for mu=2 */
    TEST(nr_numerology_init(&num, 2, 1) == 0);
    TEST(num.symbols_per_slot == 12);
    TEST(nr_numerology_init(&num, 0, 1) == -1);

    /* L2: Carrier configuration */
    nr_carrier_config_t cfg;
    TEST(nr_carrier_config_init(&cfg, 3.5e9, 100e6, 1, 0) == 0);
    TEST_FEQ(cfg.center_freq_hz, 3.5e9, 1.0);
    TEST(cfg.numerology_mu == 1);
    TEST(cfg.num_bwp == 1);
    TEST(cfg.active_bwp_id == 0);

    /* L2: BWP configuration */
    TEST(nr_bwp_configure(&cfg, 1, 0, 66, 1) == 0);
    TEST(cfg.num_bwp == 2);
    TEST(cfg.bwps[1].bwp_id == 1);
    TEST(cfg.bwps[1].num_prb == 66);

    /* L2: PRB count for bandwidth */
    TEST(nr_prb_count_for_bw(10.0, 0) == 52);
    TEST(nr_prb_count_for_bw(20.0, 0) == 106);
    TEST(nr_prb_count_for_bw(100.0, 1) == 273);

    /* L2: TBS calculation */
    int tbs = nr_tbs_calculate(1000, 500, 4, 2);
    TEST(tbs > 0);
    tbs = nr_tbs_calculate(0, 500, 4, 2);
    TEST(tbs == 0);

    /* L2: Modulation order */
    TEST(nr_modulation_order(NR_MOD_QPSK) == 2);
    TEST(nr_modulation_order(NR_MOD_QAM16) == 4);
    TEST(nr_modulation_order(NR_MOD_QAM64) == 6);
    TEST(nr_modulation_order(NR_MOD_QAM256) == 8);

    /* L2: SCS */
    TEST_FEQ(nr_scs_khz(0), 15.0, 0.01);
    TEST_FEQ(nr_scs_khz(1), 30.0, 0.01);
    TEST_FEQ(nr_scs_khz(2), 60.0, 0.01);
    TEST_FEQ(nr_scs_khz(3), 120.0, 0.01);
    TEST_FEQ(nr_scs_khz(4), 240.0, 0.01);

    /* L2: Symbols per slot */
    TEST(nr_symbols_per_slot(0) == 14);
    TEST(nr_symbols_per_slot(1) == 12);

    /* L2: Slots per frame */
    TEST(nr_slots_per_frame(0) == 10);
    TEST(nr_slots_per_frame(1) == 20);
    TEST(nr_slots_per_frame(2) == 40);

    /* L2: FFT size */
    int fft = nr_fft_size_min(0, 52); /* 10 MHz, mu=0, 52 RBs */
    TEST(fft >= 512); /* fft_size >= 52*12=624 → next power of 2 = 1024 */

    /* L2: CP length */
    int cp0 = nr_cp_length(0, 1024, 0);
    TEST(cp0 > 0);
    int cp1 = nr_cp_length(1, 1024, 0);
    TEST(cp1 > 0);
    TEST(cp0 > cp1); /* Symbol 0 has longer CP */

    /* L2: Frequency range */
    TEST(nr_is_fr1(3.5e9) == 1);
    TEST(nr_is_fr1(28.0e9) == 0);
    TEST(nr_is_fr2(28.0e9) == 1);
    TEST(nr_is_fr2(3.5e9) == 0);

    /* L2: MCS table lookup */
    nr_mcs_entry_t entry;
    TEST(nr_mcs_lookup(0, 1, &entry) == 0);
    TEST(entry.mcs_index == 0);
    TEST(entry.modulation == NR_MOD_QPSK);
    TEST(nr_mcs_from_efficiency(2.5, 1) >= 0);
    TEST(nr_mcs_from_efficiency(999.0, 1) == -1);

    /* L3: Complex arithmetic */
    nr_complex_t a = nr_complex_make(3.0, 4.0);
    TEST_FEQ(nr_complex_abs(a), 5.0, 0.001);
    TEST_FEQ(nr_complex_abs_sq(a), 25.0, 0.001);

    nr_complex_t b = nr_complex_make(1.0, 2.0);
    nr_complex_t c = nr_complex_mul(a, b);
    /* (3+j4)(1+j2) = 3-8 + j(6+4) = -5 + j10 */
    TEST_FEQ(c.re, -5.0, 0.001);
    TEST_FEQ(c.im, 10.0, 0.001);

    nr_complex_t d = nr_complex_add(a, b);
    TEST_FEQ(d.re, 4.0, 0.001);
    TEST_FEQ(d.im, 6.0, 0.001);

    nr_complex_t e = nr_complex_sub(a, b);
    TEST_FEQ(e.re, 2.0, 0.001);
    TEST_FEQ(e.im, 2.0, 0.001);

    nr_complex_t f = nr_complex_conj(a);
    TEST_FEQ(f.re, 3.0, 0.001);
    TEST_FEQ(f.im, -4.0, 0.001);

    nr_complex_t g = nr_complex_expj(M_PI / 2.0);
    TEST_FEQ(g.re, 0.0, 0.001);
    TEST_FEQ(g.im, 1.0, 0.001);

    printf("PASS: test_common (%d assertions)\n", tests_run);
    return 0;
}
