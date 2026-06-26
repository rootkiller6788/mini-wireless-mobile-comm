/**
 * @file handover_types.h
 * @brief Core handover and mobility type definitions (L1 Definitions)
 *
 * Defines all fundamental data types for handover and mobility management
 * in wireless communication systems, covering 3GPP LTE/5G NR and IEEE 802.11.
 *
 * Knowledge Coverage:
 *   L1 - All core definitions: Handover types, mobility states, measurement
 *        quantities, cell identity, UE context, handover parameters
 *   L2 - Core concepts embodied in type relationships
 *
 * References:
 *   - 3GPP TS 36.331 (LTE RRC), TS 38.331 (NR RRC)
 *   - IEEE 802.11r (Fast BSS Transition), 802.21 (MIH)
 *   - Molisch, "Wireless Communications" (2011), Ch. 17
 */

#ifndef HANDOVER_TYPES_H
#define HANDOVER_TYPES_H

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* ============================================================================
 * L1: Handover Classification Types
 * ============================================================================ */

/**
 * HandoverType - Classification of handover procedures.
 *
 * HARD: Break-before-make — UE disconnects from source before connecting to target.
 * SOFT: Make-before-break — UE connects to target while maintaining source link.
 * SOFTER: Intra-cell handover between sectors of the same base station.
 * SEAMLESS: Zero packet loss handover (target of 5G URLLC).
 * HORIZONTAL: Handover within the same radio access technology (RAT).
 * VERTICAL: Handover between different RATs (e.g., LTE ↔ WiFi).
 */
typedef enum {
    HO_HARD = 0,       /**< Break-before-make handover */
    HO_SOFT = 1,       /**< Make-before-break handover */
    HO_SOFTER = 2,     /**< Intra-eNodeB sector handover */
    HO_SEAMLESS = 3,   /**< Zero-interruption handover */
    HO_HORIZONTAL = 4, /**< Same-RAT handover */
    HO_VERTICAL = 5    /**< Inter-RAT (vertical) handover */
} HandoverType;

/**
 * HandoverTrigger - Events that trigger handover procedures.
 * Based on 3GPP TS 36.331 / TS 38.331 measurement report triggering events.
 */
typedef enum {
    HO_TRIG_A1 = 0,  /**< Serving becomes better than threshold */
    HO_TRIG_A2 = 1,  /**< Serving becomes worse than threshold */
    HO_TRIG_A3 = 2,  /**< Neighbour becomes offset better than serving */
    HO_TRIG_A4 = 3,  /**< Neighbour becomes better than threshold */
    HO_TRIG_A5 = 4,  /**< Serving worse than threshold1 AND neighbour better than threshold2 */
    HO_TRIG_A6 = 5,  /**< Neighbour becomes offset better than SCell (CA) */
    HO_TRIG_B1 = 6,  /**< Inter-RAT neighbour becomes better than threshold */
    HO_TRIG_B2 = 7   /**< Serving worse than threshold1 AND inter-RAT neighbour better than threshold2 */
} HandoverTrigger;

/**
 * HandoverPhase - State machine for handover execution.
 * Maps to 3GPP TS 38.300 handover procedure phases.
 */
typedef enum {
    HO_PHASE_IDLE = 0,          /**< No handover in progress */
    HO_PHASE_MEASUREMENT = 1,   /**< UE performing measurements */
    HO_PHASE_REPORT = 2,        /**< Measurement report sent to network */
    HO_PHASE_PREPARATION = 3,   /**< Source gNB prepares target gNB */
    HO_PHASE_EXECUTION = 4,     /**< UE executes RRC reconfiguration */
    HO_PHASE_COMPLETION = 5,    /**< Path switch and UE context release */
    HO_PHASE_FAILURE = 6        /**< Handover failure (HOF) */
} HandoverPhase;

/**
 * MobilityState - UE mobility classification.
 * Defined in 3GPP TS 36.304 / TS 38.304.
 */
typedef enum {
    MOB_STATIONARY = 0,     /**< UE not moving (< 3 km/h or immobile) */
    MOB_NORMAL = 1,         /**< Low mobility (3–30 km/h, pedestrian) */
    MOB_MEDIUM = 2,         /**< Medium mobility (30–120 km/h, urban vehicle) */
    MOB_HIGH_SPEED = 3      /**< High mobility (> 120 km/h, high-speed rail) */
} MobilityState;

/* ============================================================================
 * L1: Radio Measurement Types
 * ============================================================================ */

/**
 * MeasurementQuantity - Physical layer measurement metrics.
 *
 * RSRP: Reference Signal Received Power (LTE CRS / NR SSB), unit: dBm.
 * RSRQ: Reference Signal Received Quality, unit: dB.
 * SINR: Signal-to-Interference-plus-Noise Ratio, unit: dB.
 * RSSI: Received Signal Strength Indicator, unit: dBm.
 * CQI: Channel Quality Indicator (0–15 in LTE, per 3GPP TS 36.213).
 */
typedef struct {
    double rsrp_dbm;       /**< RSRP in dBm, range [-140, -44] per 3GPP TS 36.133 */
    double rsrq_db;        /**< RSRQ in dB, range [-19.5, -3] per 3GPP TS 36.133 */
    double sinr_db;        /**< SINR in dB */
    double rssi_dbm;       /**< RSSI in dBm */
    int    cqi;            /**< CQI index [0, 15] */
    double bler_estimate;  /**< Estimated BLER [0, 1] */
} MeasurementQuantity;

/**
 * MeasurementReport - UE measurement report (3GPP TS 36.331 §5.5).
 *
 * Contains measurements for serving cell and up to 8 neighbour cells.
 * Triggered by configured measurement events (A1–A6, B1–B2).
 */
#define MAX_MEAS_CELLS 8

typedef struct {
    uint32_t ue_id;                              /**< UE identifier (C-RNTI) */
    uint32_t serving_cell_id;                     /**< Physical Cell ID of serving cell */
    MeasurementQuantity serving_meas;             /**< Serving cell measurements */
    uint32_t num_neighbour_cells;                 /**< Number of reported neighbour cells */
    uint32_t neighbour_cell_ids[MAX_MEAS_CELLS];  /**< Neighbour cell PCI list */
    MeasurementQuantity neighbour_meas[MAX_MEAS_CELLS]; /**< Neighbour measurements */
    bool     neighbour_detected;                  /**< Whether any neighbour detected */
    double   timestamp_ms;                        /**< Measurement timestamp in ms */
} MeasurementReport;

/* ============================================================================
 * L1: Cell and Network Entity Types
 * ============================================================================ */

/**
 * CellIdentity - Physical and logical cell identification.
 *
 * PCI: Physical Cell Identity (0–503 in LTE, 0–1007 in NR).
 * CGI: Cell Global Identity (MCC+MNC+CI).
 * TAC: Tracking Area Code.
 */
typedef struct {
    uint32_t pci;          /**< Physical Cell ID */
    uint64_t cgi;          /**< Cell Global Identity (E-UTRAN / NR CGI) */
    uint16_t tac;          /**< Tracking Area Code */
    uint32_t earfcn;       /**< E-UTRA Absolute Radio Frequency Channel Number */
    uint32_t nrarfcn;      /**< NR Absolute Radio Frequency Channel Number */
    double   center_freq_hz; /**< Center frequency in Hz */
    double   bandwidth_hz;  /**< Channel bandwidth in Hz */
} CellIdentity;

/**
 * CellLoadInfo - Cell load and resource utilization.
 */
typedef struct {
    uint32_t num_active_ues;        /**< Number of active UEs in cell */
    uint32_t total_prbs;            /**< Total Physical Resource Blocks */
    uint32_t used_prbs;             /**< Currently allocated PRBs */
    double   prb_utilization;       /**< PRB utilization ratio [0, 1] */
    double   cpu_load;              /**< gNB-CU CPU load [0, 100] */
    double   backhaul_utilization;  /**< Backhaul link utilization [0, 1] */
    double   average_throughput_mbps; /**< Average DL throughput per UE */
} CellLoadInfo;

/**
 * CellInfo - Complete serving/target cell information.
 */
typedef struct {
    CellIdentity identity;          /**< Cell identification */
    double       position_x;        /**< gNB/eNB X coordinate (m) in local CS */
    double       position_y;        /**< gNB/eNB Y coordinate (m) in local CS */
    double       position_z;        /**< Antenna height (m) */
    double       tx_power_dbm;      /**< Transmit power in dBm */
    double       antenna_gain_dbi;  /**< Antenna gain in dBi */
    double       coverage_radius_m; /**< Nominal coverage radius in meters */
    CellLoadInfo load;              /**< Current cell load */
    bool         is_barred;         /**< Cell barred status (access control) */
    bool         is_reserved;       /**< Cell reserved for operator use */
    uint32_t     num_neighbours;    /**< Number of configured neighbour cells */
    uint32_t     neighbour_pcis[32]; /**< Neighbour cell PCI list */
} CellInfo;

/* ============================================================================
 * L1: UE (User Equipment) Context
 * ============================================================================ */

typedef struct {
    double position_x;         /**< UE X coordinate (m) */
    double position_y;         /**< UE Y coordinate (m) */
    double position_z;         /**< UE height (m) */
    double velocity_x;         /**< Velocity X component (m/s) */
    double velocity_y;         /**< Velocity Y component (m/s) */
    double speed_mps;          /**< Speed magnitude (m/s) */
    double heading_rad;        /**< Heading angle (radians, 0 = East) */
} UEPosition;

typedef struct {
    uint32_t      ue_id;                /**< UE identifier (C-RNTI / IMSI-temp) */
    uint32_t      serving_cell_id;      /**< Current serving cell PCI */
    uint32_t      target_cell_id;       /**< Target cell PCI (if handover in progress) */
    UEPosition    position;             /**< UE position and velocity */
    MobilityState mobility_state;       /**< Classified mobility state */
    HandoverPhase ho_phase;             /**< Current handover phase */
    double        measurement_interval_ms; /**< Measurement reporting interval */
    uint32_t      handover_count;       /**< Total number of handovers performed */
    uint32_t      handover_failure_count; /**< Total handover failures */
    double        time_since_last_ho_ms; /**< Time since last handover */
    double        pingpong_timer_ms;    /**< Ping-pong avoidance timer */
    bool          is_connected;         /**< RRC connection state */
    double        session_start_ms;     /**< Session start timestamp */
    MeasurementQuantity last_measurement; /**< Most recent measurement */
} UEContext;

/* ============================================================================
 * L1: Handover Parameters (3GPP configured)
 * ============================================================================ */

/**
 * HandoverParams - Network-configured handover parameters.
 *
 * These parameters control the handover decision and execution behavior
 * as defined in 3GPP TS 36.331 / TS 38.331.
 *
 * hysteresis_db: Hysteresis margin to prevent ping-pong (typical: 2–5 dB).
 * ttt_ms: Time-To-Trigger, the duration the triggering condition must persist.
 * a3_offset_db: Offset for A3 event (neighbour must be this much better).
 * l3_filter_coeff: Layer-3 filtering coefficient (0–19, k = coeff/4).
 * max_handover_attempts: Maximum handover attempts before declaring failure.
 * handover_margin_db: Overall handover margin.
 * ci_offset_db: Cell Individual Offset (per neighbour cell).
 */
typedef struct {
    double hysteresis_db;           /**< Hysteresis for event evaluation */
    double ttt_ms;                  /**< Time-To-Trigger in ms */
    double a3_offset_db;            /**< A3 event offset (neighbour - serving + offset) */
    double a5_serving_thresh_db;    /**< A5 serving cell threshold */
    double a5_neighbour_thresh_db;  /**< A5 neighbour cell threshold */
    int    l3_filter_coeff;         /**< L3 filter coefficient k (0..19) */
    int    max_handover_attempts;   /**< Max retry count */
    double handover_margin_db;      /**< General handover margin */
    double t304_timer_ms;           /**< T304 timer: max HO completion time */
    double t310_timer_ms;           /**< T310 timer: radio link failure detection */
    double n310_count;              /**< N310: out-of-sync indication count */
    double n311_count;              /**< N311: in-sync indication count */
    double ci_offset_db[8];         /**< Cell Individual Offset per neighbour */
} HandoverParams;

/* ============================================================================
 * L1: Handover Decision Result
 * ============================================================================ */

typedef struct {
    bool        handover_triggered;     /**< Whether handover should occur */
    uint32_t    recommended_target_id;  /**< Recommended target cell PCI */
    double      decision_confidence;    /**< Confidence metric [0, 1] */
    HandoverTrigger trigger_event;      /**< Which trigger event fired */
    double      trigger_margin_db;      /**< Margin above/below trigger threshold */
    double      serving_rsrp_dbm;       /**< Serving RSRP at decision time */
    double      target_rsrp_dbm;        /**< Target RSRP at decision time */
    const char *reason;                 /**< Human-readable decision reason */
} HandoverDecision;

/* ============================================================================
 * L1: Handover Statistics
 * ============================================================================ */

typedef struct {
    uint32_t total_handover_attempts;   /**< Total HO attempts */
    uint32_t successful_handovers;      /**< Successful HO count */
    uint32_t failed_handovers;          /**< HO failure (HOF) count */
    uint32_t pingpong_handovers;        /**< Ping-pong handover count */
    uint32_t too_early_handovers;       /**< Too-early HO (RLF soon after) */
    uint32_t too_late_handovers;        /**< Too-late HO (RLF before HO) */
    uint32_t handover_to_wrong_cell;    /**< HO to wrong cell count */
    double   average_ho_duration_ms;    /**< Mean HO execution time */
    double   ho_success_rate;           /**< HO success ratio [0, 1] */
    double   pingpong_rate;             /**< Ping-pong rate [0, 1] */
    double   radio_link_failure_rate;   /**< RLF rate [0, 1] */
} HandoverStatistics;

/* ============================================================================
 * Utility Function Declarations (L1)
 * ============================================================================ */

/* MeasurementQuantity utilities */
void meas_quantity_init(MeasurementQuantity *mq);
bool meas_quantity_is_valid(const MeasurementQuantity *mq);

/* CellInfo utilities */
void cell_info_init(CellInfo *cell);
double cell_distance_to_ue(const CellInfo *cell, const UEPosition *ue_pos);

/* UEContext utilities */
void ue_context_init(UEContext *ue, uint32_t ue_id);
void ue_update_position(UEContext *ue, double dt_seconds);

/* HandoverParams utilities */
void ho_params_init_default(HandoverParams *params);

/* HandoverStatistics utilities */
void ho_stats_init(HandoverStatistics *stats);
void ho_stats_update(HandoverStatistics *stats,
                     bool success, bool pingpong, double ho_duration_ms);
void ho_stats_print_summary(const HandoverStatistics *stats);

/* MeasurementReport utilities */
void meas_report_init(MeasurementReport *report, uint32_t ue_id, uint32_t serving_id);
int  meas_report_add_neighbour(MeasurementReport *report,
                               uint32_t pci, double rsrp_dbm,
                               double rsrq_db, double sinr_db);

/* HandoverDecision utilities */
void ho_decision_init(HandoverDecision *decision);

/* ============================================================================
 * Inline String Conversion Functions
 * ============================================================================ */

/**
 * HandoverType_to_string - Convert HandoverType enum to human-readable name.
 * @param t Handover type enum value.
 * @return Static string representing the handover type.
 */
static inline const char* HandoverType_to_string(HandoverType t) {
    switch (t) {
        case HO_HARD:       return "Hard Handover (Break-before-Make)";
        case HO_SOFT:       return "Soft Handover (Make-before-Break)";
        case HO_SOFTER:     return "Softer Handover (Intra-cell Sector)";
        case HO_SEAMLESS:   return "Seamless Handover (Zero-interruption)";
        case HO_HORIZONTAL: return "Horizontal Handover (Same RAT)";
        case HO_VERTICAL:   return "Vertical Handover (Inter-RAT)";
        default:            return "Unknown Handover Type";
    }
}

/**
 * HandoverTrigger_to_string - Convert HandoverTrigger to 3GPP event name.
 */
static inline const char* HandoverTrigger_to_string(HandoverTrigger t) {
    switch (t) {
        case HO_TRIG_A1: return "Event A1 (Serving > Threshold)";
        case HO_TRIG_A2: return "Event A2 (Serving < Threshold)";
        case HO_TRIG_A3: return "Event A3 (Neighbour > Serving + Offset)";
        case HO_TRIG_A4: return "Event A4 (Neighbour > Threshold)";
        case HO_TRIG_A5: return "Event A5 (Serving < T1 AND Neighbour > T2)";
        case HO_TRIG_A6: return "Event A6 (Neighbour > SCell + Offset)";
        case HO_TRIG_B1: return "Event B1 (Inter-RAT Neighbour > Threshold)";
        case HO_TRIG_B2: return "Event B2 (Serving < T1 AND Inter-RAT > T2)";
        default:         return "Unknown Trigger Event";
    }
}

/**
 * HandoverPhase_to_string - Convert HandoverPhase to string.
 */
static inline const char* HandoverPhase_to_string(HandoverPhase p) {
    switch (p) {
        case HO_PHASE_IDLE:         return "Idle";
        case HO_PHASE_MEASUREMENT:  return "Measurement";
        case HO_PHASE_REPORT:       return "Measurement Report";
        case HO_PHASE_PREPARATION:  return "Handover Preparation";
        case HO_PHASE_EXECUTION:    return "Handover Execution";
        case HO_PHASE_COMPLETION:   return "Handover Completion";
        case HO_PHASE_FAILURE:      return "Handover Failure (HOF)";
        default:                    return "Unknown Phase";
    }
}

#endif /* HANDOVER_TYPES_H */
