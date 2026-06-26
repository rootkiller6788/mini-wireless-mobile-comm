/**
 * @file cell_network_power.c
 * @brief Uplink & Downlink Power Control (L5)
 * Reference: 3GPP TS 38.213; Goldsmith (2005) Ch. 15
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "cell_network_power.h"
#include "cell_network_link.h"

double nr_pusch_power_dbm(const ul_pc_params_t *params,
                           double pl_db, double p0_ue_specific_db) {
    if (!params) return -999.0;
    double p0_total = params->p0_nominal_dbm + p0_ue_specific_db;
    double rb_offset = 10.0 * log10((double)params->num_rbs);
    double pl_comp = params->alpha * pl_db;
    double p_pusch = p0_total + rb_offset + pl_comp
                   + params->delta_mcs_db + params->closed_loop_correction_db;
    if (p_pusch > params->p_max_dbm) p_pusch = params->p_max_dbm;
    return p_pusch;
}

double nr_olpc_power_dbm(double p0_total_dbm, double alpha,
                          double pl_db, int num_rbs, double p_max_dbm) {
    double rb_offset = 10.0 * log10((double)num_rbs);
    double p = p0_total_dbm + rb_offset + alpha * pl_db;
    return (p < p_max_dbm) ? p : p_max_dbm;
}

double nr_clpc_accumulate(double prev_f_db, int tpc_command_index) {
    static const double tpc_table[4] = { -1.0, 0.0, 1.0, 3.0 };
    if (tpc_command_index < 0 || tpc_command_index > 3) return prev_f_db;
    return prev_f_db + tpc_table[tpc_command_index];
}

double nr_power_headroom_db(double p_cmax_dbm, double p_pusch_dbm) {
    return p_cmax_dbm - p_pusch_dbm;
}

int nr_is_power_limited(double power_headroom_db, double threshold_db) {
    return (power_headroom_db < threshold_db) ? 1 : 0;
}

double fpc_target_sinr_db(double p0_db, double alpha, double pl_db,
                           double noise_floor_dbm) {
    double rx_power = p0_db + alpha * pl_db - pl_db;
    double sinr = rx_power - noise_floor_dbm;
    return sinr;
}

double fpc_optimal_alpha(int n_ues, const double *path_losses_db,
                          const double *interference_db,
                          double noise_floor_dbm, double p_max_dbm) {
    double best_alpha = 0.8;
    double best_sum_log_sinr = -1e12;
    for (double a = 0.0; a <= 1.0; a += 0.05) {
        double sum_log_sinr = 0.0;
        for (int i = 0; i < n_ues; i++) {
            double p = nr_olpc_power_dbm(-90.0, a, path_losses_db[i], 1, p_max_dbm);
            double rx = p - path_losses_db[i];
            double sinr = rx - (interference_db[i] > noise_floor_dbm
                                ? interference_db[i] : noise_floor_dbm);
            if (sinr > -40.0) sum_log_sinr += log(sinr_linear(sinr));
        }
        if (sum_log_sinr > best_sum_log_sinr) {
            best_sum_log_sinr = sum_log_sinr; best_alpha = a;
        }
    }
    return best_alpha;
}

double fpc_interference_rise_db(double alpha_old, double alpha_new,
                                 int n_cells, double avg_pl_db) {
    double delta_alpha = alpha_new - alpha_old;
    return 10.0 * log10(1.0 + delta_alpha * (double)n_cells * pow(10.0, -avg_pl_db / 10.0));
}

double dl_epre_rs_dbm(double total_power_dbm, int num_rbs_total) {
    if (num_rbs_total <= 0) return -999.0;
    int n_sc = num_rbs_total * 12;
    return total_power_dbm - 10.0 * log10((double)n_sc);
}

double dl_epre_pdsch_a_dbm(double epre_rs_dbm, double pa_db) {
    return epre_rs_dbm + pa_db;
}

double dl_epre_pdsch_b_dbm(double epre_rs_dbm, double pa_db, double pb) {
    double rho_b_over_rho_a = pb;
    return epre_rs_dbm + pa_db + 10.0 * log10(rho_b_over_rho_a);
}

static int compare_channel_desc(const void *a, const void *b) {
    double diff = *(const double*)b - *(const double*)a;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}

int dl_waterfill_power(const double *channel_gains_linear, int n_rbs,
                        double total_power_linear, double noise_per_rb,
                        double *power_allocation) {
    if (!channel_gains_linear || !power_allocation || n_rbs <= 0) return -1;
    double *ratios = (double *)malloc((size_t)n_rbs * sizeof(double));
    if (!ratios) return -1;
    for (int i = 0; i < n_rbs; i++) {
        ratios[i] = channel_gains_linear[i] / noise_per_rb;
    }
    double *sorted = (double *)malloc((size_t)n_rbs * sizeof(double));
    for (int i = 0; i < n_rbs; i++) sorted[i] = ratios[i];
    qsort(sorted, (size_t)n_rbs, sizeof(double), compare_channel_desc);

    double mu = 0.0;
    for (int k = n_rbs; k >= 1; k--) {
        double sum = 0.0;
        for (int i = 0; i < k; i++) sum += 1.0 / sorted[i];
        mu = (total_power_linear + sum) / (double)k;
        if (mu > 1.0 / sorted[k - 1]) break;
    }
    if (mu <= 0.0) mu = total_power_linear / (double)n_rbs;

    for (int i = 0; i < n_rbs; i++) {
        double p = mu - 1.0 / ratios[i];
        power_allocation[i] = (p > 0.0) ? p : 0.0;
    }
    free(ratios); free(sorted);
    return 0;
}

int icic_soft_frequency_reuse(int num_rbs_center, int num_rbs_edge,
                               double center_power_dbm, double edge_power_dbm,
                               double *per_rb_power) {
    if (!per_rb_power || num_rbs_center < 0 || num_rbs_edge < 0) return -1;
    int total = num_rbs_center + num_rbs_edge;
    for (int i = 0; i < num_rbs_center; i++)
        per_rb_power[i] = center_power_dbm;
    for (int i = num_rbs_center; i < total; i++)
        per_rb_power[i] = edge_power_dbm;
    return total;
}
