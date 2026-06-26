/**
 * @file cell_network_handover.c
 * @brief Handover & Mobility Management (L5, L6)
 * Reference: 3GPP TS 38.331; Sesia et al. (2011) Ch. 3
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "cell_network_handover.h"
#include "cell_network_model.h"

void ho_config_init_default(ho_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->a3_offset_db = 3.0;
    cfg->a3_time_to_trigger_ms = 40.0;
    cfg->a3_report_interval_ms = 120.0;
    cfg->a5_threshold1_db = -115.0;
    cfg->a5_threshold2_db = -105.0;
    cfg->a2_threshold_db = -120.0;
    cfg->max_report_cells = 8;
    cfg->l3_filter_coeff = 4;
    cfg->time_to_trigger_samples = 4;
}

double ho_l3_filter(double prev_filtered, double current_meas, int filter_coeff) {
    if (filter_coeff < 0) filter_coeff = 0;
    if (filter_coeff > 19) filter_coeff = 19;
    double a = 1.0 / pow(2.0, (double)filter_coeff / 4.0);
    return (1.0 - a) * prev_filtered + a * current_meas;
}

int ho_evaluate_event(ho_event_type_t event, ho_config_t *cfg,
                       double serving_rsrp, double neighbor_rsrp) {
    if (!cfg) return 0;
    switch (event) {
        case HO_EVENT_A1:
            return (serving_rsrp > cfg->a2_threshold_db + cfg->a3_offset_db) ? 1 : 0;
        case HO_EVENT_A2:
            return (serving_rsrp < cfg->a2_threshold_db) ? 1 : 0;
        case HO_EVENT_A3:
            return (neighbor_rsrp > serving_rsrp + cfg->a3_offset_db) ? 1 : 0;
        case HO_EVENT_A4:
            return (neighbor_rsrp > cfg->a5_threshold2_db) ? 1 : 0;
        case HO_EVENT_A5:
            return (serving_rsrp < cfg->a5_threshold1_db &&
                    neighbor_rsrp > cfg->a5_threshold2_db) ? 1 : 0;
        case HO_EVENT_B1:
            return (neighbor_rsrp > cfg->a5_threshold2_db) ? 1 : 0;
        case HO_EVENT_B2:
            return (serving_rsrp < cfg->a5_threshold1_db &&
                    neighbor_rsrp > cfg->a5_threshold2_db) ? 1 : 0;
        default: return 0;
    }
}

int ho_process_measurement(ho_decision_engine_t *eng, const ue_meas_report_t *report) {
    if (!eng || !report) return 0;
    double sr = ho_l3_filter(eng->serving_state.filtered_rsrp, report->serving_cell.rsrp,
                              eng->config.l3_filter_coeff);
    eng->serving_state.filtered_rsrp = sr;
    eng->serving_state.filtered_rsrq = report->serving_cell.rsrq;

    eng->triggered_event = HO_EVENT_A3;
    int best_idx = -1;
    double best_rsrp = -999.0;
    int n = report->num_neighbors;
    if (n > MAX_NEIGHBOR_CELLS) n = MAX_NEIGHBOR_CELLS;

    for (int i = 0; i < n; i++) {
        if (!report->neighbor_cells[i].is_detected) continue;
        double nr = ho_l3_filter(eng->neighbor_states[i].filtered_rsrp,
                                  report->neighbor_cells[i].rsrp,
                                  eng->config.l3_filter_coeff);
        eng->neighbor_states[i].filtered_rsrp = nr;
        eng->neighbor_states[i].cell_id = report->neighbor_cells[i].cell_id;

        if (ho_evaluate_event(HO_EVENT_A3, &eng->config, sr, nr)) {
            eng->neighbor_states[i].consecutive_hysteresis_count++;
        } else {
            eng->neighbor_states[i].consecutive_hysteresis_count = 0;
        }

        if (eng->neighbor_states[i].consecutive_hysteresis_count >=
            eng->config.time_to_trigger_samples) {
            if (nr > best_rsrp) { best_rsrp = nr; best_idx = i; }
        }
    }

    eng->num_neighbors = n;
    if (best_idx >= 0) {
        eng->is_triggered = 1;
        eng->target_cell_id = eng->neighbor_states[best_idx].cell_id;
        return 1;
    }
    return 0;
}

int ho_set_target(ho_decision_engine_t *eng, uint32_t target_id) {
    if (!eng) return -1;
    eng->target_cell_id = target_id;
    eng->is_triggered = 1;
    return 0;
}

void ho_reset_engine(ho_decision_engine_t *eng) {
    if (!eng) return;
    memset(eng, 0, sizeof(*eng));
}

double ho_failure_probability(sinr_db_t target_sinr_db, double sinr0_db, double sigma_db) {
    double x = (target_sinr_db - sinr0_db) / sigma_db;
    return 1.0 / (1.0 + exp(x));
}

mob_state_t mob_estimate_state(int cell_changes, int n_cr_m, int n_cr_h,
                                double t_eval_s) {
    (void)t_eval_s;
    if (cell_changes > n_cr_h) return MOB_STATE_HIGH;
    if (cell_changes > n_cr_m) return MOB_STATE_MEDIUM;
    return MOB_STATE_NORMAL;
}

double mob_speed_scaling_factor(mob_state_t state, double sf_medium, double sf_high) {
    switch (state) {
        case MOB_STATE_HIGH:   return sf_high;
        case MOB_STATE_MEDIUM: return sf_medium;
        default:              return 1.0;
    }
}

int ho_detect_ping_pong(const uint32_t *ho_history, int history_len,
                         int window_events, double window_time_ms) {
    (void)window_time_ms;
    if (!ho_history || history_len < 3 || window_events > history_len) return 0;
    int pp_count = 0;
    for (int i = 2; i < history_len; i++) {
        if (ho_history[i] == ho_history[i - 2] &&
            ho_history[i] != ho_history[i - 1]) {
            pp_count++;
        }
    }
    return (pp_count >= window_events) ? 1 : 0;
}

double ho_optimize_a3_offset(double current_offset, double ping_pong_rate,
                              double rlf_rate, double target_ping_pong) {
    double step = 0.5;
    if (ping_pong_rate > target_ping_pong) return current_offset + step;
    if (rlf_rate > 0.02 && ping_pong_rate < target_ping_pong * 0.5)
        return current_offset - step;
    return current_offset;
}

void ho_mro_optimize(ho_config_t *cfg, double too_early_rate,
                      double too_late_rate, double ping_pong_rate) {
    if (!cfg) return;
    if (too_early_rate > 0.05 || ping_pong_rate > 0.02) {
        cfg->a3_offset_db += 1.0;
        cfg->a3_time_to_trigger_ms += 40.0;
    } else if (too_late_rate > 0.02) {
        cfg->a3_offset_db -= 0.5;
        cfg->a3_time_to_trigger_ms -= 40.0;
        if (cfg->a3_offset_db < -3.0) cfg->a3_offset_db = -3.0;
        if (cfg->a3_time_to_trigger_ms < 40.0) cfg->a3_time_to_trigger_ms = 40.0;
    }
}

double ho_interruption_time_ms(double rrc_proc_ms, double rach_ms,
                                double path_switch_ms) {
    return rrc_proc_ms + rach_ms + path_switch_ms;
}
