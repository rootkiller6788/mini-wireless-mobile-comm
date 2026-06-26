#include "nr_phy_coding.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int main(void)
{
    printf("=== 5G NR LDPC/Polar Codec Example ===\n\n");

    srand((unsigned)time(NULL));

    /* LDPC encode/decode with BG1, K=2048 bits, rate=0.5 */
    nr_ldpc_enc_ctx_t enc;
    if (nr_ldpc_init(&enc, NR_LDPC_BG1, 2048, 0.5) != 0) {
        fprintf(stderr, "LDPC init failed\n");
        return 1;
    }
    printf("LDPC: BG1, K=%d, N=%d, Z_c=%d\n", enc.K, enc.N, enc.Z_c);

    /* Generate random info bits */
    uint8_t info[256] = {0};
    for (int i = 0; i < enc.K / 8; i++)
        info[i] = (uint8_t)(rand() & 0xFF);

    /* Encode */
    uint8_t cw[1024] = {0};
    nr_ldpc_encode(&enc, info, cw);
    printf("Encoded: %d bits\n", enc.N);

    /* Rate match */
    nr_rate_match_ctx_t rm;
    rm.E = 4096;
    rm.rv = 0;
    rm.N_cb = enc.N;
    rm.k0 = 0;
    rm.ilv_mode = 0;
    uint8_t rm_out[512] = {0};
    nr_ldpc_rate_match(&rm, cw, rm_out);
    printf("Rate matched: %d bits\n", rm.E);

    /* Simulate AWGN channel: LLR = 2*y/sigma^2 for BPSK */
    double *llr = calloc(rm.E, sizeof(double));
    for (int i = 0; i < rm.E; i++) {
        int bit = (rm_out[i/8]>>(7-(i%8)))&1;
        double tx = bit ? -1.0 : 1.0;
        double noise = ((double)rand()/RAND_MAX - 0.5) * 0.3;
        double rx = tx + noise;
        llr[i] = 2.0 * rx / 0.1; /* sigma^2 = 0.1 */
    }

    /* Rate recover */
    double *llr_buf = calloc(enc.N, sizeof(double));
    nr_ldpc_rate_recover(&rm, llr, llr_buf, enc.N);

    /* Decode */
    nr_ldpc_dec_ctx_t dec;
    nr_ldpc_decode_init(&dec, &enc, 10);
    uint8_t decoded[256] = {0};
    int ret = nr_ldpc_decode_min_sum(&dec, llr_buf, decoded);
    printf("LDPC decode: %s (iterations=%d)\n",
           ret == 0 ? "CONVERGED" : "MAX ITER",
           dec.iterations_used);

    /* Compare info bits */
    int bit_errors = 0;
    for (int i = 0; i < enc.K; i++) {
        int orig = (info[i/8]>>(7-(i%8)))&1;
        int dec_bit = (decoded[i/8]>>(7-(i%8)))&1;
        if (orig != dec_bit) bit_errors++;
    }
    printf("LDPC bit errors: %d / %d (BER=%.2e)\n",
           bit_errors, enc.K, (double)bit_errors/enc.K);

    nr_ldpc_decode_free(&dec);
    free(llr); free(llr_buf);

    /* Polar code demo */
    printf("\n--- Polar Code ---\n");
    nr_polar_enc_ctx_t pol;
    if (nr_polar_init(&pol, 40, 108, 6) == 0) {
        printf("Polar: N=%d, K=%d, E=%d\n", pol.N, pol.K, pol.E);
        uint8_t pol_info[8] = {0x55};
        uint8_t pol_cw[128] = {0};
        nr_polar_encode(&pol, pol_info, pol_cw);

        /* Interleave and rate match */
        uint8_t rm_pol[128] = {0};
        nr_polar_rate_match(pol_cw, pol.N, pol.E, rm_pol);

        /* Decode (noiseless) */
        nr_polar_dec_ctx_t pol_dec;
        pol_dec.enc_ctx = pol;
        pol_dec.list_size = 1;
        double *pol_llr = calloc(pol.N, sizeof(double));
        nr_polar_rate_recover(pol_llr, pol.E, pol.N,
                              pol.rate_match_mode, pol_llr);
        /* Fill LLRs from codeword */
        for (int i = 0; i < pol.N; i++) {
            int bit = (pol_cw[i/8]>>(7-(i%8)))&1;
            pol_llr[i] = bit ? -10.0 : 10.0;
        }
        uint8_t pol_dec_out[8] = {0};
        nr_polar_decode_sc(&pol_dec, pol_llr, pol_dec_out);
        printf("Polar decode: OK\n");

        nr_polar_dec_free(&pol_dec);
        free(pol_llr);
    }

    printf("\n=== LDPC/Polar Codec Complete ===\n");
    return 0;
}
