/**
 * @file handover_types.c
 * @brief Core handover type utility implementations (L1)
 *
 * Implements initialization, validation, and utility functions for
 * the handover/mobility type system. These serve as the foundation
 * for all other modules.
 */

#include "handover_types.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * MeasurementQuantity utilities (L1 Definitions)
 * ============================================================================ */

/**
 * meas_init - Initialize MeasurementQuantity to default values.
 *
 * Sets all measurement quantities to invalid/unknown states.
 * RSRP -200 dBm represents "no signal" (below thermal noise floor).
 * RSRQ -30 dB represents worst quality.
 * SINR -40 dB represents unusable channel.
 */
void meas_quantity_init(MeasurementQuantity *mq) {
    if (!mq) return;
    mq->rsrp_dbm = -200.0;
    mq->rsrq_db = -30.0;
    mq->sinr_db = -40.0;
    mq->rssi_dbm = -200.0;
    mq->cqi = 0;
    mq->bler_estimate = 1.0;
}

/**
 * meas_quantity_is_valid - Check if measurement contains valid data.
 *
 * Valid RSRP range per 3GPP TS 36.133: [-140, -44] dBm for LTE.
 * Extended for NR to [-156, -31] dBm.
 */
bool meas_quantity_is_valid(const MeasurementQuantity *mq) {
    if (!mq) return false;
    return (mq->rsrp_dbm > -200.0 && mq->rsrp_dbm < -20.0
            && mq->rsrq_db >= -30.0 && mq->rsrq_db <= 0.0
            && mq->rssi_dbm > -200.0 && mq->rssi_dbm < -20.0);
}

/* ============================================================================
 * CellInfo utilities (L1)
 * ============================================================================ */

/**
 * cell_info_init - Initialize a CellInfo structure with defaults.
 */
void cell_info_init(CellInfo *cell) {
    if (!cell) return;
    memset(&cell->identity, 0, sizeof(CellIdentity));
    cell->position_x = 0.0;
    cell->position_y = 0.0;
    cell->position_z = 25.0; /* Typical BS height: 25m */
    cell->tx_power_dbm = 43.0; /* Typical macro: 20W = 43 dBm */
    cell->antenna_gain_dbi = 15.0; /* Typical sector antenna */
    cell->coverage_radius_m = 500.0; /* Typical urban macro: 500m */
    cell->load.num_active_ues = 0;
    cell->load.total_prbs = 100;
    cell->load.used_prbs = 0;
    cell->load.prb_utilization = 0.0;
    cell->load.cpu_load = 0.0;
    cell->load.backhaul_utilization = 0.0;
    cell->load.average_throughput_mbps = 0.0;
    cell->is_barred = false;
    cell->is_reserved = false;
    cell->num_neighbours = 0;
    memset(cell->neighbour_pcis, 0, sizeof(cell->neighbour_pcis));
}

/**
 * cell_distance_to_ue - Calculate 2D distance from cell to UE.
 *
 * Used for path loss computation and cell selection.
 */
double cell_distance_to_ue(const CellInfo *cell, const UEPosition *ue_pos) {
    if (!cell || !ue_pos) return 1e9;
    double dx = cell->position_x - ue_pos->position_x;
    double dy = cell->position_y - ue_pos->position_y;
    return sqrt(dx * dx + dy * dy);
}

/* ============================================================================
 * UEContext utilities (L1)
 * ============================================================================ */

/**
 * ue_context_init - Initialize a UEContext with defaults.
 */
void ue_context_init(UEContext *ue, uint32_t ue_id) {
    if (!ue) return;
    memset(ue, 0, sizeof(UEContext));
    ue->ue_id = ue_id;
    ue->serving_cell_id = 0;
    ue->target_cell_id = 0;
    ue->position.position_x = 0.0;
    ue->position.position_y = 0.0;
    ue->position.position_z = 1.5; /* Typical UE height */
    ue->mobility_state = MOB_STATIONARY;
    ue->ho_phase = HO_PHASE_IDLE;
    ue->measurement_interval_ms = 40.0; /* Default: 40ms (LTE) */
    ue->is_connected = false;
    ue->session_start_ms = 0.0;
    ue->pingpong_timer_ms = 5000.0; /* 5 seconds */
    meas_quantity_init(&ue->last_measurement);
}

/**
 * ue_update_position - Update UE position from velocity and time step.
 *
 * Simple kinematic update: x += v_x · Δt, y += v_y · Δt.
 */
void ue_update_position(UEContext *ue, double dt_seconds) {
    if (!ue || dt_seconds <= 0.0) return;
    ue->position.position_x += ue->position.velocity_x * dt_seconds;
    ue->position.position_y += ue->position.velocity_y * dt_seconds;
    ue->position.speed_mps = sqrt(
        ue->position.velocity_x * ue->position.velocity_x +
        ue->position.velocity_y * ue->position.velocity_y);
    ue->position.heading_rad = atan2(ue->position.velocity_y,
                                      ue->position.velocity_x);
}

/* ============================================================================
 * HandoverParams utilities (L1)
 * ============================================================================ */

/**
 * ho_params_init_default - Initialize HandoverParams with typical LTE values.
 *
 * Default values based on typical operator configurations (3GPP TS 36.331):
 *   Hysteresis: 3 dB
 *   TTT: 160 ms
 *   A3 offset: 3 dB
 *   L3 filter: k=4 (a = 0.5)
 *   T304: 2000 ms
 */
void ho_params_init_default(HandoverParams *params) {
    if (!params) return;
    params->hysteresis_db = 3.0;
    params->ttt_ms = 160.0;
    params->a3_offset_db = 3.0;
    params->a5_serving_thresh_db = -110.0;
    params->a5_neighbour_thresh_db = -105.0;
    params->l3_filter_coeff = 4;
    params->max_handover_attempts = 3;
    params->handover_margin_db = 3.0;
    params->t304_timer_ms = 2000.0;
    params->t310_timer_ms = 1000.0;
    params->n310_count = 20.0;
    params->n311_count = 10.0;
    for (int i = 0; i < 8; i++) {
        params->ci_offset_db[i] = 0.0;
    }
}

/* ============================================================================
 * HandoverStatistics utilities (L1)
 * ============================================================================ */

/**
 * ho_stats_init - Initialize HandoverStatistics.
 */
void ho_stats_init(HandoverStatistics *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(HandoverStatistics));
    stats->ho_success_rate = 1.0; /* No failures yet — optimistic prior */
}

/**
 * ho_stats_update - Update statistics after a handover event.
 *
 * Updates counters and recomputes rates based on handover outcome.
 */
void ho_stats_update(HandoverStatistics *stats,
                     bool                 success,
                     bool                 pingpong,
                     double               ho_duration_ms) {
    if (!stats) return;

    stats->total_handover_attempts++;

    if (success) {
        stats->successful_handovers++;
        /* Exponential moving average of HO duration */
        if (stats->total_handover_attempts == 1) {
            stats->average_ho_duration_ms = ho_duration_ms;
        } else {
            double alpha = 0.3; /* Smoothing factor */
            stats->average_ho_duration_ms = (1.0 - alpha) * stats->average_ho_duration_ms
                                           + alpha * ho_duration_ms;
        }
    } else {
        stats->failed_handovers++;
    }

    if (pingpong) {
        stats->pingpong_handovers++;
    }

    /* Recompute rates */
    stats->ho_success_rate = (stats->total_handover_attempts > 0)
        ? (double)stats->successful_handovers / stats->total_handover_attempts
        : 0.0;

    stats->pingpong_rate = (stats->total_handover_attempts > 0)
        ? (double)stats->pingpong_handovers / stats->total_handover_attempts
        : 0.0;
}

/**
 * ho_stats_print_summary - Print handover statistics summary.
 */
void ho_stats_print_summary(const HandoverStatistics *stats) {
    if (!stats) return;
    printf("=== Handover Statistics ===\n");
    printf("Total HO Attempts:    %u\n", stats->total_handover_attempts);
    printf("Successful HOs:       %u (%.2f%%)\n",
           stats->successful_handovers, stats->ho_success_rate * 100.0);
    printf("Failed HOs:           %u\n", stats->failed_handovers);
    printf("Ping-pong HOs:        %u (%.2f%%)\n",
           stats->pingpong_handovers, stats->pingpong_rate * 100.0);
    printf("Too-early HOs:        %u\n", stats->too_early_handovers);
    printf("Too-late HOs:         %u\n", stats->too_late_handovers);
    printf("HO to wrong cell:     %u\n", stats->handover_to_wrong_cell);
    printf("Avg HO duration:      %.2f ms\n", stats->average_ho_duration_ms);
    printf("=============================\n");
}

/* ============================================================================
 * MeasurementReport utilities (L1)
 * ============================================================================ */

/**
 * meas_report_init - Initialize a MeasurementReport.
 */
void meas_report_init(MeasurementReport *report, uint32_t ue_id, uint32_t serving_id) {
    if (!report) return;
    memset(report, 0, sizeof(MeasurementReport));
    report->ue_id = ue_id;
    report->serving_cell_id = serving_id;
    report->timestamp_ms = 0.0;
    report->neighbour_detected = false;
}

/**
 * meas_report_add_neighbour - Add a neighbour cell measurement to report.
 *
 * Returns the number of neighbours after addition (or -1 if full).
 */
int meas_report_add_neighbour(MeasurementReport *report,
                              uint32_t           pci,
                              double             rsrp_dbm,
                              double             rsrq_db,
                              double             sinr_db) {
    if (!report) return -1;
    if (report->num_neighbour_cells >= MAX_MEAS_CELLS) return -1;

    int idx = report->num_neighbour_cells;
    report->neighbour_cell_ids[idx] = pci;
    report->neighbour_meas[idx].rsrp_dbm = rsrp_dbm;
    report->neighbour_meas[idx].rsrq_db = rsrq_db;
    report->neighbour_meas[idx].sinr_db = sinr_db;
    report->num_neighbour_cells++;
    report->neighbour_detected = true;

    return report->num_neighbour_cells;
}

/* ============================================================================
 * HandoverDecision utilities (L1)
 * ============================================================================ */

/**
 * ho_decision_init - Initialize a HandoverDecision to "no handover".
 */
void ho_decision_init(HandoverDecision *decision) {
    if (!decision) return;
    decision->handover_triggered = false;
    decision->recommended_target_id = 0;
    decision->decision_confidence = 0.0;
    decision->trigger_event = HO_TRIG_A1;
    decision->trigger_margin_db = 0.0;
    decision->serving_rsrp_dbm = -200.0;
    decision->target_rsrp_dbm = -200.0;
    decision->reason = "No trigger";
}
