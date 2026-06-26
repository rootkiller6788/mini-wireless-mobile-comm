/**
 * bench_scheduler.c ¡ª Performance benchmark for scheduling algorithms
 */
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_scheduler.h"
#include "../include/cell_network_link.h"

int main(void) {
    printf("=== Scheduler Performance Benchmark ===

");

    sched_context_t ctx;
    int ue_sets[] = {4, 8, 16, 32};
    int n_sets = 4;

    for (int si = 0; si < n_sets; si++) {
        int n_ues = ue_sets[si];
        printf("--- %d UEs, 100 RBs ---
", n_ues);

        sched_init(&ctx, SCHED_ROUND_ROBIN_FREQ, 100, 1.0, 0.01);
        for (int i = 0; i < n_ues; i++)
            sched_add_ue(&ctx, (uint32_t)i, (double)(rand() % 30 - 10), 10000);

        clock_t t0 = clock();
        for (int iter = 0; iter < 10000; iter++)
            sched_round_robin(&ctx);
        clock_t t1 = clock();
        printf("  Round Robin:       %.2f us/call
",
               (double)(t1 - t0) / CLOCKS_PER_SEC / 10000.0 * 1e6);

        t0 = clock();
        for (int iter = 0; iter < 10000; iter++)
            sched_max_ci(&ctx);
        t1 = clock();
        printf("  Max C/I:           %.2f us/call
",
               (double)(t1 - t0) / CLOCKS_PER_SEC / 10000.0 * 1e6);

        t0 = clock();
        for (int iter = 0; iter < 10000; iter++)
            sched_proportional_fair(&ctx);
        t1 = clock();
        printf("  Proportional Fair: %.2f us/call
",
               (double)(t1 - t0) / CLOCKS_PER_SEC / 10000.0 * 1e6);

        t0 = clock();
        for (int iter = 0; iter < 10000; iter++)
            sched_exp_pf(&ctx, 0.1);
        t1 = clock();
        printf("  EXP/PF:            %.2f us/call
",
               (double)(t1 - t0) / CLOCKS_PER_SEC / 10000.0 * 1e6);
    }

    printf("
=== Benchmark Complete ===
");
    return 0;
}
