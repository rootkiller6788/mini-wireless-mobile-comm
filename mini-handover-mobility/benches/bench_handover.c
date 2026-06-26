/**
 * @file bench_handover.c
 * @brief Performance benchmarks for handover decision algorithms.
 *
 * Benchmarks the core handover decision algorithms and signal measurement
 * computations to quantify computational complexity.
 *
 * Build: make bench
 * Run:   ./benches/bench_handover
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "handover_types.h"
#include "handover_decision.h"
#include "signal_measurement.h"
#include "mobility_model.h"

#define BENCH_ITERATIONS 100000
#define CLOCK_MS(start, end) \
    (((double)((end) - (start))) / CLOCKS_PER_SEC * 1000.0)

int main(void) {
    clock_t clk_start, clk_end;
    double elapsed_ms;

    printf("\n=== Handover Algorithm Benchmarks ===\n");
    printf("Iterations per test: %d\n\n", BENCH_ITERATIONS);

    /* Benchmark 1: Hysteresis decision */
    clk_start = clock();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        volatile bool result __attribute__((unused));
        result = ho_decision_hysteresis(-100.0 + (i % 20),
                                        -95.0 + (i % 15),
                                        3.0);
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("Hysteresis decision:          %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    /* Benchmark 2: Event A3 */
    clk_start = clock();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        bool entry, leaving;
        ho_decision_event_a3(-105.0, -98.0 + (i % 10),
                             2.0, 2.0, 0.0, 0.0, 0.0,
                             &entry, &leaving);
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("Event A3 evaluation:          %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    /* Benchmark 3: Friis path loss */
    clk_start = clock();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        volatile double pl __attribute__((unused));
        pl = meas_friis_free_space_path_loss(100.0 + i, 2.6e9);
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("Friis path loss:              %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    /* Benchmark 4: Okumura-Hata path loss */
    clk_start = clock();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        volatile double pl __attribute__((unused));
        pl = meas_okumura_hata_path_loss(2100.0, 1.0 + (i % 2000) / 1000.0,
                                         30.0, 1.5, 0);
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("Okumura-Hata path loss:       %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    /* Benchmark 5: L3 filter */
    clk_start = clock();
    {
        double prev = -90.0;
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            prev = meas_l3_filter(prev, -85.0 + (i % 10), 4);
        }
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("L3 filter (k=4):              %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    /* Benchmark 6: TOPSIS (3 candidates, 5 criteria) */
    {
        double matrix[] = {
            -85.0, 50.0, 10.0, 30.0, 800.0,
            -78.0, 500.0, 15.0, 10.0, 1200.0,
            -60.0, 300.0, 1.0, 5.0, 400.0
        };
        double weights[] = {0.25, 0.25, 0.20, 0.15, 0.15};
        bool   benefit[] = {true, true, false, false, false};

        clk_start = clock();
        for (int i = 0; i < BENCH_ITERATIONS / 10; i++) {
            double closeness[3];
            int best;
            ho_decision_topsis(matrix, weights, benefit, 3, 5, closeness, &best);
        }
        clk_end = clock();
        elapsed_ms = CLOCK_MS(clk_start, clk_end);
        printf("TOPSIS (3×5):                 %8.3f ms  (%8.3f µs/call)\n",
               elapsed_ms, elapsed_ms * 10000.0 / BENCH_ITERATIONS);
    }

    /* Benchmark 7: Doppler shift */
    clk_start = clock();
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        volatile double fd __attribute__((unused));
        fd = mob_compute_doppler_shift(30.0 + i, 2.6e9, 0.0);
    }
    clk_end = clock();
    elapsed_ms = CLOCK_MS(clk_start, clk_end);
    printf("Doppler shift:                %8.3f ms  (%8.3f µs/call)\n",
           elapsed_ms, elapsed_ms * 1000.0 / BENCH_ITERATIONS);

    printf("\nBenchmarks complete.\n\n");
    return 0;
}
