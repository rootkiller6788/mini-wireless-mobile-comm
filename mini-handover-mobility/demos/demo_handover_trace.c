/**
 * @file demo_handover_trace.c
 * @brief Interactive demo showing handover trace with mobility visualization.
 *
 * Generates a complete handover trace for a UE moving across a multi-cell
 * deployment, demonstrating the full handover pipeline from measurement
 * through decision to execution.
 *
 * Build: make demo
 * Run:   ./demos/demo_handover_trace
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "handover_types.h"
#include "handover_decision.h"
#include "signal_measurement.h"
#include "mobility_model.h"
#include "handover_optimize.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Handover Trace Demo                                 ║\n");
    printf("║  7-cell cluster, Random Waypoint UE                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Configure 7-cell hexagonal cluster */
    #define NUM_CELLS 7
    CellInfo cells[NUM_CELLS];

    /* Hexagonal layout: center + 6 surrounding cells
     * Inter-site distance: 500 m */
    double isd = 500.0;
    double cell_positions[NUM_CELLS][2] = {
        {0.0, 0.0},                           /* Cell 0: Center */
        {isd, 0.0},                           /* Cell 1: East */
        {isd/2.0, isd * sqrt(3.0)/2.0},       /* Cell 2: Northeast */
        {-isd/2.0, isd * sqrt(3.0)/2.0},      /* Cell 3: Northwest */
        {-isd, 0.0},                          /* Cell 4: West */
        {-isd/2.0, -isd * sqrt(3.0)/2.0},     /* Cell 5: Southwest */
        {isd/2.0, -isd * sqrt(3.0)/2.0}       /* Cell 6: Southeast */
    };

    for (int i = 0; i < NUM_CELLS; i++) {
        cell_info_init(&cells[i]);
        cells[i].identity.pci = 100 + i;
        cells[i].position_x = cell_positions[i][0];
        cells[i].position_y = cell_positions[i][1];
        cells[i].position_z = 25.0;
        cells[i].tx_power_dbm = 43.0;
        cells[i].antenna_gain_dbi = 15.0;
        printf("Cell %d: PCI=%u, pos=(%.0f, %.0f)m\n",
               i, cells[i].identity.pci,
               cells[i].position_x, cells[i].position_y);
    }

    /* UE using Random Waypoint mobility */
    UEContext ue;
    ue_context_init(&ue, 99999);
    ue.position.position_x = 0.0;
    ue.position.position_y = 0.0;
    ue.is_connected = true;
    ue.mobility_state = MOB_NORMAL;

    /* RWP state */
    double dest_x = 300.0, dest_y = 200.0;
    double pause_remaining = 0.0;

    /* HO parameters */
    HandoverParams params;
    ho_params_init_default(&params);

    HandoverStatistics ho_stats;
    ho_stats_init(&ho_stats);

    printf("\n");
    printf("UE mobility: Random Waypoint (v=5-15 m/s)\n");
    printf("HO algorithm: A3 event (offset=2dB, hys=3dB, TTT=160ms)\n");
    printf("Simulation duration: 120 seconds\n\n");

    printf("%-8s %-8s %-8s %-10s %-10s %-10s %-12s %s\n",
           "Time(s)", "X(m)", "Y(m)", "Serving", "BestNbr",
           "A3Entry?", "Decision", "HO Event");
    printf("──────────────────────────────────────────────────────────────\n");

    double sim_time = 120.0;
    double dt = 0.04;
    int steps = (int)(sim_time / dt);

    int serving_idx = 0;
    int ttt_counter = 0;
    int ttt_required = (int)(params.ttt_ms / (dt * 1000.0));
    double filtered_rsrp[NUM_CELLS];
    for (int i = 0; i < NUM_CELLS; i++) filtered_rsrp[i] = -200.0;

    for (int step = 0; step < steps; step++) {
        double t = step * dt;

        /* Move UE */
        mob_random_waypoint_step(&ue.position, dt, 5.0, 15.0, 2000.0,
                                 800.0, 800.0, &dest_x, &dest_y,
                                 &pause_remaining);

        /* Measure RSRP to all cells */
        double rsrp_raw[NUM_CELLS];
        int best_nbr = -1;
        double best_nbr_rsrp = -200.0;

        for (int i = 0; i < NUM_CELLS; i++) {
            double dx = ue.position.position_x - cells[i].position_x;
            double dy = ue.position.position_y - cells[i].position_y;
            double dist = sqrt(dx * dx + dy * dy);

            double pl = meas_okumura_hata_path_loss(2100.0,
                         dist/1000.0, cells[i].position_z, 1.5, 0);
            rsrp_raw[i] = meas_compute_rssi(cells[i].tx_power_dbm,
                           cells[i].antenna_gain_dbi, 0.0,
                           pl, 0.0, 3.0, 0.0);

            /* L3 filtering */
            if (step == 0) {
                filtered_rsrp[i] = rsrp_raw[i];
            } else {
                filtered_rsrp[i] = meas_l3_filter(filtered_rsrp[i],
                                     rsrp_raw[i], params.l3_filter_coeff);
            }

            if (i != serving_idx && filtered_rsrp[i] > best_nbr_rsrp) {
                best_nbr_rsrp = filtered_rsrp[i];
                best_nbr = i;
            }
        }

        /* A3 event evaluation */
        bool entry = false, leaving = false;
        if (best_nbr >= 0) {
            ho_decision_event_a3(filtered_rsrp[serving_idx],
                                 filtered_rsrp[best_nbr],
                                 params.a3_offset_db,
                                 params.hysteresis_db,
                                 0.0, 0.0, 0.0, &entry, &leaving);
        }

        if (entry) ttt_counter++;
        else ttt_counter = 0;

        bool ho_executed = false;
        if (ttt_counter >= ttt_required) {
            /* Execute handover */
            serving_idx = best_nbr;
            ue.serving_cell_id = cells[serving_idx].identity.pci;
            ho_stats_update(&ho_stats, true, false, 45.0);
            ttt_counter = 0;
            ho_executed = true;
        }

        /* Print every second or on HO */
        if (step % 25 == 0 || ho_executed) {
            printf("%-8.1f %-8.1f %-8.1f %-10u %-10u %-10s %-12s %s\n",
                   t,
                   ue.position.position_x,
                   ue.position.position_y,
                   cells[serving_idx].identity.pci,
                   (best_nbr >= 0) ? cells[best_nbr].identity.pci : 0,
                   entry ? "YES" : "NO",
                   ho_executed ? "HO EXECUTED" : (entry ? "Waiting" : "---"),
                   ho_executed ? "<--- HO!" : "");
        }
    }

    printf("\n");
    printf("═══════════════════════════════════════════════\n");
    ho_stats_print_summary(&ho_stats);
    printf("\nDemo complete. UE is at (%.1f, %.1f)m serving PCI=%u\n",
           ue.position.position_x, ue.position.position_y,
           cells[serving_idx].identity.pci);

    return 0;
}
