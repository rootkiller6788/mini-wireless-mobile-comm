/**
 * @file handover_optimize.c
 * @brief Handover parameter optimization and advanced techniques (L5, L7, L8)
 *
 * Implements optimization algorithms for handover parameters and
 * advanced handover mechanisms defined in 3GPP Release 15-17.
 *
 * References:
 *   - 3GPP TR 38.300 §9.2.3 (NR Mobility)
 *   - 3GPP TS 38.331 §5.3.5 (Conditional Handover, Reconfiguration With Sync)
 *   - 3GPP TS 36.902 (SON: Self-Organizing Networks)
 *   - 3GPP TR 37.816 (Study on RAN-centric data collection)
 */

#include "handover_optimize.h"
#include "handover_decision.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L5: Hysteresis Margin Optimization
 *
 * This function finds the hysteresis value that minimizes a weighted cost
 * function combining handover failure probability and ping-pong probability.
 *
 * Mathematical formulation:
 *
 * The RLF probability as a function of hysteresis H:
 *   P_RLF(H) = Q((H - Δ_avg) / (σ_Δ))
 *
 * where Δ_avg is the average RSRP difference between cells, σ_Δ is the
 * standard deviation of the difference. Q(x) is the Q-function (tail
 * probability of standard normal).
 *
 * The ping-pong probability:
 *   P_pingpong(H) ≈ exp(-H² / (2·σ_shadow²))
 *
 * This is derived from the probability that two consecutive RSRP measurements
 * cross the hysteresis threshold in opposite directions (a "level crossing"
 * followed by a reverse crossing within the correlation time).
 *
 * Cost function:
 *   J(H) = w_RLF · P_RLF(H) + w_pingpong · P_pingpong(H)
 *
 * Optimization: Grid search over [H_min, H_max] with step size.
 * In production (SON), this would use gradient descent or Bayesian optimization.
 *
 * Q-function approximation (Abramowitz & Stegun 26.2.14):
 *   Q(x) ≈ (1/√(2π)) · (1/(a₁x + a₂x²)) · exp(-x²/2)
 *   with a₁=0.33267, a₂=0.12017 (error < 1e-5 for x > 0)
 *
 * Reference: Pollini (1996), "Trends in Handover Design", IEEE Comm. Mag.
 */
double ho_optimize_hysteresis(double shadow_fading_std_db,
                              double weight_rlf,
                              double weight_pingpong,
                              double min_hysteresis_db,
                              double max_hysteresis_db,
                              double step_db)
{
    if (shadow_fading_std_db <= 0.0) shadow_fading_std_db = 4.0; /* Default 4 dB */
    if (weight_rlf < 0.0) weight_rlf = 0.0;
    if (weight_pingpong < 0.0) weight_pingpong = 0.0;

    /* Normalize weights */
    double w_sum = weight_rlf + weight_pingpong;
    if (w_sum < 1e-12) {
        weight_rlf = 0.5;
        weight_pingpong = 0.5;
        w_sum = 1.0;
    }
    weight_rlf /= w_sum;
    weight_pingpong /= w_sum;

    if (min_hysteresis_db < 0.0) min_hysteresis_db = 0.0;
    if (max_hysteresis_db < min_hysteresis_db) max_hysteresis_db = min_hysteresis_db + 6.0;
    if (step_db <= 0.0) step_db = 0.1;

    double best_h = min_hysteresis_db;
    double best_cost = 1e100;

    for (double h = min_hysteresis_db; h <= max_hysteresis_db + 1e-9; h += step_db) {
        /* Approximate Q-function for RLF probability
         * Assume Δ_avg = H/2 (average difference scales with hysteresis) */
        double x = h / (shadow_fading_std_db * 1.4142); /* σ_Δ = σ·√2 */
        double q_value;

        if (x > 0.0) {
            /* Use the simpler bound: Q(x) ≈ 0.5·exp(-x²/2) / (1 + x) for x > 0 */
            q_value = 0.5 * exp(-x * x / 2.0) / (1.0 + x);
            /* Alternating signs: Abramowitz approximation */
            double t = 1.0 / (1.0 + 0.33267 * x);
            double q_abram = 0.5 * (1.0 - (0.17401 * t - 0.04752 * t * t + 0.03694 * t * t * t)
                            * exp(-x * x / 2.0));
            q_value = q_abram;
        } else {
            q_value = 0.5; /* Worst case at no hysteresis */
        }

        if (q_value < 0.0) q_value = 0.0;
        if (q_value > 1.0) q_value = 1.0;

        /* Ping-pong probability bound */
        double pp_prob = exp(-(h * h) / (2.0 * shadow_fading_std_db * shadow_fading_std_db));
        if (pp_prob > 1.0) pp_prob = 1.0;
        if (pp_prob < 0.0) pp_prob = 0.0;

        /* Weighted cost */
        double cost = weight_rlf * q_value + weight_pingpong * pp_prob;

        if (cost < best_cost) {
            best_cost = cost;
            best_h = h;
        }
    }

    return best_h;
}

/* ============================================================================
 * L5: TTT Optimization
 *
 * TTT optimization uses level-crossing rate (LCR) theory (Rice, 1944).
 *
 * For a stationary Gaussian process (shadow fading), the expected rate
 * at which the signal crosses a given threshold level R is:
 *
 *   N(R) = N_m · exp(-(R - μ)²/(2σ²))
 *
 * where N_m = √(-ρ″(0))/(2π) · (σ/√(2π)) is the maximum crossing rate,
 * and ρ″(0) is the second derivative of the autocorrelation at τ=0.
 *
 * For exponential correlation ρ(τ) = exp(-v·τ/d_corr):
 *   -ρ″(0) = (v/d_corr)²
 *   N_m = (v / d_corr) / (2π · √(2)) ≈ v / (8.89·d_corr)
 *
 * Average fade duration below R:
 *   AFD(R) = P(RSRP < R) / N(R)
 *
 * The TTT should exceed the typical fade duration by a safety factor:
 *   TTT_opt = k_safety · AFD(R_handover)
 *
 * At the handover threshold, P(RSRP < R) ≈ 0.5 (assuming R is near the
 * mean of the serving cell at the cell boundary).
 *
 * References:
 *   - Rice (1944), Bell System Technical Journal
 *   - TTT values in 3GPP TS 36.331
 */
double ho_optimize_ttt(double speed_mps,
                       double carrier_freq_hz,
                       double shadow_fading_std_db,
                       double correlation_distance_m,
                       double safety_factor)
{
    (void)carrier_freq_hz; /* Reserved for frequency-dependent TTT scaling */
    if (speed_mps < 0.01) speed_mps = 0.01;
    if (correlation_distance_m < 1.0) correlation_distance_m = 1.0;
    if (safety_factor < 1.0) safety_factor = 1.0;
    if (safety_factor > 10.0) safety_factor = 10.0;

    /* Maximum level crossing rate */
    double nm = speed_mps / (2.0 * M_PI * correlation_distance_m);

    /* Average fade duration at threshold (assume ~3 dB below mean at cell edge) */
    /* P(shadow < -3 dB) = Q(3/σ) — use Q-function approximation */
    double x_thresh = 3.0 / shadow_fading_std_db;
    double prob_below;
    {
        double t = 1.0 / (1.0 + 0.33267 * x_thresh);
        prob_below = 0.5 * (1.0 - (0.17401 * t - 0.04752 * t * t + 0.03694 * t * t * t)
                     * exp(-x_thresh * x_thresh / 2.0));
    }

    /* Level crossing rate at threshold
     * N(R) = N_m · exp(-(R-μ)²/(2σ²))
     * At 3 dB below mean: N(R) = N_m · exp(-3²/(2·σ²)) */
    double level_crossing_rate = nm * exp(-(3.0 * 3.0)
                                / (2.0 * shadow_fading_std_db * shadow_fading_std_db));

    if (level_crossing_rate < 1e-12) level_crossing_rate = 1e-12;

    /* AFD = P(below) / N(R) */
    double afd_seconds = prob_below / level_crossing_rate;

    /* Optimal TTT = safety_factor · AFD, converted to ms */
    double ttt_ms = safety_factor * afd_seconds * 1000.0;

    /* Clamp to practical range */
    if (ttt_ms < 40.0)  ttt_ms = 40.0;
    if (ttt_ms > 5120.0) ttt_ms = 5120.0;

    return ttt_ms;
}

/* ============================================================================
 * L5: Cell Individual Offset (CIO) Optimization for Load Balancing
 *
 * CIO adjustment is part of 3GPP Mobility Load Balancing (MLB) in SON.
 * The algorithm adjusts CIO proportionally to the load difference between
 * cells to shift traffic from overloaded to underloaded cells.
 *
 * Mathematical formulation (proportional control):
 *   CIO[t+1] = CIO[t] + K_p · (L_target - L_serving)
 *
 * where K_p is the proportional gain (step_size_db) and L is the PRB
 * utilization ratio.
 *
 * Stability condition: K_p must be small enough to avoid oscillation.
 * For N cells in a cluster, stability requires K_p < 2/N.
 *
 * Reference: 3GPP TS 32.522, "SON Policy Network Resource Model"
 */
double ho_optimize_cio(double current_cio_db,
                       double load_serving,
                       double load_target,
                       double step_size_db,
                       double max_cio_db)
{
    /* Clamp loads to [0, 1] */
    if (load_serving < 0.0) load_serving = 0.0;
    if (load_serving > 1.0) load_serving = 1.0;
    if (load_target < 0.0)  load_target  = 0.0;
    if (load_target > 1.0)  load_target  = 1.0;

    /* Load difference: positive = target is more loaded */
    double load_diff = load_target - load_serving;

    /* P-controller: reduce CIO if target is more loaded, increase if less */
    double new_cio = current_cio_db - step_size_db * load_diff;

    /* Clamp to [-max_cio_db, max_cio_db] */
    if (new_cio > max_cio_db)   new_cio = max_cio_db;
    if (new_cio < -max_cio_db)  new_cio = -max_cio_db;

    return new_cio;
}

/* ============================================================================
 * L8: Conditional Handover (CHO) — 3GPP Rel-16
 *
 * Conditional Handover (3GPP TR 38.300 §9.2.3.2) is a major enhancement
 * in NR mobility. Unlike traditional handover where preparation and execution
 * are tightly coupled, CHO decouples them:
 *
 *   1. Source gNB prepares handover to one or more candidate target cells
 *   2. RRC Reconfiguration (with sync) is sent to UE with CHO conditions
 *   3. UE stores the configuration but does NOT immediately execute
 *   4. UE monitors the configured CHO execution condition
 *   5. When condition is met, UE autonomously applies the stored configuration
 *
 * Key benefit: Handover preparation occurs while the source link is still
 * strong, significantly reducing handover failure probability at cell edge.
 *
 * CHO execution condition (3GPP TS 38.331):
 *   Event A3: Mn + Ofn + Ocn - Hys > Ms + Ofs + Ocs + Off
 *   Event A5: Ms + Hys < Thresh1 AND Mn + Ofn + Ocn - Hys > Thresh2
 */
bool ho_conditional_evaluate(double serving_rsrp_dbm,
                             double target_rsrp_dbm,
                             double serving_threshold_dbm,
                             double target_threshold_dbm,
                             bool   ttt_condition_met)
{
    /* Serving cell must be weak enough to trigger CHO execution */
    bool serving_weak = (serving_rsrp_dbm < serving_threshold_dbm);

    /* Target cell must be strong enough */
    bool target_strong = (target_rsrp_dbm > target_threshold_dbm);

    /* TTT condition must be satisfied (prevents false triggers) */
    return serving_weak && target_strong && ttt_condition_met;
}

void ho_conditional_prepare(const MeasurementReport *report,
                            double                   min_rsrp_dbm,
                            int                      max_candidates,
                            uint32_t                *candidate_pcis,
                            int                     *num_candidates)
{
    if (!report || !candidate_pcis || !num_candidates) return;

    *num_candidates = 0;

    /* Sort neighbour cells by RSRP and select top N that exceed threshold */
    /* Simplified: linear scan with insertion into sorted list */
    typedef struct {
        uint32_t pci;
        double   rsrp;
    } CandidatePair;

    CandidatePair sorted[MAX_MEAS_CELLS];
    int sorted_count = 0;

    for (uint32_t i = 0; i < report->num_neighbour_cells && i < MAX_MEAS_CELLS; i++) {
        if (report->neighbour_meas[i].rsrp_dbm >= min_rsrp_dbm) {
            /* Insert in descending RSRP order */
            CandidatePair cp = {report->neighbour_cell_ids[i],
                                report->neighbour_meas[i].rsrp_dbm};
            int pos = sorted_count;
            while (pos > 0 && sorted[pos - 1].rsrp < cp.rsrp) {
                sorted[pos] = sorted[pos - 1];
                pos--;
            }
            sorted[pos] = cp;
            sorted_count++;
            if (sorted_count > MAX_MEAS_CELLS) sorted_count = MAX_MEAS_CELLS;
        }
    }

    *num_candidates = (sorted_count < max_candidates) ? sorted_count : max_candidates;
    for (int i = 0; i < *num_candidates; i++) {
        candidate_pcis[i] = sorted[i].pci;
    }
}

/* ============================================================================
 * L8: DAPS Handover — 3GPP Rel-16
 *
 * DAPS (Dual Active Protocol Stack) handover maintains simultaneous user
 * plane connectivity with both source and target cells during handover
 * execution. The UE has two complete protocol stacks active in parallel.
 *
 * This achieves 0 ms user plane interruption — a critical requirement for
 * URLLC (Ultra-Reliable Low-Latency Communications) services.
 *
 * DAPS operation:
 *   1. UE receives DL data from both source and target
 *   2. UE transmits UL data only to source until RACH to target succeeds
 *   3. After RACH success, UE switches UL to target
 *   4. Source link is released after target is fully established
 *
 * Feasibility constraints:
 *   - UE must support DAPS (capability signaling)
 *   - Target must have sufficient resources
 *   - Source must support PDCP duplication/forwarding during transition
 */
bool ho_daps_evaluate(bool ue_daps_capable,
                      bool target_daps_capable,
                      int  target_prb_available,
                      int  required_prbs)
{
    /* All three conditions must be satisfied */
    if (!ue_daps_capable)    return false;
    if (!target_daps_capable) return false;
    if (target_prb_available < required_prbs) return false;

    /* DAPS requires at least 2 PRBs for SRB (signaling) + data */
    if (required_prbs < 2) return false;

    return true;
}

/* ============================================================================
 * L7: Mobility Robustness Optimization (MRO) — 3GPP SON
 *
 * MRO (3GPP TS 38.300 §15.3) automatically detects and corrects handover
 * parameter misconfigurations. This is a core SON (Self-Organizing Networks)
 * function in LTE and 5G NR.
 *
 * Handover problem classification:
 *
 * Type 0: No problem detected
 *
 * Type 1: Too-late handover
 *   Symptom: RLF occurs in source cell before handover is triggered
 *   Root cause: Handover trigger conditions too conservative
 *   Correction: Decrease TTT (faster trigger) and/or decrease A3 offset
 *
 * Type 2: Too-early handover
 *   Symptom: RLF occurs in target cell shortly after handover, UE
 *            re-establishes in source cell
 *   Root cause: Handover triggered prematurely (insufficient TTT/margin)
 *   Correction: Increase TTT and/or increase CIO
 *
 * Type 3: Handover to wrong cell
 *   Symptom: RLF occurs in target cell, UE re-establishes in a third cell
 *   Root cause: Cell boundary geometry or incorrect CIO configuration
 *   Correction: Adjust CIO for both target and correct cell
 *
 * Reference: 3GPP TS 36.902 §4.4 (SON for LTE)
 */
int ho_mro_diagnose(bool rlf_occurred_before_ho,
                    bool rlf_occurred_after_ho,
                    bool reconnected_to_source,
                    bool reconnected_to_other)
{
    if (rlf_occurred_before_ho) {
        /* RLF happened while still connected to source → too-late HO */
        return 1;
    }

    if (rlf_occurred_after_ho) {
        if (reconnected_to_source) {
            /* RLF in target, reconnected to source → too-early HO */
            return 2;
        } else if (reconnected_to_other) {
            /* RLF in target, reconnected to different cell → wrong cell */
            return 3;
        }
    }

    /* No problem detected */
    return 0;
}

void ho_mro_correct(int    problem_type,
                    double current_ttt_ms,
                    double current_a3_offset_db,
                    double current_cio_db,
                    double *new_ttt_ms,
                    double *new_a3_offset_db,
                    double *new_cio_db)
{
    if (!new_ttt_ms || !new_a3_offset_db || !new_cio_db) return;

    *new_ttt_ms = current_ttt_ms;
    *new_a3_offset_db = current_a3_offset_db;
    *new_cio_db = current_cio_db;

    switch (problem_type) {
        case 1: /* Too-late HO */
            /* Reduce TTT by one step (3GPP defined TTT steps) */
            *new_ttt_ms = current_ttt_ms * 0.5;
            /* Increase A3 offset to trigger sooner (more aggressive) */
            *new_a3_offset_db = current_a3_offset_db - 1.0;
            break;

        case 2: /* Too-early HO */
            /* Increase TTT by one step */
            *new_ttt_ms = current_ttt_ms * 2.0;
            /* Decrease CIO to target (make it less attractive) */
            *new_cio_db = current_cio_db - 1.0;
            break;

        case 3: /* Wrong cell HO */
            /* Reduce CIO for wrong target, increase for correct cell */
            *new_cio_db = current_cio_db - 2.0;
            /* Also increase TTT slightly for more reliable measurement */
            *new_ttt_ms = current_ttt_ms * 1.5;
            break;

        default:
            /* No correction needed */
            break;
    }

    /* Clamp to reasonable ranges */
    if (*new_ttt_ms < 40.0)   *new_ttt_ms = 40.0;
    if (*new_ttt_ms > 5120.0) *new_ttt_ms = 5120.0;
    if (*new_a3_offset_db < -30.0) *new_a3_offset_db = -30.0;
    if (*new_a3_offset_db > 30.0)  *new_a3_offset_db = 30.0;
    if (*new_cio_db < -24.0)  *new_cio_db = -24.0;
    if (*new_cio_db > 24.0)   *new_cio_db = 24.0;
}

/* ============================================================================
 * L8: ML-Based Next Cell Prediction
 *
 * This function implements a lightweight trend-based prediction of the
 * next best serving cell. It uses linear regression on RSRP measurements
 * over time to extrapolate which cell will have the strongest RSRP at
 * a future time horizon.
 *
 * Model: For each cell c, fit RSRP_c(t) = a_c + b_c · t
 *   Using least squares on the last N measurements:
 *     b_c = Σ((t_i - t̄)(RSRP_i - RSRP̄)) / Σ(t_i - t̄)²
 *     a_c = RSRP̄ - b_c · t̄
 *
 * Then predict RSRP at t + Δt:
 *   RSRP̂_c(t + Δt) = a_c + b_c · (t_last + Δt)
 *
 * Select the cell with maximum predicted RSRP.
 *
 * This is a simplified approach compared to deep learning (LSTM, Transformer)
 * approaches in research, but is computationally feasible for on-device
 * implementation and captures the essential trend information.
 */
uint32_t ho_predict_next_cell(const MeasurementReport *measurement_history,
                              int                       num_reports,
                              double                    prediction_horizon_ms)
{
    if (!measurement_history || num_reports < 2) {
        /* Insufficient data: return current strongest neighbour */
        if (measurement_history && measurement_history->num_neighbour_cells > 0) {
            return measurement_history->neighbour_cell_ids[0];
        }
        return 0;
    }

    /* Collect all unique cell IDs across history */
    #define MAX_UNIQUE_CELLS 16
    uint32_t unique_pcis[MAX_UNIQUE_CELLS];
    int unique_count = 0;

    for (int r = 0; r < num_reports; r++) {
        /* Add serving cell */
        bool found = false;
        for (int u = 0; u < unique_count; u++) {
            if (unique_pcis[u] == measurement_history[r].serving_cell_id) {
                found = true;
                break;
            }
        }
        if (!found && unique_count < MAX_UNIQUE_CELLS) {
            unique_pcis[unique_count++] = measurement_history[r].serving_cell_id;
        }

        /* Add neighbour cells */
        for (uint32_t n = 0; n < measurement_history[r].num_neighbour_cells; n++) {
            found = false;
            for (int u = 0; u < unique_count; u++) {
                if (unique_pcis[u] == measurement_history[r].neighbour_cell_ids[n]) {
                    found = true;
                    break;
                }
            }
            if (!found && unique_count < MAX_UNIQUE_CELLS) {
                unique_pcis[unique_count++] = measurement_history[r].neighbour_cell_ids[n];
            }
        }
    }

    /* For each unique cell, perform linear regression on RSRP over time */
    double best_predicted_rsrp = -200.0;
    uint32_t best_pci = measurement_history[num_reports - 1].serving_cell_id;

    /* Compute time statistics */
    double t_start = measurement_history[0].timestamp_ms;
    double t_end   = measurement_history[num_reports - 1].timestamp_ms;
    double t_mean  = 0.0;
    for (int r = 0; r < num_reports; r++) {
        t_mean += (measurement_history[r].timestamp_ms - t_start);
    }
    t_mean /= num_reports;

    for (int u = 0; u < unique_count; u++) {
        double rsrp_sum = 0.0;
        double rsrp_time_sum = 0.0;
        int    count = 0;

        for (int r = 0; r < num_reports; r++) {
            double rsrp = -200.0;
            double t = measurement_history[r].timestamp_ms - t_start;

            if (measurement_history[r].serving_cell_id == unique_pcis[u]) {
                rsrp = measurement_history[r].serving_meas.rsrp_dbm;
            } else {
                for (uint32_t n = 0; n < measurement_history[r].num_neighbour_cells; n++) {
                    if (measurement_history[r].neighbour_cell_ids[n] == unique_pcis[u]) {
                        rsrp = measurement_history[r].neighbour_meas[n].rsrp_dbm;
                        break;
                    }
                }
            }

            if (rsrp > -199.0) {
                rsrp_sum += rsrp;
                rsrp_time_sum += t;
                count++;
            }
        }

        if (count < 2) continue;

        double rsrp_mean = rsrp_sum / count;
        double time_mean_cell = rsrp_time_sum / count;

        /* Compute slope b = Σ(t_i - t̄)(rsrp_i - rsrp̄) / Σ(t_i - t̄)² */
        double num = 0.0, den = 0.0;
        for (int r = 0; r < num_reports; r++) {
            double rsrp = -200.0;
            double t = measurement_history[r].timestamp_ms - t_start;

            if (measurement_history[r].serving_cell_id == unique_pcis[u]) {
                rsrp = measurement_history[r].serving_meas.rsrp_dbm;
            } else {
                for (uint32_t n = 0; n < measurement_history[r].num_neighbour_cells; n++) {
                    if (measurement_history[r].neighbour_cell_ids[n] == unique_pcis[u]) {
                        rsrp = measurement_history[r].neighbour_meas[n].rsrp_dbm;
                        break;
                    }
                }
            }

            if (rsrp > -199.0) {
                double dt = t - time_mean_cell;
                double dr = rsrp - rsrp_mean;
                num += dt * dr;
                den += dt * dt;
            }
        }

        double slope = (fabs(den) > 1e-12) ? (num / den) : 0.0;

        /* Predict: RSRP at t_end + prediction_horizon */
        double t_predict = (t_end - t_start) + prediction_horizon_ms;
        double predicted_rsrp = rsrp_mean + slope * (t_predict - time_mean_cell);

        if (predicted_rsrp > best_predicted_rsrp) {
            best_predicted_rsrp = predicted_rsrp;
            best_pci = unique_pcis[u];
        }
    }

    return best_pci;
}

/* ============================================================================
 * L7: Energy-Efficient Handover Decision
 *
 * For battery-constrained IoT devices (NB-IoT, LTE-M), handover decisions
 * must consider energy cost. A handover consumes energy for:
 *
 *   1. Measurement reporting (uplink signaling)
 *   2. RRC reconfiguration reception
 *   3. Random access procedure (PRACH preamble transmission)
 *   4. RRC reconfiguration complete transmission
 *
 * Total HO energy (LTE): ~1-2 Joules (depending on RACH contention)
 * In comparison, staying on a weak cell increases Tx power, consuming
 * more energy over time.
 *
 * Decision rule:
 *   HO_beneficial IF: E_saved_by_better_link(T_stay) > E_handover_cost
 *
 * where T_stay is the expected time the UE will remain in the target cell.
 *
 * Reference: Wang et al. (2014), "Energy-efficient handover for IoT",
 * IEEE Internet of Things Journal.
 */
bool ho_energy_efficient_decision(double rsrp_gain_db,
                                  double tx_power_alpha,
                                  double ue_battery_pct,
                                  double signaling_cost_j,
                                  double low_battery_threshold)
{
    (void)tx_power_alpha; /* Reserved for PA efficiency model */
    if (rsrp_gain_db < 0.0) {
        /* No gain — handover is pure cost */
        return false;
    }

    /* Energy saved by lower Tx power over time T_stay (assume 30s = 30000 ms):
     * ΔP_tx_dBm = rsrp_gain_db (RSRP improvement = Tx power reduction needed)
     * ΔP_tx_linear = 10^(ΔP_tx_dBm/10)
     * E_saved = P_tx_ref · (1 - 1/ΔP_tx_linear) · T_stay · tx_power_alpha
     *
     * For simplicity, use a linearized model:
     * E_saved ≈ rsrp_gain_db * energy_per_db_per_second * T_stay */

    double energy_saved_j = rsrp_gain_db * 0.05 * 30.0; /* 0.05 J/dB/s, 30s stay */

    /* Conservative mode at low battery */
    double effective_threshold = signaling_cost_j;
    if (ue_battery_pct < low_battery_threshold) {
        effective_threshold *= 2.0; /* More conservative below threshold */
    }

    return (energy_saved_j > effective_threshold);
}

/* ============================================================================
 * L7: Admission Control for Handover
 *
 * Target cell admission control ensures that accepting a handover UE does
 * not degrade the QoS of existing UEs below acceptable levels. This is a
 * critical function at the target gNB/eNB.
 *
 * Admission decision based on:
 *
 * 1. Resource availability:
 *    PRB_available ≥ PRB_required
 *
 * 2. Post-admission utilization constraint:
 *    (PRB_used + PRB_required) / PRB_total ≤ U_max
 *
 * 3. SINR impact constraint:
 *    ΔSINR_avg ≈ -10·log10(1 + 1/N_active) ≤ ΔSINR_max
 *    (adding one UE increases interference to others by ~10·log10(1+1/N))
 *
 * Reference: 3GPP TS 36.413 §8.4 (S1AP Handover Resource Allocation)
 */
bool ho_admission_control(const CellLoadInfo *target_load,
                          int                  ue_required_prbs,
                          double               admission_threshold,
                          double               ue_estimated_sinr_db,
                          double               max_sinr_degradation_db)
{
    if (!target_load) return false;
    if (ue_required_prbs <= 0) return false;

    /* Check 1: Resource availability */
    int prb_available = target_load->total_prbs - target_load->used_prbs;
    if (prb_available < ue_required_prbs) {
        return false;
    }

    /* Check 2: Post-admission utilization */
    double post_util = (double)(target_load->used_prbs + ue_required_prbs)
                     / (double)target_load->total_prbs;
    if (post_util > admission_threshold) {
        return false;
    }

    /* Check 3: SINR degradation to existing UEs
     * Adding one UE increases total interference:
     * ΔI ≈ P_new / P_existing_per_UE ≈ 1/N_active (assuming equal power) */
    if (target_load->num_active_ues > 0) {
        double sinr_degradation = 10.0 * log10(1.0 + 1.0 / target_load->num_active_ues);
        if (sinr_degradation > max_sinr_degradation_db) {
            return false;
        }
    }

    /* Check 4: Incoming UE must have adequate SINR */
    if (ue_estimated_sinr_db < -6.0) {
        /* Below QPSK minimum SINR (~-5 dB with coding) */
        return false;
    }

    /* Ignore unused parameter warning avoidance */
    (void)ue_estimated_sinr_db;

    return true;
}
