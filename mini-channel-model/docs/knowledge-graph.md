# Knowledge Graph: mini-channel-model

## L1: Definitions (Complete)

| # | Definition | C Type | Location |
|---|-----------|--------|----------|
| 1 | Path Loss (dB) | `pathloss_model_t` | `include/pathloss.h` |
| 2 | Fading Types | `fading_type_t` | `include/channel_defs.h` |
| 3 | Doppler Shift f_d | `doppler_params_t` | `include/channel_defs.h` |
| 4 | Coherence Time T_c | computed in `channel_coherence_time()` | `src/channel_core.c` |
| 5 | Coherence Bandwidth B_c | computed in `channel_coherence_bandwidth()` | `src/channel_core.c` |
| 6 | RMS Delay Spread sigma_tau | `power_delay_profile_t` | `include/channel_defs.h` |
| 7 | Rician K-factor | `fading_params_t.k_factor_db` | `include/channel_defs.h` |
| 8 | Nakagami m-parameter | `fading_params_t.m_parameter` | `include/channel_defs.h` |
| 9 | Shadow Fading Std Dev | `fading_params_t.sigma_shadow_db` | `include/channel_defs.h` |
| 10 | MIMO Channel Matrix H | `mimo_channel_matrix_t` | `include/channel_defs.h` |
| 11 | SNR (dB), SINR | `channel_state_info_t.snr_db` | `include/channel_defs.h` |
| 12 | Channel Capacity C | `channel_capacity_t` | `include/channel_defs.h` |
| 13 | Level Crossing Rate N_rho | `lcr_afd_result_t.lcr_hz` | `include/channel_defs.h` |
| 14 | Average Fade Duration tau_rho | `lcr_afd_result_t.afd_s` | `include/channel_defs.h` |
| 15 | Angular Spread | `fading_params_t.angular_spread_deg` | `include/channel_defs.h` |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Large-scale vs Small-scale fading | `FADING_LOGNORMAL` vs `FADING_RAYLEIGH` |
| 2 | Frequency-selective vs Flat fading | `selectivity_type_t` |
| 3 | Fast vs Slow fading | `timevar_type_t` |
| 4 | Path Loss Exponent | `pathloss_params_t.path_loss_exponent` |
| 5 | Shadow Fading Correlation | `fading_generate_lognormal()` |
| 6 | Multipath Resolution | PDP tap structure |
| 7 | Spatial Diversity | MIMO channel matrix |
| 8 | Channel Estimation | CSI snapshot structure |
| 9 | Link Budget Analysis | `pathloss_compute()` + `channel_noise_power_dbm()` |
| 10 | Fading Margin | LCR/AFD computation |

## L3: Mathematical Structures (Complete)

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Complex Baseband Channel | `double complex` throughout |
| 2 | Rayleigh PDF/CDF | `fading_rayleigh_pdf/cdf` |
| 3 | Rician PDF/CDF | `fading_rician_pdf/cdf` |
| 4 | Nakagami-m PDF | `fading_nakagami_pdf` |
| 5 | Log-normal PDF | `fading_lognormal_pdf` |
| 6 | Weibull PDF | `fading_weibull_pdf` |
| 7 | Jakes Doppler PSD | `doppler_jakes_psd()` |
| 8 | Clarke Autocorrelation J_0 | `fading_clarke_autocorrelation()` |
| 9 | Kronecker Correlation Model | `mimo_generate_kronecker()` |
| 10 | Exponential Correlation | `mimo_exponential_correlation()` |
| 11 | Channel Transfer Function | `multipath_transfer_function()` |
| 12 | Frequency Correlation | `multipath_freq_correlation()` |
| 13 | Modified Bessel I_0 | `fading_bessel_i0()` (internal) |
| 14 | Marcum Q Function | `fading_marcum_q1()` (internal) |
| 15 | Power Delay Profile Moments | `multipath_pdp_compute_moments()` |

## L4: Fundamental Laws (Complete)

| # | Theorem / Law | Implementation | Verification |
|---|--------------|---------------|-------------|
| 1 | Friis Free-Space Equation | `pathloss_friis_free_space()` | `test_l4_friis_pathloss` |
| 2 | Shannon-Hartley Capacity | `shannon_*` functions | `test_l4_shannon_capacity` |
| 3 | Two-Ray Ground Reflection | `pathloss_two_ray()` | Structural |
| 4 | Okumura-Hata Empirical Model | `pathloss_okumura_hata_*()` | `test_l4_okumura_hata` |
| 5 | COST-231 Hata Model | `pathloss_cost231_hata()` | Structural |
| 6 | Walfisch-Ikegami Model | `pathloss_walfisch_ikegami_*()` | Structural |
| 7 | Clarke's Autocorrelation | `fading_clarke_autocorrelation()` | `test_l5_clarke_autocorrelation` |
| 8 | Jakes Doppler PSD | `doppler_jakes_psd()` | Structural |
| 9 | Telatar MIMO Capacity | `mimo_capacity_equal_power()` | `test_l6_mimo_capacity` |
| 10 | LCR/AFD Rayleigh Formulas | `doppler_lcr/afd_rayleigh()` | `test_l4_lcr_afd` |
| 11 | MRC Diversity Combining | `multipath_rake_mrc_weights()` | `test_l6_rake_combining` |
| 12 | Water-filling Optimality | `mimo_capacity_waterfilling()` | Structural |
| 13 | ITU-R P.1238 Indoor Model | `pathloss_itu_indoor()` | Structural |
| 14 | 3GPP TR 38.901 Path Loss | `pathloss_3gpp_umi/uma()` | `test_l7_5g_pathloss` |

## L5: Algorithms/Methods (Complete)

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | Box-Muller Gaussian Generation | `fading_rand_normal()` |
| 2 | Sum-of-Sinusoids (Jakes) | `fading_jakes_init/next/free()` |
| 3 | Cholesky Decomposition | `fading_cholesky_decomp()` |
| 4 | Correlated Fading Generation | `fading_generate_correlated_rayleigh()` |
| 5 | Tapped Delay Line (TDL) | `multipath_tdl_process()` |
| 6 | Frequency Response (DFT) | `multipath_freq_response()` |
| 7 | Rake MRC Combining | `multipath_rake_mrc_weights()` |
| 8 | Rake EGC Combining | `multipath_rake_egc_weights()` |
| 9 | Power Iteration (Eigenvalue) | `power_iteration_dominant_eigenvalue()` |
| 10 | Water-Filling Power Allocation | `mimo_capacity_waterfilling()` |
| 11 | LCR Computation for Rayleigh | `doppler_lcr_rayleigh()` |
| 12 | AFD Computation for Rayleigh | `doppler_afd_rayleigh()` |
| 13 | Nakagami Generation | `fading_generate_nakagami()` |
| 14 | Correlated Rician from K-factor | `fading_generate_rician_from_k()` |

## L6: Canonical Problems (Complete)

| # | Problem | Implementation |
|---|---------|---------------|
| 1 | Wireless Link Budget (Urban Macro) | `examples/example_basic_link.c` |
| 2 | BER over Fading Channels | `examples/example_ber_fading.c` |
| 3 | MIMO Capacity Evaluation | `examples/example_mimo_capacity.c` |
| 4 | TDL Channel for OFDM | `multipath_tdl_process_block()` |
| 5 | LTE EPA/EVA/ETU Profiles | `multipath_generate_epa/eva/etu()` |
| 6 | 3GPP TDL-A/B/C Profiles | `multipath_generate_tdl_a/b/c()` |

## L7: Applications (Complete)

| # | Application | Implementation |
|---|------------|---------------|
| 1 | 5G NR UMi Path Loss | `pathloss_3gpp_umi()` |
| 2 | 5G NR UMa Path Loss | `pathloss_3gpp_uma()` |
| 3 | Massive MIMO Channel Hardening | `mimo_channel_hardening_metric()` |

## L8: Advanced Topics (Complete)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | 3GPP 3D Spatial Channel Model | `mimo_generate_3gpp_3d()` |
| 2 | Kronecker Correlated MIMO | `mimo_generate_kronecker()` |
| 3 | Massive MIMO Asymptotic Orthogonality | `mimo_generate_massive_iid()` |

## L9: Research Frontiers (Partial — documented)

| # | Topic | Status |
|---|-------|--------|
| 1 | RIS Channel Modeling | Documented, not implemented |
| 2 | mmWave Beam-Squint Channel | Documented in knowledge graph |
| 3 | AI-based Channel Estimation | Future work |
| 4 | THz Molecular Absorption Channel | Future work |
| 5 | 6G Semantic Channel | Research frontier |
