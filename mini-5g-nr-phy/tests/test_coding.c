#include "nr_phy_coding.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int tests_run = 0;
#define TEST(expr) do { if(!(expr)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);exit(1);} tests_run++; } while(0)

int main(void)
{
    /* CRC computations */
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t crc24c = nr_crc24c(test_data, 32);
    TEST(crc24c != 0);
    uint32_t crc24a = nr_crc24a(test_data, 32);
    TEST(crc24a != 0);
    uint16_t crc16 = nr_crc16(test_data, 32);
    TEST(crc16 != 0);
    uint8_t crc6 = nr_crc6(test_data, 32);
    TEST((crc6 & 0xC0) == 0);
    uint16_t crc11 = nr_crc11(test_data, 32);
    TEST((crc11 & 0xF800) == 0);

    /* Bit packing */
    uint8_t bits[] = {1,0,1,0,1,1,0,0, 0,1,0,1,0,0,1,1};
    uint8_t packed[2];
    nr_bits_pack(bits, 16, packed);
    TEST(packed[0] == 0xAC);
    TEST(packed[1] == 0x53);

    uint8_t unpacked[16];
    nr_bits_unpack(packed, 2, unpacked, 16);
    for (int i = 0; i < 16; i++) TEST(unpacked[i] == bits[i]);

    /* LDPC encoder */
    nr_ldpc_enc_ctx_t ldpc_enc;
    TEST(nr_ldpc_init(&ldpc_enc, NR_LDPC_BG1, 1024, 0.5) == 0);
    TEST(ldpc_enc.Z_c > 0);
    TEST(ldpc_enc.N > 0);

    uint8_t info[128] = {0};
    for (int i = 0; i < 64; i++) info[i] = (uint8_t)(i * 3 + 1);
    uint8_t cw[1024] = {0};
    nr_ldpc_encode(&ldpc_enc, info, cw);

    /* Rate matching */
    nr_rate_match_ctx_t rm;
    rm.E = 2048;
    rm.rv = 0;
    rm.N_cb = ldpc_enc.N;
    rm.k0 = 0;
    rm.ilv_mode = 0;
    uint8_t rm_out[256] = {0};
    int out_bits = nr_ldpc_rate_match(&rm, cw, rm_out);
    TEST(out_bits == 2048);

    /* LDPC decoder */
    nr_ldpc_dec_ctx_t ldpc_dec;
    TEST(nr_ldpc_decode_init(&ldpc_dec, &ldpc_enc, 5) == 0);

    double *llr = calloc(ldpc_enc.N, sizeof(double));
    for (int i = 0; i < ldpc_enc.N; i++) {
        int bi = (cw[i/8]>>(7-(i%8)))&1;
        llr[i] = bi ? -5.0 : 5.0;
    }
    uint8_t decoded[128] = {0};
    int ret = nr_ldpc_decode_min_sum(&ldpc_dec, llr, decoded);
    TEST(ret == 0 || ret == 1);

    nr_ldpc_decode_free(&ldpc_dec);
    free(llr);

    /* Polar encoder */
    nr_polar_enc_ctx_t pol_enc;
    TEST(nr_polar_init(&pol_enc, 40, 100, 6) == 0);
    TEST(pol_enc.N >= 64);
    uint8_t pol_info[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint8_t pol_cw[128] = {0};
    nr_polar_encode(&pol_enc, pol_info, pol_cw);

    /* Polar rate matching */
    uint8_t pol_rm[128] = {0};
    int pol_out = nr_polar_rate_match(pol_cw, pol_enc.N, pol_enc.E, pol_rm);
    TEST(pol_out == pol_enc.E);

    /* Sub-block interleaver */
    uint8_t ilv[128] = {0};
    nr_polar_subblock_interleave(pol_cw, pol_enc.N, ilv);

    /* Polar SC decoder */
    nr_polar_dec_ctx_t pol_dec;
    pol_dec.enc_ctx = pol_enc;
    pol_dec.list_size = 1;
    double *pol_llr = calloc(pol_enc.N, sizeof(double));
    for (int i = 0; i < pol_enc.N; i++) {
        int bi = (pol_cw[i/8]>>(7-(i%8)))&1;
        pol_llr[i] = bi ? -10.0 : 10.0;
    }
    uint8_t pol_dec_out[8] = {0};
    int pol_ret = nr_polar_decode_sc(&pol_dec, pol_llr, pol_dec_out);
    TEST(pol_ret == 0);

    nr_polar_dec_free(&pol_dec);
    free(pol_llr);

    printf("PASS: test_coding (%d assertions)\n", tests_run);
    return 0;
}
