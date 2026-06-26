/**
 * @file cell_network_scheduler.h
 * @brief Cellular Network ? Packet Scheduling Algorithms (L5)
 *
 * Reference: Molisch (2011) Ch. 19; Sesia, Toufik, Baker "LTE?The UMTS
 *            Long Term Evolution" (2011) Ch. 9
 *            3GPP TS 36.321 "MAC Protocol"
 *
 * Implements Round Robin (RR), Max C/I, and Proportional Fair (PF)
 * scheduling algorithms for the downlink shared channel.
 */

#ifndef CELL_NETWORK_SCHEDULER_H
#define CELL_NETWORK_SCHEDULER_H

#include "cell_network_defs.h"

/* ================================================================
 * L5: UE Scheduling State
 * ================================================================ */

typedef struct {
    uint32_t  ue_id;
    sinr_db_t sinr;
    cqi_t     cqi;
    double    spectral_eff;
    double    achievable_rate_mbps;  /* Instantaneous rate */
    double    avg_rate_mbps;         /* Exponential moving average throughput */
    double    last_scheduled_ms;     /* Last time UE was scheduled */
    int       buffer_bytes;          /* Data waiting in buffer */
    double    pf_metric;             /* Proportional Fair metric */
    double    qos_weight;            /* Weight from QoS profile */
    int       is_active;
} ue_sched_state_t;

/** Scheduler context for one cell (scheduling epoch) */
typedef struct {
    ue_sched_state_t ues[MAX_UE_PER_GNB];
    int              num_ues;
    int              num_rbs;           /* Number of resource blocks */
    double           tti_ms;            /* Transmission Time Interval */
    double           alpha_pf;          /* PF smoothing factor (typ. 0.001-0.01) */
    uint32_t         epoch_ms;
    scheduler_type_t type;
} sched_context_t;

/** Scheduling result for one UE in one TTI */
typedef struct {
    uint32_t ue_id;
    int      rbs_allocated;
    double   rate_allocated_mbps;
    double   throughput_actual_bps;
} sched_result_t;

/** Full scheduling decision for one TTI */
typedef struct {
    sched_result_t ue_allocations[MAX_UE_PER_GNB];
    int            num_allocated;
    double         total_cell_throughput_bps;
    uint32_t       tti_ms;
} sched_decision_t;

/* ================================================================
 * L5: Scheduler Core Functions
 * ================================================================ */

/** Initialize scheduler context */
void sched_init(sched_context_t *ctx, scheduler_type_t type,
                int num_rbs, double tti_ms, double alpha_pf);

/** Add or update UE in scheduler */
int sched_add_ue(sched_context_t *ctx, uint32_t ue_id,
                 sinr_db_t sinr, int buffer_bytes);

/** Update UE SINR (on measurement report) */
void sched_update_sinr(sched_context_t *ctx, uint32_t ue_id,
                       sinr_db_t new_sinr);

/** Remove UE from scheduler */
void sched_remove_ue(sched_context_t *ctx, uint32_t ue_id);

/* ================================================================
 * L5: Scheduling Algorithms
 * ================================================================ */

/** Round Robin: allocate RBs equally among active UEs.
 *  Fairness = 1, throughput may be suboptimal.
 *  Complexity: O(N_UE)
 */
sched_decision_t sched_round_robin(sched_context_t *ctx);

/** Max C/I (Maximum Carrier-to-Interference): greedy,
 *  allocate RBs to UEs with best SINR.
 *  Maximizes cell throughput but unfair to cell-edge users.
 *  Complexity: O(N_UE * N_RB)
 */
sched_decision_t sched_max_ci(sched_context_t *ctx);

/** Proportional Fair (PF): allocate RBs to maximize
 *  sum of log-average-rates.
 *  For UE i on RB j: metric = R_ij / T_i(t)
 *  where R_ij is achievable rate on RB j, T_i(t) is exponential
 *  moving average throughput.
 *  Complexity: O(N_UE * N_RB)
 *
 *  Reference: Kelly, "Charging and rate control for elastic traffic"
 *  European Trans. Telecom. (1997)
 */
sched_decision_t sched_proportional_fair(sched_context_t *ctx);

/** Proportional Fair with Exponential rule (EXP/PF):
 *  balances latency-sensitive and best-effort traffic.
 *  metric = exp((a_i * W_i - aW_avg) / (1 + sqrt(aW_avg))) * R_ij / T_i
 */
sched_decision_t sched_exp_pf(sched_context_t *ctx, double delay_weight);

/* ================================================================
 * L5: Throughput Calculation & Fairness Metrics
 * ================================================================ */

/** Compute achievable rate per RB using Shannon formula (with alpha attenuation) */
double sched_rb_rate_bps(double sinr_db, double rb_bandwidth_hz,
                          double alpha_attenuation);

/** Update exponential moving average throughput
 *  T_i(t+1) = (1 - alpha) * T_i(t) + alpha * R_i(t)
 */
double sched_update_avg_throughput(double old_avg, double current_rate,
                                    double alpha);

/** Compute Jain's fairness index: J = (sum x_i)^2 / (N * sum x_i^2)
 *  J = 1: perfect fairness, J = 1/N: worst fairness
 *  Reference: Jain, "The Art of Computer Systems Performance Analysis" (1991)
 */
double sched_jain_fairness(const double *throughputs, int n);

/** Compute cell spectral efficiency (bps/Hz/cell) */
double sched_cell_spectral_efficiency_bps_per_hz(
    double total_throughput_bps, double total_bandwidth_hz);

/** Compute cell-edge throughput (5th percentile) from throughput array */
double sched_cell_edge_throughput_bps(double *throughputs, int n,
                                       double percentile);

/** Compute user throughput percentiles */
void sched_throughput_percentiles(const double *throughputs, int n,
                                   double *p5, double *p50, double *p95);

#endif /* CELL_NETWORK_SCHEDULER_H */
