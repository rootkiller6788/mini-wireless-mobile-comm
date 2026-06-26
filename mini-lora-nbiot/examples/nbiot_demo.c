/**
 * @file nbiot_demo.c
 * @brief NB-IoT PHY demo -- NPSS/NSSS, cell search, PSM power analysis
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "nbiot_phy.h"

int main(void) {
    printf("=== NB-IoT PHY Demo ===\n\n");

    printf("NB-IoT System Parameters:\n");
    printf("  Bandwidth: 180 kHz\n");
    printf("  Subcarriers: 12 (15 kHz spacing)\n");
    printf("  Frame duration: 10 ms\n");
    printf("  Max PCID: 504\n\n");

    printf("[L3] Zadoff-Chu sequence (N=11, u=5 for NPSS):\n");
    double complex zc[11];
    nbiot_zadoff_chu(11, 5, zc);
    printf("  z[0]=%.4f+j%.4f |z|=%.4f\n", creal(zc[0]), cimag(zc[0]), cabs(zc[0]));
    printf("  z[5]=%.4f+j%.4f |z|=%.4f\n", creal(zc[5]), cimag(zc[5]), cabs(zc[5]));

    printf("\n[L5] NPSS generation for PCID=0:\n");
    nbiot_subframe_t sf;
    nbiot_npss_generate(&sf, 0);
    double npss_pwr = 0.0;
    for (int sc = 0; sc < 12; sc++)
        for (int sy = 3; sy < 14; sy++)
            npss_pwr += cabs(sf.re_grid[sc][sy]) * cabs(sf.re_grid[sc][sy]);
    printf("  NPSS total power: %.4f (expected ~1.0)\n", npss_pwr);

    printf("\n[L5] NSSS generation for PCID=123, SFN=0:\n");
    nbiot_nsss_generate(&sf, 123, 0);
    double nsss_pwr = 0.0;
    for (int sc = 0; sc < 12; sc++)
        for (int sy = 3; sy < 14; sy++)
            nsss_pwr += cabs(sf.re_grid[sc][sy]) * cabs(sf.re_grid[sc][sy]);
    printf("  NSSS total power: %.4f\n", nsss_pwr);

    printf("\n[L5] Frequency hopping pattern (PCID=100, 16 slots):\n");
    uint8_t hop[16];
    nbiot_hopping_pattern(100, 16, hop);
    printf("  SC offsets: ");
    for (int i = 0; i < 16; i++) printf("%d ", hop[i]);
    printf("\n");

    printf("\n[L4] NB-IoT MCL calculation:\n");
    double mcl = nbiot_max_coupling_loss(23.0, -141.0);
    printf("  MCL (23dBm TX, -141dBm RX): %.0f dB\n", mcl);
    printf("  NB-IoT target MCL: 164 dB\n");

    printf("\nDemo complete.\n");
    return 0;
}
