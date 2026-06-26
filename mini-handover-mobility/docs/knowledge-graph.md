# Knowledge Graph — mini-handover-mobility

Nine-level knowledge coverage map for handover and mobility management in wireless communications.

## L1: Definitions

| ID | Topic | C Implementation | Source |
|----|-------|-----------------|--------|
| L1-01 | Handover Type Classification | `HandoverType` enum (hard/soft/softer/seamless/horizontal/vertical) | handover_types.h |
| L1-02 | Handover Trigger Events | `HandoverTrigger` enum (A1-A6, B1-B2 per 3GPP) | handover_types.h |
| L1-03 | Handover Execution Phases | `HandoverPhase` enum (measurement→completion→failure) | handover_types.h |
| L1-04 | Mobility State Classification | `MobilityState` enum (stationary/normal/medium/high-speed) | handover_types.h |
| L1-05 | Physical Layer Measurements | `MeasurementQuantity` struct (RSRP/RSRQ/SINR/RSSI/CQI) | handover_types.h |
| L1-06 | Measurement Report Structure | `MeasurementReport` struct (serving+neighbours) | handover_types.h |
| L1-07 | Cell Identity | `CellIdentity` struct (PCI/CGI/TAC/EARFCN) | handover_types.h |
| L1-08 | Cell Load Information | `CellLoadInfo` struct (PRB utilization, active UEs) | handover_types.h |
| L1-09 | UE Context | `UEContext` struct (position, velocity, HO state, stats) | handover_types.h |
| L1-10 | Handover Parameters | `HandoverParams` struct (hysteresis, TTT, offsets, timers) | handover_types.h |
| L1-11 | Handover Decision Result | `HandoverDecision` struct (trigger flag, confidence, reason) | handover_types.h |
| L1-12 | Handover Statistics | `HandoverStatistics` struct (success/failure/pingpong/RLF rates) | handover_types.h |
| L1-13 | WiFi FT States | `WiFiFTState` enum (802.11r Fast Transition phases) | handover_protocol.h |
| L1-14 | MIH Link Types | `MIHLinkType` enum (802.3/802.11/802.16/3GPP) | handover_protocol.h |
| L1-15 | RRC Connection States | `RRCState` enum (IDLE/INACTIVE/CONNECTED) | handover_protocol.h |

## L2: Core Concepts

| ID | Topic | Implementation | Source |
|----|-------|---------------|--------|
| L2-01 | Hysteresis-based Handover | `ho_decision_hysteresis()` | handover_decision.c |
| L2-02 | Time-To-Trigger (TTT) | `ho_decision_ttt_evaluate()` | handover_decision.c |
| L2-03 | Ping-Pong Handover Effect | `ho_detect_pingpong()` | handover_decision.c |
| L2-04 | Radio Link Failure (RLF) | `ho_failure_recovery()` | handover_protocol.c |
| L2-05 | RRC State Machine | `ho_rrc_state_transition()` | handover_protocol.c |
| L2-06 | Handover Admission Control | `ho_admission_control()` | handover_optimize.c |
| L2-07 | Measurement-based Triggering | L3 filtering + event evaluation | signal_measurement.c |
| L2-08 | Cell Reselection vs Handover | Mobility state classification | mobility_model.c |
| L2-09 | Make-Before-Break vs Break-Before-Make | DAPS handover evaluation | handover_optimize.c |
| L2-10 | Horizontal vs Vertical Handover | MIH vertical decision + WSM/TOPSIS | handover_decision.c |

## L3: Mathematical Structures

| ID | Topic | Implementation | Source |
|----|-------|---------------|--------|
| L3-01 | Random Walk (Brownian Motion) | `mob_random_walk_step()` | mobility_model.c |
| L3-02 | Random Waypoint Mobility | `mob_random_waypoint_step()` | mobility_model.c |
| L3-03 | Gauss-Markov Mobility Process | `mob_gauss_markov_step()` | mobility_model.c |
| L3-04 | Levy Walk (Power-law flights) | `mob_levy_walk_step()` | mobility_model.c |
| L3-05 | Directional/Manhattan Grid | `mob_directional_step()` | mobility_model.c |
| L3-06 | Reference Point Group Mobility | `mob_group_rpgm_step()` | mobility_model.c |
| L3-07 | Mean Square Displacement Analysis | `mob_compute_msd()` | mobility_model.c |
| L3-08 | Log-Normal Shadow Fading | `meas_shadow_fading_generate()` | signal_measurement.c |
| L3-09 | Rayleigh Fading (Jakes' Model) | `meas_rayleigh_fading_generate()` | signal_measurement.c |
| L3-10 | Rician Fading | `meas_rician_fading_generate()` | signal_measurement.c |
| L3-11 | Kalman Filter (State-Space Model) | `meas_kalman_filter_rsrp()` | signal_measurement.c |
| L3-12 | L3 IIR Filter (EWMA) | `meas_l3_filter()` | signal_measurement.c |
| L3-13 | Okumura-Hata Path Loss Model | `meas_okumura_hata_path_loss()` | signal_measurement.c |
| L3-14 | COST 231 Hata Model | `meas_cost231_hata_path_loss()` | signal_measurement.c |
| L3-15 | 3GPP TR 38.901 UMa Model | `meas_3gpp_38_901_uma_path_loss()` | signal_measurement.c |
| L3-16 | Log-Distance Path Loss | `meas_log_distance_path_loss()` | signal_measurement.c |

## L4: Fundamental Laws

| ID | Theorem/Law | Formula | Implementation | Source |
|----|------------|---------|---------------|--------|
| L4-01 | Friis Transmission Equation | PL = 20·log10(4πd/λ) | `meas_friis_free_space_path_loss()` | signal_measurement.c |
| L4-02 | Doppler Effect | f_d = (v/c)·f_c·cos(θ) | `mob_compute_doppler_shift()` | mobility_model.c |
| L4-03 | Clarke's Coherence Time | T_c ≈ 0.423/f_d_max | `mob_compute_coherence_time()` | mobility_model.c |
| L4-04 | Ping-Pong Probability Bound | P ≤ exp(-H²/(2σ²)) | `ho_decision_hysteresis()` | handover_decision.c |
| L4-05 | Handover Rate Formula | λ_HO ≈ 2v/(πR) | `mob_handover_rate_estimate()` | mobility_model.c |
| L4-06 | Level-Crossing Rate (Rice 1944) | N(R) = N_m·exp(-(R-μ)²/(2σ²)) | `ho_optimize_ttt()` | handover_optimize.c |
| L4-07 | 3GPP Event A3 Formula | Mn+Ofn+Ocn-Hys > Ms+Ofs+Ocs+Off | `ho_decision_event_a3()` | handover_decision.c |
| L4-08 | 3GPP Event A5 Formula | Ms+Hys<Th1 AND Mn+Ofn+Ocn-Hys>Th2 | `ho_decision_event_a5()` | handover_decision.c |
| L4-09 | Shannon Capacity (SINR basis) | C = B·log₂(1+SINR) | `meas_compute_sinr()` | signal_measurement.c |

## L5: Algorithms/Methods

| ID | Algorithm | Complexity | Implementation | Source |
|----|----------|-----------|---------------|--------|
| L5-01 | 3GPP A3 Event Evaluation | O(1) | `ho_decision_event_a3()` | handover_decision.c |
| L5-02 | 3GPP A5 Event Evaluation | O(1) | `ho_decision_event_a5()` | handover_decision.c |
| L5-03 | TTT Sliding Window | O(W) | `ho_decision_ttt_evaluate()` | handover_decision.c |
| L5-04 | Ping-Pong Detection | O(H) | `ho_detect_pingpong()` | handover_decision.c |
| L5-05 | TOPSIS (Hwang & Yoon 1981) | O(n·m) | `ho_decision_topsis()` | handover_decision.c |
| L5-06 | Weighted Sum Model (WSM) | O(n·m) | `ho_decision_weighted_sum()` | handover_decision.c |
| L5-07 | Grey Relational Analysis (GRA) | O(n·m) | `ho_decision_gra()` | handover_decision.c |
| L5-08 | Kalman Filter (Prediction+Update) | O(1) | `meas_kalman_filter_rsrp()` | signal_measurement.c |
| L5-09 | 3GPP L3 Filtering (IIR) | O(1) | `meas_l3_filter()` | signal_measurement.c |
| L5-10 | Hysteresis Optimization (Grid Search) | O(K) | `ho_optimize_hysteresis()` | handover_optimize.c |
| L5-11 | TTT Optimization (LCR-based) | O(1) | `ho_optimize_ttt()` | handover_optimize.c |
| L5-12 | CIO Load-Balancing (P-Controller) | O(1) | `ho_optimize_cio()` | handover_optimize.c |
| L5-13 | Next-Cell Prediction (Linear Reg.) | O(N·C) | `ho_predict_next_cell()` | handover_optimize.c |

## L6: Canonical Problems

| ID | Problem | Standard | Implementation | Source |
|----|---------|---------|---------------|--------|
| L6-01 | LTE X2 Intra-frequency HO | 3GPP TS 36.300 | `ho_lte_x2_procedure_step()` | handover_protocol.c |
| L6-02 | NR N2-based HO | 3GPP TS 38.300 | `ho_nr_n2_procedure_step()` | handover_protocol.c |
| L6-03 | WiFi 802.11r Fast BSS Transition | IEEE 802.11r | `ho_wifi_ft_procedure_step()` | handover_protocol.c |
| L6-04 | WiFi 802.11k Neighbor Report | IEEE 802.11k | `ho_wifi_11k_neighbor_report()` | handover_protocol.c |
| L6-05 | IEEE 802.21 Vertical HO (MIH) | IEEE 802.21 | `ho_mih_vertical_decision()` | handover_protocol.c |
| L6-06 | HO Failure Recovery (RRC Re-est.) | 3GPP TS 36.331 | `ho_failure_recovery()` | handover_protocol.c |
| L6-07 | HO Latency Estimation | 3GPP TR 36.839 | `ho_latency_estimate()` | handover_protocol.c |
| L6-08 | LTE A3 HO Simulation | 3GPP TS 36.331 | example_lte_a3.c | examples/ |
| L6-09 | WiFi FT Roaming Simulation | IEEE 802.11r | example_wifi_roaming.c | examples/ |
| L6-10 | VHO Multi-Criteria Simulation | IEEE 802.21 | example_vertical_handover.c | examples/ |

## L7: Applications

| ID | Application | Standard | Implementation | Source |
|----|------------|---------|---------------|--------|
| L7-01 | Mobility Robustness Optimization (MRO) | 3GPP TS 32.522 (SON) | `ho_mro_diagnose()`, `ho_mro_correct()` | handover_optimize.c |
| L7-02 | Handover Admission Control | 3GPP TS 36.413 | `ho_admission_control()` | handover_optimize.c |
| L7-03 | Energy-Efficient Handover | IoT/MTC optimization | `ho_energy_efficient_decision()` | handover_optimize.c |
| L7-04 | 5G NR Handover Optimization | 3GPP TR 38.300 | Hysteresis/TTT/CIO optimization | handover_optimize.c |

## L8: Advanced Topics

| ID | Topic | Standard | Implementation | Source |
|----|-------|---------|---------------|--------|
| L8-01 | Conditional Handover (CHO) | 3GPP Rel-16 | `ho_conditional_evaluate()`, `ho_conditional_prepare()` | handover_optimize.c |
| L8-02 | DAPS Handover (0ms interruption) | 3GPP Rel-16 | `ho_daps_evaluate()` | handover_optimize.c |
| L8-03 | ML-Based Cell Prediction | Research | `ho_predict_next_cell()` | handover_optimize.c |
| L8-04 | Load-Balancing CIO Optimization | 3GPP MLB | `ho_optimize_cio()` | handover_optimize.c |

## L9: Research Frontiers

| ID | Topic | Status |
|----|-------|--------|
| L9-01 | 6G Mobility Management (AI-native, cell-free) | Documented |
| L9-02 | LEO Satellite Handover (Doppler pre-compensation, ephemeris) | Documented |
| L9-03 | Semantic Communication for Handover Decision | Documented |
| L9-04 | RIS (Reconfigurable Intelligent Surface) Assisted HO | Documented |
| L9-05 | Quantum-Assisted Network Selection | Documented |
