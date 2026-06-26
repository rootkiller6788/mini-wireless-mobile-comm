#include "nr_phy_ssb.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(void)
{
    printf("=== 5G NR Cell Search (PSS/SSS) Example ===\n\n");
    srand((unsigned)time(NULL));

    int nid2_true = 1;
    int nid1_true = 173;
    int nid_true = 3 * nid1_true + nid2_true;

    printf("True Cell ID: NID=3*%d+%d=%d\n", nid1_true, nid2_true, nid_true);

    /* Generate PSS sequence */
    double pss_seq[NR_PSS_LEN];
    nr_pss_sequence(nid2_true, pss_seq);

    /* Generate SSS sequence */
    double sss_seq[NR_SSS_LEN];
    nr_sss_sequence(nid1_true, nid2_true, sss_seq);

    /* Create synthetic baseband signal */
    int fft_size = 1024;
    int cp_len = 80;
    int total_sig_len = fft_size * 5 + cp_len * 5 + 500;
    nr_complex_t *rx_signal = calloc(total_sig_len, sizeof(nr_complex_t));

    /* Place PSS at sample offset 200 (simulating timing offset) */
    int pss_start = 200;
    double noise_std = 0.05;
    for (int i = 0; i < total_sig_len; i++) {
        rx_signal[i].re = ((double)rand()/RAND_MAX - 0.5) * noise_std;
        rx_signal[i].im = ((double)rand()/RAND_MAX - 0.5) * noise_std;
    }
    for (int i = 0; i < NR_PSS_LEN && (pss_start + i) < total_sig_len; i++) {
        rx_signal[pss_start + i].re += pss_seq[i] * 3.0;
    }

    /* PSS detection */
    nr_pss_result_t pss_res;
    nr_pss_detect(rx_signal, total_sig_len, fft_size, 15000.0, &pss_res);
    printf("PSS: detected NID2=%d (true=%d), timing=%d (true=%d), %s\n",
           pss_res.nid2, nid2_true, pss_res.best_timing, pss_start,
           pss_res.nid2 == nid2_true ? "OK" : "MISS");

    /* Now detect SSS in frequency domain */
    int sss_start = pss_res.best_timing + fft_size * 2;
    nr_complex_t rx_sss[NR_SSS_LEN];
    for (int i = 0; i < NR_SSS_LEN && (sss_start + i) < total_sig_len; i++) {
        rx_sss[i] = nr_complex_make(sss_seq[i] * 5.0, 0.0);
        rx_sss[i].re += ((double)rand()/RAND_MAX - 0.5) * noise_std;
        rx_sss[i].im += ((double)rand()/RAND_MAX - 0.5) * noise_std;
    }

    nr_sss_result_t sss_res;
    nr_sss_detect(rx_sss, pss_res.nid2, &sss_res);
    printf("SSS: detected NID1=%d (true=%d), cell_id=%d (true=%d), %s\n",
           sss_res.nid1, nid1_true, sss_res.cell_id.nid, nid_true,
           sss_res.nid1 == nid1_true ? "OK" : "MISS");

    /* RSRP measurement */
    double rsrp = nr_ssb_rsrp(rx_sss);
    printf("SS-RSRP: %.1f dBm\n", rsrp);

    /* Full cell search */
    nr_cell_search_result_t full_result;
    int ret = nr_cell_search(rx_signal, total_sig_len, 15, fft_size, 4,
                              &full_result);
    printf("Full cell search: %s, cell_id=%d\n",
           ret == 0 ? "DETECTED" : "NOT FOUND",
           full_result.cell_id.nid);

    free(rx_signal);
    printf("\n=== Cell Search Complete ===\n");
    return 0;
}
