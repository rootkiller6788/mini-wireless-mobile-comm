/**
 * @file cell_network_handover.h
 * @brief Cellular Network ? Handover & Mobility Management (L5, L6)
 *
 * Reference: 3GPP TS 38.331 "RRC Protocol Specification"
 *            3GPP TS 36.331 "E-UTRA RRC Protocol"
 *            Molisch (2011) Ch. 17; Sesia et al. (2011) Ch. 3, 14
 *
 * Implements handover event evaluation (A1-A5, B1-B2), measurement
 * report processing, handover decision algorithms, and
 * mobility state estimation (normal/medium/high).
 */

#ifndef CELL_NETWORK_HANDOVER_H
#define CELL_NETWORK_HANDOVER_H

#include "cell_network_defs.h"
#include "cell_network_model.h"

/* ================================================================
 * L5: Handover Configuration
 * ================================================================ */

typedef struct {
    /* A3 event: Neighbor becomes offset better than serving */
    double a3_offset_db;         /* Hysteresis offset for A3 */
    double a3_time_to_trigger_ms;/* TTT: time condition must hold */
    double a3_report_interval_ms;/* Reporting interval */

    /* A5 event: Serving worse than threshold1, neighbor better than threshold2 */
    double a5_threshold1_db;     /* Serving becomes worse than threshold */
    double a5_threshold2_db;     /* Neighbor becomes better than threshold */

    /* A2 event: Serving becomes worse than threshold (triggers inter-freq) */
    double a2_threshold_db;

    /* Common */
    uint8_t max_report_cells;    /* Max cells in measurement report */
    double  l3_filter_coeff;     /* Layer 3 filter coefficient (0..19) */
    int     time_to_trigger_samples;  /* Samples at reporting interval */
} ho_config_t;

/** Handover measurement state per cell */
typedef struct {
    uint32_t   cell_id;
    rsrp_dbm_t filtered_rsrp;      /* L3 filtered RSRP */
    rsrq_db_t  filtered_rsrq;      /* L3 filtered RSRQ */
    int        consecutive_hysteresis_count;
    int        is_satisfied;
    uint32_t   first_trigger_time_ms;
} ho_meas_state_t;

/** Handover decision state machine */
typedef struct {
    ho_config_t     config;
    ho_meas_state_t serving_state;
    ho_meas_state_t neighbor_states[MAX_NEIGHBOR_CELLS];
    int             num_neighbors;
    ho_event_type_t triggered_event;
    int             is_triggered;
    uint32_t        trigger_time_ms;
    uint32_t        target_cell_id;
    int             ho_in_progress;
    double          ho_failure_prob;
} ho_decision_engine_t;

/* ================================================================
 * L5: UE Mobility State
 * ================================================================ */

typedef enum {
    MOB_STATE_NORMAL   = 0,
    MOB_STATE_MEDIUM   = 1,  /* Medium mobility (30-60 km/h typical) */
    MOB_STATE_HIGH     = 2   /* High mobility (> 60 km/h) */
} mob_state_t;

/** Mobility state estimation based on cell change count */
typedef struct {
    mob_state_t state;
    int         cell_change_count;
    double      evaluation_period_s;
    double      speed_estimate_kmh;
} ue_mobility_state_t;

/* ================================================================
 * L5: Handover Functions
 * ================================================================ */

/** Initialize handover configuration with default 3GPP values */
void ho_config_init_default(ho_config_t *cfg);

/** Layer 3 filtering (3GPP TS 38.331 Sec 5.5.3.2):
 *  F_n = (1-a)*F_{n-1} + a*M_n
 *  where a = 1/2^(k/4), k = filterCoefficient (0..19)
 *  Complexity: O(1) per measurement
 */
double ho_l3_filter(double prev_filtered, double current_meas,
                    int filter_coeff);

/** Evaluate handover event for given serving and neighbor measurements.
 *  Returns 1 if event condition is met.
 *
 *  Events:
 *    A1: Serving > threshold  (stops inter-freq measurements)
 *    A2: Serving < threshold  (starts inter-freq measurements)
 *    A3: Neighbor > Serving + offset
 *    A4: Neighbor > threshold
 *    A5: Serving < threshold1 AND Neighbor > threshold2
 *    B1: Inter-RAT neighbor > threshold
 */
int ho_evaluate_event(ho_event_type_t event, ho_config_t *cfg,
                       double serving_rsrp, double neighbor_rsrp);

/** Process a measurement report through the handover decision engine.
 *  Implements time-to-trigger (TTT) and hysteresis.
 *  Returns 1 if handover should be triggered, 0 otherwise.
 *  Complexity: O(N_neighbors)
 */
int ho_process_measurement(ho_decision_engine_t *eng,
                            const ue_meas_report_t *report);

/** Set the target cell for handover */
int ho_set_target(ho_decision_engine_t *eng, uint32_t target_id);

/** Reset handover engine state */
void ho_reset_engine(ho_decision_engine_t *eng);

/** Get handover failure probability based on SINR of target cell.
 *  Uses empirical model: P_fail = 1 / (1 + exp((SINR - SINR0)/sigma))
 */
double ho_failure_probability(sinr_db_t target_sinr_db,
                               double sinr0_db, double sigma_db);

/* ================================================================
 * L5: Mobility State Estimation
 * ================================================================ */

/** Estimate UE mobility state from cell change count.
 *  Parameters from 3GPP TS 38.304:
 *    N_cr_M: medium threshold, N_cr_H: high threshold
 */
mob_state_t mob_estimate_state(int cell_changes, int n_cr_m, int n_cr_h,
                                double t_eval_s);

/** Get speed-dependent scaling factor for TTT (sf-Medium, sf-High) */
double mob_speed_scaling_factor(mob_state_t state,
                                 double sf_medium, double sf_high);

/* ================================================================
 * L6: Handover Optimization
 * ================================================================ */

/** Handover ping-pong detection: count HO events within window */
int ho_detect_ping_pong(const uint32_t *ho_history, int history_len,
                         int window_events, double window_time_ms);

/** Optimize A3 offset based on ping-pong rate and RLF rate.
 *  Increase offset to reduce ping-pong; decrease to reduce RLF.
 */
double ho_optimize_a3_offset(double current_offset,
                              double ping_pong_rate,
                              double rlf_rate,
                              double target_ping_pong);

/** MRO (Mobility Robustness Optimization): self-optimizing A3 parameters.
 *  Reference: 3GPP TS 36.902 "SON"
 */
void ho_mro_optimize(ho_config_t *cfg,
                      double too_early_rate, double too_late_rate,
                      double ping_pong_rate);

/** Compute handover interruption time (ms):
 *  T_interrupt = T_RRC_proc + T_RACH + T_path_switch
 *  Typical: 30-50 ms for LTE, < 0 ms for NR make-before-break
 */
double ho_interruption_time_ms(double rrc_proc_ms, double rach_ms,
                                double path_switch_ms);

#endif /* CELL_NETWORK_HANDOVER_H */
