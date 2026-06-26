/**
 * @file handover_optimize.h
 * @brief Handover parameter optimization and advanced techniques (L7/L8)
 *
 * Implements optimization algorithms for handover parameters and advanced
 * handover mechanisms from 3GPP Release 15-17 (5G NR).
 *
 * Knowledge Coverage:
 *   L5 - Optimization algorithms
 *   L7 - Applications: 5G NR handover optimization, self-organizing networks
 *   L8 - Advanced: Conditional handover, DAPS, ML-based prediction, load balancing
 *
 * References:
 *   - 3GPP TR 38.300 §9.2.3 (NR Mobility)
 *   - 3GPP TR 38.331 §5.3.5 (Conditional Handover)
 *   - 3GPP TR 38.401 (NG-RAN Architecture)
 *   - Molisch, "Wireless Communications" (2011), Ch. 17-19
 */

#ifndef HANDOVER_OPTIMIZE_H
#define HANDOVER_OPTIMIZE_H

#include "handover_types.h"

/* ============================================================================
 * L5: Hysteresis Margin Optimization
 * ============================================================================ */

/**
 * ho_optimize_hysteresis - Optimal hysteresis margin computation.
 *
 * The hysteresis margin faces a fundamental trade-off:
 *   Low hysteresis → fast handover, but high ping-pong risk
 *   High hysteresis → low ping-pong, but risk of RLF before handover
 *
 * Optimal solution (using cost function minimization):
 *   H* = argmin_H [ w_R · P_RLF(H) + w_P · P_pingpong(H) + w_S · N_signaling(H) ]
 *
 * where P_RLF ≈ Q((H - Δ_avg)/(σ_shadow·√2))
 *       P_pingpong ≈ exp(-H²/(2·σ_shadow²))
 *
 * This function computes the optimal hysteresis that minimizes a weighted
 * cost of handover failures and ping-pong events.
 *
 * @param shadow_fading_std_db  Standard deviation of shadow fading (dB).
 * @param weight_rlf            Weight for RLF cost.
 * @param weight_pingpong        Weight for ping-pong cost.
 * @param min_hysteresis_db     Lower bound on hysteresis.
 * @param max_hysteresis_db     Upper bound on hysteresis.
 * @param step_db               Search step size.
 * @return Optimal hysteresis value in dB.
 */
double ho_optimize_hysteresis(double shadow_fading_std_db,
                              double weight_rlf,
                              double weight_pingpong,
                              double min_hysteresis_db,
                              double max_hysteresis_db,
                              double step_db);

/* ============================================================================
 * L5: TTT (Time-To-Trigger) Optimization
 * ============================================================================ */

/**
 * ho_optimize_ttt - Optimal TTT computation.
 *
 * TTT optimization balances handover delay against unnecessary handovers:
 *   Long TTT → filters out fast fading but increases handover delay
 *   Short TTT → fast reaction but prone to false triggers
 *
 * The optimization uses the theory of level-crossing rates (Rice, 1944):
 *   Expected level-crossing rate N(R) gives the average rate at which the
 *   signal crosses a given threshold R. The TTT should be set longer than
 *   the average fade duration at the handover threshold.
 *
 * Average fade duration at threshold R:
 *   AFD(R) = P(RSRP < R) / N(R)
 *
 * Optimal TTT ≈ k · AFD, where k ≥ 1 is a safety factor.
 *
 * @param speed_mps             UE speed.
 * @param carrier_freq_hz       Carrier frequency.
 * @param shadow_fading_std_db  Shadow fading standard deviation.
 * @param correlation_distance_m Shadow fading correlation distance.
 * @param safety_factor         TTT safety factor (typically 1.5–3.0).
 * @return Optimal TTT in milliseconds.
 */
double ho_optimize_ttt(double speed_mps,
                       double carrier_freq_hz,
                       double shadow_fading_std_db,
                       double correlation_distance_m,
                       double safety_factor);

/* ============================================================================
 * L5: Cell Individual Offset (CIO) Optimization
 * ============================================================================ */

/**
 * ho_optimize_cio - Optimize Cell Individual Offset for load balancing.
 *
 * CIO is used in 3GPP LTE/NR to bias handover toward or away from specific
 * cells, enabling Mobility Load Balancing (MLB). A positive CIO makes the
 * cell more attractive (expands coverage), negative CIO makes it less
 * attractive (shrinks coverage).
 *
 * Algorithm: Adjust CIO proportionally to load difference:
 *   CIO_new = CIO_old + β · (load_target - load_serving)
 *
 * where β is the step size and load is PRB utilization.
 *
 * @param current_cio_db       Current CIO value.
 * @param load_serving         PRB utilization of serving cell [0, 1].
 * @param load_target          PRB utilization of target cell [0, 1].
 * @param step_size_db         Adjustment step size.
 * @param max_cio_db           Maximum allowed CIO.
 * @return Updated CIO value.
 */
double ho_optimize_cio(double current_cio_db,
                       double load_serving,
                       double load_target,
                       double step_size_db,
                       double max_cio_db);

/* ============================================================================
 * L8: Conditional Handover (CHO) — 3GPP Rel-16
 * ============================================================================ */

/**
 * ho_conditional_evaluate - Evaluate Conditional Handover execution condition.
 *
 * Conditional Handover (3GPP Rel-16) decouples handover preparation from
 * execution. The network prepares multiple target cells in advance, and the
 * UE autonomously executes handover when the configured condition is met.
 *
 * Benefits:
 *   - Reduced handover failure (especially at cell edge)
 *   - Robustness for high-speed UEs
 *   - Reduced signaling overhead during critical moments
 *
 * CHO execution condition:
 *   IF RSRP_serving < CHO_serving_threshold
 *      AND RSRP_target > CHO_target_threshold
 *      AND condition satisfied for TTT
 *   THEN execute handover to target
 *
 * @param serving_rsrp_dbm       Current serving RSRP.
 * @param target_rsrp_dbm        Target cell RSRP.
 * @param serving_threshold_dbm  Release threshold for serving cell.
 * @param target_threshold_dbm   Admission threshold for target cell.
 * @param ttt_condition_met      Whether TTT condition is satisfied.
 * @return true if CHO execution condition is met.
 */
bool ho_conditional_evaluate(double serving_rsrp_dbm,
                             double target_rsrp_dbm,
                             double serving_threshold_dbm,
                             double target_threshold_dbm,
                             bool   ttt_condition_met);

/**
 * ho_conditional_prepare - Prepare target cell list for CHO.
 *
 * Selects the top N candidate cells for conditional handover preparation
 * based on measurement report.
 *
 * @param report               UE measurement report.
 * @param min_rsrp_dbm         Minimum RSRP for candidate selection.
 * @param max_candidates       Maximum number of CHO candidates.
 * @param[out] candidate_pcis  Selected candidate cell PCIs.
 * @param[out] num_candidates  Number of selected candidates.
 */
void ho_conditional_prepare(const MeasurementReport *report,
                            double                   min_rsrp_dbm,
                            int                      max_candidates,
                            uint32_t                *candidate_pcis,
                            int                     *num_candidates);

/* ============================================================================
 * L8: DAPS Handover — 3GPP Rel-16
 * ============================================================================ */

/**
 * ho_daps_evaluate - Evaluate DAPS (Dual Active Protocol Stack) handover.
 *
 * DAPS handover (3GPP Rel-16) maintains simultaneous RRC connection with
 * both source and target cells during handover execution. The UE has two
 * protocol stacks active in parallel, achieving near-zero interruption time
 * (target: 0 ms for URLLC services).
 *
 * Key innovation: UE simultaneously receives user data from both cells
 * during the handover execution phase, then releases the source link only
 * after the target link is fully established.
 *
 * DAPS feasibility check:
 *   - UE must support DAPS capability
 *   - Target must have sufficient resources
 *   - Source must support DAPS forwarding
 *
 * @param ue_daps_capable       UE supports DAPS.
 * @param target_daps_capable   Target cell supports DAPS.
 * @param target_prb_available  Available PRBs at target.
 * @param required_prbs         PRBs required for UE.
 * @return true if DAPS handover is feasible.
 */
bool ho_daps_evaluate(bool   ue_daps_capable,
                      bool   target_daps_capable,
                      int    target_prb_available,
                      int    required_prbs);

/* ============================================================================
 * L7: Mobility Robustness Optimization (MRO) — 3GPP SON
 * ============================================================================ */

/**
 * ho_mro_diagnose - Diagnose handover problem type.
 *
 * MRO (3GPP TS 36.902 / TS 38.300 §15.3) automatically detects and corrects
 * handover problems in Self-Organizing Networks (SON):
 *
 * Problem types:
 *   0 = No problem
 *   1 = Too-late handover (RLF before HO triggered)
 *   2 = Too-early handover (RLF shortly after HO to target)
 *   3 = Handover to wrong cell (RLF after HO, then reconnects to different cell)
 *
 * Diagnosis based on RLF report analysis.
 *
 * @param rlf_occurred_before_ho     RLF happened before HO trigger.
 * @param rlf_occurred_after_ho      RLF happened within Tstore after HO.
 * @param reconnected_to_source      UE reconnected to source after RLF.
 * @param reconnected_to_other       UE reconnected to different cell.
 * @return Problem type code (0-3).
 */
int ho_mro_diagnose(bool rlf_occurred_before_ho,
                    bool rlf_occurred_after_ho,
                    bool reconnected_to_source,
                    bool reconnected_to_other);

/**
 * ho_mro_correct - Apply MRO correction to handover parameters.
 *
 * Based on MRO diagnosis, adjusts handover parameters:
 *   Too-late: Decrease TTT and/or increase A3 offset
 *   Too-early: Increase TTT and/or decrease CIO for target
 *   Wrong cell: Adjust CIO for both target and correct cell
 *
 * @param problem_type         Diagnosed problem (0-3).
 * @param current_ttt_ms       Current TTT value.
 * @param current_a3_offset_db Current A3 offset.
 * @param current_cio_db       Current CIO to target.
 * @param[out] new_ttt_ms      Adjusted TTT.
 * @param[out] new_a3_offset_db Adjusted A3 offset.
 * @param[out] new_cio_db      Adjusted CIO.
 */
void ho_mro_correct(int    problem_type,
                    double current_ttt_ms,
                    double current_a3_offset_db,
                    double current_cio_db,
                    double *new_ttt_ms,
                    double *new_a3_offset_db,
                    double *new_cio_db);

/* ============================================================================
 * L8: ML-Based Handover Prediction
 * ============================================================================ */

/**
 * ho_predict_next_cell - Predict the next handover target cell.
 *
 * Uses a simple moving-average trend of RSRP measurements to predict which
 * cell will be the strongest at the next measurement interval. This is a
 * lightweight prediction suitable for on-device implementation.
 *
 * Prediction model: linear extrapolation of RSRP trend
 *   RSRP(t+Δt) ≈ RSRP(t) + gradient(t) · Δt
 *
 * @param measurement_history Array of past measurement reports (newest last).
 * @param num_reports         Number of reports in history.
 * @param prediction_horizon_ms How far ahead to predict.
 * @return Predicted best cell PCI.
 */
uint32_t ho_predict_next_cell(const MeasurementReport *measurement_history,
                              int                       num_reports,
                              double                    prediction_horizon_ms);

/* ============================================================================
 * L7: Energy-Efficient Handover
 * ============================================================================ */

/**
 * ho_energy_efficient_decision - Energy-aware handover decision.
 *
 * For IoT and battery-constrained devices, handover decisions should consider
 * energy cost. This function balances signal quality improvement against the
 * energy cost of performing a handover.
 *
 * Handover IF: E_quality_gain > E_handover_cost
 *
 * E_quality_gain: Energy saved by better link (lower Tx power for same QoS)
 * E_handover_cost: Energy consumed by signaling + RACH procedure
 *
 * @param rsrp_gain_db        Expected RSRP improvement.
 * @param tx_power_alpha      Power amplifier efficiency factor.
 * @param ue_battery_pct       Current battery level [0, 100].
 * @param signaling_cost_j    Estimated handover signaling energy (J).
 * @param low_battery_threshold Battery threshold for conservative mode.
 * @return true if handover is energy-beneficial.
 */
bool ho_energy_efficient_decision(double rsrp_gain_db,
                                  double tx_power_alpha,
                                  double ue_battery_pct,
                                  double signaling_cost_j,
                                  double low_battery_threshold);

/* ============================================================================
 * L7: Admission Control for Handover
 * ============================================================================ */

/**
 * ho_admission_control - Target cell admission control.
 *
 * Before executing handover, the target cell must verify it can accommodate
 * the incoming UE without degrading existing users' QoS below acceptable levels.
 *
 * Admission criteria:
 *   1. Available PRBs ≥ PRBs required by UE
 *   2. Post-admission PRB utilization ≤ admission_threshold
 *   3. Post-admission average SINR degradation ≤ max_sinr_degradation_db
 *
 * @param target_load          Current target cell load info.
 * @param ue_required_prbs     PRBs required for the incoming UE.
 * @param admission_threshold   Max PRB utilization after admission [0, 1].
 * @param ue_estimated_sinr_db Estimated SINR at target.
 * @param max_sinr_degradation_db Max allowable SINR degradation for existing UEs.
 * @return true if UE can be admitted.
 */
bool ho_admission_control(const CellLoadInfo *target_load,
                          int                  ue_required_prbs,
                          double               admission_threshold,
                          double               ue_estimated_sinr_db,
                          double               max_sinr_degradation_db);

#endif /* HANDOVER_OPTIMIZE_H */
