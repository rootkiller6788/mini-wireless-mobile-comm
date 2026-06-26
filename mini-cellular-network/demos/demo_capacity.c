/**
 * demo_capacity.c �� Erlang capacity & dimensioning interactive demo
 */
#include <stdio.h>
#include <math.h>
#include "../include/cell_network_defs.h"
#include "../include/cell_network_link.h"
extern double erlang_b_blocking_prob(int, double);
extern double erlang_b_required_channels(double, double);
extern double erlang_c_waiting_prob(int, double);
extern double erlang_c_avg_wait_time_ms(int, double, double);
extern double cell_throughput_mbps(double, double, double);
extern double area_capacity_mbps_per_sqkm(double, double);

int main(void) {
    printf("=== Cellular Capacity Dimensioning Demo ===

");

    printf("--- Erlang B: Blocking Probability ---
");
    printf("Trunk efficiency at 2%% blocking:
");
    int channels[] = {1, 5, 10, 20, 50, 100};
    for (int i = 0; i < 6; i++) {
        int ch = channels[i];
        double tr = (double)ch * 0.7;
        double pb = erlang_b_blocking_prob(ch, tr);
        printf("  %3d channels: offered=%.1f Erl, blocking=%.4f
", ch, tr, pb);
    }

    printf("
--- Erlang B: Required Channels ---
");
    double erlangs[] = {1.0, 10.0, 50.0, 100.0};
    for (int i = 0; i < 4; i++) {
        int ch = erlang_b_required_channels(erlangs[i], 0.02);
        printf("  %.0f Erl at 2%% blocking: %d channels needed
", erlangs[i], ch);
    }

    printf("
--- Erlang C: Call Center Queuing ---
");
    for (int s = 5; s <= 20; s += 5) {
        double pw = erlang_c_waiting_prob(s, (double)s * 0.8);
        double wt = erlang_c_avg_wait_time_ms(s, (double)s * 0.8, 180.0);
        printf("  %d agents, 80%% load: P(wait)=%.4f, avg_wait=%.1f ms
", s, pw, wt);
    }

    printf("
--- Cell Throughput (Shannon) ---
");
    printf("20 MHz BW at various SINR:
");
    double sinr_vals[] = {-5.0, 0.0, 5.0, 10.0, 15.0, 20.0};
    for (int i = 0; i < 6; i++) {
        double tput = cell_throughput_mbps(20.0, sinr_vals[i], 0.75);
        double acap = area_capacity_mbps_per_sqkm(tput, 2.5);
        printf("  SINR=%.0f dB: %.1f Mbps/cell, %.1f Mbps/km^2
", sinr_vals[i], tput, acap);
    }

    printf("
=== Demo Complete ===
");
    return 0;
}
