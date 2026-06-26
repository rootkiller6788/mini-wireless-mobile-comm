/**
 * @file cell_network_link.c
 * @brief Radio Link �� Path Loss, SINR, Link Budget, CQI (L3, L4, L5)
 *
 * Reference: Friis (1946), Hata (1980), COST 231
 *            Molisch (2011) Ch. 4; 3GPP TR 38.901
 *            Shannon (1948)
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cell_network_link.h"
#include "cell_network_defs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define KTB_DBM_PER_HZ (-173.93)

/* ================================================================
 * L3: Distance Functions
 * ================================================================ */

double haversine_distance_km(double lat1, double lng1,
                              double lat2, double lng2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlng = (lng2 - lng1) * M_PI / 180.0;
    double a = sin(dlat/2.0) * sin(dlat/2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlng/2.0) * sin(dlng/2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return 6371.0 * c;  /* Earth radius = 6371 km */
}

double euclidean_distance_m(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1, dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

double three_d_distance_m(double x1, double y1, double h1,
                           double x2, double y2, double h2) {
    double dx = x2 - x1, dy = y2 - y1, dh = h2 - h1;
    return sqrt(dx * dx + dy * dy + dh * dh);
}

/* ================================================================
 * L3: Free-Space Path Loss (Friis equation in dB)
 * ================================================================ */

double fspl_db(double distance_km, double freq_mhz) {
    if (distance_km <= 0.0 || freq_mhz <= 0.0) return 0.0;
    return 20.0 * log10(distance_km) + 20.0 * log10(freq_mhz) + 32.45;
}

/* ================================================================
 * L3: Okumura-Hata Path Loss Model
 * ================================================================ */

double hata_urban_macro_db(double freq_mhz, double d_km,
                            double h_b_m, double h_m_m) {
    if (freq_mhz < 150.0 || freq_mhz > 1500.0) return -1.0;
    if (d_km < 1.0 || d_km > 20.0) return -1.0;
    if (h_b_m < 30.0 || h_b_m > 200.0) return -1.0;
    if (h_m_m < 1.0 || h_m_m > 10.0) return -1.0;

    double a_hm;
    if (freq_mhz <= 200.0) {
        a_hm = 8.29 * pow(log10(1.54 * h_m_m), 2.0) - 1.1;
    } else if (freq_mhz <= 1500.0) {
        a_hm = 3.2 * pow(log10(11.75 * h_m_m), 2.0) - 4.97;
    } else {
        a_hm = 3.2 * pow(log10(11.75 * h_m_m), 2.0) - 4.97;
    }

    double log_f = log10(freq_mhz);
    double log_hb = log10(h_b_m);
    double log_d = log10(d_km);

    double pl_db = 69.55 + 26.16 * log_f - 13.82 * log_hb
                 - a_hm + (44.9 - 6.55 * log_hb) * log_d;
    return pl_db;
}

double hata_suburban_db(double freq_mhz, double d_km,
                         double h_b_m, double h_m_m) {
    double pl_urban = hata_urban_macro_db(freq_mhz, d_km, h_b_m, h_m_m);
    if (pl_urban < 0.0) return pl_urban;
    double correction = 2.0 * pow(log10(freq_mhz / 28.0), 2.0) + 5.4;
    return pl_urban - correction;
}

double hata_rural_db(double freq_mhz, double d_km,
                      double h_b_m, double h_m_m) {
    double pl_urban = hata_urban_macro_db(freq_mhz, d_km, h_b_m, h_m_m);
    if (pl_urban < 0.0) return pl_urban;
    double log_f = log10(freq_mhz);
    double correction = 4.78 * log_f * log_f - 18.33 * log_f + 40.94;
    return pl_urban - correction;
}

/* ================================================================
 * L3: COST-231 Hata Model
 * ================================================================ */

double cost231_hata_db(double freq_mhz, double d_km,
                        double h_b_m, double h_m_m, double c_m) {
    if (freq_mhz < 1500.0 || freq_mhz > 2000.0) return -1.0;
    if (d_km < 1.0 || d_km > 20.0) return -1.0;
    if (h_b_m < 30.0 || h_b_m > 200.0) return -1.0;
    if (h_m_m < 1.0 || h_m_m > 10.0) return -1.0;

    double a_hm = 3.2 * pow(log10(11.75 * h_m_m), 2.0) - 4.97;
    double log_f = log10(freq_mhz);
    double log_hb = log10(h_b_m);
    double log_d = log10(d_km);

    double pl_db = 46.3 + 33.9 * log_f - 13.82 * log_hb
                 - a_hm + (44.9 - 6.55 * log_hb) * log_d + c_m;
    return pl_db;
}

/* ================================================================
 * L3: 3GPP TR 38.901 Models �� UMa and UMi
 * ================================================================ */

double tr38901_uma_los_db(double d_2d_m, double d_3d_m,
                           double f_c_GHz, double h_bs_m, double h_ut_m) {
    if (d_2d_m < 10.0 || d_2d_m > 5000.0) return -1.0;
    double pl1 = 28.0 + 22.0 * log10(d_3d_m) + 20.0 * log10(f_c_GHz);
    double h_eff_bs = h_bs_m - 1.0;
    double h_eff_ut = h_ut_m - 1.0;
    double d_bp = 4.0 * h_eff_bs * h_eff_ut * f_c_GHz * 1e9 / 3e8;
    double pl2 = 28.0 + 40.0 * log10(d_3d_m) + 20.0 * log10(f_c_GHz)
               - 9.0 * log10(d_bp * d_bp + (h_bs_m - h_ut_m) * (h_bs_m - h_ut_m));
    return (d_2d_m < d_bp) ? pl1 : pl2;
}

double tr38901_uma_nlos_db(double d_2d_m, double d_3d_m,
                            double f_c_GHz, double h_bs_m, double h_ut_m) {
    double pl_los = tr38901_uma_los_db(d_2d_m, d_3d_m, f_c_GHz, h_bs_m, h_ut_m);
    if (pl_los < 0.0) return -1.0;
    double pl_nlos = 13.54 + 39.08 * log10(d_3d_m) + 20.0 * log10(f_c_GHz)
                   - 0.6 * (h_ut_m - 1.5);
    return (pl_nlos > pl_los) ? pl_nlos : pl_los;
}

double tr38901_umi_los_db(double d_2d_m, double d_3d_m,
                           double f_c_GHz, double h_bs_m, double h_ut_m) {
    if (d_2d_m < 10.0 || d_2d_m > 5000.0) return -1.0;
    double h_eff_bs = h_bs_m - 1.0;
    double h_eff_ut = h_ut_m - 1.0;
    double d_bp = 4.0 * h_eff_bs * h_eff_ut * f_c_GHz * 1e9 / 3e8;
    if (d_2d_m < d_bp) {
        return 32.4 + 21.0 * log10(d_3d_m) + 20.0 * log10(f_c_GHz);
    } else {
        return 32.4 + 40.0 * log10(d_3d_m) + 20.0 * log10(f_c_GHz)
               - 9.5 * log10(d_bp * d_bp + (h_bs_m - h_ut_m) * (h_bs_m - h_ut_m));
    }
}

double tr38901_umi_nlos_db(double d_2d_m, double d_3d_m,
                            double f_c_GHz, double h_bs_m, double h_ut_m) {
    double pl_los = tr38901_umi_los_db(d_2d_m, d_3d_m, f_c_GHz, h_bs_m, h_ut_m);
    if (pl_los < 0.0) return -1.0;
    double pl_nlos = 35.3 * log10(d_3d_m) + 22.4 + 21.3 * log10(f_c_GHz)
                   - 0.3 * (h_ut_m - 1.5);
    return (pl_nlos > pl_los) ? pl_nlos : pl_los;
}

/* ================================================================
 * L3: Generic Log-Distance Path Loss
 * ================================================================ */

double log_distance_pl_db(double d_m, double d0_m,
                          double pl_d0_db, double path_loss_exp) {
    if (d_m <= 0.0 || d0_m <= 0.0) return 0.0;
    return pl_d0_db + 10.0 * path_loss_exp * log10(d_m / d0_m);
}

double path_loss_with_shadowing_db(double d_m, double d0_m,
    double pl_d0_db, double n, double sigma_db) {
    double pl = log_distance_pl_db(d_m, d0_m, pl_d0_db, n);
    return pl + sigma_db * ((double)rand() / RAND_MAX * 2.0 - 1.0) * 3.0;
}

/* ================================================================
 * L3: SINR Computation
 * ================================================================ */

double rx_power_dbm(double tx_power_dbm, double tx_gain_dbi,
                     double rx_gain_dbi, double path_loss_db,
                     double cable_loss_db, double other_loss_db) {
    return tx_power_dbm + tx_gain_dbi + rx_gain_dbi
           - path_loss_db - cable_loss_db - other_loss_db;
}

double noise_floor_dbm(double bandwidth_hz, double noise_figure_db) {
    if (bandwidth_hz <= 0.0) return -999.0;
    return KTB_DBM_PER_HZ + 10.0 * log10(bandwidth_hz) + noise_figure_db;
}

double sinr_db(double rx_power_dbm, double interference_dbm,
               double noise_floor_dbm) {
    double n_linear = pow(10.0, noise_floor_dbm / 10.0);
    double i_linear = pow(10.0, interference_dbm / 10.0);
    double rx_linear = pow(10.0, rx_power_dbm / 10.0);
    double sinr_linear = rx_linear / (i_linear + n_linear);
    if (sinr_linear <= 0.0) return -50.0;
    return 10.0 * log10(sinr_linear);
}

double total_interference_dbm(const double *interferer_powers_dbm, int n) {
    if (!interferer_powers_dbm || n <= 0) return -200.0;
    double sum_linear = 0.0;
    for (int i = 0; i < n; i++) {
        sum_linear += pow(10.0, interferer_powers_dbm[i] / 10.0);
    }
    if (sum_linear <= 0.0) return -200.0;
    return 10.0 * log10(sum_linear);
}

/* ================================================================
 * L4: Shannon-Hartley Theorem
 * ================================================================ */

double shannon_capacity_bps(double bandwidth_hz, double sinr_linear) {
    if (bandwidth_hz <= 0.0 || sinr_linear <= 0.0) return 0.0;
    return bandwidth_hz * log2(1.0 + sinr_linear);
}

double shannon_capacity_from_sinr_db(double bandwidth_hz, double sinr_db_val) {
    double sinr_lin = pow(10.0, sinr_db_val / 10.0);
    return shannon_capacity_bps(bandwidth_hz, sinr_lin);
}

double spectral_efficiency_bps_per_hz(double sinr_linear) {
    if (sinr_linear <= 0.0) return 0.0;
    return log2(1.0 + sinr_linear);
}

double min_sinr_for_efficiency_db(double target_eff_bps_per_hz) {
    if (target_eff_bps_per_hz <= 0.0) return -50.0;
    double sinr_linear = pow(2.0, target_eff_bps_per_hz) - 1.0;
    if (sinr_linear <= 0.0) return -50.0;
    return 10.0 * log10(sinr_linear);
}

/* ================================================================
 * L4: Link Budget Analysis
 * ================================================================ */

link_budget_t compute_downlink_budget(
    double tx_power_dbm, double tx_gain_dbi, double tx_losses_db,
    double rx_gain_dbi, double rx_noise_figure_db,
    double bandwidth_hz, double target_sinr_db,
    double path_loss_db) {

    link_budget_t lb;
    memset(&lb, 0, sizeof(lb));

    lb.tx_eirp_dbm = tx_power_dbm + tx_gain_dbi - tx_losses_db;
    double rx_power = lb.tx_eirp_dbm + rx_gain_dbi - path_loss_db;
    lb.rx_power_dbm = rx_power;

    double nf = noise_floor_dbm(bandwidth_hz, rx_noise_figure_db);
    double sinr = sinr_db(rx_power, -200.0, nf);  /* No interference, noise-limited */
    lb.sinr_db = sinr;

    lb.rx_sensitivity_dbm = nf + target_sinr_db;
    lb.max_path_loss_db = lb.tx_eirp_dbm + rx_gain_dbi - lb.rx_sensitivity_dbm;
    lb.link_margin_db = lb.max_path_loss_db - path_loss_db;
    lb.is_viable = (lb.link_margin_db > 0.0);
    lb.tx_power_dbm = tx_power_dbm;
    lb.path_loss_db = path_loss_db;

    return lb;
}

link_budget_t compute_link_budget_with_margin(
    double tx_power_dbm, double tx_gain_dbi, double tx_losses_db,
    double rx_gain_dbi, double rx_noise_figure_db,
    double bandwidth_hz, double target_sinr_db,
    double path_loss_db, double shadow_margin_db,
    double penetration_loss_db) {

    double total_path_loss = path_loss_db + shadow_margin_db + penetration_loss_db;
    return compute_downlink_budget(tx_power_dbm, tx_gain_dbi, tx_losses_db,
        rx_gain_dbi, rx_noise_figure_db, bandwidth_hz, target_sinr_db,
        total_path_loss);
}

double estimate_cell_range_km(double mapl_db, double freq_mhz,
                               double h_bs_m, double h_ue_m) {
    if (mapl_db <= 0.0) return 0.0;
    double d_low = 0.1, d_high = 50.0;
    for (int iter = 0; iter < 50; iter++) {
        double d_mid = (d_low + d_high) / 2.0;
        double pl = hata_urban_macro_db(freq_mhz, d_mid, h_bs_m, h_ue_m);
        if (pl < mapl_db) d_low = d_mid;
        else d_high = d_mid;
    }
    return (d_low + d_high) / 2.0;
}

double estimate_cell_range_cost231_km(double mapl_db, double freq_mhz,
                                       double h_bs_m, double h_ue_m,
                                       double c_m) {
    if (mapl_db <= 0.0) return 0.0;
    double d_low = 0.1, d_high = 20.0;
    for (int iter = 0; iter < 50; iter++) {
        double d_mid = (d_low + d_high) / 2.0;
        double pl = cost231_hata_db(freq_mhz, d_mid, h_bs_m, h_ue_m, c_m);
        if (pl < mapl_db) d_low = d_mid;
        else d_high = d_mid;
    }
    return (d_low + d_high) / 2.0;
}

/* ================================================================
 * L5: CQI-to-MCS Mapping
 * ================================================================ */

static const cqi_mcs_entry_t cqi_tbl[16] = {
    {0,  "N/A",   0.0,   0.0,     -20.0},
    {1,  "QPSK",  0.076, 0.1523,   -7.0},
    {2,  "QPSK",  0.12,  0.2344,   -5.0},
    {3,  "QPSK",  0.19,  0.3770,   -3.0},
    {4,  "QPSK",  0.30,  0.6016,   -1.0},
    {5,  "QPSK",  0.44,  0.8770,    1.0},
    {6,  "QPSK",  0.59,  1.1758,    3.0},
    {7,  "16QAM", 0.37,  1.4766,    5.0},
    {8,  "16QAM", 0.48,  1.9141,    8.0},
    {9,  "16QAM", 0.60,  2.4063,   11.0},
    {10, "64QAM", 0.45,  2.7305,   13.0},
    {11, "64QAM", 0.55,  3.3223,   15.0},
    {12, "64QAM", 0.65,  3.9023,   17.0},
    {13, "64QAM", 0.75,  4.5234,   19.0},
    {14, "64QAM", 0.85,  5.1152,   21.0},
    {15, "64QAM", 0.93,  5.5547,   23.0}
};

const cqi_mcs_entry_t *cqi_table(void) {
    return cqi_tbl;
}

cqi_t sinr_to_cqi(sinr_db_t sinr_db_val) {
    for (int i = 15; i >= 1; i--) {
        if (sinr_db_val >= cqi_tbl[i].sinr_threshold_db)
            return (cqi_t)i;
    }
    return 0;
}

double cqi_to_efficiency(cqi_t cqi) {
    if (cqi > 15) return 0.0;
    return cqi_tbl[cqi].spectral_efficiency_bps_per_hz;
}

double cqi_to_data_rate_mbps(cqi_t cqi, double bandwidth_mhz) {
    double eff = cqi_to_efficiency(cqi);
    return eff * bandwidth_mhz;  /* Mbps = bps/Hz * MHz */
}

double cqi_sinr_threshold_db(cqi_t cqi) {
    if (cqi > 15) return -99.0;
    return cqi_tbl[cqi].sinr_threshold_db;
}

cqi_t amc_select_mcs(sinr_db_t sinr_db_val) {
    cqi_t cqi = sinr_to_cqi(sinr_db_val);
    if (cqi == 0) return 1;
    return cqi;
}
