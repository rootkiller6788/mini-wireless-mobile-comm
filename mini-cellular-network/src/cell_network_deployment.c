/**
 * @file cell_network_deployment.c
 * @brief Cellular Network Deployment Planning (L6, L7)
 * Reference: 3GPP TR 36.942; Mishra (2006); Molisch (2011) Ch. 17
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "cell_network_defs.h"
#include "cell_network_link.h"
#include "cell_network_model.h"

coverage_plan_output_t plan_coverage(const coverage_plan_input_t *inp) {
    coverage_plan_output_t out; memset(&out, 0, sizeof(out));
    if (!inp || inp->target_area_sqkm <= 0.0) return out;
    double sigma = inp->shadow_std_dev_db;
    double margin_db = sigma * 1.28;
    double nf_db = -174.0 + 10.0 * log10(inp->bandwidth_hz) + inp->noise_figure_db;
    double rx_sens = nf_db + inp->target_sinr_db;
    double eirp = inp->tx_power_dbm + inp->tx_gain_dbi;
    double mapl_db = eirp + inp->rx_gain_dbi - rx_sens - margin_db - inp->penetration_loss_db;
    out.mapl_db = mapl_db;
    out.cell_radius_km = estimate_cell_range_km(mapl_db, inp->freq_mhz, inp->h_bs_m, inp->h_ue_m);
    out.cell_area_sqkm = 2.598 * out.cell_radius_km * out.cell_radius_km;
    if (out.cell_area_sqkm <= 0.0) { out.is_feasible = 0; return out; }
    double n = inp->target_area_sqkm / out.cell_area_sqkm;
    out.num_cells = (int)ceil(n);
    if (out.num_cells < 1) out.num_cells = 1;
    out.is_feasible = (out.cell_radius_km > 0.1);
    return out;
}

double isd_from_radius_km(double r) { return 1.732 * r; }
double radius_from_isd_km(double isd) { return isd / 1.732; }

int sites_for_area_hex_grid(double area_sqkm, double isd_km) {
    if (isd_km <= 0.0) return -1;
    double cell_area = 0.866 * isd_km * isd_km;
    return (int)ceil(area_sqkm / cell_area);
}

double cell_edge_sinr_db(int reuse_factor, double gamma, int n_interf,
                          double nf_dbm, double rx_ref_dbm) {
    double d_or = freq_reuse_d_over_r(reuse_factor);
    double sir_lin = pow(d_or, gamma) / (double)n_interf;
    double rx_lin = pow(10.0, rx_ref_dbm / 10.0);
    double interf_lin = rx_lin / sir_lin;
    double n_lin = pow(10.0, nf_dbm / 10.0);
    double sinr_lin = rx_lin / (interf_lin + n_lin);
    return 10.0 * log10(sinr_lin);
}

/* ================================================================
 * L7: 5G NR gNB Deployment Engineering
 * ================================================================ */

nr_deployment_output_t plan_nr_deployment(const nr_deployment_input_t *inp) {
    nr_deployment_output_t out; memset(&out, 0, sizeof(out));
    if (!inp) return out;
    double isd_u = 0.5, isd_s = 1.5, isd_r = 5.0;
    if (inp->freq_mhz > 3000.0) { isd_u = 0.2; isd_s = 0.8; isd_r = 3.0; }
    double ca_u = 0.866 * isd_u * isd_u, ca_s = 0.866 * isd_s * isd_s;
    double ca_r = 0.866 * isd_r * isd_r;
    out.num_urban_sites = (int)ceil(inp->urban_area_sqkm / ca_u);
    out.num_suburban_sites = (int)ceil(inp->suburban_area_sqkm / ca_s);
    out.num_rural_sites = (int)ceil(inp->rural_area_sqkm / ca_r);
    out.total_sites = (double)(out.num_urban_sites + out.num_suburban_sites + out.num_rural_sites);
    out.area_covered_sqkm = inp->urban_area_sqkm + inp->suburban_area_sqkm + inp->rural_area_sqkm;
    out.total_investment_million_usd = out.num_urban_sites * 0.15 + out.num_suburban_sites * 0.05 + out.num_rural_sites * 0.08;
    out.avg_cell_throughput_mbps = inp->bandwidth_mhz * 3.0 * (double)inp->mimo_layers;
    return out;
}

/* ================================================================
 * L7: Site Selection �� Greedy Set Cover
 * ================================================================ */

typedef struct { double x_m, y_m, demand_weight; } candidate_site_t;

int select_sites_greedy(const candidate_site_t *candidates, int n_candidates,
                         double cell_radius_m, int max_sites,
                         int *selected_indices, double *coverage_achieved) {
    if (!candidates || !selected_indices || n_candidates <= 0) return -1;
    int *covered = (int *)calloc((size_t)n_candidates, sizeof(int));
    if (!covered) return -1;
    int num_selected = 0; double total_demand = 0.0, covered_demand = 0.0;
    for (int i = 0; i < n_candidates; i++) total_demand += candidates[i].demand_weight;
    for (int iter = 0; iter < max_sites; iter++) {
        int best_idx = -1; double best_new = 0.0;
        for (int i = 0; i < n_candidates; i++) {
            if (covered[i]) continue;
            double new_d = candidates[i].demand_weight;
            for (int j = 0; j < n_candidates; j++) {
                if (covered[j]) continue;
                double dx = candidates[i].x_m - candidates[j].x_m;
                double dy = candidates[i].y_m - candidates[j].y_m;
                if (dx*dx + dy*dy <= cell_radius_m*cell_radius_m) new_d += candidates[j].demand_weight;
            }
            if (new_d > best_new) { best_new = new_d; best_idx = i; }
        }
        if (best_idx < 0) break;
        selected_indices[num_selected++] = best_idx;
        for (int j = 0; j < n_candidates; j++) {
            if (covered[j]) continue;
            double dx = candidates[best_idx].x_m - candidates[j].x_m;
            double dy = candidates[best_idx].y_m - candidates[j].y_m;
            if (dx*dx + dy*dy <= cell_radius_m*cell_radius_m) {
                covered[j] = 1; covered_demand += candidates[j].demand_weight;
            }
        }
    }
    *coverage_achieved = (total_demand > 0.0) ? covered_demand / total_demand : 0.0;
    free(covered);
    return num_selected;
}

/* ================================================================
 * L8: HetNet Deployment with Small Cells
 * ================================================================ */

int hetnet_small_cells_needed(double macro_area_sqkm, double hotspot_ratio,
                               double macro_isd_km, double small_cell_radius_km) {
    (void)macro_isd_km;
    double hot_area = macro_area_sqkm * hotspot_ratio;
    double small_cell_area = 2.598 * small_cell_radius_km * small_cell_radius_km;
    if (small_cell_area <= 0.0) return 0;
    return (int)ceil(hot_area / small_cell_area);
}

double hetnet_capacity_gain_db(double macro_throughput, double small_cell_throughput,
                                int n_small_cells, double offload_ratio) {
    double total_with_small = macro_throughput + (double)n_small_cells * small_cell_throughput;
    double gain = total_with_small / (macro_throughput * (1.0 + offload_ratio));
    return 10.0 * log10(gain);
}
