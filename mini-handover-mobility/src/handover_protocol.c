/**
 * @file handover_protocol.c
 * @brief Handover protocol state machines and signaling procedures (L2, L6)
 *
 * Implements protocol-layer handover procedures for 3GPP LTE/5G NR and
 * IEEE 802.11 systems. Each function implements a canonical handover
 * procedure as specified in the relevant standards.
 *
 * References:
 *   - 3GPP TS 36.300 §10.1.2 (LTE Handover)
 *   - 3GPP TS 38.300 §9.2.3 (NR Handover)
 *   - IEEE 802.11r-2008 (Fast BSS Transition)
 *   - IEEE 802.21-2017 (Media Independent Handover)
 */

#include "handover_protocol.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * L2: RRC State Machine
 *
 * 5G NR RRC states (3GPP TS 38.331 §4.2.1):
 *
 * RRC_IDLE:
 *   - PLMN selection, cell selection/reselection
 *   - Paging for mobile-terminated data
 *   - No RRC context stored in RAN
 *
 * RRC_INACTIVE (introduced in 5G NR, Rel-15):
 *   - RAN stores UE context (AS context, security keys)
 *   - Paging initiated by RAN (RAN-based notification area)
 *   - UE-controlled mobility (cell reselection)
 *   - Fast transition to CONNECTED (RRC Resume, <10ms target)
 *
 * RRC_CONNECTED:
 *   - UE has dedicated RRC connection
 *   - Network-controlled mobility (handover)
 *   - UE-specific data and control channels
 *   - RAN knows UE at cell level
 *
 * Valid transitions:
 *   IDLE → CONNECTED: RRC Setup (contention-based RACH)
 *   CONNECTED → IDLE: RRC Release
 *   INACTIVE → CONNECTED: RRC Resume (contention-free RACH possible)
 *   CONNECTED → INACTIVE: RRC Release with SuspendConfig
 *   IDLE → INACTIVE: Not allowed (must go via CONNECTED)
 *   INACTIVE → IDLE: RRC Release from INACTIVE
 */
bool ho_rrc_state_transition(RRCState   current_state,
                             RRCState   target_state,
                             UEContext *ue_context)
{
    if (!ue_context) return false;

    /* Validate transition */
    bool valid = false;
    switch (current_state) {
        case RRC_IDLE:
            valid = (target_state == RRC_CONNECTED);
            break;
        case RRC_INACTIVE:
            valid = (target_state == RRC_CONNECTED || target_state == RRC_IDLE);
            break;
        case RRC_CONNECTED:
            valid = (target_state == RRC_IDLE || target_state == RRC_INACTIVE);
            break;
        default:
            return false;
    }

    if (!valid) return false;

    /* Execute state change */
    ue_context->is_connected = (target_state == RRC_CONNECTED);

    /* Update handover phase if transitioning to CONNECTED */
    if (target_state == RRC_CONNECTED && current_state != RRC_CONNECTED) {
        ue_context->ho_phase = HO_PHASE_MEASUREMENT;
    }

    if (target_state == RRC_IDLE) {
        ue_context->ho_phase = HO_PHASE_IDLE;
    }

    return true;
}

/* ============================================================================
 * L6: LTE X2 Handover Procedure
 *
 * The X2 handover is the primary intra-LTE handover procedure, specified
 * in 3GPP TS 36.300 §10.1.2.1. It requires an X2 interface between the
 * source and target eNBs.
 *
 * Detailed procedure:
 *
 * Phase 1 — Preparation:
 *   1. Source eNB sends MEASUREMENT CONTROL to UE
 *   2. UE sends MEASUREMENT REPORT (A3/A5 event triggered)
 *   3. Source eNB makes handover decision
 *   4. Source eNB sends HANDOVER REQUEST to target eNB (X2AP)
 *       - Contains: UE context, E-RABs to be forwarded, target cell ID
 *   5. Target eNB performs admission control
 *   6. Target eNB sends HANDOVER REQUEST ACKNOWLEDGE
 *       - Contains: RRCConnectionReconfiguration (with mobilityControlInfo)
 *
 * Phase 2 — Execution:
 *   7. Source eNB sends RRCConnectionReconfiguration to UE
 *   8. Source eNB sends SN STATUS TRANSFER to target
 *       - Contains: PDCP SN, HFN for each E-RAB
 *   9. UE detaches from source, synchronizes to target (RACH)
 *   10. Target eNB sends UL allocation + TA to UE
 *   11. UE sends RRCConnectionReconfigurationComplete to target
 *
 * Phase 3 — Completion:
 *   12. Target eNB sends PATH SWITCH REQUEST to MME (S1AP)
 *   13. MME updates serving gateway (S-GW): modify bearer
 *   14. MME sends PATH SWITCH REQUEST ACKNOWLEDGE
 *   15. Target eNB sends UE CONTEXT RELEASE to source eNB (X2AP)
 *   16. Source eNB releases UE context
 *
 * This function models the state machine and returns the expected
 * next state given the current state and outcomes.
 */
bool ho_lte_x2_procedure_step(HoX2State          current_state,
                              UEContext         *ue,
                              const CellInfo    *serving_cell,
                              const CellInfo    *target_cell,
                              const HandoverDecision *ho_decision,
                              bool               rrc_reconf_accepted,
                              bool               rach_success,
                              bool               path_switch_ok,
                              HoX2State         *next_state)
{
    if (!ue || !next_state) return false;

    *next_state = current_state; /* Default: stay in current state */

    switch (current_state) {
        case X2HO_INIT:
            if (ho_decision && ho_decision->handover_triggered) {
                *next_state = X2HO_MEASUREMENT;
                ue->ho_phase = HO_PHASE_MEASUREMENT;
            }
            break;

        case X2HO_MEASUREMENT:
            /* Measurement report sent → proceed to HO request */
            if (ho_decision && ho_decision->handover_triggered) {
                *next_state = X2HO_HO_REQUEST;
                ue->ho_phase = HO_PHASE_PREPARATION;
            }
            break;

        case X2HO_HO_REQUEST:
            /* Target must have capacity (admission control) */
            if (target_cell && !target_cell->is_barred
                && target_cell->load.prb_utilization < 0.95) {
                *next_state = X2HO_HO_REQUEST_ACK;
            } else {
                *next_state = X2HO_FAILED;
                ue->ho_phase = HO_PHASE_FAILURE;
                ue->handover_failure_count++;
            }
            break;

        case X2HO_HO_REQUEST_ACK:
            *next_state = X2HO_SN_STATUS;
            break;

        case X2HO_SN_STATUS:
            *next_state = X2HO_RRC_RECONF;
            ue->ho_phase = HO_PHASE_EXECUTION;
            break;

        case X2HO_RRC_RECONF:
            if (rrc_reconf_accepted) {
                *next_state = X2HO_UE_DETACH;
                ue->target_cell_id = target_cell ? target_cell->identity.pci : 0;
            } else {
                *next_state = X2HO_FAILED;
                ue->ho_phase = HO_PHASE_FAILURE;
                ue->handover_failure_count++;
            }
            break;

        case X2HO_UE_DETACH:
            *next_state = X2HO_RANDOM_ACCESS;
            break;

        case X2HO_RANDOM_ACCESS:
            if (rach_success) {
                *next_state = X2HO_RRC_RECONF_COMPL;
            } else {
                *next_state = X2HO_FAILED;
                ue->ho_phase = HO_PHASE_FAILURE;
                ue->handover_failure_count++;
            }
            break;

        case X2HO_RRC_RECONF_COMPL:
            *next_state = X2HO_PATH_SWITCH;
            ue->ho_phase = HO_PHASE_COMPLETION;
            break;

        case X2HO_PATH_SWITCH:
            if (path_switch_ok) {
                *next_state = X2HO_UE_CONTEXT_RELEASE;
            } else {
                *next_state = X2HO_FAILED;
                ue->ho_phase = HO_PHASE_FAILURE;
                ue->handover_failure_count++;
            }
            break;

        case X2HO_UE_CONTEXT_RELEASE:
            *next_state = X2HO_COMPLETE;
            ue->ho_phase = HO_PHASE_COMPLETION;
            ue->handover_count++;
            ue->serving_cell_id = ue->target_cell_id;
            ue->target_cell_id = 0;
            ue->time_since_last_ho_ms = 0.0;
            break;

        case X2HO_COMPLETE:
            /* Handover complete — UE now in normal connected mode */
            ue->ho_phase = HO_PHASE_IDLE;
            return false; /* Procedure finished */

        case X2HO_FAILED:
            /* Handover failed — need recovery */
            ue->ho_phase = HO_PHASE_FAILURE;
            return false; /* Procedure finished (failure) */

        default:
            return false;
    }

    /* Avoid unused parameter warnings */
    (void)serving_cell;

    return true;
}

/* ============================================================================
 * L6: 5G NR N2 Handover Procedure
 *
 * The N2-based handover (3GPP TS 38.300 §9.2.3.1) is used when there is no
 * Xn interface between gNBs, or when Xn handover is not possible. The AMF
 * (Access and Mobility Management Function) relays handover signaling via
 * the N2 (NGAP) interface.
 *
 * Key differences from LTE X2:
 *   - Uses NG-RAN architecture (gNB-CU/gNB-DU split possible)
 *   - PDU Session-based QoS (each PDU session can have multiple QoS flows)
 *   - AMF involvement in all N2 handovers
 *   - Support for inter-system handover (5GS to EPS)
 *
 * When Xn is available, the handover is Xn-based (similar to LTE X2),
 * avoiding AMF involvement until path switch.
 */
bool ho_nr_n2_procedure_step(bool   xn_available,
                             bool   amf_available,
                             int    pdu_sessions_active,
                             int    qos_flows_per_session,
                             bool  *use_n2_fallback)
{
    if (!use_n2_fallback) return false;

    /* If Xn is available and has sufficient capacity, prefer Xn handover */
    if (xn_available && pdu_sessions_active <= 16) {
        *use_n2_fallback = false;
        return true; /* Xn handover is feasible */
    }

    /* N2 fallback needed */
    if (!amf_available) {
        *use_n2_fallback = false;
        return false; /* Neither Xn nor N2 available — handover impossible */
    }

    /* N2 handover is feasible but has higher latency due to AMF processing */
    *use_n2_fallback = true;

    /* Check QoS flow limits for N2 handover */
    int total_qos_flows = pdu_sessions_active * qos_flows_per_session;
    if (total_qos_flows > 64) {
        /* N2 handover with too many QoS flows may be slow */
        return true; /* Feasible but degraded */
    }

    return true;
}

/* ============================================================================
 * L6: WiFi 802.11r Fast BSS Transition (FT)
 *
 * IEEE 802.11r-2008 standardizes Fast BSS Transition to minimize the
 * authentication overhead during WiFi roaming. It is critical for VoIP
 * over WiFi, real-time applications, and seamless enterprise WiFi mobility.
 *
 * The key innovation is key hierarchy:
 *
 *   PMK-R0 (Pairwise Master Key, level-0):
 *     Derived from MSK (generated during initial EAP authentication)
 *     PMK-R0 = KDF-256(MSK, "FT-R0", SSID, MDID, R0KH-ID, S0KH-ID)
 *
 *   PMK-R1 (Pairwise Master Key, level-1):
 *     Derived from PMK-R0 for each target AP
 *     PMK-R1 = KDF-256(PMK-R0, "FT-R1", BSSID, R1KH-ID)
 *
 * The FT 4-way handshake replaces the full EAP exchange:
 *   1. FT Authentication Request/Response (2 frames)
 *   2. FT Reassociation Request/Response (2 frames)
 *   Total: 4 frames (~10-20 ms) vs full EAP (~500-1000 ms)
 *
 * Over-the-Air FT: STA communicates directly with target AP.
 * Over-the-DS FT: STA communicates with target AP via current AP.
 */
bool ho_wifi_ft_procedure_step(WiFiFTState current_state,
                               const char *sta_id,
                               const char *target_bssid,
                               bool        pmk_r1_available,
                               bool        ft_over_ds,
                               uint16_t    mobility_domain_id,
                               WiFiFTState *next_state)
{
    if (!sta_id || !target_bssid || !next_state) return false;

    *next_state = current_state;

    switch (current_state) {
        case FT_INITIAL:
            if (pmk_r1_available) {
                /* PMK-R1 is cached for target — can proceed with FT */
                *next_state = FT_AUTH_REQUEST;
            } else {
                /* Need full initial mobility domain association first */
                *next_state = FT_FAILED;
            }
            break;

        case FT_AUTH_REQUEST:
            /* FT Authentication Request sent to target */
            *next_state = FT_AUTH_RESPONSE;
            break;

        case FT_AUTH_RESPONSE:
            /* FT Authentication Response received with ANonce, SNonce */
            *next_state = FT_REASSOC_REQUEST;
            break;

        case FT_REASSOC_REQUEST:
            /* FT Reassociation Request sent (includes RSNIE with PTK derivation) */
            *next_state = FT_REASSOC_RESPONSE;
            break;

        case FT_REASSOC_RESPONSE:
            /* FT Reassociation Response with GTK, IGTK */
            *next_state = FT_COMPLETE;
            break;

        case FT_COMPLETE:
            return false; /* FT complete */

        case FT_FAILED:
            return false; /* FT failed, fall back to full 802.11 authentication */

        default:
            return false;
    }

    /* Avoid unused warnings */
    (void)ft_over_ds;
    (void)mobility_domain_id;

    return true;
}

/* ============================================================================
 * L6: WiFi 802.11k Neighbor Report
 *
 * IEEE 802.11k-2008 standardizes Radio Resource Measurement, including
 * the Neighbor Report mechanism. An AP can provide a STA with information
 * about neighboring APs, reducing the need for active scanning.
 *
 * The Neighbor Report element (802.11k §7.3.2.37) contains:
 *   - BSSID
 *   - BSSID Information (AP reachability, security, key scope, capabilities)
 *   - Operating Class (channel starting frequency)
 *   - Channel Number
 *   - PHY Type
 *   - Optional subelements (TSF offset, roaming consortium, etc.)
 *
 * Benefits:
 *   - Reduces scanning time from 100s of ms to <10 ms
 *   - Reduces power consumption (no need to probe all channels)
 *   - Improves roaming decision with network-assisted info
 */
void ho_wifi_11k_neighbor_report(const char   *current_bssid,
                                 int           channel,
                                 const char   *neighbor_bssids[],
                                 const int     neighbor_channels[],
                                 const double  neighbor_rssi[],
                                 int           num_neighbors,
                                 uint8_t      *report_buffer,
                                 int          *report_length)
{
    if (!current_bssid || !neighbor_bssids || !neighbor_channels
        || !neighbor_rssi || !report_buffer || !report_length) return;
    if (num_neighbors < 0 || num_neighbors > 32) return;

    /* Build simplified Neighbor Report
     * Format: [Element ID (1B) | Length (1B) | Neighbor List (variable)]
     * Each neighbor: [BSSID (6B) | Info (4B) | Channel (1B) | RSSI (1B)]
     */

    int offset = 0;
    report_buffer[offset++] = 0x34; /* Element ID: Neighbor Report */
    int length_offset = offset;
    offset++; /* Length field filled at end */

    for (int i = 0; i < num_neighbors; i++) {
        /* BSSID (6 bytes) — simplified: encode as string hash for demo */
        uint32_t bssid_hash = 0;
        for (int c = 0; neighbor_bssids[i][c] && c < 17; c++) {
            bssid_hash = bssid_hash * 31 + neighbor_bssids[i][c];
        }
        report_buffer[offset++] = (uint8_t)(bssid_hash & 0xFF);
        report_buffer[offset++] = (uint8_t)((bssid_hash >> 8) & 0xFF);
        report_buffer[offset++] = (uint8_t)((bssid_hash >> 16) & 0xFF);
        report_buffer[offset++] = 0x00; /* Reserved */

        /* Channel */
        report_buffer[offset++] = (uint8_t)(neighbor_channels[i] & 0xFF);

        /* RSSI quantized: map [-100, -30] dBm to [0, 255] */
        double clamped_rssi = neighbor_rssi[i];
        if (clamped_rssi < -100.0) clamped_rssi = -100.0;
        if (clamped_rssi > -30.0)  clamped_rssi = -30.0;
        uint8_t rssi_byte = (uint8_t)((clamped_rssi + 100.0) * (255.0 / 70.0));
        report_buffer[offset++] = rssi_byte;
    }

    /* Fill in the length field */
    report_buffer[length_offset] = (uint8_t)(offset - 2);

    *report_length = offset;

    /* Avoid unused warnings */
    (void)current_bssid;
    (void)channel;
}

/* ============================================================================
 * L6: IEEE 802.21 MIH Vertical Handover
 *
 * IEEE 802.21-2017 defines the Media Independent Handover (MIH) framework
 * for seamless vertical handover across heterogeneous networks (3GPP,
 * WiFi, WiMAX, Ethernet).
 *
 * MIH Function (MIHF) provides three services:
 *
 * 1. MIES (Media Independent Event Service):
 *    - Link Up/Down/Going Down/Handover Imminent
 *    - Link parameters change (signal strength, quality)
 *
 * 2. MICS (Media Independent Command Service):
 *    - MIH_Switch: Initiate link switch
 *    - MIH_Scan: Request candidate scanning
 *
 * 3. MIIS (Media Independent Information Service):
 *    - Network maps, neighbor information
 *    - Service provider info, cost, QoS
 *
 * This function implements a simple policy-based vertical handover decision
 * using MIH-style link quality and cost metrics.
 */
void ho_mih_vertical_decision(MIHLinkType   current_link_type,
                              const MIHLinkType *available_links,
                              const double *link_quality_scores,
                              const double *link_cost_indicators,
                              int           num_candidates,
                              bool          prefer_wifi_offload,
                              MIHLinkType  *selected_link,
                              int          *selected_index)
{
    if (!available_links || !link_quality_scores || !link_cost_indicators
        || !selected_link || !selected_index) return;
    if (num_candidates <= 0) return;

    double best_utility = -1e100;
    *selected_index = 0;
    *selected_link = available_links[0];

    for (int i = 0; i < num_candidates; i++) {
        /* Utility = quality_score - cost_penalty */
        double cost_penalty = link_cost_indicators[i] * 0.1;

        /* WiFi offload preference: bonus for 802.11 links */
        double offload_bonus = 0.0;
        if (prefer_wifi_offload && available_links[i] == MIH_LINK_802_11) {
            offload_bonus = 0.2;
        }

        /* Same-RAT preference: bonus for same technology */
        double same_rat_bonus = 0.0;
        if (available_links[i] == current_link_type) {
            same_rat_bonus = 0.1; /* Slight preference for horizontal HO */
        }

        double utility = link_quality_scores[i] - cost_penalty
                       + offload_bonus + same_rat_bonus;

        if (utility > best_utility) {
            best_utility = utility;
            *selected_index = i;
            *selected_link = available_links[i];
        }
    }
}

/* ============================================================================
 * L6: Handover Failure Recovery
 *
 * When handover fails, the UE initiates RRC Connection Re-establishment
 * (3GPP TS 36.331 §5.3.7 / TS 38.331 §5.3.7).
 *
 * Procedure:
 *   1. UE performs cell selection (TS 36.304 / TS 38.304)
 *   2. UE sends RRCConnectionReestablishmentRequest to selected cell
 *   3. Network responds with RRCConnectionReestablishment or Reject
 *
 * Conditions for successful re-establishment:
 *   1. Selected cell is prepared (UE context available, either at the
 *      target cell or via X2/Xn fetch from the source)
 *   2. T311 timer has not expired (default: 10 seconds in LTE)
 *   3. UE identity can be verified (short MAC-I verification)
 *
 * Failure recovery is critical for network KPI: call drop rate is the
 * most important metric for operator handover performance evaluation.
 */
bool ho_failure_recovery(UEContext      *ue,
                         const CellInfo *candidate_cells,
                         int             num_candidates,
                         bool            t311_timer_active,
                         bool            ue_context_available,
                         double          min_rsrp_dbm)
{
    if (!ue || !candidate_cells || num_candidates <= 0) return false;

    /* T311 must not have expired */
    if (!t311_timer_active) return false;

    /* UE context must be available at source cell */
    if (!ue_context_available) return false;

    /* Select best cell for re-establishment */
    int best_idx = -1;
    /* Best cell chosen based on: not barred, not reserved, highest likely RSRP.
     * For simplicity, select the first non-barred cell above threshold. */
    for (int i = 0; i < num_candidates; i++) {
        if (!candidate_cells[i].is_barred && !candidate_cells[i].is_reserved) {
            /* Check minimum RSRP: estimate from distance and tx_power */
            double dist_m = sqrt(
                pow(ue->position.position_x - candidate_cells[i].position_x, 2.0) +
                pow(ue->position.position_y - candidate_cells[i].position_y, 2.0));
            /* Friis path loss: PL(dB) = 20*log10(d_m) + 20*log10(f_Hz) - 147.55 */
            double path_loss = 20.0 * log10(dist_m + 0.001)
                             + 20.0 * log10(2.6e9) - 147.55;
            double est_rsrp = candidate_cells[i].tx_power_dbm
                            + candidate_cells[i].antenna_gain_dbi
                            - path_loss;

            if (est_rsrp >= min_rsrp_dbm) {
                best_idx = i;
                break;
            }
        }
    }

    if (best_idx < 0) return false;

    /* Re-establishment successful */
    ue->serving_cell_id = candidate_cells[best_idx].identity.pci;
    ue->is_connected = true;
    ue->ho_phase = HO_PHASE_IDLE;
    ue->handover_failure_count--; /* Recovered from failure */

    return true;
}

/* ============================================================================
 * L6: Handover Latency Model
 *
 * End-to-end handover latency is the sum of delays across all phases.
 * Understanding these latencies is essential for:
 *   - Dimensioning X2/Xn/S1/N2 interfaces
 *   - Setting T304 timer (HO completion supervision)
 *   - Meeting URLLC reliability targets (0.5 ms for some services)
 *
 * LTE X2 handover latency budget (control plane, rough estimates):
 *   Measurement + Reporting:  50–200 ms
 *   HO Preparation (X2):      10–30 ms
 *   RRC Reconfiguration:      15 ms (RRC procedure delay)
 *   UE processing + RACH:     20–30 ms
 *   Path Switch (S1):         10–20 ms
 *   UE Context Release:        5 ms
 *   Total CP:                 110–300 ms
 *
 * User plane interruption (time without data):
 *   Standard: ~50 ms (between UE detach and RRC Reconfig Complete)
 *   DAPS (Rel-16): 0 ms
 *
 * 5G NR with DAPS targets 0 ms interruption for URLLC services.
 */
double ho_latency_estimate(double meas_interval_ms,
                           double ttt_ms,
                           double x2_latency_ms,
                           double rach_latency_ms,
                           double path_switch_latency_ms,
                           bool   use_daps)
{
    double t_measurement, t_preparation, t_execution, t_completion, t_total;
    ho_latency_breakdown(meas_interval_ms, ttt_ms, x2_latency_ms,
                         rach_latency_ms, path_switch_latency_ms, use_daps,
                         &t_measurement, &t_preparation, &t_execution,
                         &t_completion, &t_total);
    return t_total;
}

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
                          double *t_total)
{
    if (!t_measurement || !t_preparation || !t_execution
        || !t_completion || !t_total) return;

    /* Measurement phase: time from first measurement to triggering event */
    *t_measurement = meas_interval_ms + ttt_ms;

    /* Preparation phase: X2/Xn handover request/ACK */
    *t_preparation = 2.0 * x2_latency_ms; /* REQ + ACK */

    /* Execution phase: RRC reconfiguration + UE detach + RACH
     * In DAPS mode, user plane interruption is 0 during this phase */
    *t_execution = 15.0 + rach_latency_ms; /* 15ms RRC processing + RACH */

    /* Completion phase: Path switch + context release */
    *t_completion = 2.0 * path_switch_latency_ms; /* REQ + ACK */

    if (use_daps) {
        /* DAPS: user plane interruption is 0 ms */
        *t_execution = 0.0;
    }

    *t_total = *t_measurement + *t_preparation + *t_execution + *t_completion;
}
