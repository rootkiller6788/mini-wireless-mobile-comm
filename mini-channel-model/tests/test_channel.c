/**
 * @file test_channel.c
 * @brief Assert-based tests for mini-channel-model
 *
 * Tests cover: L1-L6 knowledge points.
 * Each test validates a specific theorem or algorithm.
 * All tests use standard assert().
 */

#include "../include/channel_defs.h"
#include "../include/pathloss.h"
#include "../include/fading.h"
#include "../include/multipath.h"
#include "../include/mimo_channel.h"
#include "../include/doppler.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EPSILON 1e-6
#define EPSILON_DB 0.1

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_NEAR(a, b, eps) do { \
    if (fabs((a) - (b)) > (eps)) { \
        printf("  FAIL: %g != %g (tol=%g)\n", (double)(a), (double)(b), (eps)); \
        tests_failed++; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { printf("  FAIL: expected true\n"); tests_failed++; } \
    else { tests_passed++; } \
} while(0)

#define TEST(name) printf("  %s ... ", name)

/*============================================================================
 * L1: Core Definitions Tests
 *============================================================================*/

static void test_l1_wavelength(void)
{
    printf("L1: Wavelength\n");
    TEST("wavelength_2_4GHz");
    double lam = channel_wavelength(2.4e9);
    ASSERT_NEAR(lam, 0.1249, 0.001);

    TEST("wavelength_5GHz");
    lam = channel_wavelength(5.0e9);
    ASSERT_NEAR(lam, 0.05996, 0.001);

    TEST("wavelength_invalid");
    lam = channel_wavelength(0.0);
    ASSERT_NEAR(lam, -1.0, EPSILON);
}

static void test_l1_db_conversion(void)
{
    printf("L1: dB/Linear Conversion\n");
    TEST("db_to_linear_0dB");
    ASSERT_NEAR(channel_db_to_linear(0.0), 1.0, EPSILON);

    TEST("db_to_linear_10dB");
    ASSERT_NEAR(channel_db_to_linear(10.0), 10.0, EPSILON);

    TEST("db_to_linear_n10dB");
    ASSERT_NEAR(channel_db_to_linear(-10.0), 0.1, EPSILON);

    TEST("linear_to_db_1");
    ASSERT_NEAR(channel_linear_to_db(1.0), 0.0, EPSILON);

    TEST("linear_to_db_100");
    ASSERT_NEAR(channel_linear_to_db(100.0), 20.0, EPSILON);

    TEST("linear_to_db_invalid");
    ASSERT_NEAR(channel_linear_to_db(0.0), -200.0, EPSILON);
}

static void test_l1_noise_power(void)
{
    printf("L1: Thermal Noise Power\n");
    TEST("noise_at_290K_1MHz_0NF");
    double n = channel_noise_power_dbm(1e6, 290.0, 0.0);
    ASSERT_NEAR(n, -114.0, 1.0);

    TEST("noise_at_290K_1Hz_0NF");
    n = channel_noise_power_dbm(1.0, 290.0, 0.0);
    ASSERT_NEAR(n, -174.0, 1.0);

    TEST("noise_with_3dB_NF");
    n = channel_noise_power_dbm(1e6, 290.0, 3.0);
    ASSERT_NEAR(n, -111.0, 1.0);
}

/*============================================================================
 * L4: Fundamental Laws Tests
 *============================================================================*/

static void test_l4_friis_pathloss(void)
{
    printf("L4: Friis Free-Space Path Loss\n");
    TEST("friis_1m_2_4GHz");
    double pl = pathloss_friis_free_space(1.0, 2.4e9);
    ASSERT_NEAR(pl, 40.0, 1.0);

    TEST("friis_100m_2_4GHz");
    pl = pathloss_friis_free_space(100.0, 2.4e9);
    ASSERT_NEAR(pl, 80.0, 1.0);

    TEST("friis_invalid");
    pl = pathloss_friis_free_space(0.0, 2.4e9);
    ASSERT_NEAR(pl, -1.0, EPSILON);

    TEST("friis_distance_monotonic");
    double pl1 = pathloss_friis_free_space(10.0, 1e9);
    double pl2 = pathloss_friis_free_space(100.0, 1e9);
    ASSERT_TRUE(pl2 > pl1);
}

static void test_l4_okumura_hata(void)
{
    printf("L4: Okumura-Hata Model\n");
    TEST("okumura_hata_urban_1km");
    double pl = pathloss_okumura_hata_urban(1.0, 900.0, 30.0, 1.5, 0);
    ASSERT_TRUE(pl > 100.0 && pl < 150.0);

    TEST("okumura_hata_suburban");
    double pl_sub = pathloss_okumura_hata_suburban(1.0, 900.0, 30.0, 1.5);
    ASSERT_TRUE(pl_sub < pl);

    TEST("okumura_hata_rural");
    double pl_rural = pathloss_okumura_hata_rural(1.0, 900.0, 30.0, 1.5);
    ASSERT_TRUE(pl_rural < pl_sub);
}

static void test_l4_shannon_capacity(void)
{
    printf("L4: Shannon-Hartley Theorem\n");
    TEST("shannon_0dB");
    double c = 1e6 * log2(1.0 + 1.0);
    ASSERT_NEAR(c, 1e6, 1000.0);

    TEST("shannon_20dB");
    double snr_linear = 100.0;
    c = 1e6 * log2(1.0 + snr_linear);
    ASSERT_NEAR(c, 6.66e6, 1e5);
}

/*============================================================================
 * L4: Doppler Tests
 *============================================================================*/

static void test_l4_doppler_shift(void)
{
    printf("L4: Doppler Shift\n");
    TEST("doppler_30ms_2_4GHz");
    double fd = channel_doppler_shift(30.0, 2.4e9);
    ASSERT_NEAR(fd, 240.0, 1.0);

    TEST("doppler_3ms_900MHz");
    fd = channel_doppler_shift(3.0, 900e6);
    ASSERT_NEAR(fd, 9.0, 0.1);

    TEST("coherence_time_240Hz");
    double tc = channel_coherence_time(240.0);
    ASSERT_TRUE(tc > 0.001 && tc < 0.01);

    TEST("coherence_time_static");
    tc = channel_coherence_time(0.0);
    ASSERT_TRUE(tc > 1000.0);
}

static void test_l4_lcr_afd(void)
{
    printf("L4: LCR and AFD\n");
    double fd = 100.0;

    TEST("lcr_rayleigh_0dB");
    double lcr = doppler_lcr_rayleigh(fd, 1.0);
    ASSERT_TRUE(lcr > 0.0 && lcr < 500.0);

    TEST("afd_rayleigh_0dB");
    double afd = doppler_afd_rayleigh(fd, 1.0);
    ASSERT_TRUE(afd > 0.0);

    TEST("lcr_low_threshold");
    double lcr_low = doppler_lcr_rayleigh(fd, 0.1);
    double lcr_high = doppler_lcr_rayleigh(fd, 1.0);
    ASSERT_TRUE(lcr_low < lcr_high);

    TEST("afd_increases_with_threshold");
    double afd_low = doppler_afd_rayleigh(fd, 0.1);
    double afd_high = doppler_afd_rayleigh(fd, 1.0);
    ASSERT_TRUE(afd_high > afd_low);
}

/*============================================================================
 * L3: Fading Distribution Tests
 *============================================================================*/

static void test_l3_rayleigh_pdf(void)
{
    printf("L3: Rayleigh PDF\n");
    double sigma = 1.0;

    TEST("rayleigh_pdf_peak");
    double pdf_peak = fading_rayleigh_pdf(sigma, sigma);
    double pdf_half = fading_rayleigh_pdf(0.5, sigma);
    ASSERT_TRUE(pdf_peak > pdf_half);

    TEST("rayleigh_cdf_median");
    double median = sigma * sqrt(2.0 * log(2.0));
    double cdf_med = fading_rayleigh_cdf(median, sigma);
    ASSERT_NEAR(cdf_med, 0.5, 0.05);

    TEST("rayleigh_generate_positive");
    {
        int all_positive = 1;
        for (int i = 0; i < 100; i++) {
            double r = fading_generate_rayleigh(1.0);
            if (r < 0.0) all_positive = 0;
        }
        ASSERT_TRUE(all_positive);
    }
}

static void test_l3_rician_pdf(void)
{
    printf("L3: Rician PDF\n");
    TEST("rician_k0_equals_rayleigh");
    double pdf_rice = fading_rician_pdf(1.0, 0.0, 1.0);
    double pdf_rayl = fading_rayleigh_pdf(1.0, 1.0);
    ASSERT_NEAR(pdf_rice, pdf_rayl, EPSILON);

    TEST("rician_generate_from_k");
    double r = fading_generate_rician_from_k(1.0, 10.0);
    ASSERT_TRUE(r >= 0.0);
}

static void test_l3_nakagami_pdf(void)
{
    printf("L3: Nakagami-m PDF\n");
    TEST("nakagami_m1");
    double pdf_nak = fading_nakagami_pdf(1.0, 1.0, 1.0);
    ASSERT_TRUE(pdf_nak >= 0.0);

    TEST("nakagami_generate");
    double r = fading_generate_nakagami(2.0, 1.0);
    ASSERT_TRUE(r >= 0.0);
}

static void test_l3_lognormal(void)
{
    printf("L3: Log-normal Shadow Fading\n");
    TEST("lognormal_generate");
    double s = fading_generate_lognormal(8.0);
    ASSERT_TRUE(fabs(s) < 50.0);
}

/*============================================================================
 * L6: Multipath Tests
 *============================================================================*/

static void test_l6_pdp_generation(void)
{
    printf("L6: Power Delay Profile Generation\n");
    power_delay_profile_t *pdp = multipath_pdp_alloc(10);

    TEST("epa_generate");
    int rc = multipath_generate_epa(pdp);
    ASSERT_TRUE(rc == 0);
    ASSERT_TRUE(pdp->num_taps == 7);
    ASSERT_TRUE(pdp->rms_delay_ns > 0);

    TEST("eva_generate");
    rc = multipath_generate_eva(pdp);
    ASSERT_TRUE(rc == 0);
    ASSERT_TRUE(pdp->rms_delay_ns > 100.0);

    TEST("etu_generate");
    rc = multipath_generate_etu(pdp);
    ASSERT_TRUE(rc == 0);
    ASSERT_TRUE(pdp->rms_delay_ns > 500.0);

    multipath_pdp_free(pdp);
}

static void test_l6_tdl_channel(void)
{
    printf("L6: Tapped Delay Line Channel\n");
    power_delay_profile_t *pdp = multipath_pdp_alloc(10);
    multipath_generate_epa(pdp);

    TEST("tdl_init");
    multipath_tdl_t *tdl = multipath_tdl_init(pdp, 30.72e6, 2.0e9, 3.0);
    ASSERT_TRUE(tdl != NULL);

    TEST("tdl_process");
    double complex input = 1.0 + 0.0 * I;
    for (int i = 0; i < 100; i++) {
        double complex output = multipath_tdl_process(tdl, input);
        ASSERT_TRUE(cabs(output) >= 0.0);
    }

    TEST("tdl_process_block");
    double complex in_block[64];
    double complex out_block[64];
    for (int i = 0; i < 64; i++) in_block[i] = 1.0 + 0.0 * I;
    multipath_tdl_process_block(tdl, in_block, out_block, 64);
    ASSERT_TRUE(cabs(out_block[0]) >= 0.0);

    multipath_tdl_free(tdl);
    multipath_pdp_free(pdp);
}

static void test_l6_rake_combining(void)
{
    printf("L6: Rake Receiver Combining\n");
    double complex taps[] = {1.0 + 0.0*I, 0.7 + 0.1*I, 0.3 - 0.2*I};
    size_t n = 3;
    double complex weights_mrc[3], weights_egc[3];

    TEST("mrc_weights");
    multipath_rake_mrc_weights(taps, n, weights_mrc);
    ASSERT_NEAR(creal(weights_mrc[0]), 1.0, EPSILON);
    ASSERT_NEAR(cimag(weights_mrc[0]), 0.0, EPSILON);

    TEST("egc_weights_unit_magnitude");
    multipath_rake_egc_weights(taps, n, weights_egc);
    ASSERT_NEAR(cabs(weights_egc[0]), 1.0, EPSILON);

    TEST("mrc_snr_gain");
    double snr_combined = multipath_rake_snr_mrc(taps, n, 10.0);
    ASSERT_TRUE(snr_combined > 10.0);

    double complex taps_equal[] = {1.0 + 0.0*I, 1.0 + 0.0*I};
    snr_combined = multipath_rake_snr_mrc(taps_equal, 2, 0.0);
    ASSERT_TRUE(snr_combined > 2.5 && snr_combined < 3.5);
}

/*============================================================================
 * L6: MIMO Tests
 *============================================================================*/

static void test_l6_mimo_channel(void)
{
    printf("L6: MIMO Channel Matrix\n");

    TEST("mimo_alloc_2x2");
    mimo_channel_matrix_t *mimo = mimo_channel_alloc(2, 2);
    ASSERT_TRUE(mimo != NULL);
    ASSERT_TRUE(mimo->num_rx == 2);
    ASSERT_TRUE(mimo->num_tx == 2);

    TEST("mimo_iid_generate");
    int rc = mimo_generate_iid_rayleigh(mimo);
    ASSERT_TRUE(rc == 0);

    TEST("mimo_frobenius_norm");
    double fnorm = mimo_frobenius_norm(mimo);
    ASSERT_TRUE(fnorm > 0.0);
    ASSERT_TRUE(fnorm < 10.0);

    TEST("mimo_get_set");
    mimo_set_element(mimo, 0, 1, 3.0 + 4.0*I);
    double complex el = mimo_get_element(mimo, 0, 1);
    ASSERT_NEAR(creal(el), 3.0, EPSILON);
    ASSERT_NEAR(cimag(el), 4.0, EPSILON);

    mimo_channel_free(mimo);
}

static void test_l6_mimo_capacity(void)
{
    printf("L6: MIMO Capacity (Telatar 1999)\n");

    mimo_channel_matrix_t *mimo = mimo_channel_alloc(2, 2);
    mimo_generate_iid_rayleigh(mimo);

    TEST("capacity_equal_power");
    channel_capacity_t cap = {0};
    int rc = mimo_capacity_equal_power(mimo, 20.0, 1e6, &cap);
    ASSERT_TRUE(rc == 0);
    ASSERT_TRUE(cap.capacity_bps > 0.0);

    double se_siso = log2(1.0 + 100.0);
    ASSERT_TRUE(cap.spectral_efficiency > se_siso * 0.8);
    ASSERT_TRUE(cap.spectral_efficiency < se_siso * 2.5);

    mimo_capacity_free(&cap);

    TEST("capacity_waterfilling");
    rc = mimo_capacity_waterfilling(mimo, 20.0, 1e6, &cap);
    ASSERT_TRUE(rc == 0);

    mimo_capacity_free(&cap);
    mimo_channel_free(mimo);
}

static void test_l6_exponential_correlation(void)
{
    printf("L6: Exponential Correlation Model\n");
    double corr[16];
    mimo_exponential_correlation(corr, 4, 0.5);

    TEST("exp_corr_diagonal");
    ASSERT_NEAR(corr[0], 1.0, EPSILON);

    TEST("exp_corr_adjacent");
    ASSERT_NEAR(corr[1], 0.5, EPSILON);

    TEST("exp_corr_far");
    ASSERT_NEAR(corr[3], 0.125, EPSILON);
}

/*============================================================================
 * L5: Algorithm Tests
 *============================================================================*/

static void test_l5_cholesky(void)
{
    printf("L5: Cholesky Decomposition\n");
    double A[] = {4.0, 2.0, 2.0, 3.0};
    double L[4] = {0};

    TEST("cholesky_decomp");
    int rc = fading_cholesky_decomp(A, 2, L);
    ASSERT_TRUE(rc == 0);
    ASSERT_NEAR(L[0], 2.0, EPSILON);       /* L[0,0] */
    ASSERT_NEAR(L[2], 1.0, EPSILON);       /* L[1,0] */
    ASSERT_NEAR(L[3], sqrt(2.0), EPSILON); /* L[1,1] */

    TEST("cholesky_not_pd");
    double A_bad[] = {1.0, 2.0, 2.0, 1.0};
    rc = fading_cholesky_decomp(A_bad, 2, L);
    ASSERT_TRUE(rc == -1);
}

static void test_l5_fading_generation(void)
{
    printf("L5: Fading Generation\n");

    TEST("rayleigh_statistics");
    double sum = 0.0, sum_sq = 0.0;
    int N = 10000;
    for (int i = 0; i < N; i++) {
        double r = fading_generate_rayleigh(1.0);
        sum += r;
        sum_sq += r * r;
    }
    double mean = sum / N;
    double mean_sq = sum_sq / N;
    ASSERT_NEAR(mean, 1.253, 0.05);
    ASSERT_NEAR(mean_sq, 2.0, 0.1);

    TEST("complex_gaussian");
    double complex cg = fading_rand_complex_gaussian(1.0);
    ASSERT_TRUE(cabs(cg) >= 0.0);
}

static void test_l5_clarke_autocorrelation(void)
{
    printf("L5: Clarke Autocorrelation\n");
    TEST("acf_tau0");
    double acf = fading_clarke_autocorrelation(0.0, 100.0, 1.0);
    ASSERT_NEAR(acf, 1.0, 0.01);

    TEST("acf_first_zero");
    double tau_zero = 2.4048 / (2.0 * M_PI * 100.0);
    acf = fading_clarke_autocorrelation(tau_zero, 100.0, 1.0);
    ASSERT_NEAR(acf, 0.0, 0.05);
}

/*============================================================================
 * L7: Application Tests
 *============================================================================*/

static void test_l7_5g_pathloss(void)
{
    printf("L7: 5G NR Path Loss Models\n");
    TEST("3gpp_umi_los");
    double pl = pathloss_3gpp_umi(100.0, 3.5, 10.0, 1.5, 1);
    ASSERT_TRUE(pl > 70.0 && pl < 120.0);

    TEST("3gpp_uma_los");
    pl = pathloss_3gpp_uma(500.0, 3.5, 25.0, 1.5, 1);
    ASSERT_TRUE(pl > 80.0);
}

static void test_l7_massive_mimo(void)
{
    printf("L7: Massive MIMO\n");

    mimo_channel_matrix_t *mimo = mimo_channel_alloc(4, 64);
    mimo_generate_massive_iid(mimo);

    TEST("massive_mimo_hardening");
    double metric = mimo_channel_hardening_metric(mimo);
    ASSERT_TRUE(metric < 0.5);

    mimo_channel_free(mimo);
}

/*============================================================================
 * L8: Advanced Tests
 *============================================================================*/

static void test_l8_3gpp_3d_channel(void)
{
    printf("L8: 3GPP 3D MIMO Channel\n");
    mimo_channel_matrix_t *mimo = mimo_channel_alloc(4, 8);
    int rc = mimo_generate_3gpp_3d(mimo, 6, 15.0, 10.0, 3.5e9, 0.5, 0.5);
    ASSERT_TRUE(rc == 0);

    double fnorm = mimo_frobenius_norm(mimo);
    ASSERT_NEAR(fnorm, 1.0, 0.1);

    mimo_channel_free(mimo);
}

static void test_l8_diversity_order(void)
{
    printf("L8: Diversity Order\n");
    mimo_channel_matrix_t *mimo = mimo_channel_alloc(2, 2);
    mimo_generate_iid_rayleigh(mimo);

    double div_order = mimo_diversity_order(mimo);
    ASSERT_TRUE(div_order >= 1.0);

    mimo_channel_free(mimo);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("=== mini-channel-model Test Suite ===\n\n");

    fading_rand_seed(42);

    test_l1_wavelength();
    test_l1_db_conversion();
    test_l1_noise_power();
    test_l3_rayleigh_pdf();
    test_l3_rician_pdf();
    test_l3_nakagami_pdf();
    test_l3_lognormal();
    test_l4_friis_pathloss();
    test_l4_okumura_hata();
    test_l4_shannon_capacity();
    test_l4_doppler_shift();
    test_l4_lcr_afd();
    test_l5_cholesky();
    test_l5_fading_generation();
    test_l5_clarke_autocorrelation();
    test_l6_pdp_generation();
    test_l6_tdl_channel();
    test_l6_rake_combining();
    test_l6_mimo_channel();
    test_l6_mimo_capacity();
    test_l6_exponential_correlation();
    test_l7_5g_pathloss();
    test_l7_massive_mimo();
    test_l8_3gpp_3d_channel();
    test_l8_diversity_order();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
