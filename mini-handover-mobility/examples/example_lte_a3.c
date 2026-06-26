/**
 * @file example_lte_a3.c
 * @brief LTE Event A3 intra-frequency handover simulation (L6 Canonical Problem)
 *
 * This example simulates a UE moving between two LTE cells, using the
 * 3GPP Event A3 handover trigger (neighbour becomes offset better than
 * serving). It demonstrates:
 *   - Path loss computation using Okumura-Hata model
 *   - RSRP measurement with shadow fading
 *   - L3 filtering of measurements
 *   - A3 event evaluation with hysteresis and TTT
 *   - Handover execution
 *
 * Build: make examples
 * Run:   ./examples/example_lte_a3
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "handover_types.h"
#include "handover_decision.h"
#include "signal_measurement.h"
#include "mobility_model.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  LTE Event A3 Handover Simulation                    ║\n");
    printf("║  UE moving from Cell A toward Cell B                 ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Configure cells */
    CellInfo cell_a, cell_b;
    cell_info_init(&cell_a);
    cell_info_init(&cell_b);

    cell_a.identity.pci = 100;
    cell_a.position_x = 0.0;
    cell_a.position_y = 0.0;
    cell_a.position_z = 30.0;
    cell_a.tx_power_dbm = 43.0;
    cell_a.antenna_gain_dbi = 15.0;

    cell_b.identity.pci = 200;
    cell_b.position_x = 2000.0;
    cell_b.position_y = 0.0;
    cell_b.position_z = 30.0;
    cell_b.tx_power_dbm = 43.0;
    cell_b.antenna_gain_dbi = 15.0;

    printf("Cell A: PCI=%u, position=(0, 0, 30)m, Ptx=43 dBm\n",
           cell_a.identity.pci);
    printf("Cell B: PCI=%u, position=(2000, 0, 30)m, Ptx=43 dBm\n",
           cell_b.identity.pci);
    printf("Inter-site distance: 2000 m\n\n");

    /* Configure UE */
    UEContext ue;
    ue_context_init(&ue, 10001);
    ue.position.position_x = 100.0; /* Start near Cell A */
    ue.position.position_y = 0.0;
    ue.position.velocity_x = 10.0;   /* 10 m/s ≈ 36 km/h, walking toward B */
    ue.position.velocity_y = 0.0;
    ue.serving_cell_id = 100;
    ue.is_connected = true;
    ue.mobility_state = MOB_NORMAL;

    /* Handover parameters (typical LTE defaults) */
    HandoverParams params;
    ho_params_init_default(&params);
    params.hysteresis_db = 3.0;
    params.ttt_ms = 160.0;
    params.a3_offset_db = 2.0;
    params.l3_filter_coeff = 4;

    printf("Handover Parameters:\n");
    printf("  Hysteresis: %.1f dB\n", params.hysteresis_db);
    printf("  TTT: %.0f ms\n", params.ttt_ms);
    printf("  A3 Offset: %.1f dB\n", params.a3_offset_db);
    printf("  L3 Filter k: %d\n", params.l3_filter_coeff);
    printf("\n");

    /* Simulation parameters */
    double sim_time_s = 250.0;
    double dt_s = 0.04; /* 40 ms measurement interval (LTE) */
    int steps = (int)(sim_time_s / dt_s);

    /* State variables */
    double serving_rsrp_filtered = -200.0;
    double neighbour_rsrp_filtered = -200.0;
    double shadow_a = 0.0;
    double shadow_b = 0.0;
    int ttt_counter = 0;
    int ttt_required = (int)(params.ttt_ms / (dt_s * 1000.0));
    bool ho_in_progress = false;
    int serving_cell = 100;

    HandoverStatistics ho_stats;
    ho_stats_init(&ho_stats);

    printf("Simulation: UE moves at 10 m/s for %.0f seconds\n", sim_time_s);
    printf("%-8s %-8s %-12s %-12s %-8s %-8s %s\n",
           "Time(s)", "Pos(m)", "RSRP_A(dBm)", "RSRP_B(dBm)",
           "A3Entry", "TTT", "Event");
    printf("──────────────────────────────────────────────────────────────\n");

    for (int step = 0; step < steps; step++) {
        double t = step * dt_s;

        /* Update UE position */
        ue_update_position(&ue, dt_s);

        /* Compute distance to each cell */
        double dist_a = cell_distance_to_ue(&cell_a, &ue.position);
        double dist_b = cell_distance_to_ue(&cell_b, &ue.position);

        /* Path loss (Okumura-Hata urban, 2.6 GHz) */
        double pl_a = meas_okumura_hata_path_loss(2600.0,
                         dist_a / 1000.0, cell_a.position_z,
                         ue.position.position_z, 0);
        double pl_b = meas_okumura_hata_path_loss(2600.0,
                         dist_b / 1000.0, cell_b.position_z,
                         ue.position.position_z, 0);

        /* Shadow fading update */
        shadow_a = meas_shadow_fading_generate(shadow_a, 8.0,
                         ue.position.speed_mps, dt_s, 50.0);
        shadow_b = meas_shadow_fading_generate(shadow_b, 8.0,
                         ue.position.speed_mps, dt_s, 50.0);

        /* RSRP measurements */
        double rsrp_a_raw = meas_compute_rssi(cell_a.tx_power_dbm,
                            cell_a.antenna_gain_dbi, 0.0,
                            pl_a, shadow_a, 3.0, 0.0);
        double rsrp_b_raw = meas_compute_rssi(cell_b.tx_power_dbm,
                            cell_b.antenna_gain_dbi, 0.0,
                            pl_b, shadow_b, 3.0, 0.0);

        /* L3 filtering */
        int serving_idx = (serving_cell == 100) ? 0 : 1;

        if (step == 0) {
            serving_rsrp_filtered = rsrp_a_raw;
            neighbour_rsrp_filtered = rsrp_b_raw;
        } else {
            if (serving_idx == 0) {
                serving_rsrp_filtered = meas_l3_filter(serving_rsrp_filtered,
                                        rsrp_a_raw, params.l3_filter_coeff);
                neighbour_rsrp_filtered = meas_l3_filter(neighbour_rsrp_filtered,
                                         rsrp_b_raw, params.l3_filter_coeff);
            } else {
                serving_rsrp_filtered = meas_l3_filter(serving_rsrp_filtered,
                                        rsrp_b_raw, params.l3_filter_coeff);
                neighbour_rsrp_filtered = meas_l3_filter(neighbour_rsrp_filtered,
                                         rsrp_a_raw, params.l3_filter_coeff);
            }
        }

        /* A3 event evaluation */
        bool entry, leaving;
        ho_decision_event_a3(serving_rsrp_filtered, neighbour_rsrp_filtered,
                             params.a3_offset_db, params.hysteresis_db,
                             0.0, 0.0, 0.0, &entry, &leaving);

        /* TTT counter */
        if (entry && !ho_in_progress) {
            ttt_counter++;
        } else {
            if (!ho_in_progress) ttt_counter = 0;
        }

        bool ttt_met = (ttt_counter >= ttt_required);

        /* Print at 1-second intervals */
        if (step % 25 == 0 || (ttt_met && !ho_in_progress)) {
            const char *event_str = "";
            if (ho_in_progress) {
                event_str = (serving_cell == 100) ? "HO→B" : "HO→A";
            } else if (ttt_met) {
                event_str = "HO TRIGGERED!";
                /* Execute handover */
                serving_cell = (serving_cell == 100) ? 200 : 100;
                ue.serving_cell_id = serving_cell;
                ho_in_progress = true;
                ho_stats_update(&ho_stats, true, false, 50.0);
            }

            printf("%-8.2f %-8.1f %-12.1f %-12.1f %-8s %-8d %s\n",
                   t, ue.position.position_x,
                   (serving_cell == 100) ? rsrp_a_raw : rsrp_b_raw,
                   (serving_cell == 100) ? rsrp_b_raw : rsrp_a_raw,
                   entry ? "YES" : "NO",
                   ttt_counter,
                   event_str);
        }

        /* Reset HO flag after some time */
        if (ho_in_progress && ttt_counter == 0) {
            ho_in_progress = false;
        }
    }

    printf("\n");
    ho_stats_print_summary(&ho_stats);

    printf("\nSimulation complete.\n");
    printf("Final position: (%.1f, %.1f) m\n",
           ue.position.position_x, ue.position.position_y);
    printf("Final serving cell: PCI=%d\n", serving_cell);

    return 0;
}
