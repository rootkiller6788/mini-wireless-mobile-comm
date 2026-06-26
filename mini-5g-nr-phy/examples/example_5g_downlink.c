#include "nr_phy_common.h"
#include "nr_phy_ofdm.h"
#include "nr_phy_channel.h"
#include "nr_phy_coding.h"
#include "nr_phy_mimo.h"
#include "nr_phy_pdcch.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(void)
{
    printf("=== 5G NR Downlink PHY Processing Chain ===\n\n");

    /* Carrier config: 3.5 GHz, 100 MHz, mu=1 (30 kHz SCS) */
    nr_carrier_config_t carrier;
    nr_carrier_config_init(&carrier, 3.5e9, 100e6, 1, 0);
    printf("Carrier: %.2f GHz, %.0f MHz BW, mu=%d, %d PRBs\n",
           carrier.center_freq_hz/1e9, carrier.bandwidth_hz/1e6,
           carrier.numerology_mu, carrier.num_prb);

    /* Active BWP */
    nr_bwp_config_t *bwp = &carrier.bwps[0];
    printf("BWP: start_prb=%d, num_prb=%d\n", bwp->start_prb, bwp->num_prb);

    /* Numerolgy info */
    double scs = nr_scs_khz(carrier.numerology_mu);
    int slots = nr_slots_per_frame(carrier.numerology_mu);
    printf("SCS=%.0f kHz, Slots/frame=%d\n", scs, slots);

    /* Create DCI 1_0 scheduling assignment */
    nr_dci_1_0_t dci;
    memset(&dci, 0, sizeof(dci));
    dci.freq_domain_assign = (10 << 6) | 30;
    dci.time_domain_assign = 0;
    dci.vrb_to_prb_mapping = 0;
    dci.mcs = 10; /* QPSK, rate ~0.3 */
    dci.new_data_indicator = 1;
    dci.redundancy_version = 0;
    dci.harq_process_number = 0;
    dci.downlink_assignment_index = 0;
    dci.tpc_command = 0;
    dci.pucch_resource_indicator = 0;
    dci.pdsch_to_harq_feedback = 4;

    /* Pack DCI */
    uint8_t dci_bits[NR_DCI_1_0_BITS/8 + 2];
    int dci_nbits;
    nr_dci_1_0_pack(&dci, dci_bits, &dci_nbits);
    printf("DCI 1_0: %d bits, MCS=%d\n", dci_nbits, dci.mcs);

    /* DCI CRC attach with RNTI */
    int rnti = 0x1234;
    uint8_t dci_crc[NR_DCI_MAX_BITS/8 + 4];
    nr_dci_crc_attach(dci_bits, dci_nbits, rnti, dci_crc);
    printf("DCI + CRC: %d bits (CRC24 attached)\n", dci_nbits + 24);

    /* CRC check */
    int crc_ok = nr_dci_crc_check(dci_crc, dci_nbits + 24, rnti);
    printf("CRC check with RNTI=0x%04X: %s\n", rnti, crc_ok ? "OK" : "FAIL");

    /* Derive PDSCH allocation */
    nr_pdsch_alloc_t pdsch_alloc;
    nr_pdsch_alloc_from_dci(&dci, bwp, &pdsch_alloc);
    printf("PDSCH: %d PRBs, start=%d, MCS=%d\n",
           pdsch_alloc.num_prb, pdsch_alloc.start_prb, dci.mcs);

    /* MIMO configuration */
    nr_mimo_config_t mimo_cfg;
    nr_mimo_config_init(&mimo_cfg, 4, 2, 2, 2, 1, 4, 1, 1);
    printf("MIMO: %d TX ports, %d layers\n",
           mimo_cfg.num_tx_ports, mimo_cfg.num_layers);

    /* Compute MIMO capacity */
    nr_complex_t H[8];
    for (int i = 0; i < 8; i++)
        H[i] = nr_complex_make((double)((i+3)%5)*0.3 + 0.2,
                                (double)(i%3)*0.1);
    double snr_lin = pow(10.0, 15.0/10.0);
    double cap = nr_mimo_capacity(2, 4, H, snr_lin);
    printf("MIMO capacity: %.2f bps/Hz at 15 dB SNR\n", cap);
    printf("  = %.2f Gbps at 100 MHz BW\n", cap * 100e6 / 1e9);

    /* Shannon capacity comparison */
    double shannon_cap = nr_channel_capacity(100e6, snr_lin);
    printf("SISO Shannon: %.2f Gbps\n", shannon_cap / 1e9);

    /* Path loss at various distances */
    printf("\n--- Path Loss (UMa, 3.5 GHz) ---\n");
    double dists[] = {100.0, 200.0, 500.0, 1000.0};
    for (int i = 0; i < 4; i++) {
        double pl = nr_pathloss_db(NR_PATHLOSS_UMA, dists[i], 3.5, 25.0, 1.5, 1);
        double snr_db = nr_snr_dbm(43.0, pl, 15.0, 2.0, 100e6, 7.0, NULL);
        printf("  d=%.0fm: PL=%.1f dB, SNR=%.1f dB\n", dists[i], pl, snr_db);
    }

    printf("\n=== 5G Downlink Chain Complete ===\n");
    return 0;
}
