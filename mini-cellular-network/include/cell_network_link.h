/**
 * @file cell_network_link.h
 * @brief Cellular Network Radio Link ? Path Loss, SINR, Link Budget (L3, L4)
 *
 * Reference: Molisch (2011) Ch. 4, 17; 3GPP TR 38.901 "Channel Model"
 *            Friis (1946), Hata (1980), COST 231
 *            Shannon (1948) "A Mathematical Theory of Communication"
 *
 * Implements path loss models (Okumura-Hata, COST-231 Hata, 3GPP UMa/UMi),
 * SINR calculation, Shannon capacity, link budget analysis, and
 * CQI-to-MCS mapping.
 */

#ifndef CELL_NETWORK_LINK_H
#define CELL_NETWORK_LINK_H

#include "cell_network_defs.h"

/* ================================================================
 * L3: Distance computation between gNB and UE
 * ================================================================ */

/** Haversine distance between two geographic points (km) */
double haversine_distance_km(double lat1, double lng1,
                              double lat2, double lng2);

/** Cartesian distance (meters) */
double euclidean_distance_m(double x1, double y1, double x2, double y2);

/** 3D distance including antenna heights (meters) */
double three_d_distance_m(double x1, double y1, double h1,
                           double x2, double y2, double h2);

/* ================================================================
 * L3: Path Loss Models (dB)
 * ================================================================ */

/** Free-space path loss (FSPL): PL = 20*log10(d_km) + 20*log10(f_MHz) + 32.45
 *  Reference: Friis transmission equation in dB form
 */
double fspl_db(double distance_km, double freq_mhz);

/** Okumura-Hata path loss model for urban macrocell
 *  Valid: 150 <= f_MHz <= 1500, 1 <= d_km <= 20,
 *         30 <= h_b_m <= 200, 1 <= h_m_m <= 10
 *  Returns PL in dB
 */
double hata_urban_macro_db(double freq_mhz, double d_km,
                            double h_b_m, double h_m_m);

/** Okumura-Hata for suburban areas (correction on urban) */
double hata_suburban_db(double freq_mhz, double d_km,
                         double h_b_m, double h_m_m);

/** Okumura-Hata for rural/open areas */
double hata_rural_db(double freq_mhz, double d_km,
                      double h_b_m, double h_m_m);

/** COST-231 Hata model (extends to 2000 MHz)
 *  Valid: 1500 <= f_MHz <= 2000, 1 <= d_km <= 20,
 *        30 <= h_b_m <= 200, 1 <= h_m_m <= 10
 *  C_m = 0 dB for medium city/suburban, 3 dB for metropolitan
 */
double cost231_hata_db(double freq_mhz, double d_km,
                        double h_b_m, double h_m_m, double c_m);

/** 3GPP TR 38.901 UMa (Urban Macro) path loss model for NR
 *  d_2d_m: 2D distance, d_3d_m: 3D distance (incl. heights)
 *  f_c_GHz: center frequency in GHz
 *  h_bs_m: BS height, h_ut_m: UT height
 *  Returns PL in dB
 */
double tr38901_uma_los_db(double d_2d_m, double d_3d_m,
                           double f_c_GHz, double h_bs_m, double h_ut_m);
double tr38901_uma_nlos_db(double d_2d_m, double d_3d_m,
                            double f_c_GHz, double h_bs_m, double h_ut_m);

/** 3GPP TR 38.901 UMi (Urban Micro) - Street Canyon */
double tr38901_umi_los_db(double d_2d_m, double d_3d_m,
                           double f_c_GHz, double h_bs_m, double h_ut_m);
double tr38901_umi_nlos_db(double d_2d_m, double d_3d_m,
                            double f_c_GHz, double h_bs_m, double h_ut_m);

/** Generic log-distance model: PL(d) = PL(d0) + 10*n*log10(d/d0) */
double log_distance_pl_db(double d_m, double d0_m,
                          double pl_d0_db, double path_loss_exp);

/** Combined path loss including shadow fading (log-normal, sigma dB std dev) */
double path_loss_with_shadowing_db(double d_m, double d0_m,
    double pl_d0_db, double n, double sigma_db);

/* ================================================================
 * L3: SINR Computation
 * ================================================================ */

/** Compute received power in dBm:
 *  P_rx = P_tx + G_tx + G_rx - PL - L_cable - L_body - L_penetration
 */
double rx_power_dbm(double tx_power_dbm, double tx_gain_dbi,
                     double rx_gain_dbi, double path_loss_db,
                     double cable_loss_db, double other_loss_db);

/** Compute SINR in dB:
 *  SINR = P_rx_dbm - 10*log10(10^(I_dbm/10) + 10^(N_dbm/10))
 *  where I_dbm is total interference, N_dbm is noise floor
 */
double sinr_db(double rx_power_dbm, double interference_dbm,
               double noise_floor_dbm);

/** Compute noise floor: N = -174 + 10*log10(B_Hz) + NF_dB  (dBm) */
double noise_floor_dbm(double bandwidth_hz, double noise_figure_db);

/** Compute total interference from multiple sources (sum in linear, return dBm) */
double total_interference_dbm(const double *interferer_powers_dbm, int n);

/** Convert SINR from dB to linear */
static inline double sinr_linear(double sinr_db_val) {
    return pow(10.0, sinr_db_val / 10.0);
}

/** Convert SINR from linear to dB */
static inline double sinr_to_db(double sinr_linear_val) {
    return 10.0 * log10(sinr_linear_val);
}

/* ================================================================
 * L4: Shannon-Hartley Capacity
 * ================================================================ */

/** Shannon-Hartley theorem: C = B * log2(1 + SINR_linear)
 *  Capacity in bps, bandwidth B in Hz, SINR as linear ratio
 *  Reference: Shannon (1948), Theorem 2
 */
double shannon_capacity_bps(double bandwidth_hz, double sinr_linear);

/** Shannon capacity with SINR in dB */
double shannon_capacity_from_sinr_db(double bandwidth_hz, double sinr_db_val);

/** Spectral efficiency (bps/Hz): eta = log2(1 + SINR) */
double spectral_efficiency_bps_per_hz(double sinr_linear);

/** Minimum SINR required for given spectral efficiency (inverse of Shannon) */
double min_sinr_for_efficiency_db(double target_eff_bps_per_hz);

/* ================================================================
 * L4: Link Budget
 * ================================================================ */

/** Link budget result structure */
typedef struct {
    double tx_power_dbm;
    double tx_eirp_dbm;
    double path_loss_db;
    double rx_power_dbm;
    double rx_sensitivity_dbm;
    double link_margin_db;
    double max_path_loss_db;  /* MAPL: Maximum Allowable Path Loss */
    double sinr_db;
    double cell_range_km;
    int    is_viable;
} link_budget_t;

/** Compute downlink link budget */
link_budget_t compute_downlink_budget(
    double tx_power_dbm, double tx_gain_dbi, double tx_losses_db,
    double rx_gain_dbi, double rx_noise_figure_db,
    double bandwidth_hz, double target_sinr_db,
    double path_loss_db);

/** Compute link budget with shadow fading margin */
link_budget_t compute_link_budget_with_margin(
    double tx_power_dbm, double tx_gain_dbi, double tx_losses_db,
    double rx_gain_dbi, double rx_noise_figure_db,
    double bandwidth_hz, double target_sinr_db,
    double path_loss_db, double shadow_margin_db,
    double penetration_loss_db);

/** Estimate cell range from MAPL using Okumura-Hata */
double estimate_cell_range_km(double mapl_db, double freq_mhz,
                               double h_bs_m, double h_ue_m);

/** Estimate cell range from MAPL using Cost-231 Hata */
double estimate_cell_range_cost231_km(double mapl_db, double freq_mhz,
                                       double h_bs_m, double h_ue_m,
                                       double c_m);

/* ================================================================
 * L5: CQI to MCS Mapping (Link Adaptation)
 * ================================================================ */

/** Get CQI table (3GPP TS 36.213 Table 7.2.3-1, 15 entries) */
const cqi_mcs_entry_t *cqi_table(void);

/** Map SINR (dB) to CQI index (0-15) using threshold-based mapping */
cqi_t sinr_to_cqi(sinr_db_t sinr_db_val);

/** Map CQI to spectral efficiency */
double cqi_to_efficiency(cqi_t cqi);

/** Map CQI to approximate data rate: Rate = BW * efficiency */
double cqi_to_data_rate_mbps(cqi_t cqi, double bandwidth_mhz);

/** Get required SINR threshold for a given CQI */
double cqi_sinr_threshold_db(cqi_t cqi);

/** Adaptive modulation and coding: select best CQI for given SINR */
cqi_t amc_select_mcs(sinr_db_t sinr_db_val);

#endif /* CELL_NETWORK_LINK_H */
