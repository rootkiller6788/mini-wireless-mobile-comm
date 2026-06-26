#include "nr_phy_mimo.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

static int tests_run = 0;
#define TEST(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);exit(1);} tests_run++; } while(0)
#define TEST_FEQ(a,b,e) do { if(fabs((a)-(b))>(e)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);exit(1);} tests_run++; } while(0)

int main(void)
{
    /* MIMO configuration */
    nr_mimo_config_t cfg;
    nr_mimo_config_init(&cfg, 4, 2, 2, 2, 1, 4, 1, 1);
    TEST(cfg.num_tx_ports == 4);
    TEST(cfg.num_rx_ports == 2);
    TEST(cfg.num_layers == 2);

    /* Type I codebook */
    nr_complex_t W[8];
    TEST(nr_mimo_codebook_type1(&cfg, W) == 0);

    /* Precoding */
    nr_complex_t sym_in[2] = {{1.0,0.0}, {0.0,1.0}};
    nr_complex_t sym_out[4];
    nr_mimo_precode(W, 4, 2, sym_in, sym_out);

    /* MIMO capacity for identity channel */
    nr_complex_t H_ident[4];
    H_ident[0] = nr_complex_make(1.0, 0.0);
    H_ident[1] = nr_complex_make(0.0, 0.0);
    H_ident[2] = nr_complex_make(0.0, 0.0);
    H_ident[3] = nr_complex_make(1.0, 0.0);
    double cap = nr_mimo_capacity(2, 2, H_ident, 10.0);
    TEST(cap > 0.0);

    /* MIMO detection */
    nr_mimo_det_ctx_t det;
    TEST(nr_mimo_det_init(&det, 2, 2, H_ident, 0.1) == 0);

    nr_complex_t rx[2] = {{1.2,0.1}, {0.8,-0.2}};
    nr_complex_t tx_est[2];
    nr_mimo_det_zf(&det, rx, tx_est);
    nr_mimo_det_mmse(&det, rx, tx_est);

    double sinr[8];
    nr_mimo_det_layers_sinr(&det, sinr);

    nr_mimo_det_free(&det);

    /* Condition number */
    double kappa = nr_mimo_condition_number(2, 2, H_ident);
    TEST_FEQ(kappa, 1.0, 0.1);

    /* Rank estimation */
    int rank = nr_mimo_rank_estimate(2, 2, H_ident, 10.0, 0.1);
    TEST(rank >= 1);

    /* Water-filling */
    double eigenvals[2] = {2.0, 1.0};
    double p_alloc[2];
    double wf_cap = nr_mimo_waterfilling(eigenvals, 2, 2.0, 0.5, p_alloc);
    TEST(wf_cap > 0.0);
    TEST(p_alloc[0] >= p_alloc[1]); /* Stronger mode gets more power */

    /* Massive MIMO precoders */
    nr_complex_t H_massive[8];
    for (int i = 0; i < 8; i++)
        H_massive[i] = nr_complex_make((double)(i+1)*0.1, (double)i*0.05);
    nr_complex_t W_mf[16];
    nr_mimo_mf_precoder(2, 8, H_massive, W_mf);
    nr_complex_t W_zf[16];
    nr_mimo_zf_precoder(2, 8, H_massive, W_zf);

    printf("PASS: test_mimo (%d assertions)\n", tests_run);
    return 0;
}
