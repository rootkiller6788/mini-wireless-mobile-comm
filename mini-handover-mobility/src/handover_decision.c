/**
 * @file handover_decision.c
 * @brief Handover decision algorithm implementations (L2, L4, L5)
 *
 * Each function implements an independent knowledge point for handover
 * decision-making as defined in 3GPP TS 36.331 / TS 38.331.
 */

#include "handover_decision.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * L4: Hysteresis Decision — Fundamental handover model
 *
 * Theorem (Ping-Pong Probability Bound):
 *   For two cells with shadow-fading difference ΔRSRP ~ N(μ, σ²√2),
 *   the probability of ping-pong (handover to B then back to A) within
 *   time window T_p is bounded by:
 *
 *     P(pingpong) ≤ exp(-H² / (4σ²))
 *
 *   where H is the hysteresis margin and σ is the shadow fading std.
 *
 * Derivation:
 *   Ping-pong occurs when RSRP_B > RSRP_A + H (handover to B), and then
 *   RSRP_A > RSRP_B + H (handover back to A) within T_p.
 *
 *   P(pingpong) = P(RSRP_B - RSRP_A > H) · P(RSRP_A - RSRP_B > H | prev)
 *                ≤ P(Δ > H)²
 *                ≤ exp(-H²/(2σ²_Δ))  [Chernoff bound for Gaussian]
 *                = exp(-H²/(4σ²))
 *
 * References:
 *   - Molisch (2011), Ch. 17.4
 *   - Pollini (1996), "Trends in Handover Design", IEEE Comm. Mag.
 */
bool ho_decision_hysteresis(double serving_rsrp_dbm,
                            double target_rsrp_dbm,
                            double hysteresis_db)
{
    /* Guard against invalid inputs */
    if (hysteresis_db < 0.0) {
        return false;
    }

    /* Hysteresis rule: handover only if target exceeds serving by hysteresis */
    double delta = target_rsrp_dbm - serving_rsrp_dbm;
    return (delta > hysteresis_db);
}

/* ============================================================================
 * L5: 3GPP Event A3 — Neighbour becomes offset better than PCell
 *
 * This implements the exact A3 entering and leaving conditions from
 * 3GPP TS 36.331 §5.5.4.4 and TS 38.331 §5.5.4.4.
 *
 * A3 Entering: Mn + Ofn + Ocn - Hys > Ms + Ofs + Ocs + Off
 * A3 Leaving:  Mn + Ofn + Ocn + Hys < Ms + Ofs + Ocs + Off
 *
 * Physical meaning: Neighbour cell must be at least (Off + Hys) dB better
 * than the serving cell to trigger handover, and must drop below
 * (Off - Hys) dB to cancel.
 */
void ho_decision_event_a3(double serving_rsrp_dbm,
                          double neighbour_rsrp_dbm,
                          double a3_offset_db,
                          double hysteresis_db,
                          double serving_cio_db,
                          double neighbour_cio_db,
                          double freq_offset_db,
                          bool  *entry_condition,
                          bool  *leaving_condition)
{
    /* Sanitize inputs */
    if (!entry_condition || !leaving_condition) return;

    /* Compute the adjusted serving and neighbour values per 3GPP formula.
     * Note: Ofs and Ofn are combined into freq_offset_db for simplicity.
     * In reality, they may differ per frequency band. */

    /* Serving quality metric (Q_serving): Ms + Ofs + Ocs (lower is worse) */
    double serving_quality = serving_rsrp_dbm + freq_offset_db + serving_cio_db;

    /* Neighbour quality metric (Q_neighbour): Mn + Ofn + Ocn (lower is worse) */
    double neighbour_quality = neighbour_rsrp_dbm + freq_offset_db + neighbour_cio_db;

    /* Entering condition: Mn + Ofn + Ocn - Hys > Ms + Ofs + Ocs + Off
     * => neighbour_quality - hysteresis_db > serving_quality + a3_offset_db */
    double entering_delta = neighbour_quality - hysteresis_db
                          - serving_quality - a3_offset_db;
    *entry_condition = (entering_delta > 0.0);

    /* Leaving condition: Mn + Ofn + Ocn + Hys < Ms + Ofs + Ocs + Off
     * => neighbour_quality + hysteresis_db < serving_quality + a3_offset_db */
    double leaving_delta = serving_quality + a3_offset_db
                         - neighbour_quality - hysteresis_db;
    *leaving_condition = (leaving_delta > 0.0);
}

/* ============================================================================
 * L5: 3GPP Event A5 — Serving worse than T1 AND neighbour better than T2
 *
 * A5 Entering: Ms + Hys < Thresh1 AND Mn + Ofn + Ocn - Hys > Thresh2
 * A5 Leaving:  Ms - Hys > Thresh1 OR  Mn + Ofn + Ocn + Hys < Thresh2
 *
 * Event A5 is used for coverage-based handover — when the serving cell
 * becomes too weak regardless of how strong the neighbour is (as long as
 * neighbour is above threshold). This ensures handover before RLF.
 */
void ho_decision_event_a5(double serving_rsrp_dbm,
                          double neighbour_rsrp_dbm,
                          double threshold1_db,
                          double threshold2_db,
                          double hysteresis_db,
                          double neighbour_cio_db,
                          double freq_offset_db,
                          bool  *entry_condition,
                          bool  *leaving_condition)
{
    if (!entry_condition || !leaving_condition) return;

    double neighbour_adjusted = neighbour_rsrp_dbm + freq_offset_db
                              + neighbour_cio_db;

    /* Entering (both must be true):
     *   Ms + Hys < Threshold1  →  serving is too weak
     *   Mn + Ofn + Ocn - Hys > Threshold2  →  neighbour is good enough */
    bool cond1_entry = (serving_rsrp_dbm + hysteresis_db) < threshold1_db;
    bool cond2_entry = (neighbour_adjusted - hysteresis_db) > threshold2_db;
    *entry_condition = cond1_entry && cond2_entry;

    /* Leaving (either is true):
     *   Ms - Hys > Threshold1  →  serving is adequate again
     *   Mn + Ofn + Ocn + Hys < Threshold2  →  neighbour is too weak */
    bool cond1_leave = (serving_rsrp_dbm - hysteresis_db) > threshold1_db;
    bool cond2_leave = (neighbour_adjusted + hysteresis_db) < threshold2_db;
    *leaving_condition = cond1_leave || cond2_leave;
}

/* ============================================================================
 * L5: RSSI Threshold-Based Decision
 *
 * Simple and widely used in WiFi (signal-strength-based roaming) and
 * legacy cellular systems. The decision is purely based on absolute
 * RSSI thresholds with a minimum improvement margin to avoid ping-pong.
 *
 * Handover IF:
 *   1. RSSI_serving < rssi_threshold (service degrading)
 *   2. RSSI_target > RSSI_serving + min_improvement (worth switching)
 */
bool ho_decision_rssi_threshold(double serving_rssi_dbm,
                                double target_rssi_dbm,
                                double rssi_threshold_dbm,
                                double min_improvement_db)
{
    /* Cannot handover if target is not even as good as serving */
    if (target_rssi_dbm <= serving_rssi_dbm) {
        return false;
    }

    /* Serving must be below acceptable threshold */
    if (serving_rssi_dbm >= rssi_threshold_dbm) {
        return false;
    }

    /* Improvement must justify the signaling cost */
    double improvement = target_rssi_dbm - serving_rssi_dbm;
    return (improvement >= min_improvement_db);
}

/* ============================================================================
 * L5: SINR-Based Decision
 *
 * SINR-based handover provides a more accurate decision than RSSI-based
 * because it accounts for interference. This is particularly important
 * in dense deployments where interference dominates noise.
 *
 * SINR = P_signal / (P_interference + P_noise)
 *
 * Handover IF:
 *   1. SINR_serving < sinr_threshold (QoS degrading)
 *   2. SINR_target > SINR_serving + margin (significant improvement)
 */
bool ho_decision_sinr(double serving_sinr_db,
                      double target_sinr_db,
                      double sinr_threshold_db,
                      double margin_db)
{
    /* Serving SINR below threshold → need handover */
    if (serving_sinr_db >= sinr_threshold_db) {
        return false;
    }

    /* Target must offer meaningful improvement */
    double delta_sinr = target_sinr_db - serving_sinr_db;
    return (delta_sinr > margin_db);
}

/* ============================================================================
 * L5: Time-To-Trigger (TTT) Mechanism
 *
 * The TTT mechanism prevents unnecessary handovers due to temporary signal
 * fluctuations (fast fading). The triggering condition must be continuously
 * satisfied for the entire TTT duration before the handover is executed.
 *
 * Implementation: sliding window of boolean condition evaluations.
 * Each sample represents one measurement period (typically 40 ms in LTE).
 *
 * Mathematical justification:
 *   For Rayleigh fading, the average fade duration (AFD) below threshold
 *   R is given by:
 *     AFD = (exp(ρ²) - 1) / (ρ·f_d·√(2π))
 *   where ρ = R / R_rms and f_d is the maximum Doppler frequency.
 *
 *   TTT should be set significantly longer than the maximum expected AFD
 *   to avoid triggering on short fades. TTT values in LTE: 0, 40, 64, 80,
 *   100, 128, 160, 256, 320, 480, 512, 640, 1024, 1280, 2560, 5120 ms.
 *
 * References:
 *   - 3GPP TS 36.331 §5.5.4 (TTT configuration)
 *   - Rice (1944), "Mathematical Analysis of Random Noise"
 */
bool ho_decision_ttt_evaluate(const bool condition_history[],
                              int   history_length,
                              int   required_samples)
{
    if (!condition_history || required_samples <= 0 || history_length <= 0) {
        return false;
    }

    if (required_samples > history_length) {
        return false; /* Not enough history yet */
    }

    /* Check if the most recent 'required_samples' entries are all TRUE */
    int consecutive = 0;
    for (int i = 0; i < required_samples; i++) {
        if (condition_history[i]) {
            consecutive++;
        } else {
            return false; /* Break in the required window */
        }
    }

    /* Unused variable warning avoided — use it or not depending on compiler */
    (void)consecutive;

    return true;
}

/* ============================================================================
 * L5: Ping-Pong Handover Detection
 *
 * A ping-pong handover occurs when a UE repeatedly hands over between
 * two cells within a short time window (typically 5 seconds per 3GPP
 * TS 32.425). This wastes signaling resources and degrades QoS.
 *
 * Detection algorithm:
 *   For each handover event i in history:
 *     Find the most recent prior handover j where PCI matches
 *     If time(i) - time(j) < pingpong_window, flag as ping-pong
 *
 * 3GPP definition (TS 32.425 §4.3.1):
 *   Ping-pong HO = Handover from cell A to cell B, and then back to
 *   cell A within a predefined short time period (default 5 s).
 */
bool ho_detect_pingpong(const uint32_t handover_history_pci[],
                        const double   handover_history_time[],
                        int            history_count,
                        double         pingpong_window_ms)
{
    if (!handover_history_pci || !handover_history_time || history_count < 2) {
        return false;
    }

    /* Check the most recent handover against its predecessor */
    uint32_t current_pci = handover_history_pci[0];
    double   current_time = handover_history_time[0];

    /* Look backward for a handover to the same PCI */
    for (int i = 1; i < history_count; i++) {
        if (handover_history_pci[i] == current_pci) {
            double time_diff = current_time - handover_history_time[i];
            if (time_diff > 0 && time_diff < pingpong_window_ms) {
                return true;
            }
            /* Only check the most recent occurrence of this PCI */
            break;
        }
    }

    return false;
}

/* ============================================================================
 * L5: Weighted Sum Model (WSM) for Multi-Criteria Decision
 *
 * The Weighted Sum Model is the simplest multi-criteria decision method.
 * It computes a weighted sum of normalized attribute values:
 *
 *   Score_j = Σ_i w_i · a_ij_normalized
 *
 * where a_ij is attribute i for candidate j, w_i is the weight of
 * attribute i (Σ w_i = 1).
 *
 * Normalization (linear scaling):
 *   For benefit attribute (higher is better):
 *     a_norm = (a - a_min) / (a_max - a_min)
 *   For cost attribute (lower is better):
 *     a_norm = (a_max - a) / (a_max - a_min)
 *
 * Used in vertical handover: consider RSSI, bandwidth, cost, power, security.
 *
 * References:
 *   - Triantaphyllou, "Multi-Criteria Decision Making Methods" (2000)
 *   - Wang & Kuo, "Mathematical modeling for network selection in
 *     heterogeneous wireless networks", IEEE Comm. Surveys (2011)
 */
void ho_decision_weighted_sum(const double *attributes,
                              const double *weights,
                              const bool   *benefit_direction,
                              int           num_candidates,
                              int           num_attributes,
                              double       *scores,
                              int          *best_candidate)
{
    if (!attributes || !weights || !benefit_direction
        || !scores || !best_candidate) return;
    if (num_candidates <= 0 || num_attributes <= 0) return;

    /* Step 1: Find min and max of each attribute across all candidates */
    double *attr_min = (double *)malloc(num_attributes * sizeof(double));
    double *attr_max = (double *)malloc(num_attributes * sizeof(double));
    if (!attr_min || !attr_max) {
        if (attr_min) free(attr_min);
        if (attr_max) free(attr_max);
        return;
    }

    for (int j = 0; j < num_attributes; j++) {
        attr_min[j] = 1e100;
        attr_max[j] = -1e100;
    }

    for (int i = 0; i < num_candidates; i++) {
        for (int j = 0; j < num_attributes; j++) {
            double val = attributes[i * num_attributes + j];
            if (val < attr_min[j]) attr_min[j] = val;
            if (val > attr_max[j]) attr_max[j] = val;
        }
    }

    /* Step 2: Normalize and compute weighted sum */
    double best_score = -1e100;
    *best_candidate = 0;

    for (int i = 0; i < num_candidates; i++) {
        scores[i] = 0.0;
        for (int j = 0; j < num_attributes; j++) {
            double val = attributes[i * num_attributes + j];
            double range = attr_max[j] - attr_min[j];
            double normalized;

            if (range < 1e-12) {
                /* All values equal for this attribute */
                normalized = 1.0;
            } else {
                if (benefit_direction[j]) {
                    /* Benefit: higher is better */
                    normalized = (val - attr_min[j]) / range;
                } else {
                    /* Cost: lower is better */
                    normalized = (attr_max[j] - val) / range;
                }
            }

            scores[i] += weights[j] * normalized;
        }

        if (scores[i] > best_score) {
            best_score = scores[i];
            *best_candidate = i;
        }
    }

    free(attr_min);
    free(attr_max);
}

/* ============================================================================
 * L5: TOPSIS — Technique for Order Preference by Similarity to Ideal Solution
 *
 * TOPSIS (Hwang & Yoon, 1981) is a multi-criteria decision method widely
 * used in vertical handover (VHO) network selection problems. It ranks
 * candidates by their distance to the ideal best and ideal worst solutions.
 *
 * Mathematical steps:
 *
 * Step 1: Normalize decision matrix (Euclidean/vector normalization)
 *   r_ij = x_ij / sqrt(Σ_k x_kj²)
 *
 * Step 2: Weight the normalized matrix
 *   v_ij = w_j · r_ij
 *
 * Step 3: Determine ideal best (A*) and ideal worst (A-)
 *   A* = {max_i v_ij | j ∈ J_benefit,  min_i v_ij | j ∈ J_cost}
 *   A- = {min_i v_ij | j ∈ J_benefit,  max_i v_ij | j ∈ J_cost}
 *
 * Step 4: Compute separation measures
 *   S_i* = sqrt(Σ_j (v_ij - v_j*)²)
 *   S_i- = sqrt(Σ_j (v_ij - v_j-)²)
 *
 * Step 5: Compute closeness coefficient
 *   C_i* = S_i- / (S_i* + S_i-)  ∈ [0, 1]
 *
 * Best candidate: max C_i*
 *
 * References:
 *   - Hwang & Yoon, "Multiple Attribute Decision Making" (1981)
 *   - Lahby et al., "Network Selection Decision Based on TOPSIS",
 *     IEEE Conf. on Network & Service Mgmt (2010)
 */
void ho_decision_topsis(const double *decision_matrix,
                        const double *weights,
                        const bool   *is_benefit,
                        int           num_candidates,
                        int           num_criteria,
                        double       *closeness_coeff,
                        int          *best_index)
{
    if (!decision_matrix || !weights || !is_benefit
        || !closeness_coeff || !best_index) return;
    if (num_candidates <= 0 || num_criteria <= 0) return;

    /* Allocate working memory */
    double *norm_matrix = (double *)malloc(num_candidates * num_criteria * sizeof(double));
    double *ideal_best  = (double *)malloc(num_criteria * sizeof(double));
    double *ideal_worst = (double *)malloc(num_criteria * sizeof(double));
    if (!norm_matrix || !ideal_best || !ideal_worst) {
        if (norm_matrix) free(norm_matrix);
        if (ideal_best) free(ideal_best);
        if (ideal_worst) free(ideal_worst);
        return;
    }

    /* Step 1: Euclidean normalization
     *   norm_factor_j = sqrt(sum_i x_ij²) */
    for (int j = 0; j < num_criteria; j++) {
        double sum_sq = 0.0;
        for (int i = 0; i < num_candidates; i++) {
            double v = decision_matrix[i * num_criteria + j];
            sum_sq += v * v;
        }
        double norm_factor = sqrt(sum_sq);
        if (norm_factor < 1e-15) norm_factor = 1.0;
        for (int i = 0; i < num_candidates; i++) {
            norm_matrix[i * num_criteria + j] =
                decision_matrix[i * num_criteria + j] / norm_factor;
        }
    }

    /* Step 2: Weight the normalized matrix */
    for (int i = 0; i < num_candidates; i++) {
        for (int j = 0; j < num_criteria; j++) {
            norm_matrix[i * num_criteria + j] *= weights[j];
        }
    }

    /* Step 3: Ideal best and worst solutions */
    for (int j = 0; j < num_criteria; j++) {
        if (is_benefit[j]) {
            /* Benefit criterion: max is best, min is worst */
            double best  = -1e100;
            double worst = 1e100;
            for (int i = 0; i < num_candidates; i++) {
                double v = norm_matrix[i * num_criteria + j];
                if (v > best)  best  = v;
                if (v < worst) worst = v;
            }
            ideal_best[j]  = best;
            ideal_worst[j] = worst;
        } else {
            /* Cost criterion: min is best, max is worst */
            double best  = 1e100;
            double worst = -1e100;
            for (int i = 0; i < num_candidates; i++) {
                double v = norm_matrix[i * num_criteria + j];
                if (v < best)  best  = v;
                if (v > worst) worst = v;
            }
            ideal_best[j]  = best;
            ideal_worst[j] = worst;
        }
    }

    /* Step 4: Separation measures */
    double *S_best  = (double *)malloc(num_candidates * sizeof(double));
    double *S_worst = (double *)malloc(num_candidates * sizeof(double));
    if (!S_best || !S_worst) {
        if (S_best) free(S_best);
        if (S_worst) free(S_worst);
        free(norm_matrix); free(ideal_best); free(ideal_worst);
        return;
    }

    for (int i = 0; i < num_candidates; i++) {
        S_best[i]  = 0.0;
        S_worst[i] = 0.0;
        for (int j = 0; j < num_criteria; j++) {
            double v = norm_matrix[i * num_criteria + j];
            double db = v - ideal_best[j];
            double dw = v - ideal_worst[j];
            S_best[i]  += db * db;
            S_worst[i] += dw * dw;
        }
        S_best[i]  = sqrt(S_best[i]);
        S_worst[i] = sqrt(S_worst[i]);
    }

    /* Step 5: Closeness coefficient */
    double best_closeness = -1.0;
    *best_index = 0;
    for (int i = 0; i < num_candidates; i++) {
        double denom = S_best[i] + S_worst[i];
        if (denom < 1e-15) {
            closeness_coeff[i] = 1.0; /* Perfect score for degenerate case */
        } else {
            closeness_coeff[i] = S_worst[i] / denom;
        }
        if (closeness_coeff[i] > best_closeness) {
            best_closeness = closeness_coeff[i];
            *best_index = i;
        }
    }

    free(S_best);
    free(S_worst);
    free(norm_matrix);
    free(ideal_best);
    free(ideal_worst);
}

/* ============================================================================
 * L5: GRA — Grey Relational Analysis
 *
 * Grey Relational Analysis (Deng, 1982) measures the degree of correlation
 * between each candidate and an ideal reference sequence. It is based on
 * grey system theory, which handles incomplete or uncertain information.
 *
 * Grey relational coefficient:
 *   γ(x₀(k), x_i(k)) = (Δ_min + ρ·Δ_max) / (Δ_i(k) + ρ·Δ_max)
 *
 * where:
 *   Δ_i(k) = |x₀(k) - x_i(k)|  (absolute difference)
 *   Δ_min = min_i min_k Δ_i(k)
 *   Δ_max = max_i max_k Δ_i(k)
 *   ρ = distinguishing coefficient (0 < ρ ≤ 1, typically 0.5)
 *
 * Grey relational grade:
 *   Γ(x₀, x_i) = (1/n) · Σ_k γ(x₀(k), x_i(k))
 *
 * References:
 *   - Deng (1982), "Control problems of grey systems", Systems & Control Letters
 *   - Song & Jamalipour, "Network Selection in an Integrated WLAN and UMTS
 *     Environment Using GRA", IEEE Trans. Veh. Tech. (2005)
 */
void ho_decision_gra(const double *decision_matrix,
                     const double *reference,
                     double        distinguishing_coeff,
                     int           num_candidates,
                     int           num_criteria,
                     double       *grades,
                     int          *best_index)
{
    if (!decision_matrix || !reference || !grades || !best_index) return;
    if (num_candidates <= 0 || num_criteria <= 0) return;
    if (distinguishing_coeff <= 0.0) distinguishing_coeff = 0.5;

    /* Step 1: Normalize decision matrix using min-max normalization */
    double *norm = (double *)malloc(num_candidates * num_criteria * sizeof(double));
    if (!norm) return;

    for (int j = 0; j < num_criteria; j++) {
        double col_min = 1e100, col_max = -1e100;
        for (int i = 0; i < num_candidates; i++) {
            double v = decision_matrix[i * num_criteria + j];
            if (v < col_min) col_min = v;
            if (v > col_max) col_max = v;
        }
        double range = col_max - col_min;
        if (range < 1e-12) range = 1.0;
        for (int i = 0; i < num_candidates; i++) {
            norm[i * num_criteria + j] =
                (decision_matrix[i * num_criteria + j] - col_min) / range;
        }
    }

    /* Also normalize the reference (assume reference is already ideal) */
    double *norm_ref = (double *)malloc(num_criteria * sizeof(double));
    if (!norm_ref) { free(norm); return; }
    for (int j = 0; j < num_criteria; j++) {
        /* For already ideal reference, set to 1.0 (max normalized value) */
        /* But if reference was provided in original units, normalize it */
        double col_min = 1e100, col_max = -1e100;
        for (int i = 0; i < num_candidates; i++) {
            double v = decision_matrix[i * num_criteria + j];
            if (v < col_min) col_min = v;
            if (v > col_max) col_max = v;
        }
        double range = col_max - col_min;
        if (range < 1e-12) range = 1.0;
        norm_ref[j] = (reference[j] - col_min) / range;
        if (norm_ref[j] > 1.0) norm_ref[j] = 1.0;
        if (norm_ref[j] < 0.0) norm_ref[j] = 0.0;
    }

    /* Step 2: Compute absolute difference matrix */
    double *delta = (double *)malloc(num_candidates * num_criteria * sizeof(double));
    if (!delta) { free(norm); free(norm_ref); return; }

    double delta_min = 1e100, delta_max = -1e100;
    for (int i = 0; i < num_candidates; i++) {
        for (int j = 0; j < num_criteria; j++) {
            delta[i * num_criteria + j] = fabs(norm_ref[j] - norm[i * num_criteria + j]);
            if (delta[i * num_criteria + j] < delta_min)
                delta_min = delta[i * num_criteria + j];
            if (delta[i * num_criteria + j] > delta_max)
                delta_max = delta[i * num_criteria + j];
        }
    }

    /* Step 3: Grey relational coefficient */
    double *gamma = (double *)malloc(num_candidates * num_criteria * sizeof(double));
    if (!gamma) { free(norm); free(norm_ref); free(delta); return; }

    for (int i = 0; i < num_candidates; i++) {
        for (int j = 0; j < num_criteria; j++) {
            double numerator = delta_min + distinguishing_coeff * delta_max;
            double denominator = delta[i * num_criteria + j]
                               + distinguishing_coeff * delta_max;
            if (denominator < 1e-15) denominator = 1e-15;
            gamma[i * num_criteria + j] = numerator / denominator;
        }
    }

    /* Step 4: Grey relational grade (equal weight per criterion) */
    double best_grade = -1.0;
    *best_index = 0;

    for (int i = 0; i < num_candidates; i++) {
        double sum = 0.0;
        for (int j = 0; j < num_criteria; j++) {
            sum += gamma[i * num_criteria + j];
        }
        grades[i] = sum / num_criteria;
        if (grades[i] > best_grade) {
            best_grade = grades[i];
            *best_index = i;
        }
    }

    free(norm);
    free(norm_ref);
    free(delta);
    free(gamma);
}
