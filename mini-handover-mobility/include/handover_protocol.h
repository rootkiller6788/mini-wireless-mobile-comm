/**
 * @file handover_protocol.h
 * @brief Handover protocol state machines and signaling procedures (L2, L6)
 *
 * Implements the protocol-layer handover procedures for LTE, 5G NR, and WiFi
 * as specified in 3GPP and IEEE standards.
 *
 * Knowledge Coverage:
 *   L2 - Core concepts: RRC state machine, handover procedure phases
 *   L6 - Canonical problems: LTE X2/S1 handover, NR N2 handover,
 *        WiFi 802.11r fast transition, vertical handover (802.21 MIH)
 *
 * References:
 *   - 3GPP TS 36.331 (LTE RRC), TS 36.413 (S1AP), TS 36.423 (X2AP)
 *   - 3GPP TS 38.331 (NR RRC), TS 38.413 (NGAP), TS 38.423 (XnAP)
 *   - IEEE 802.11r-2008 (Fast BSS Transition)
 *   - IEEE 802.21-2017 (Media Independent Handover)
 */

#ifndef HANDOVER_PROTOCOL_H
#define HANDOVER_PROTOCOL_H

#include "handover_types.h"

/* ============================================================================
 * L2: RRC State Machine (3GPP TS 38.331)
 * ============================================================================ */

/**
 * RRCCState - RRC connection states.
 *
 * RRC_IDLE:    No RRC connection. UE camps on cell, monitors paging.
 * RRC_INACTIVE: RRC connection suspended (5G NR only, Rel-15).
 * RRC_CONNECTED: Active RRC connection. Data transfer and handover possible.
 */
typedef enum {
    RRC_IDLE = 0,
    RRC_INACTIVE = 1,
    RRC_CONNECTED = 2
} RRCState;

/**
 * ho_rrc_state_transition - Execute an RRC state transition.
 *
 * Valid transitions (3GPP TS 38.331):
 *   IDLE → CONNECTED (RRC Setup)
 *   CONNECTED → IDLE (RRC Release)
 *   INACTIVE → CONNECTED (RRC Resume)
 *   CONNECTED → INACTIVE (RRC Release with suspend)
 *   IDLE → INACTIVE (not allowed)
 *   INACTIVE → IDLE (RRC Release)
 *
 * @param current_state      Current RRC state.
 * @param target_state       Desired RRC state.
 * @param ue_context         UE context (updated with state change).
 * @return true if the transition is valid and executed.
 */
bool ho_rrc_state_transition(RRCState   current_state,
                             RRCState   target_state,
                             UEContext *ue_context);

/* ============================================================================
 * L6: LTE X2 Handover Procedure (3GPP TS 36.423)
 * ============================================================================ */

/**
 * HoX2State - X2 handover internal states.
 */
typedef enum {
    X2HO_INIT = 0,           /**< Initial state */
    X2HO_MEASUREMENT = 1,    /**< UE measurement report received */
    X2HO_HO_REQUEST = 2,     /**< HANDOVER REQUEST sent to target */
    X2HO_HO_REQUEST_ACK = 3, /**< HANDOVER REQUEST ACK received */
    X2HO_SN_STATUS = 4,      /**< SN STATUS TRANSFER in progress */
    X2HO_RRC_RECONF = 5,     /**< RRC Connection Reconfiguration sent */
    X2HO_UE_DETACH = 6,      /**< UE detached from source */
    X2HO_RANDOM_ACCESS = 7,  /**< UE performing random access to target */
    X2HO_RRC_RECONF_COMPL = 8, /**< RRC Reconfiguration Complete received */
    X2HO_PATH_SWITCH = 9,    /**< PATH SWITCH REQUEST to MME */
    X2HO_UE_CONTEXT_RELEASE = 10, /**< UE CONTEXT RELEASE at source */
    X2HO_COMPLETE = 11,      /**< Handover successfully completed */
    X2HO_FAILED = 12         /**< Handover failure */
} HoX2State;

/**
 * ho_lte_x2_procedure_step - Execute one step of LTE X2 handover procedure.
 *
 * The X2 handover (3GPP TS 36.300 §10.1.2.1) is the primary intra-MME/serving
 * gateway handover procedure in LTE. Control plane signaling goes through the
 * X2 interface directly between eNBs, while the MME is only involved for
 * path switching at the final stage.
 *
 * Procedure phases:
 *   Phase 1: Preparation (HO Request → HO Request Ack)
 *   Phase 2: Execution (SN Status Transfer → RACH to target)
 *   Phase 3: Completion (Path Switch Request → UE Context Release)
 *
 * Average interruption time: ~50 ms (3GPP Rel-8 target).
 *
 * @param current_state        Current X2 HO state.
 * @param ue                   UE context.
 * @param serving_cell         Source eNB cell.
 * @param target_cell          Target eNB cell.
 * @param ho_decision          Handover decision result.
 * @param rrc_reconf_accepted  Whether UE accepted RRC reconfiguration.
 * @param rach_success         Whether random access to target succeeded.
 * @param path_switch_ok       Whether MME accepted path switch.
 * @param[out] next_state      Next X2 HO state.
 * @return true if procedure should continue, false if complete or failed.
 */
bool ho_lte_x2_procedure_step(HoX2State          current_state,
                              UEContext         *ue,
                              const CellInfo    *serving_cell,
                              const CellInfo    *target_cell,
                              const HandoverDecision *ho_decision,
                              bool               rrc_reconf_accepted,
                              bool               rach_success,
                              bool               path_switch_ok,
                              HoX2State         *next_state);

/* ============================================================================
 * L6: 5G NR N2 Handover Procedure (3GPP TS 38.413)
 * ============================================================================ */

/**
 * ho_nr_n2_procedure_step - Execute one step of 5G NR N2 handover procedure.
 *
 * N2-based handover in 5G NR (3GPP TS 38.300 §9.2.3.1) involves the AMF
 * when there is no Xn interface between gNBs. The AMF relays handover
 * signaling between source and target gNBs.
 *
 * Key differences from LTE X2:
 *   - Uses NGAP protocol over N2 interface
 *   - Supports PDU Session-based QoS flows
 *   - Can use dual connectivity (EN-DC, NR-DC) during handover
 *
 * @param xn_available         Whether Xn interface exists between gNBs.
 * @param amf_available        Whether AMF is reachable.
 * @param pdu_sessions_active  Number of active PDU sessions.
 * @param qos_flows_per_session Average QoS flows per session.
 * @param[out] use_n2_fallback True if N2 fallback is needed.
 * @return true if N2 handover is feasible.
 */
bool ho_nr_n2_procedure_step(bool   xn_available,
                             bool   amf_available,
                             int    pdu_sessions_active,
                             int    qos_flows_per_session,
                             bool  *use_n2_fallback);

/* ============================================================================
 * L6: WiFi 802.11r Fast BSS Transition (FT)
 * ============================================================================ */

/**
 * WiFiFTState - 802.11r Fast Transition states.
 */
typedef enum {
    FT_INITIAL = 0,           /**< Initial association to current AP */
    FT_AUTH_REQUEST = 1,      /**< FT Authentication Request sent */
    FT_AUTH_RESPONSE = 2,     /**< FT Authentication Response received */
    FT_REASSOC_REQUEST = 3,   /**< FT Reassociation Request sent */
    FT_REASSOC_RESPONSE = 4,  /**< FT Reassociation Response received */
    FT_COMPLETE = 5,          /**< Fast Transition complete */
    FT_FAILED = 6             /**< FT failed, need full 802.11 auth */
} WiFiFTState;

/**
 * ho_wifi_ft_procedure_step - Execute one step of 802.11r Fast Transition.
 *
 * IEEE 802.11r (Fast BSS Transition) reduces the authentication overhead
 * during WiFi roaming. It allows the 802.1X authentication to be pre-
 * negotiated with the target AP before the STA actually roams.
 *
 * Two modes:
 *   Over-the-Air FT: STA communicates directly with target AP.
 *   Over-the-DS FT: STA communicates with target AP via current AP.
 *
 * Key reduction: Full EAP authentication (hundreds of ms) → FT (tens of ms).
 *
 * @param current_state        Current FT state.
 * @param sta_id              Station MAC address.
 * @param target_bssid        Target AP BSSID.
 * @param pmk_r1_available    Whether PMK-R1 is cached at target.
 * @param ft_over_ds          Use Over-the-DS mode (vs Over-the-Air).
 * @param mobility_domain_id  Mobility domain identifier.
 * @param[out] next_state     Next FT state.
 * @return true if FT should continue.
 */
bool ho_wifi_ft_procedure_step(WiFiFTState current_state,
                               const char *sta_id,
                               const char *target_bssid,
                               bool        pmk_r1_available,
                               bool        ft_over_ds,
                               uint16_t    mobility_domain_id,
                               WiFiFTState *next_state);

/* ============================================================================
 * L6: WiFi 802.11k Assisted Roaming
 * ============================================================================ */

/**
 * ho_wifi_11k_neighbor_report - Generate 802.11k Neighbor Report.
 *
 * IEEE 802.11k enables APs to provide neighbor reports to STAs, reducing
 * the time spent scanning for candidate APs during roaming decisions.
 *
 * The neighbor report contains:
 *   - BSSID of neighbor APs
 *   - Channel number
 *   - Supported capabilities
 *   - Signal quality estimates
 *
 * @param current_bssid       Current AP BSSID.
 * @param channel             Current channel.
 * @param neighbor_bssids     Array of neighbor BSSIDs.
 * @param neighbor_channels   Array of neighbor channels.
 * @param neighbor_rssi       Array of neighbor RSSI values.
 * @param num_neighbors       Number of neighbors.
 * @param[out] report_buffer  Pre-allocated buffer for neighbor report.
 * @param[out] report_length  Length of generated report in bytes.
 */
void ho_wifi_11k_neighbor_report(const char   *current_bssid,
                                 int           channel,
                                 const char   *neighbor_bssids[],
                                 const int     neighbor_channels[],
                                 const double  neighbor_rssi[],
                                 int           num_neighbors,
                                 uint8_t      *report_buffer,
                                 int          *report_length);

/* ============================================================================
 * L6: IEEE 802.21 Media Independent Handover (MIH)
 * ============================================================================ */

/**
 * MIHLinkType - IEEE 802.21 link types.
 */
typedef enum {
    MIH_LINK_802_3 = 0,      /**< Ethernet */
    MIH_LINK_802_11 = 1,     /**< WiFi */
    MIH_LINK_802_16 = 2,     /**< WiMAX */
    MIH_LINK_3GPP = 3,       /**< 3GPP (LTE, NR) */
    MIH_LINK_3GPP2 = 4       /**< 3GPP2 (cdma2000) */
} MIHLinkType;

/**
 * ho_mih_vertical_decision - IEEE 802.21 MIH-based vertical handover decision.
 *
 * IEEE 802.21 provides a framework for seamless handover across heterogeneous
 * networks. The MIH Function (MIHF) provides link-layer intelligence and
 * network information to upper layers for handover optimization.
 *
 * MIH services:
 *   Media Independent Event Service (MIES): link up/down, quality changes
 *   Media Independent Command Service (MICS): initiate handover
 *   Media Independent Information Service (MIIS): neighbor maps, network info
 *
 * @param current_link_type   Type of current serving link.
 * @param available_links     Array of available candidate link types.
 * @param link_quality_scores Quality scores for each candidate.
 * @param link_cost_indicators Cost indicators for each candidate.
 * @param num_candidates      Number of candidate links.
 * @param prefer_wifi_offload Whether WiFi offloading is preferred.
 * @param[out] selected_link  Selected link type.
 * @param[out] selected_index Index of selected candidate.
 */
void ho_mih_vertical_decision(MIHLinkType   current_link_type,
                              const MIHLinkType *available_links,
                              const double *link_quality_scores,
                              const double *link_cost_indicators,
                              int           num_candidates,
                              bool          prefer_wifi_offload,
                              MIHLinkType  *selected_link,
                              int          *selected_index);

/* ============================================================================
 * L6: Handover Failure Recovery
 * ============================================================================ */

/**
 * ho_failure_recovery - Execute handover failure recovery procedure.
 *
 * When handover fails (HOF), the UE initiates RRC Connection Re-establishment
 * (3GPP TS 36.331 §5.3.7 / TS 38.331 §5.3.7). The procedure selects the
 * best available cell and attempts to re-establish the RRC connection.
 *
 * Key constraints:
 *   - Re-establishment must occur within T311 timer
 *   - UE selects cell with strongest RSRP that is not barred
 *   - Source cell must have UE context for successful re-establishment
 *
 * @param ue                   UE context (state updated on success).
 * @param candidate_cells      Array of detected cells for re-establishment.
 * @param num_candidates       Number of detected cells.
 * @param t311_timer_active    Whether T311 timer has not expired.
 * @param ue_context_available Whether source cell retains UE context.
 * @param min_rsrp_dbm        Minimum RSRP for cell selection.
 * @return true if re-establishment succeeds.
 */
bool ho_failure_recovery(UEContext      *ue,
                         const CellInfo *candidate_cells,
                         int             num_candidates,
                         bool            t311_timer_active,
                         bool            ue_context_available,
                         double          min_rsrp_dbm);

/* ============================================================================
 * L6: Handover Latency Model
 * ============================================================================ */

/**
 * ho_latency_estimate - Estimate total handover latency.
 *
 * Total handover interruption time consists of:
 *   T_total = T_measurement + T_report + T_prep + T_exec + T_completion
 *
 * Typical LTE X2 handover latency breakdown:
 *   - Measurement & Report: 50–200 ms (depends on measurement interval)
 *   - HO Preparation: 10–20 ms (X2 signaling)
 *   - HO Execution: 30–50 ms (RRC reconfig + RACH)
 *   - HO Completion: 10–15 ms (path switch)
 *   - Total: ~100–285 ms (with 0 ms user plane interruption ~50 ms)
 *
 * 5G NR target: 0 ms interruption (DAPS / Rel-16).
 *
 * @param meas_interval_ms     Measurement reporting interval.
 * @param ttt_ms               Time-To-Trigger.
 * @param x2_latency_ms        X2 interface latency.
 * @param rach_latency_ms      Random access latency.
 * @param path_switch_latency_ms MME/AMF path switch latency.
 * @param use_daps             Whether DAPS is used (0 interruption).
 * @return Estimated total handover latency in ms.
 */
double ho_latency_estimate(double meas_interval_ms,
                           double ttt_ms,
                           double x2_latency_ms,
                           double rach_latency_ms,
                           double path_switch_latency_ms,
                           bool   use_daps);

/**
 * ho_latency_breakdown - Provide detailed latency breakdown.
 *
 * Returns individual phase latencies for measurement, preparation, execution,
 * and completion phases of the handover procedure.
 *
 * @param meas_interval_ms     Measurement interval.
 * @param ttt_ms               TTT duration.
 * @param x2_latency_ms        X2/Xn signaling latency.
 * @param rach_latency_ms      RACH latency.
 * @param path_switch_latency_ms Path switch latency.
 * @param use_daps             DAPS mode.
 * @param[out] t_measurement   Measurement phase latency.
 * @param[out] t_preparation   Preparation phase latency.
 * @param[out] t_execution     Execution phase latency.
 * @param[out] t_completion    Completion phase latency.
 * @param[out] t_total         Total handover latency.
 */
void ho_latency_breakdown(double  meas_interval_ms,
                          double  ttt_ms,
                          double  x2_latency_ms,
                          double  rach_latency_ms,
                          double  path_switch_latency_ms,
                          bool    use_daps,
                          double *t_measurement,
                          double *t_preparation,
                          double *t_execution,
                          double *t_completion,
                          double *t_total);

#endif /* HANDOVER_PROTOCOL_H */
