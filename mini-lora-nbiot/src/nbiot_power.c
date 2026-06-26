/**
 * @file nbiot_power.c
 * @brief NB-IoT Power Saving -- eDRX, PSM, CE level selection, power modeling
 *
 * Knowledge: L2 eDRX/PSM concepts, L5 power consumption state machine,
 *            L6 battery life optimization, CE level selection
 *
 * NB-IoT power saving modes (3GPP Release 13):
 *   PSM (Power Saving Mode):
 *     - UE remains registered but unreachable
 *     - Deepest sleep: ~3 uA current draw
 *     - Periodic TAU (Tracking Area Update) for reachability
 *     - Active timer T3324: 2s - 186 min
 *     - Extended T3412: up to 413 days
 *
 *   eDRX (extended Discontinuous Reception):
 *     - UE listens for paging at extended intervals
 *     - eDRX cycle: 5.12s - 2621.44s (43 min)
 *     - PTW (Paging Time Window) for actual listening
 *     - Higher current than PSM, lower latency
 *
 *   C-DRX (Connected mode DRX):
 *     - During RRC Connected state
 *     - Short cycles for active data transfer
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nbiot_phy.h"
#include "lora_nbiot_common.h"

/* ======================================================================
   L2: PSM (Power Saving Mode) State Machine
   ====================================================================== */

/**
 * PSM state
 */
/* psm_state_t declared in nbiot_phy.h */

/* nbiot_psm_state_t declared in nbiot_phy.h */

/*
 * Initialize PSM state with typical values.
 *
 * Default: T3324=10s, T3412=24h, PSM=3uA, idle=5mA, active=50mA.
 */
void nbiot_psm_init(nbiot_psm_state_t *psm)
{
    if (!psm) return;
    memset(psm, 0, sizeof(*psm));
    psm->state = PSM_STATE_ACTIVE;
    psm->t3324_seconds = 10.0;
    psm->t3412_seconds = 24.0 * 3600.0;
    psm->psm_current_ua = 3.0;
    psm->idle_current_ma = 5.0;
    psm->active_current_ma = 50.0;
    psm->tau_duration_sec = 2.0;
    psm->tau_current_ma = 200.0;  /* TX during TAU at 23 dBm */
}

/*
 * Advance PSM state machine by dt seconds.
 *
 * State transitions:
 *   ACTIVE → (T3324 expires) → PSM
 *   PSM    → (T3412 expires) → TAU
 *   TAU    → (TAU complete)   → ACTIVE
 *
 * @param psm  PSM state (updated in place)
 * @param dt   Time step in seconds
 * @return Current state after advancement
 */
psm_state_t nbiot_psm_advance(nbiot_psm_state_t *psm, double dt)
{
    if (!psm || dt <= 0.0) return psm ? psm->state : PSM_STATE_ACTIVE;

    switch (psm->state) {
        case PSM_STATE_ACTIVE:
            /* Starting active timer */
            psm->state = PSM_STATE_ACTIVE_TIMER;
            psm->t3324_elapsed = 0.0;
            psm->t3412_elapsed = 0.0;
            break;

        case PSM_STATE_ACTIVE_TIMER:
            psm->t3324_elapsed += dt;
            psm->t3412_elapsed += dt;
            if (psm->t3324_elapsed >= psm->t3324_seconds) {
                psm->state = PSM_STATE_PSM;
                psm->t3324_elapsed = 0.0;
            }
            break;

        case PSM_STATE_PSM:
            psm->t3412_elapsed += dt;
            if (psm->t3412_elapsed >= psm->t3412_seconds) {
                psm->state = PSM_STATE_TAU;
                psm->t3412_elapsed = 0.0;
            }
            break;

        case PSM_STATE_TAU:
            psm->t3324_elapsed += dt;
            if (psm->t3324_elapsed >= psm->tau_duration_sec) {
                psm->state = PSM_STATE_ACTIVE;
                psm->tau_count++;
                psm->t3324_elapsed = 0.0;
            }
            break;

        default:
            break;
    }

    return psm->state;
}

/*
 * Get instantaneous current draw in the current PSM state.
 *
 * @param psm PSM state
 * @return Current in mA
 */
double nbiot_psm_current_ma(const nbiot_psm_state_t *psm)
{
    if (!psm) return 0.0;

    switch (psm->state) {
        case PSM_STATE_PSM:
            return psm->psm_current_ua / 1000.0;
        case PSM_STATE_TAU:
            return psm->tau_current_ma;
        case PSM_STATE_ACTIVE:
            return psm->active_current_ma;
        case PSM_STATE_ACTIVE_TIMER:
            return psm->idle_current_ma;
        default:
            return 0.0;
    }
}

/*
 * Estimate average current over the full PSM cycle.
 *
 * Average current calculation:
 *   I_avg = (T_active * I_active + T_idle * I_idle
 *          + T_psm * I_psm + T_tau * I_tau) / T_total
 *
 * @param psm PSM state with timer values configured
 * @return Average current in mA
 */
double nbiot_psm_average_current_ma(const nbiot_psm_state_t *psm)
{
    if (!psm) return 0.0;

    double t_active = 1.0;  /* Assume 1 second active per data event */
    double t_idle = psm->t3324_seconds;
    double t_psm  = psm->t3412_seconds - psm->t3324_seconds - psm->tau_duration_sec;
    if (t_psm < 0.0) t_psm = 0.0;
    double t_tau  = psm->tau_duration_sec;
    double t_total = t_idle + t_psm + t_tau + t_active;

    if (t_total <= 0.0) return 0.0;

    double e_active = t_active * psm->active_current_ma;
    double e_idle   = t_idle   * psm->idle_current_ma;
    double e_psm    = t_psm    * (psm->psm_current_ua / 1000.0);
    double e_tau    = t_tau    * psm->tau_current_ma;

    return (e_active + e_idle + e_psm + e_tau) / t_total;
}

/* ======================================================================
   L2: eDRX (extended DRX) Configuration
   ====================================================================== */

/* nbiot_edrx_config_t declared in nbiot_phy.h */

/*
 * Initialize eDRX configuration with typical values.
 *
 * Default: 81.92s cycle, 10.24s PTW, 2.56s DRX cycle.
 */
void nbiot_edrx_init(nbiot_edrx_config_t *edrx)
{
    if (!edrx) return;
    memset(edrx, 0, sizeof(*edrx));
    edrx->edrx_cycle_seconds = 81.92;
    edrx->ptw_seconds = 10.24;
    edrx->drx_cycle_seconds = 2.56;
    edrx->rx_on_duration_ms = 10.0;   /* ~1 subframe (PDCCH monitoring) */
    edrx->sleep_current_ua = 50.0;    /* Higher than PSM (retaining context) */
    edrx->rx_current_ma = 50.0;
}

/*
 * Calculate eDRX duty cycle and average current.
 *
 * Duty cycle = (PTW_duration / eDRX_cycle) * (RX_on / DRX_cycle)
 *
 * For default: DC = (10.24 / 81.92) * (0.01 / 2.56) = 0.125 * 0.0039 = 0.049%
 *
 * Average current:
 *   I_avg = DC_active * I_rx + DC_ptw_sleep * I_ptw_sleep
 *         + DC_deep_sleep * I_sleep
 *
 * @param edrx      eDRX configuration
 * @param avg_ma    Output: average current in mA
 * @param duty_pct  Output: duty cycle percentage
 */
void nbiot_edrx_power_analysis(const nbiot_edrx_config_t *edrx,
                                double *avg_ma, double *duty_pct)
{
    if (!edrx) return;

    double t_rx = edrx->rx_on_duration_ms / 1000.0;
    double t_cycle = edrx->edrx_cycle_seconds;
    double t_ptw = edrx->ptw_seconds;
    double t_drx = edrx->drx_cycle_seconds;

    if (t_cycle <= 0.0 || t_drx <= 0.0) {
        if (avg_ma) *avg_ma = 0.0;
        if (duty_pct) *duty_pct = 0.0;
        return;
    }

    /* Time in RX per PTW cycle */
    double n_drx_in_ptw = t_ptw / t_drx;
    double t_rx_per_ptw = n_drx_in_ptw * t_rx;

    /* Fraction of time in RX */
    double frac_rx = t_rx_per_ptw / t_cycle;

    /* Fraction of time in PTW sleep (DRX sleep, higher current) */
    double frac_ptw_sleep = (t_ptw - t_rx_per_ptw) / t_cycle;

    /* Fraction of time in deep sleep (between PTWs) */
    double frac_sleep = 1.0 - frac_rx - frac_ptw_sleep;

    /* Average current */
    double i_rx = edrx->rx_current_ma;
    double i_ptw_sleep = 1.0;  /* ~1 mA during DRX sleep */
    double i_sleep = edrx->sleep_current_ua / 1000.0;

    double i_avg = frac_rx * i_rx + frac_ptw_sleep * i_ptw_sleep
                 + frac_sleep * i_sleep;

    if (avg_ma) *avg_ma = i_avg;
    if (duty_pct) *duty_pct = frac_rx * 100.0;
}

/* ======================================================================
   L5: Coverage Enhancement Level Selection
   ====================================================================== */

/*
 * Select optimal Coverage Enhancement (CE) level based on RSRP.
 *
 * CE level determines the number of repetitions for NPDSCH/NPDCCH,
 * trading off data rate for coverage.
 *
 * CE Level 0 (Normal):  RSRP >= -115 dBm, up to 16 reps
 * CE Level 1 (Robust):  RSRP >= -125 dBm, up to 128 reps
 * CE Level 2 (Extreme): RSRP >= -135 dBm, up to 2048 reps
 *
 * Actual thresholds vary by deployment and network configuration.
 * The UE measures RSRP and selects the minimum CE level that
 * meets the reliability target.
 *
 * @param rsrp_dbm  Measured RSRP in dBm
 * @param ce_level  Output: recommended CE level
 * @param num_reps  Output: estimated number of repetitions needed
 * @return 0 on success
 */
int nbiot_select_ce_level(double rsrp_dbm, nbiot_ce_level_t *ce_level,
                           uint16_t *num_reps)
{
    if (!ce_level || !num_reps) return -1;

    if (rsrp_dbm >= -110.0) {
        *ce_level = NBIOT_CE_LEVEL_0;
        *num_reps = 1;
    } else if (rsrp_dbm >= -120.0) {
        *ce_level = NBIOT_CE_LEVEL_0;
        *num_reps = 4;
    } else if (rsrp_dbm >= -125.0) {
        *ce_level = NBIOT_CE_LEVEL_1;
        *num_reps = 16;
    } else if (rsrp_dbm >= -130.0) {
        *ce_level = NBIOT_CE_LEVEL_1;
        *num_reps = 64;
    } else if (rsrp_dbm >= -135.0) {
        *ce_level = NBIOT_CE_LEVEL_2;
        *num_reps = 256;
    } else if (rsrp_dbm >= -140.0) {
        *ce_level = NBIOT_CE_LEVEL_2;
        *num_reps = 1024;
    } else {
        *ce_level = NBIOT_CE_LEVEL_2;
        *num_reps = 2048;
    }

    return 0;
}

/*
 * Calculate NB-IoT data rate given CE level and MCS.
 *
 * R_effective = R_peak / N_repetitions
 *
 * Peak rates depend on the number of subcarriers and MCS index.
 * For NPDSCH (downlink), 12 SC, QPSK (MCS 0-12):
 *   Peak: 680 bits/subframe = 680 kbps (before repetition)
 *
 * With CE2 (2048 repetitions): 680 / 2048 = 332 bps (extreme coverage)
 *
 * @param mcs_index   Modulation and coding scheme index
 * @param num_reps    Number of repetitions
 * @param is_uplink   1 for NPUSCH, 0 for NPDSCH
 * @return Effective data rate in bps
 */
double nbiot_data_rate_with_ce(uint8_t mcs_index, uint16_t num_reps,
                                int is_uplink)
{
    /*
     * Peak TBS per subframe (from 3GPP TS 36.213 table, simplified)
     * MCS 0:  16 bits, MCS 2: 56 bits, MCS 4: 120 bits
     * MCS 6: 208 bits, MCS 8: 328 bits, MCS 10: 504 bits
     * MCS 12: 680 bits
     */
    uint16_t tbs_per_sf;
    switch (mcs_index) {
        case 0:  tbs_per_sf = 16;  break;
        case 1:  tbs_per_sf = 24;  break;
        case 2:  tbs_per_sf = 56;  break;
        case 3:  tbs_per_sf = 88;  break;
        case 4:  tbs_per_sf = 120; break;
        case 5:  tbs_per_sf = 160; break;
        case 6:  tbs_per_sf = 208; break;
        case 7:  tbs_per_sf = 256; break;
        case 8:  tbs_per_sf = 328; break;
        case 9:  tbs_per_sf = 408; break;
        case 10: tbs_per_sf = 504; break;
        case 11: tbs_per_sf = 600; break;
        case 12: tbs_per_sf = 680; break;
        default: tbs_per_sf = 16;
    }

    /* Uplink uses single-tone or multi-tone, roughly 1/6 of DL rate */
    if (is_uplink) tbs_per_sf = (uint16_t)((double)tbs_per_sf / 6.0);

    if (num_reps == 0) num_reps = 1;

    /* Subframes per second: 1000 (1 ms each) */
    double peak_bps = (double)tbs_per_sf * 1000.0;
    return peak_bps / (double)num_reps;
}

/*
 * Estimate NB-IoT battery life with PSM and periodic reporting.
 *
 * Combines:
 *   1. Periodic data transmission (NPUSCH + NPDSCH for ACK)
 *   2. PSM between transmissions
 *   3. Periodic TAU
 *
 * @param psm              PSM configuration
 * @param battery_mah      Battery capacity in mAh
 * @param battery_v        Battery voltage
 * @param tx_interval_s    Interval between data reports
 * @param tx_data_bytes    Payload per report
 * @param mcs_index        Uplink MCS index
 * @param num_reps         Number of repetitions
 * @return Estimated battery life in years
 */
double nbiot_battery_life_psm(const nbiot_psm_state_t *psm,
                               double battery_mah, double battery_v,
                               double tx_interval_s, uint16_t tx_data_bytes,
                               uint8_t mcs_index, uint16_t num_reps)
{
    if (!psm || tx_interval_s <= 0.0) return 0.0;

    /* Uplink data rate with repetitions */
    double ul_rate = nbiot_data_rate_with_ce(mcs_index, num_reps, 1);
    if (ul_rate <= 0.0) return 0.0;

    /* TX duration: data_bits / rate */
    double tx_duration = (double)(tx_data_bytes * 8) / ul_rate;

    /* RX duration for ACK (typically 1-2 subframes) */
    double rx_duration = 0.005;  /* 5 ms */

    /* Total active time per cycle */
    double t_active = tx_duration + rx_duration;

    /* Sleep time */
    double t_sleep = tx_interval_s - t_active;
    if (t_sleep < 0.0) t_sleep = 0.0;

    /* Energy per cycle */
    double e_tx = battery_v * (200.0 / 1000.0) * tx_duration;  /* 200 mA TX */
    double e_rx = battery_v * (psm->active_current_ma / 1000.0) * rx_duration;
    double e_sleep = battery_v * (psm->psm_current_ua / 1e6) * t_sleep;

    double e_cycle = e_tx + e_rx + e_sleep;
    if (e_cycle <= 0.0) return 0.0;

    /* Energy capacity */
    double e_cap = battery_mah * battery_v * 3.6;  /* Joules */

    double num_cycles = e_cap / e_cycle;
    return num_cycles * tx_interval_s / (365.25 * 24.0 * 3600.0);
}

/*
 * Compare PSM vs eDRX power efficiency for different application profiles.
 *
 * PSM is best for:
 *   - Infrequent reporting (hours/days)
 *   - Tolerable downlink latency (hours)
 *
 * eDRX is best for:
 *   - Moderate reporting (minutes)
 *   - Lower downlink latency (seconds to minutes)
 *   - Device-triggered communication
 *
 * @param report_interval_s  Reporting interval in seconds
 * @param psm_avg_ma         Output: PSM average current in mA
 * @param edrx_avg_ma        Output: eDRX average current in mA
 * @return Recommended mode: 0=PSM, 1=eDRX
 */
int nbiot_power_mode_select(double report_interval_s,
                             double *psm_avg_ma, double *edrx_avg_ma)
{
    /* PSM configuration: 10s active timer, daily TAU */
    nbiot_psm_state_t psm;
    nbiot_psm_init(&psm);

    double t3412 = 86400.0;  /* 24 hours */
    if (report_interval_s > t3412) t3412 = report_interval_s * 1.5;
    psm.t3412_seconds = t3412;
    psm.t3324_seconds = 10.0;

    double psm_avg = nbiot_psm_average_current_ma(&psm);

    /* eDRX configuration: cycle = min(report_interval/2, 2621.44) */
    nbiot_edrx_config_t edrx;
    nbiot_edrx_init(&edrx);
    double edrx_cycle = report_interval_s / 2.0;
    if (edrx_cycle > 2621.44) edrx_cycle = 2621.44;
    if (edrx_cycle < 5.12) edrx_cycle = 5.12;
    edrx.edrx_cycle_seconds = edrx_cycle;
    edrx.ptw_seconds = edrx_cycle / 8.0;

    double edrx_avg, duty;
    nbiot_edrx_power_analysis(&edrx, &edrx_avg, &duty);

    if (psm_avg_ma) *psm_avg_ma = psm_avg;
    if (edrx_avg_ma) *edrx_avg_ma = edrx_avg;

    return (psm_avg <= edrx_avg) ? 0 : 1;
}
