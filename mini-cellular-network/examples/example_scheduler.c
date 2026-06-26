/**
 * example_scheduler.c ¡ª L5: Packet scheduler comparison
 * Demonstrates: Round Robin, Max C/I, Proportional Fair scheduling,
 *               throughput and fairness trade-off.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_scheduler.h"
#include "../include/cell_network_link.h"

int main(void) {
    printf("=== Scheduler Comparison: RR vs Max-C/I vs PF ===

");

    sched_context_t ctx;
    double throughputs_rr[8], throughputs_ci[8], throughputs_pf[8];
    int n_ues = 8;
    double snrs[8] = {20.0, 18.0, 12.0, 5.0, 3.0, -2.0, -5.0, -8.0};

    /* Round Robin */
    sched_init(&ctx, SCHED_RR, 100, 1.0, 0.01);
    for (int i = 0; i < n_ues; i++) sched_add_ue(&ctx, (uint32_t)i, snrs[i], 10000);
    sched_decision_t d_rr = sched_round_robin(&ctx);
    double sum_rr = 0.0;
    for (int i = 0; i < d_rr.num_allocated; i++) {
        throughputs_rr[i] = d_rr.ue_allocations[i].throughput_actual_bps;
        sum_rr += throughputs_rr[i];
    }
    double jain_rr = sched_jain_fairness(throughputs_rr, d_rr.num_allocated);

    /* Max C/I */
    sched_init(&ctx, SCHED_MAX_CI, 100, 1.0, 0.01);
    for (int i = 0; i < n_ues; i++) sched_add_ue(&ctx, (uint32_t)i, snrs[i], 10000);
    sched_decision_t d_ci = sched_max_ci(&ctx);
    double sum_ci = 0.0;
    int n_ci = d_ci.num_allocated;
    for (int i = 0; i < n_ci; i++) {
        throughputs_ci[i] = d_ci.ue_allocations[i].throughput_actual_bps;
        sum_ci += throughputs_ci[i];
    }
    double jain_ci = n_ci > 0 ? sched_jain_fairness(throughputs_ci, n_ci) : 0.0;

    /* Proportional Fair */
    sched_init(&ctx, SCHED_PROPORTIONAL_FAIR, 100, 1.0, 0.005);
    for (int i = 0; i < n_ues; i++) sched_add_ue(&ctx, (uint32_t)i, snrs[i], 10000);
    sched_decision_t d_pf = sched_proportional_fair(&ctx);
    double sum_pf = 0.0;
    for (int i = 0; i < d_pf.num_allocated; i++) {
        throughputs_pf[i] = d_pf.ue_allocations[i].throughput_actual_bps;
        sum_pf += throughputs_pf[i];
    }
    double jain_pf = sched_jain_fairness(throughputs_pf, d_pf.num_allocated);

    printf("%-20s %15s %15s
", "Scheduler", "Cell Tput (Mbps)", "Jain Fairness");
    printf("%-20s %15.2f %15.3f
", "Round Robin", sum_rr/1e6, jain_rr);
    printf("%-20s %15.2f %15.3f
", "Max C/I", sum_ci/1e6, jain_ci);
    printf("%-20s %15.2f %15.3f
", "Proportional Fair", sum_pf/1e6, jain_pf);
    printf("
Observation: Max C/I maximizes throughput but unfair;
");
    printf("PF balances throughput and fairness (Jain index near 1).
");

    return 0;
}
