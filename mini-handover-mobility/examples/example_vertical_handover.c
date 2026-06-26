/**
 * @file example_vertical_handover.c
 * @brief Vertical handover (LTE ↔ WiFi) using TOPSIS multi-criteria decision (L6)
 *
 * This example demonstrates IEEE 802.21 MIH-based vertical handover
 * (inter-RAT) decision between LTE, 5G NR, and WiFi networks using
 * the TOPSIS multi-criteria decision algorithm.
 *
 * Decision criteria: RSSI, bandwidth, cost, latency, power consumption
 *
 * Build: make examples
 * Run:   ./examples/example_vertical_handover
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "handover_types.h"
#include "handover_decision.h"
#include "handover_protocol.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Vertical Handover: LTE ↔ 5G ↔ WiFi                 ║\n");
    printf("║  Multi-Criteria Decision using TOPSIS & GRA          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Define candidate networks
     *
     * Criteria:
     *   C1: RSSI (dBm) — benefit (higher is better)
     *   C2: Bandwidth (Mbps) — benefit
     *   C3: Cost (cents/MB) — cost (lower is better)
     *   C4: Latency (ms) — cost
     *   C5: Power consumption (mW) — cost
     */
    #define NUM_CANDIDATES 3
    #define NUM_CRITERIA   5

    const char *network_names[] = {
        "LTE (2.6 GHz)",
        "5G NR (3.5 GHz)",
        "WiFi 6 (5 GHz)"
    };

    /* Decision matrix: [candidate × criteria] */
    double matrix[NUM_CANDIDATES * NUM_CRITERIA] = {
        /* RSSI(dBm)  BW(Mbps)  Cost(¢/MB)  Lat(ms)  Power(mW) */
        -85.0,       50.0,     10.0,        30.0,     800.0,   /* LTE */
        -78.0,       500.0,    15.0,        10.0,     1200.0,  /* 5G NR */
        -60.0,       300.0,    1.0,         5.0,      400.0    /* WiFi 6 */
    };

    double weights[NUM_CRITERIA] = {0.25, 0.25, 0.20, 0.15, 0.15};
    bool   is_benefit[NUM_CRITERIA] = {true, true, false, false, false};

    printf("Candidate Networks:\n");
    printf("%-18s %10s %10s %10s %10s %10s\n",
           "Network", "RSSI", "BW(Mbps)", "Cost", "Lat(ms)", "Power(mW)");
    printf("──────────────────────────────────────────────────────────\n");
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        printf("%-18s %10.1f %10.1f %10.1f %10.1f %10.1f\n",
               network_names[i],
               matrix[i * NUM_CRITERIA + 0],
               matrix[i * NUM_CRITERIA + 1],
               matrix[i * NUM_CRITERIA + 2],
               matrix[i * NUM_CRITERIA + 3],
               matrix[i * NUM_CRITERIA + 4]);
    }
    printf("\n");

    printf("Criterion Weights:\n");
    printf("  RSSI: %.2f  BW: %.2f  Cost: %.2f  Latency: %.2f  Power: %.2f\n",
           weights[0], weights[1], weights[2], weights[3], weights[4]);
    printf("\n");

    /* ================================================================
     * TOPSIS Decision
     * ================================================================ */
    printf("═══════════ TOPSIS Decision ═══════════\n\n");

    double closeness[NUM_CANDIDATES];
    int topsis_best;
    ho_decision_topsis(matrix, weights, is_benefit,
                       NUM_CANDIDATES, NUM_CRITERIA,
                       closeness, &topsis_best);

    printf("TOPSIS Closeness Coefficients:\n");
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        printf("  %-18s C* = %.4f %s\n",
               network_names[i],
               closeness[i],
               (i == topsis_best) ? " ← BEST" : "");
    }
    printf("\n");
    printf("TOPSIS Recommendation: %s\n\n", network_names[topsis_best]);

    /* ================================================================
     * GRA Decision
     * ================================================================ */
    printf("═══════════ GRA Decision ═══════════\n\n");

    /* Ideal reference: best RSSI, highest BW, lowest cost, lowest latency, lowest power */
    double reference[NUM_CRITERIA] = {-60.0, 500.0, 1.0, 5.0, 400.0};

    double gra_grades[NUM_CANDIDATES];
    int gra_best;
    ho_decision_gra(matrix, reference, 0.5,
                    NUM_CANDIDATES, NUM_CRITERIA,
                    gra_grades, &gra_best);

    printf("GRA Grey Relational Grades (ρ=0.5):\n");
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        printf("  %-18s Γ = %.4f %s\n",
               network_names[i],
               gra_grades[i],
               (i == gra_best) ? " ← BEST" : "");
    }
    printf("\n");
    printf("GRA Recommendation: %s\n\n", network_names[gra_best]);

    /* ================================================================
     * MIH Vertical Handover
     * ================================================================ */
    printf("═══════════ MIH (IEEE 802.21) Decision ═══════════\n\n");

    MIHLinkType available_links[] = {
        MIH_LINK_3GPP,   /* LTE */
        MIH_LINK_3GPP,   /* 5G NR (also 3GPP) */
        MIH_LINK_802_11  /* WiFi */
    };
    double link_quality[] = {0.6, 0.8, 0.9}; /* Quality scores [0,1] */
    double link_cost[]    = {3.0, 5.0, 1.0}; /* Relative cost */

    MIHLinkType selected;
    int selected_idx;

    /* Without WiFi offload preference */
    printf("Scenario 1: No WiFi offload preference\n");
    ho_mih_vertical_decision(MIH_LINK_3GPP, available_links,
                             link_quality, link_cost, 3, false,
                             &selected, &selected_idx);
    printf("  Selected: %s (quality=%.1f, cost=%.1f)\n\n",
           network_names[selected_idx],
           link_quality[selected_idx], link_cost[selected_idx]);

    /* With WiFi offload preference */
    printf("Scenario 2: WiFi offload preferred\n");
    ho_mih_vertical_decision(MIH_LINK_3GPP, available_links,
                             link_quality, link_cost, 3, true,
                             &selected, &selected_idx);
    printf("  Selected: %s (quality=%.1f, cost=%.1f)\n\n",
           network_names[selected_idx],
           link_quality[selected_idx], link_cost[selected_idx]);

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("═══════════ Summary ═══════════\n\n");
    printf("TOPSIS selected: %s\n", network_names[topsis_best]);
    printf("GRA selected:    %s\n", network_names[gra_best]);
    printf("\n");
    printf("TOPSIS and GRA are complementary MCDM methods:\n");
    printf("  TOPSIS: Distance-based (Euclidean distance to ideal)\n");
    printf("  GRA:    Correlation-based (grey relational grade)\n");
    printf("\n");

    return 0;
}
