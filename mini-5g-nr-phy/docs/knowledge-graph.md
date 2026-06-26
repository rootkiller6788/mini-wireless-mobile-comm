# Knowledge Graph — 5G NR Physical Layer

## L1: Definitions (Complete)

| ID | Def | Type | Location |
|----|-----|------|----------|
| L1.1 | Numerology (mu, SCS) | `nr_numerology_t` | include/nr_phy_common.h |
| L1.2 | Frame/Slot/Symbol | `nr_re_index_t` | include/nr_phy_common.h |
| L1.3 | Resource Block (12 SC) | `nr_prb_alloc_t` | include/nr_phy_common.h |
| L1.4 | Bandwidth Part (BWP) | `nr_bwp_config_t` | include/nr_phy_common.h |
| L1.5 | CORESET | `nr_coreset_config_t` | include/nr_phy_common.h |
| L1.6 | Search Space | `nr_search_space_t` | include/nr_phy_common.h |
| L1.7 | Physical Channels | `nr_chan_type_t` enum | include/nr_phy_common.h |
| L1.8 | Modulation Schemes | `nr_mod_scheme_t` enum | include/nr_phy_common.h |
| L1.9 | MCS Table Entry | `nr_mcs_entry_t` | include/nr_phy_common.h |
| L1.10 | Transport Block Config | `nr_tb_config_t` | include/nr_phy_common.h |
| L1.11 | DMRS Configuration | `nr_dmrs_config_t` | include/nr_phy_common.h |
| L1.12 | PRACH Configuration | `nr_prach_config_t` | include/nr_phy_common.h |
| L1.13 | Complex Sample | `nr_complex_t` | include/nr_phy_common.h |
| L1.14 | Cell Identity (NID) | `nr_phy_cell_id_t` | include/nr_phy_ssb.h |
| L1.15 | PSS/SSS/MIB types | `nr_pss_result_t`, etc. | include/nr_phy_ssb.h |
| L1.16 | DCI Format 1_0 | `nr_dci_1_0_t` | include/nr_phy_pdcch.h |
| L1.17 | MIMO Config | `nr_mimo_config_t` | include/nr_phy_mimo.h |
| L1.18 | CSI Report | `nr_csi_report_t` | include/nr_phy_mimo.h |
| L1.19 | Channel Tap | `nr_tap_t` | include/nr_phy_channel.h |
| L1.20 | LDPC/Polar Context | `nr_ldpc_enc_ctx_t`, `nr_polar_enc_ctx_t` | include/nr_phy_coding.h |

## L2: Core Concepts (Complete)

| ID | Concept | Implementation |
|----|---------|---------------|
| L2.1 | Scalable OFDM numerology | `nr_numerology_init()` |
| L2.2 | Frame/TDD structure | `nr_slot_format_t` |
| L2.3 | Bandwidth adaptation (BWP) | `nr_bwp_configure()` |
| L2.4 | Resource element mapping | `nr_re_index_to_position()` |
| L2.5 | Transport block size calculation | `nr_tbs_calculate()` |
| L2.6 | MCS table selection | `nr_mcs_lookup()` |
| L2.7 | Frequency ranges (FR1/FR2) | `nr_is_fr1()`, `nr_is_fr2()` |
| L2.8 | CP-OFDM vs DFT-s-OFDM | `nr_ofdm_modulate()` / `nr_dft_s_ofdm_modulate()` |
| L2.9 | Channel coding (LDPC/Polar) | `nr_ldpc_init()` / `nr_polar_init()` |
| L2.10 | Spatial multiplexing | `nr_mimo_precode()` |
| L2.11 | Beamforming (Type I codebook) | `nr_mimo_codebook_type1()` |
| L2.12 | Cell search (PSS/SSS) | `nr_cell_search()` |
| L2.13 | Initial access (PBCH/MIB) | `nr_mib_decode()` |
| L2.14 | PDCCH blind decoding | `nr_pdcch_get_candidates()` |
| L2.15 | HARQ (NDI/RV/CBG) | DCI fields in DCI 1_0 |

## L3: Mathematical Structures (Complete)

| ID | Structure | Implementation |
|----|-----------|---------------|
| L3.1 | Complex arithmetic | inline functions in nr_phy_common.h |
| L3.2 | GF(2) operations | `Bit.add`/`Bit.mul` in nr_phy.lean |
| L3.3 | FFT/IFFT (radix-2 DIT) | `nr_fft()` in nr_phy_ofdm.c |
| L3.4 | DFT spreading | `nr_dft_s_ofdm_modulate()` |
| L3.5 | CRC polynomials | `nr_crc24c()`, `nr_crc24a()`, etc. |
| L3.6 | Gold sequences (SSS, PBCH DMRS) | `nr_sss_sequence()`, `nr_pbch_dmrs_sequence()` |
| L3.7 | m-sequences (PSS) | `nr_pss_sequence()` |
| L3.8 | MIMO channel matrix | `nr_mimo_channel_matrix()` |
| L3.9 | SVD decomposition | `nr_mimo_svd()` |
| L3.10 | Polar kernel (Butterfly) | `polar_butterfly()` |

## L4: Fundamental Laws (Complete)

| ID | Law/Theorem | Implementation |
|----|------------|---------------|
| L4.1 | OFDM orthogonality | `nr_numerology_init()` (docs) |
| L4.2 | Nyquist sampling (FFT size) | `nr_fft_size_min()` |
| L4.3 | Shannon-Hartley theorem | `nr_channel_capacity()`, Lean proof |
| L4.4 | Friis transmission equation | `nr_pathloss_db(NR_PATHLOSS_FREE_SPACE)` |
| L4.5 | Doppler shift equation | `nr_tdl_init()` (f_d = v*f_c/c) |
| L4.6 | MIMO capacity (Telatar 1999) | `nr_mimo_capacity()` |
| L4.7 | Water-filling (MIMO) | `nr_mimo_waterfilling()` |
| L4.8 | GF(2) field axioms (LDPC) | Lean proofs: gf2_add_assoc, etc. |
| L4.9 | Polar code capacity-achieving | `nr_polar_init()` (Arikan 2009 docs) |

## L5: Algorithms/Methods (Complete)

| ID | Algorithm | Implementation |
|----|-----------|---------------|
| L5.1 | Radix-2 FFT (Cooley-Tukey) | `nr_fft()` |
| L5.2 | CP-OFDM modulation | `nr_ofdm_modulate()` |
| L5.3 | DFT-s-OFDM modulation | `nr_dft_s_ofdm_modulate()` |
| L5.4 | PAPR computation | `nr_ofdm_papr()` |
| L5.5 | LS channel estimation | `nr_chan_est_ls()` |
| L5.6 | MMSE channel estimation | `nr_chan_est_mmse()` |
| L5.7 | LDPC encoding (QC-LDPC) | `nr_ldpc_encode()` |
| L5.8 | Min-sum BP decoding | `nr_ldpc_decode_min_sum()` |
| L5.9 | Polar encoding (Arikan) | `nr_polar_encode()` |
| L5.10 | SC decoding | `nr_polar_decode_sc()` |
| L5.11 | SCL decoding | `nr_polar_decode_scl()` |
| L5.12 | ZF MIMO detection | `nr_mimo_det_zf()` |
| L5.13 | MMSE MIMO detection | `nr_mimo_det_mmse()` |
| L5.14 | MMSE-SIC detection | `nr_mimo_det_mmse_sic()` |
| L5.15 | Type I codebook precoding | `nr_mimo_codebook_type1()` |
| L5.16 | PSS time-domain detection | `nr_pss_detect()` |
| L5.17 | SSS frequency-domain detection | `nr_sss_detect()` |
| L5.18 | PDCCH candidate hashing | `nr_pdcch_get_candidates()` |
| L5.19 | DCI CRC (RNTI scrambling) | `nr_dci_crc_attach()` |
| L5.20 | PDSCH scrambling (Gold seq) | `nr_pdsch_scramble()` |
| L5.21 | QAM soft demodulation | `nr_pdsch_demodulate_soft()` |
| L5.22 | Jakes Doppler channel gen | `nr_tdl_generate()` |
| L5.23 | TDL channel application | `nr_tdl_apply()` |
| L5.24 | Rate matching (LDPC/Polar) | `nr_ldpc_rate_match()` / `nr_polar_rate_match()` |
| L5.25 | Jacobi SVD | `nr_mimo_svd()` |

## L6: Canonical Problems (Complete)

| ID | Problem | Solution Location |
|----|---------|------------------|
| L6.1 | Complete cell search | `nr_cell_search()` + example_ssb_detection.c |
| L6.2 | PDSCH TX/RX chain | `nr_pdsch_full_chain_tx/rx()` |
| L6.3 | LDPC encode/decode loop | example_ldpc_codec.c |
| L6.4 | OFDM TX/RX with channel | example_ofdm_chain.c |
| L6.5 | 5G downlink processing | example_5g_downlink.c |
| L6.6 | PDCCH blind decoding | `nr_pdcch_get_candidates()` |
| L6.7 | SSB beam sweeping | `nr_pbch_dmrs_detect_ssb_index()` |

## L7: Applications (Partial+)

| ID | Application | Implementation |
|----|-------------|---------------|
| L7.1 | 5G NR FR1 gNB PHY chain | example_5g_downlink.c |
| L7.2 | UE cell search & MIB decode | example_ssb_detection.c |
| L7.3 | LDPC/Polar channel coding | example_ldpc_codec.c |
| L7.4 | Link budget analysis | demo/nr_demo.py |
| L7.5 | URLLC via Polar codes | `nr_polar_init()` with CRC |

## L8: Advanced Topics (Partial+)

| ID | Topic | Implementation |
|----|-------|---------------|
| L8.1 | Massive MIMO precoding | `nr_mimo_mf_precoder()`, `nr_mimo_zf_precoder()` |
| L8.2 | MMSE-SIC MIMO detection | `nr_mimo_det_mmse_sic()` |
| L8.3 | Polar SCL decoding | `nr_polar_decode_scl()` |
| L8.4 | TDL channel emulation | `nr_tdl_init()` / `nr_tdl_generate()` |
| L8.5 | MIMO channel reciprocity | Lean: `TDD_ChannelPair` |

## L9: Research Frontiers (Partial — documented)

| ID | Topic | Status |
|----|-------|--------|
| L9.1 | RIS-assisted 5G-Advanced | Documented (see wireless_phy_sec.h sibling) |
| L9.2 | AI-native air interface | Documented in knowledge-graph |
| L9.3 | Semantic communication PHY | Reference in docs |
| L9.4 | FR2 mmWave beam management | `nr_is_fr2()`, FR2 numerology support |
