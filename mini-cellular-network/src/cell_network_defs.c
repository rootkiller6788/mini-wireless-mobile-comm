/**
 * @file cell_network_defs.c
 * @brief Cellular Network ? Definitions & Initialization Utilities (L1)
 *
 * Reference: 3GPP TS 38.300, TS 23.501, TS 38.213
 *
 * Initialization functions for data structures, QCI/5QI profiles,
 * and cell/UE configuration helpers.
 */

#include <string.h>
#include <stdio.h>
#include "cell_network_defs.h"

/* ================================================================
 * L1: Constants
 * ================================================================ */

/** Thermal noise density: k*T0 = -174 dBm/Hz at T0 = 290 K */
#define KTB_DBM_PER_HZ  (-173.93)

/** Boltzmann constant (J/K) */
#define BOLTZMANN_K  1.380649e-23

/** Reference temperature (K) */
#define T0_KELVIN  290.0

/* ================================================================
 * L1: QString for cell_type_t
 * ================================================================ */

const char *cell_type_str(cell_type_t t) {
    static const char *names[] = {
        "Macro", "Micro", "Pico", "Femto"
    };
    if (t > CELL_TYPE_FEMTO) return "Undefined";
    return names[t];
}

/* ================================================================
 * L1: QString for RAT
 * ================================================================ */

const char *rat_type_str(rat_type_t r) {
    static const char *names[] = {
        "GERAN", "UTRAN", "E-UTRAN", "NR", "WiFi"
    };
    if (r > RAT_WIFI) return "Unknown";
    return names[r];
}

/* ================================================================
 * L1: QString for network element
 * ================================================================ */

const char *network_element_str(network_element_t ne) {
    static const char *names[] = {
        "UE", "gNB", "eNB", "AMF", "SMF", "UPF",
        "MME", "SGW", "PGW", "HSS", "UDM", "AUSF",
        "NRF", "NSSF", "PCF"
    };
    if (ne > NE_PCF) return "Unknown";
    return names[ne];
}

/* ================================================================
 * L1: RRC state string
 * ================================================================ */

const char *rrc_state_str(rrc_state_t s) {
    static const char *names[] = {
        "RRC_IDLE", "RRC_INACTIVE", "RRC_CONNECTED",
        "RRC_SETUP", "RRC_RELEASE"
    };
    if (s > RRC_RELEASE) return "Unknown";
    return names[s];
}

/* ================================================================
 * L1: PCI computation
 * ================================================================ */

/** PCI = 3 * N_ID1 + N_ID2
 *  N_ID1 ? [0,335], N_ID2 ? [0,2]
 *  Returns PCI in [0, 1007]
 */
uint32_t pci_compute(int n_id1, int n_id2) {
    if (n_id1 < 0 || n_id1 > 335 || n_id2 < 0 || n_id2 > 2)
        return 0xFFFFFFFF;  /* Invalid */
    return (uint32_t)(3 * n_id1 + n_id2);
}

/** Decompose PCI into N_ID1 and N_ID2
 *  N_ID1 = PCI / 3, N_ID2 = PCI % 3
 */
void pci_decompose(uint32_t pci, int *n_id1, int *n_id2) {
    if (pci > 1007) {
        *n_id1 = -1;
        *n_id2 = -1;
        return;
    }
    *n_id2 = (int)(pci % 3);
    *n_id1 = (int)((pci - (uint32_t)(*n_id2)) / 3);
}

/* ================================================================
 * L1: Band info ? get center frequency and range designation
 * ================================================================ */

/** Get approximate center frequency for NR band (MHz) */
double nr_band_center_freq_mhz(nr_band_t band) {
    switch (band) {
        case NR_BAND_N1:   return 2140.0;
        case NR_BAND_N3:   return 1842.5;
        case NR_BAND_N5:   return 881.5;
        case NR_BAND_N7:   return 2655.0;
        case NR_BAND_N8:   return 942.5;
        case NR_BAND_N20:  return 806.0;
        case NR_BAND_N28:  return 780.5;
        case NR_BAND_N38:  return 2595.0;
        case NR_BAND_N41:  return 2593.0;
        case NR_BAND_N77:  return 3750.0;
        case NR_BAND_N78:  return 3550.0;
        case NR_BAND_N79:  return 4700.0;
        case NR_BAND_N257: return 28000.0;
        case NR_BAND_N258: return 25875.0;
        case NR_BAND_N260: return 38500.0;
        case NR_BAND_N261: return 27925.0;
        default:           return 0.0;
    }
}

/** Get frequency range designation for a band */
freq_range_t nr_band_freq_range(nr_band_t band) {
    if (band >= NR_BAND_N257) return FR2_MMWAVE;
    return FR1_SUB6_GHZ;
}

/* ================================================================
 * L1: Standardized 5QI profiles (3GPP TS 23.501 Table 5.7.4-1)
 * ================================================================ */

/** Initialize all standard 5QI profiles */
int qos_init_standard_profiles(qos_profile_t *profiles, int max_n) {
    int n = 0;
    /* 5QI=1: GBR, Conversational Voice, PDB=100ms, PER=10^-2 */
    if (n < max_n) {
        profiles[n].five_qi = 1;
        profiles[n].packet_delay_budget_ms = 100.0;
        profiles[n].packet_error_rate = 1e-2;
        profiles[n].is_gbr = 1;
        profiles[n].default_priority = 20;
        strncpy(profiles[n].service_desc, "Conversational Voice",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=2: GBR, Conversational Video (live), PDB=150ms, PER=10^-3 */
    if (n < max_n) {
        profiles[n].five_qi = 2;
        profiles[n].packet_delay_budget_ms = 150.0;
        profiles[n].packet_error_rate = 1e-3;
        profiles[n].is_gbr = 1;
        profiles[n].default_priority = 40;
        strncpy(profiles[n].service_desc, "Conversational Video (Live)",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=3: GBR, Real Time Gaming, PDB=50ms, PER=10^-3 */
    if (n < max_n) {
        profiles[n].five_qi = 3;
        profiles[n].packet_delay_budget_ms = 50.0;
        profiles[n].packet_error_rate = 1e-3;
        profiles[n].is_gbr = 1;
        profiles[n].default_priority = 30;
        strncpy(profiles[n].service_desc, "Real Time Gaming / V2X",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=4: GBR, Buffered Video, PDB=300ms, PER=10^-6 */
    if (n < max_n) {
        profiles[n].five_qi = 4;
        profiles[n].packet_delay_budget_ms = 300.0;
        profiles[n].packet_error_rate = 1e-6;
        profiles[n].is_gbr = 1;
        profiles[n].default_priority = 50;
        strncpy(profiles[n].service_desc, "Non-Conversational Video (Buffered)",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=65: GBR, Mission Critical PTT, PDB=75ms, PER=10^-2 */
    if (n < max_n) {
        profiles[n].five_qi = 65;
        profiles[n].packet_delay_budget_ms = 75.0;
        profiles[n].packet_error_rate = 1e-2;
        profiles[n].is_gbr = 1;
        profiles[n].default_priority = 7;
        strncpy(profiles[n].service_desc, "Mission Critical PTT Voice",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=5: Non-GBR, IMS Signalling, PDB=100ms, PER=10^-6 */
    if (n < max_n) {
        profiles[n].five_qi = 5;
        profiles[n].packet_delay_budget_ms = 100.0;
        profiles[n].packet_error_rate = 1e-6;
        profiles[n].is_gbr = 0;
        profiles[n].default_priority = 10;
        strncpy(profiles[n].service_desc, "IMS Signalling",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=6: Non-GBR, Video/TCP (www, e-mail, chat), PDB=300ms, PER=10^-6 */
    if (n < max_n) {
        profiles[n].five_qi = 6;
        profiles[n].packet_delay_budget_ms = 300.0;
        profiles[n].packet_error_rate = 1e-6;
        profiles[n].is_gbr = 0;
        profiles[n].default_priority = 60;
        strncpy(profiles[n].service_desc, "Video (Buffered) / TCP Internet",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=7: Non-GBR, Voice/Video/Live, PDB=100ms, PER=10^-3 */
    if (n < max_n) {
        profiles[n].five_qi = 7;
        profiles[n].packet_delay_budget_ms = 100.0;
        profiles[n].packet_error_rate = 1e-3;
        profiles[n].is_gbr = 0;
        profiles[n].default_priority = 70;
        strncpy(profiles[n].service_desc, "Voice / Video / Interactive Gaming",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=8: Non-GBR, TCP premium (e.g. TCP-based video), PDB=300ms, PER=10^-6 */
    if (n < max_n) {
        profiles[n].five_qi = 8;
        profiles[n].packet_delay_budget_ms = 300.0;
        profiles[n].packet_error_rate = 1e-6;
        profiles[n].is_gbr = 0;
        profiles[n].default_priority = 80;
        strncpy(profiles[n].service_desc, "TCP Premium Data",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    /* 5QI=9: Non-GBR, default bearer, PDB=300ms, PER=10^-6 */
    if (n < max_n) {
        profiles[n].five_qi = 9;
        profiles[n].packet_delay_budget_ms = 300.0;
        profiles[n].packet_error_rate = 1e-6;
        profiles[n].is_gbr = 0;
        profiles[n].default_priority = 90;
        strncpy(profiles[n].service_desc, "Default Bearer (Best Effort)",
                sizeof(profiles[n].service_desc)-1);
        n++;
    }
    return n;
}

/* ================================================================
 * L1: gNB parameter initialization
 * ================================================================ */

void gnb_params_init_default(gnb_params_t *p, uint32_t cell_id) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->cell_id = cell_id;
    p->type = CELL_TYPE_MACRO;
    p->tx_power_dbm = 43.0;        /* 20 W = 43 dBm typical macro */
    p->antenna_gain_dbi = 15.0;
    p->antenna_height_m = 30.0;
    p->cable_loss_db = 2.0;
    p->noise_figure_db = 5.0;
    p->center_freq_mhz = 2140.0;
    p->bandwidth_mhz = 20.0;
    p->band = NR_BAND_N1;
    p->numerology = NUMEROLOGY_0;
    p->mec_delay_ms = 10.0;
    p->status = CELL_STATUS_ACTIVE;
}

/* ================================================================
 * L1: UE parameter initialization
 * ================================================================ */

void ue_params_init_default(ue_params_t *p, uint32_t ue_id) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->ue_id = ue_id;
    p->tx_power_dbm_max = 23.0;   /* Class 3 UE */
    p->antenna_gain_dbi = 0.0;     /* 0 dBi typical */
    p->antenna_height_m = 1.5;
    p->noise_figure_db = 9.0;      /* UE typically higher NF */
    p->speed_kmh = 3.0;           /* Pedestrian default */
    p->num_rats = 2;
    p->supported_rats[0] = RAT_NR;
    p->supported_rats[1] = RAT_EUTRAN;
}

/* ================================================================
 * L1: Measurement report initialization
 * ================================================================ */

void ue_meas_report_init(ue_meas_report_t *rpt) {
    if (!rpt) return;
    memset(rpt, 0, sizeof(*rpt));
    rpt->serving_cell.rsrp = -110.0;
    rpt->serving_cell.rsrq = -15.0;
    rpt->serving_cell.sinr = -5.0;
}

/* ================================================================
 * L1: UE context initialization
 * ================================================================ */

void ue_context_init(ue_context_t *ctx, uint64_t imsi) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->imsi = imsi;
    ctx->rrc_state = RRC_IDLE;
    ctx->emm_state = EMM_DEREGISTERED;
    ctx->ecm_state = ECM_IDLE;
}

/* ================================================================
 * L1: Thermal noise computation
 * ================================================================ */

/** Compute thermal noise power in dBm:
 *  P_noise = -174 + 10*log10(BW_Hz) + NF_dB
 *  (k*T0 = -173.93 dBm/Hz at 290K, rounded to -174)
 */
double thermal_noise_power_dbm(double bandwidth_hz, double noise_figure_db) {
    if (bandwidth_hz <= 0.0) return -999.0;
    return -173.93 + 10.0 * log10(bandwidth_hz) + noise_figure_db;
}

/** Compute thermal noise power spectral density (dBm/Hz) */
double thermal_noise_psd_dbm_per_hz(double noise_figure_db) {
    return -173.93 + noise_figure_db;
}

/* ================================================================
 * L1: numerology helpers (runtime versions)
 * ================================================================ */

/** Number of RBs for given bandwidth and numerology */
int nr_num_rbs(double bandwidth_mhz, numerology_t mu) {
    double scs_khz = numer_scs_khz(mu);
    /* Each RB = 12 subcarriers * SCS */
    double rb_bandwidth_khz = 12.0 * scs_khz;
    double bw_khz = bandwidth_mhz * 1000.0;
    /* Guard band: approximately 10% overhead */
    double usable_bw_khz = bw_khz * 0.90;
    int n_rb = (int)(usable_bw_khz / rb_bandwidth_khz);
    return n_rb;
}

/** Number of OFDM symbols per slot (normal CP) */
int nr_symbols_per_slot(void) {
    return 14;
}

/** Number of slots per subframe (1 ms) */
int nr_slots_per_subframe(numerology_t mu) {
    return (int)pow(2.0, (double)mu);
}

/** Number of slots per radio frame (10 ms) */
int nr_slots_per_frame(numerology_t mu) {
    return 10 * nr_slots_per_subframe(mu);
}
