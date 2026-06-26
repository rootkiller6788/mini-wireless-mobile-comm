#include "nr_phy_ssb.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

static int tests_run = 0;
#define TEST(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);exit(1);} tests_run++; } while(0)

int main(void)
{
    /* PSS sequence generation */
    double pss0[NR_PSS_LEN];
    nr_pss_sequence(0, pss0);
    double pss1[NR_PSS_LEN];
    nr_pss_sequence(1, pss1);

    /* PSS sequences should be different for different nid2 */
    int diff = 0;
    for (int i = 0; i < NR_PSS_LEN; i++)
        if (fabs(pss0[i] - pss1[i]) > 0.1) diff++;
    TEST(diff > 10); /* At least some positions differ */

    /* PSS values should be +1 or -1 (BPSK) */
    for (int i = 0; i < NR_PSS_LEN; i++) {
        TEST(fabs(pss0[i]) > 0.9);
    }

    /* SSS sequence generation */
    double sss_seq[NR_SSS_LEN];
    nr_sss_sequence(0, 0, sss_seq);
    nr_sss_sequence(1, 0, pss0); /* reuse buffer */
    diff = 0;
    for (int i = 0; i < NR_SSS_LEN; i++)
        if (fabs(sss_seq[i] - pss0[i]) > 0.1) diff++;
    TEST(diff > 10);

    /* Cell ID */
    nr_sss_result_t sss_res;
    nr_complex_t rx_sss[NR_SSS_LEN];
    /* Build synthetic received SSS for nid1=42, nid2=1 */
    double ref_sss[NR_SSS_LEN];
    nr_sss_sequence(42, 1, ref_sss);
    for (int i = 0; i < NR_SSS_LEN; i++)
        rx_sss[i] = nr_complex_make(ref_sss[i] * 3.0, 0.0);
    nr_sss_detect(rx_sss, 1, &sss_res);
    TEST(sss_res.detected == 1);
    TEST(sss_res.nid1 == 42);
    TEST(sss_res.cell_id.nid == 3 * 42 + 1);

    /* PBCH DMRS */
    double dmrs_seq[NR_PBCH_DMRS_SC];
    nr_pbch_dmrs_sequence(42, 0, 0, dmrs_seq);

    /* RSRP measurement */
    nr_complex_t rx_sss_sig[NR_SSS_LEN];
    for (int i = 0; i < NR_SSS_LEN; i++)
        rx_sss_sig[i] = nr_complex_make(0.01, 0.0);
    double rsrp = nr_ssb_rsrp(rx_sss_sig);
    TEST(rsrp < -5.0); /* 0.01 amplitude → -10 dBm */

    /* SINR measurement */
    nr_complex_t rx_noise[NR_SSS_LEN];
    for (int i = 0; i < NR_SSS_LEN; i++)
        rx_noise[i] = nr_complex_make(0.001, 0.0);
    double sinr = nr_ssb_sinr(rx_sss_sig, rx_noise);
    TEST(sinr > 0.0);

    /* MIB decode */
    nr_mib_t mib;
    uint8_t pbch_payload[4] = {0x40, 0x00, 0x10, 0x00};
    TEST(nr_mib_decode(pbch_payload, &mib) == 0);
    TEST(mib.sfn_6msb >= 0);

    printf("PASS: test_ssb (%d assertions)\n", tests_run);
    return 0;
}
