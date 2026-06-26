/**
 * @file cell_network_power.h
 * @brief Cellular Network ? Power Control (L5)
 *
 * Reference: 3GPP TS 38.213 "NR Physical Layer Procedures for Control"
 *            3GPP TS 36.213 "E-UTRA Physical Layer Procedures"
 *            Goldsmith (2005) Ch. 15; Sesia et al. (2011) Ch. 20
 *
 * Implements open-loop power control (OLPC), closed-loop power
 * control (CLPC), fractional path loss compensation, and
 * uplink/downlink power allocation.
 */

#ifndef CELL_NETWORK_POWER_H
#define CELL_NETWORK_POWER_H

#include "cell_network_defs.h"

/* ================================================================
 * L5: Power Control Parameters
 * ================================================================ */

/** Uplink power control configuration (NR PUSCH) */
typedef struct {
    double p0_nominal_dbm;         /* P0_nominal: -90 to -60 dBm typical */
    double alpha;                  /* Fractional path loss compensation 0..1 */
    double p_max_dbm;              /* UE max power (23 dBm typical) */
    int    num_rbs;                /* Number of allocated RBs */
    double delta_mcs_db;           /* MCS-based adjustment */
    double closed_loop_correction_db; /* Accumulated TPC commands */
    power_control_mode_t mode;
} ul_pc_params_t;

/** Downlink power allocation per RB */
typedef struct {
    double  total_power_dbm;       /* Total gNB transmit power */
    double  pa_db;                 /* Power ratio: rho_A (PDSCH-to-RS) */
    double  pb;                    /* Power ratio: rho_B / rho_A */
    int     num_rbs_total;         /* Total RBs in system bandwidth */
    int    *rb_power_allocation;   /* Per-RB power boost (index) */
} dl_pa_params_t;

/* ================================================================
 * L5: Uplink Power Control (NR PUSCH)
 * ================================================================ */

/** Compute UL transmit power for PUSCH (NR):
 *  P_PUSCH = min{P_CMAX, P0 + 10*log10(2^mu * M_RB) + alpha*PL + Delta_TF + f}
 *  where: P0 = P0_nominal + P0_UE_specific
 *         alpha = fractional path loss compensation factor
 *         PL = downlink path loss estimate (dB)
 *         Delta_TF = MCS-based offset
 *         f = closed-loop correction
 *
 *  Reference: 3GPP TS 38.213 Sec 7.1.1
 */
double nr_pusch_power_dbm(const ul_pc_params_t *params,
                           double pl_db, double p0_ue_specific_db);

/** Open-loop only: f = 0, Delta_TF = 0 */
double nr_olpc_power_dbm(double p0_total_dbm, double alpha,
                          double pl_db, int num_rbs, double p_max_dbm);

/** Closed-loop TPC accumulation:
 *  f(i) = f(i-1) + delta_PUSCH
 *  where delta_PUSCH from TPC command table: {-1, 0, 1, 3} dB
 */
double nr_clpc_accumulate(double prev_f_db, int tpc_command_index);

/** UL power headroom: PH = P_CMAX - P_PUSCH  (dB)
 *  Positive: remaining power; negative: UE power limited
 */
double nr_power_headroom_db(double p_cmax_dbm, double p_pusch_dbm);

/** Determine if UE is power-limited (PH < threshold) */
int nr_is_power_limited(double power_headroom_db, double threshold_db);

/* ================================================================
 * L5: Fractional Power Control (FPC)
 * ================================================================ */

/** Fractional Path Loss Compensation:
 *  alpha = 1: full compensation (cell-edge performance)
 *  alpha = 0: no compensation (max throughput, high interference)
 *  Typical: alpha = 0.7-0.8 for balanced performance
 *
 *  Reference: Goldsmith (2005) Ch. 15.4
 */
double fpc_target_sinr_db(double p0_db, double alpha, double pl_db,
                           double noise_floor_dbm);

/** Optimize alpha for max sum-log-utility:
 *  max sum_i log(SINR_i) = max sum_i log(P_i * g_i / I_i)
 */
double fpc_optimal_alpha(int n_ues, const double *path_losses_db,
                          const double *interference_db,
                          double noise_floor_dbm, double p_max_dbm);

/** FPC interference impact: estimate average interference rise */
double fpc_interference_rise_db(double alpha_old, double alpha_new,
                                 int n_cells, double avg_pl_db);

/* ================================================================
 * L5: Downlink Power Allocation
 * ================================================================ */

/** Compute EPRE (Energy Per Resource Element):
 *  EPRE_RS = Total_power / (N_RB * N_SC_per_RB)
 *  EPRE_PDSCH = EPRE_RS + rho_A (or rho_B for type B symbols)
 *  rho_A: {-6, -4.77, -3, -1.77, 0, 1, 2, 3} dB
 *  rho_B / rho_A ? {1, 4/5, 3/5, 2/5}
 */
double dl_epre_rs_dbm(double total_power_dbm, int num_rbs_total);
double dl_epre_pdsch_a_dbm(double epre_rs_dbm, double pa_db);
double dl_epre_pdsch_b_dbm(double epre_rs_dbm, double pa_db, double pb);

/** Water-filling power allocation across RBs:
 *  P_k = max(0, mu - 1/gamma_k)
 *  where gamma_k = |h_k|^2 / (N0 * B_k) is channel gain-to-noise ratio
 *  mu is water level chosen to meet total power constraint.
 *
 *  Complexity: O(N log N)
 *  Reference: Goldsmith (2005) Ch. 4.4.2
 */
int dl_waterfill_power(const double *channel_gains_linear, int n_rbs,
                        double total_power_linear, double noise_per_rb,
                        double *power_allocation);

/* ================================================================
 * L5: Inter-Cell Interference Coordination (ICIC)
 * ================================================================ */

/** ICIC power pattern: reduce power on certain RBs for cell-edge users */
typedef struct {
    int    num_rbs;
    int   *power_pattern;       /* 0=normal, 1=reduced */
    double reduced_power_offset_db;
} icic_pattern_t;

/** Generate soft frequency reuse pattern:
 *  Inner (center) band: full power, reuse-1
 *  Outer (edge) band:  reduced power, reuse-3
 */
int icic_soft_frequency_reuse(int num_rbs_center, int num_rbs_edge,
                               double center_power_dbm,
                               double edge_power_dbm,
                               double *per_rb_power);

#endif /* CELL_NETWORK_POWER_H */
