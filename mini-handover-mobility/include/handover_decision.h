/**
 * @file handover_decision.h
 * @brief Handover decision algorithms (L2 Core Concepts + L5 Algorithms)
 *
 * Implements the fundamental handover decision logic used in 3GPP LTE/5G NR
 * and IEEE 802.11 wireless systems. Each function encapsulates an independent
 * decision algorithm that determines whether and where to handover.
 *
 * Knowledge Coverage:
 *   L2 - Core decision concepts: A3/A5 events, hysteresis, TTT, ping-pong avoidance
 *   L5 - Algorithm implementations: event-based, RSSI-based, SINR-based,
 *        weighted sum, TOPSIS multi-criteria decision
 *
 * References:
 *   - 3GPP TS 36.331 §5.5.4 (Measurement report triggering)
 *   - 3GPP TS 38.331 §5.5.4 (NR measurement report triggering)
 *   - Molisch, "Wireless Communications" (2011), Ch. 17-18
 *   - Holma & Toskala, "LTE for UMTS" (2009), Ch. 9
 */

#ifndef HANDOVER_DECISION_H
#define HANDOVER_DECISION_H

#include "handover_types.h"

/* ============================================================================
 * L4: Fundamental Law — Hysteresis Decision Model
 * ============================================================================ */

/**
 * ho_decision_hysteresis - Perform handover decision with hysteresis.
 *
 * This implements the fundamental hysteresis-based handover model:
 *   Handover IF: RSRP_target - RSRP_serving > hysteresis
 *
 * The hysteresis margin prevents the "ping-pong" effect where a UE oscillates
 * between two cells at the cell boundary. This is the most basic handover
 * decision algorithm and forms the foundation for 3GPP event-based triggers.
 *
 * Theorem (Ping-Pong Probability Bound):
 *   With hysteresis H (dB) and shadow fading std σ (dB), the probability of
 *   ping-pong within time T is bounded by:
 *     P(pingpong) ≤ exp(-H²/(2σ²)) · T/T_correlation
 *
 * @param serving_rsrp_dbm    Serving cell RSRP in dBm.
 * @param target_rsrp_dbm     Target cell RSRP in dBm.
 * @param hysteresis_db       Hysteresis margin in dB (typical: 2-6 dB).
 * @return true if target is better than serving plus hysteresis.
 */
bool ho_decision_hysteresis(double serving_rsrp_dbm,
                            double target_rsrp_dbm,
                            double hysteresis_db);

/* ============================================================================
 * L5: 3GPP Event A3 — Neighbour becomes offset better than serving
 * ============================================================================ */

/**
 * ho_decision_event_a3 - 3GPP LTE/NR Event A3 handover decision.
 *
 * Event A3 (3GPP TS 36.331 §5.5.4.4):
 *   Entering condition: Mn + Ofn + Ocn - Hys > Ms + Ofs + Ocs + Off
 *   Leaving condition:  Mn + Ofn + Ocn + Hys < Ms + Ofs + Ocs + Off
 *
 * Where:
 *   Mn = neighbour cell RSRP
 *   Ofn = frequency-specific offset for neighbour
 *   Ocn = cell individual offset for neighbour (CIO)
 *   Ms = serving cell RSRP
 *   Ofs = frequency-specific offset for serving
 *   Ocs = cell individual offset for serving
 *   Hys = hysteresis
 *   Off = A3 offset parameter
 *
 * This is the primary intra-frequency handover trigger in LTE and 5G NR.
 *
 * @param serving_rsrp_dbm   Serving cell RSRP in dBm.
 * @param neighbour_rsrp_dbm Neighbour cell RSRP in dBm.
 * @param a3_offset_db       A3 offset (Off parameter).
 * @param hysteresis_db      Hysteresis value.
 * @param serving_cio_db     Cell Individual Offset for serving cell.
 * @param neighbour_cio_db   Cell Individual Offset for neighbour cell.
 * @param freq_offset_db     Frequency-specific offset.
 * @param[out] entry_condition True if entering condition is met.
 * @param[out] leaving_condition True if leaving condition is met.
 */
void ho_decision_event_a3(double serving_rsrp_dbm,
                          double neighbour_rsrp_dbm,
                          double a3_offset_db,
                          double hysteresis_db,
                          double serving_cio_db,
                          double neighbour_cio_db,
                          double freq_offset_db,
                          bool  *entry_condition,
                          bool  *leaving_condition);

/* ============================================================================
 * L5: 3GPP Event A5 — Serving worse than T1 AND neighbour better than T2
 * ============================================================================ */

/**
 * ho_decision_event_a5 - 3GPP LTE/NR Event A5 handover decision.
 *
 * Event A5 (3GPP TS 36.331 §5.5.4.6):
 *   Entering: Ms + Hys < Threshold1  AND  Mn + Ofn + Ocn - Hys > Threshold2
 *   Leaving:  Ms - Hys > Threshold1  OR   Mn + Ofn + Ocn + Hys < Threshold2
 *
 * A5 is used when the serving cell becomes too weak regardless of neighbour,
 * typically for coverage-based handover.
 *
 * @param serving_rsrp_dbm    Serving cell RSRP.
 * @param neighbour_rsrp_dbm  Neighbour cell RSRP.
 * @param threshold1_db       Serving cell absolute threshold.
 * @param threshold2_db       Neighbour cell absolute threshold.
 * @param hysteresis_db       Hysteresis.
 * @param neighbour_cio_db    Neighbour cell individual offset.
 * @param freq_offset_db      Frequency-specific offset.
 * @param[out] entry_condition  True if entering condition met.
 * @param[out] leaving_condition True if leaving condition met.
 */
void ho_decision_event_a5(double serving_rsrp_dbm,
                          double neighbour_rsrp_dbm,
                          double threshold1_db,
                          double threshold2_db,
                          double hysteresis_db,
                          double neighbour_cio_db,
                          double freq_offset_db,
                          bool  *entry_condition,
                          bool  *leaving_condition);

/* ============================================================================
 * L5: RSSI-Based Handover Decision
 * ============================================================================ */

/**
 * ho_decision_rssi_threshold - RSSI threshold-based handover decision.
 *
 * Simple handover triggered when RSSI drops below an absolute threshold.
 * Commonly used in WiFi roaming (802.11k/v) and early cellular systems.
 *
 * Handover IF: RSSI_serving < rssi_threshold_dbm AND RSSI_target > RSSI_serving
 *
 * @param serving_rssi_dbm    Serving AP/BTS RSSI.
 * @param target_rssi_dbm     Target AP/BTS RSSI.
 * @param rssi_threshold_dbm  Minimum acceptable RSSI threshold.
 * @param min_improvement_db  Minimum RSSI improvement required (avoids ping-pong).
 * @return true if handover should be triggered.
 */
bool ho_decision_rssi_threshold(double serving_rssi_dbm,
                                double target_rssi_dbm,
                                double rssi_threshold_dbm,
                                double min_improvement_db);

/* ============================================================================
 * L5: SINR-Based Handover Decision
 * ============================================================================ */

/**
 * ho_decision_sinr - SINR-based handover decision.
 *
 * Triggers handover when the serving cell SINR is below threshold and a
 * neighbour cell offers better SINR. This is more accurate than RSSI-based
 * decision because it accounts for interference.
 *
 * Handover IF: SINR_serving < sinr_threshold_db
 *              AND SINR_target > SINR_serving + margin_db
 *
 * @param serving_sinr_db    Serving cell SINR in dB.
 * @param target_sinr_db     Target cell SINR in dB.
 * @param sinr_threshold_db  Minimum acceptable SINR.
 * @param margin_db          Required SINR improvement margin.
 * @return true if handover should be triggered.
 */
bool ho_decision_sinr(double serving_sinr_db,
                      double target_sinr_db,
                      double sinr_threshold_db,
                      double margin_db);

/* ============================================================================
 * L5: TTT (Time-To-Trigger) Mechanism
 * ============================================================================ */

/**
 * ho_decision_ttt_evaluate - Time-To-Trigger evaluation.
 *
 * TTT is the duration for which the triggering condition must persist before
 * the handover is actually executed. This reduces unnecessary handovers due
 * to temporary signal fluctuations (fast fading).
 *
 * The TTT mechanism works as a sliding window counter:
 *   For each measurement period, check if condition is met.
 *   If condition is continuously met for TTT_duration/measurement_period samples,
 *   then trigger handover.
 *
 * @param condition_history    Array of boolean condition results, newest first.
 * @param history_length       Number of samples in history.
 * @param required_samples     Minimum consecutive TRUE samples (TTT/sampling_period).
 * @return true if TTT condition is satisfied (continuous triggering).
 */
bool ho_decision_ttt_evaluate(const bool condition_history[],
                              int   history_length,
                              int   required_samples);

/* ============================================================================
 * L5: Ping-Pong Handover Detection
 * ============================================================================ */

/**
 * ho_detect_pingpong - Detect ping-pong handover pattern.
 *
 * A ping-pong handover occurs when a UE repeatedly hands over between two cells
 * within a short time window. This is caused by insufficient hysteresis margins
 * and leads to degraded user experience and increased signaling load.
 *
 * Detection criterion (3GPP TS 32.425 definition):
 *   Handover from cell A to B, then back to A within T_pingpong (e.g., 5 seconds).
 *
 * @param handover_history_pci  Array of past handover target cell PCIs.
 * @param handover_history_time Array of past handover timestamps (ms).
 * @param history_count         Number of entries in history.
 * @param pingpong_window_ms    Maximum time window for ping-pong detection.
 * @return true if a ping-pong pattern is detected.
 */
bool ho_detect_pingpong(const uint32_t handover_history_pci[],
                        const double   handover_history_time[],
                        int            history_count,
                        double         pingpong_window_ms);

/* ============================================================================
 * L5: Weighted Sum Multi-Criteria Decision (WSM)
 * ============================================================================ */

/**
 * ho_decision_weighted_sum - Weighted sum model for multi-criteria handover.
 *
 * Used primarily for vertical handover (inter-RAT) decisions where multiple
 * attributes must be considered simultaneously:
 *   Score = Σ (w_i · normalized_attribute_i)
 *
 * Attributes typically include: RSSI, bandwidth, cost, power consumption,
 * latency, security, and network load.
 *
 * Mathematical basis: multi-attribute utility theory (MAUT).
 *
 * @param attributes          Matrix of attribute values [num_candidates][num_attrs].
 * @param weights             Weight vector [num_attrs] (sum = 1.0).
 * @param benefit_direction   Is higher better? (true=benefit, false=cost).
 * @param num_candidates      Number of candidate networks.
 * @param num_attributes      Number of decision attributes.
 * @param[out] scores         Computed scores for each candidate.
 * @param[out] best_candidate  Index of best candidate (max score).
 */
void ho_decision_weighted_sum(const double *attributes,
                              const double *weights,
                              const bool   *benefit_direction,
                              int           num_candidates,
                              int           num_attributes,
                              double       *scores,
                              int          *best_candidate);

/* ============================================================================
 * L5: TOPSIS (Technique for Order Preference by Similarity to Ideal Solution)
 * ============================================================================ */

/**
 * ho_decision_topsis - TOPSIS multi-criteria handover decision.
 *
 * TOPSIS (Hwang & Yoon, 1981): ranks alternatives based on their distance
 * from the ideal best and ideal worst solutions. Widely used in VHO research.
 *
 * Steps:
 *   1. Normalize decision matrix
 *   2. Weight normalized matrix
 *   3. Determine ideal best (A*) and ideal worst (A-) solutions
 *   4. Calculate Euclidean distance to A* and A-
 *   5. Compute closeness coefficient: C_i = S_i- / (S_i+ + S_i-)
 *   6. Rank by C_i (higher = better)
 *
 * @param decision_matrix     [num_candidates × num_criteria] matrix.
 * @param weights             Criterion weights [num_criteria].
 * @param is_benefit          Benefit (true) or cost (false) per criterion.
 * @param num_candidates      Number of candidate networks.
 * @param num_criteria        Number of decision criteria.
 * @param[out] closeness_coeff Closeness coefficient per candidate.
 * @param[out] best_index      Index of candidate with highest closeness.
 */
void ho_decision_topsis(const double *decision_matrix,
                        const double *weights,
                        const bool   *is_benefit,
                        int           num_candidates,
                        int           num_criteria,
                        double       *closeness_coeff,
                        int          *best_index);

/* ============================================================================
 * L5: GRA (Grey Relational Analysis) for Handover
 * ============================================================================ */

/**
 * ho_decision_gra - Grey Relational Analysis for handover network selection.
 *
 * GRA (Deng, 1982) is a multi-criteria decision method based on grey system
 * theory. It measures the degree of correlation between candidate sequences
 * and a reference (ideal) sequence. Used in heterogeneous network selection.
 *
 * Grey relational grade:
 *   Γ_i = (1/n) · Σ γ_i(k), where γ_i(k) is the grey relational coefficient.
 *
 * @param decision_matrix  [num_candidates × num_criteria] matrix.
 * @param reference        Ideal reference sequence [num_criteria].
 * @param distinguishing_coeff ρ in (0, 1], typically 0.5.
 * @param num_candidates   Number of candidates.
 * @param num_criteria     Number of criteria.
 * @param[out] grades      Grey relational grade per candidate [0, 1].
 * @param[out] best_index  Index of best candidate.
 */
void ho_decision_gra(const double *decision_matrix,
                     const double *reference,
                     double        distinguishing_coeff,
                     int           num_candidates,
                     int           num_criteria,
                     double       *grades,
                     int          *best_index);

#endif /* HANDOVER_DECISION_H */
