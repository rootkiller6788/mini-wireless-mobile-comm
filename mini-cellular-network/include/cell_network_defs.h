/**
 * @file cell_network_defs.h
 * @brief Cellular Network ? Core Definitions (L1: Knowledge Level 1)
 *
 * Reference: Molisch, "Wireless Communications" (2011), Ch. 17-18
 *            3GPP TS 38.300 "NR Overall Description"
 *            3GPP TS 36.300 "E-UTRA Overall Description"
 *
 * Core data structures, enumerations, and constants for cellular network modeling.
 */

#ifndef CELL_NETWORK_DEFS_H
#define CELL_NETWORK_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

/* ================================================================
 * L1: Cell Type Definitions
 * ================================================================ */

typedef enum {
    CELL_TYPE_MACRO  = 0,
    CELL_TYPE_MICRO  = 1,
    CELL_TYPE_PICO   = 2,
    CELL_TYPE_FEMTO  = 3,
    CELL_TYPE_UNDEFINED = 0xFF
} cell_type_t;

typedef enum {
    CELL_STATUS_ACTIVE     = 0,
    CELL_STATUS_SLEEP      = 1,
    CELL_STATUS_BARRED     = 2,
    CELL_STATUS_RESERVED   = 3,
    CELL_STATUS_DEGRADED   = 4
} cell_status_t;

/* ================================================================
 * L1: Radio Access Technology (RAT) Types
 * ================================================================ */

typedef enum {
    RAT_GERAN  = 0,
    RAT_UTRAN  = 1,
    RAT_EUTRAN = 2,
    RAT_NR     = 3,
    RAT_WIFI   = 4
} rat_type_t;

/* ================================================================
 * L1: Frequency Band Enumeration (3GPP NR FR1 + FR2)
 * ================================================================ */

typedef enum {
    NR_BAND_N1   = 1,
    NR_BAND_N3   = 3,
    NR_BAND_N5   = 5,
    NR_BAND_N7   = 7,
    NR_BAND_N8   = 8,
    NR_BAND_N20  = 20,
    NR_BAND_N28  = 28,
    NR_BAND_N38  = 38,
    NR_BAND_N41  = 41,
    NR_BAND_N77  = 77,
    NR_BAND_N78  = 78,
    NR_BAND_N79  = 79,
    NR_BAND_N257 = 257,
    NR_BAND_N258 = 258,
    NR_BAND_N260 = 260,
    NR_BAND_N261 = 261
} nr_band_t;

typedef enum {
    FR1_SUB6_GHZ = 1,
    FR2_MMWAVE   = 2
} freq_range_t;

/* ================================================================
 * L1: Network Element Types (5GC + EPC)
 * ================================================================ */

typedef enum {
    NE_UE     = 0,
    NE_GNB    = 1,
    NE_ENB    = 2,
    NE_AMF    = 3,
    NE_SMF    = 4,
    NE_UPF    = 5,
    NE_MME    = 6,
    NE_SGW    = 7,
    NE_PGW    = 8,
    NE_HSS    = 9,
    NE_UDM    = 10,
    NE_AUSF   = 11,
    NE_NRF    = 12,
    NE_NSSF   = 13,
    NE_PCF    = 14
} network_element_t;

/* ================================================================
 * L1: Measurement Quantities
 * ================================================================ */

typedef double rsrp_dbm_t;
typedef double rsrq_db_t;
typedef double rssi_dbm_t;
typedef double sinr_db_t;
typedef uint8_t cqi_t;
typedef uint8_t pmi_t;
typedef uint8_t ri_t;

/* ================================================================
 * L1: Measurement Report Structure
 * ================================================================ */

#define MAX_NEIGHBOR_CELLS  32
#define MAX_UE_PER_GNB      256
#define MAX_GNB_IN_CLUSTER  64
#define MAX_LAYERS          4
#define MAX_HANDOVER_CANDIDATES 16

typedef struct {
    uint32_t    cell_id;
    rsrp_dbm_t  rsrp;
    rsrq_db_t   rsrq;
    rssi_dbm_t  rssi;
    sinr_db_t   sinr;
    int         is_detected;
} cell_meas_t;

typedef struct {
    cell_meas_t  serving_cell;
    cell_meas_t  neighbor_cells[MAX_NEIGHBOR_CELLS];
    int          num_neighbors;
    uint32_t     timestamp_ms;
} ue_meas_report_t;

/* ================================================================
 * L1: Physical Cell Identity (PCI)
 * ================================================================ */

typedef struct {
    int n_id1;
    int n_id2;
} pci_t;

/* ================================================================
 * L1: Subcarrier Spacing & Numerology (NR)
 * ================================================================ */

typedef enum {
    NUMEROLOGY_0  = 0,
    NUMEROLOGY_1  = 1,
    NUMEROLOGY_2  = 2,
    NUMEROLOGY_3  = 3,
    NUMEROLOGY_4  = 4
} numerology_t;

static inline double numer_scs_khz(numerology_t mu) {
    return 15.0 * pow(2.0, (double)mu);
}

static inline double numer_symbol_us(numerology_t mu) {
    return 1000.0 / numer_scs_khz(mu);
}

static inline double numer_slot_ms(numerology_t mu) {
    return 1.0 / pow(2.0, (double)mu);
}

/* ================================================================
 * L1: QoS Parameters
 * ================================================================ */

typedef struct {
    uint8_t  five_qi;
    double   packet_delay_budget_ms;
    double   packet_error_rate;
    int      is_gbr;
    int      default_priority;
    char     service_desc[64];
} qos_profile_t;

#define NUM_STANDARD_5QI  10

/* ================================================================
 * L2: Channel Types
 * ================================================================ */

typedef enum {
    LCH_BCCH  = 0,
    LCH_PCCH  = 1,
    LCH_CCCH  = 2,
    LCH_DCCH  = 3,
    LCH_DTCH  = 4,
    LCH_UNKNOWN = 0xFF
} logical_ch_t;

typedef enum {
    TCH_BCH   = 0,
    TCH_PCH   = 1,
    TCH_DL_SCH = 2,
    TCH_UL_SCH = 3,
    TCH_RACH  = 4
} transport_ch_t;

typedef enum {
    PCH_PBCH    = 0,
    PCH_PDCCH   = 1,
    PCH_PDSCH   = 2,
    PCH_PUCCH   = 3,
    PCH_PUSCH   = 4,
    PCH_PRACH   = 5,
    PCH_PSS     = 6,
    PCH_SSS     = 7
} physical_ch_t;

/* ================================================================
 * L1: UE State Machine
 * ================================================================ */

typedef enum {
    RRC_IDLE            = 0,
    RRC_INACTIVE        = 1,
    RRC_CONNECTED       = 2,
    RRC_SETUP           = 3,
    RRC_RELEASE         = 4
} rrc_state_t;

typedef enum {
    EMM_DEREGISTERED   = 0,
    EMM_REGISTERED     = 1,
    EMM_DEREGISTERED_INIT = 2
} emm_state_t;

typedef enum {
    ECM_IDLE    = 0,
    ECM_CONNECTED = 1
} ecm_state_t;

/* ================================================================
 * L1: UE context
 * ================================================================ */
typedef struct {
    uint64_t     imsi;
    uint32_t     m_tmsi;
    rrc_state_t  rrc_state;
    emm_state_t  emm_state;
    ecm_state_t  ecm_state;
    uint32_t     serving_pci;
    uint32_t     serving_enb_id;
    double       last_activity_ms;
} ue_context_t;

/* ================================================================
 * L1: Base station physical parameters
 * ================================================================ */

typedef struct {
    uint32_t    cell_id;
    cell_type_t type;
    double      latitude_deg;
    double      longitude_deg;
    double      altitude_m;
    double      tx_power_dbm;
    double      antenna_gain_dbi;
    double      antenna_height_m;
    double      cable_loss_db;
    double      noise_figure_db;
    double      center_freq_mhz;
    double      bandwidth_mhz;
    nr_band_t   band;
    numerology_t numerology;
    double      mec_delay_ms;
    cell_status_t status;
} gnb_params_t;

/* ================================================================
 * L1: UE physical parameters
 * ================================================================ */

typedef struct {
    uint32_t    ue_id;
    double      tx_power_dbm_max;
    double      antenna_gain_dbi;
    double      antenna_height_m;
    double      noise_figure_db;
    double      speed_kmh;
    double      latitude_deg;
    double      longitude_deg;
    rat_type_t  supported_rats[4];
    int         num_rats;
} ue_params_t;

/* ================================================================
 * L1: MCS Table entry (CQI-to-MCS mapping)
 * ================================================================ */

typedef struct {
    uint8_t  cqi_index;
    char     modulation[8];
    double   code_rate;
    double   spectral_efficiency_bps_per_hz;
    double   sinr_threshold_db;
} cqi_mcs_entry_t;

/* ================================================================
 * L1: Handover event types
 * ================================================================ */

typedef enum {
    HO_EVENT_A1 = 1,
    HO_EVENT_A2 = 2,
    HO_EVENT_A3 = 3,
    HO_EVENT_A4 = 4,
    HO_EVENT_A5 = 5,
    HO_EVENT_B1 = 6,
    HO_EVENT_B2 = 7
} ho_event_type_t;

typedef enum {
    HO_TYPE_INTRA_FREQ  = 0,
    HO_TYPE_INTER_FREQ  = 1,
    HO_TYPE_INTER_RAT   = 2,
    HO_TYPE_X2          = 3,
    HO_TYPE_S1          = 4,
    HO_TYPE_NG          = 5,
    HO_TYPE_XN          = 6
} ho_type_t;

/* ================================================================
 * L1: Scheduling algorithm type
 * ================================================================ */

typedef enum {
    SCHED_RR              = 0,
    SCHED_MAX_CI          = 1,
    SCHED_PROPORTIONAL_FAIR = 2,
    SCHED_ROUND_ROBIN_FREQ  = 3,
    SCHED_EXP_PF          = 4
} scheduler_type_t;

/* ================================================================
 * L1: Power control mode
 * ================================================================ */

typedef enum {
    PC_OPEN_LOOP  = 0,
    PC_CLOSED_LOOP = 1,
    PC_OLPC_WITH_CLPC = 2
} power_control_mode_t;

/* ================================================================
 * L1: Function declarations for defs.c utilities
 * ================================================================ */

const char *cell_type_str(cell_type_t t);
const char *rat_type_str(rat_type_t r);
const char *network_element_str(network_element_t ne);
const char *rrc_state_str(rrc_state_t s);
uint32_t pci_compute(int n_id1, int n_id2);
void pci_decompose(uint32_t pci, int *n_id1, int *n_id2);
double nr_band_center_freq_mhz(nr_band_t band);
freq_range_t nr_band_freq_range(nr_band_t band);
int qos_init_standard_profiles(qos_profile_t *profiles, int max_n);
void gnb_params_init_default(gnb_params_t *p, uint32_t cell_id);
void ue_params_init_default(ue_params_t *p, uint32_t ue_id);
void ue_meas_report_init(ue_meas_report_t *rpt);
void ue_context_init(ue_context_t *ctx, uint64_t imsi);
double thermal_noise_power_dbm(double bandwidth_hz, double noise_figure_db);
double thermal_noise_psd_dbm_per_hz(double noise_figure_db);
int nr_num_rbs(double bandwidth_mhz, numerology_t mu);
int nr_symbols_per_slot(void);
int nr_slots_per_subframe(numerology_t mu);
int nr_slots_per_frame(numerology_t mu);

/* ================================================================
 * Deployment types (L6) — declared here for test accessibility
 * ================================================================ */

typedef struct {
    double target_area_sqkm, coverage_probability, freq_mhz;
    double h_bs_m, h_ue_m, tx_power_dbm, tx_gain_dbi, rx_gain_dbi;
    double noise_figure_db, bandwidth_hz, target_sinr_db;
    double shadow_std_dev_db, penetration_loss_db;
} coverage_plan_input_t;

typedef struct {
    double mapl_db, cell_radius_km, cell_area_sqkm;
    double cell_edge_sinr_db, area_capacity_mbps_per_sqkm;
    int num_cells, is_feasible;
} coverage_plan_output_t;

coverage_plan_output_t plan_coverage(const coverage_plan_input_t *inp);

double cell_edge_sinr_db(int reuse_factor, double path_loss_exp,
                          int n_interferers, double noise_floor_dbm,
                          double rx_power_dbm_ref);

typedef struct {
    double urban_area_sqkm, suburban_area_sqkm, rural_area_sqkm;
    double freq_mhz, bandwidth_mhz, target_edge_rate_mbps;
    int mimo_layers;
} nr_deployment_input_t;

typedef struct {
    int num_urban_sites, num_suburban_sites, num_rural_sites;
    double total_sites, total_investment_million_usd;
    double area_covered_sqkm, avg_cell_throughput_mbps;
} nr_deployment_output_t;

nr_deployment_output_t plan_nr_deployment(const nr_deployment_input_t *inp);

#endif /* CELL_NETWORK_DEFS_H */
