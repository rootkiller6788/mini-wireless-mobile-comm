/**
 * @file cell_network_capacity.c
 * @brief Capacity Dimensioning �� Erlang B/C, Queuing, Traffic Theory (L3, L5, L6)
 * Reference: Kleinrock, "Queueing Systems" (1975); Erlang (1917)
 *            Molisch (2011) Ch. 17
 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "cell_network_defs.h"

/* ================================================================
 * L5: Erlang B �� Blocking probability for loss systems
 * ================================================================ */

double erlang_b_blocking_prob(int channels, double traffic_erlang) {
    if (channels <= 0 || traffic_erlang < 0.0) return 0.0;
    double prob = 1.0;
    for (int i = 1; i <= channels; i++) {
        prob = (traffic_erlang * prob) / ((double)i + traffic_erlang * prob);
    }
    return prob;
}

int erlang_b_required_channels(double traffic_erlang, double target_blocking) {
    if (traffic_erlang <= 0.0 || target_blocking <= 0.0) return 0;
    for (int c = 1; c <= 1000; c++) {
        if (erlang_b_blocking_prob(c, traffic_erlang) <= target_blocking)
            return c;
    }
    return -1;
}

/* ================================================================
 * L5: Erlang C �� Waiting probability for queued systems
 * ================================================================ */

double erlang_c_waiting_prob(int servers, double traffic_erlang) {
    if (servers <= 0 || traffic_erlang <= 0.0) return 0.0;
    if (traffic_erlang >= (double)servers) return 1.0;

    double sum = 0.0;
    double fact = 1.0;
    for (int k = 0; k < servers; k++) {
        if (k > 0) fact *= (double)k;
        sum += pow(traffic_erlang, (double)k) / fact;
    }
    fact *= (double)servers;
    double last_term = pow(traffic_erlang, (double)servers) / fact;
    double erlang_c = last_term / (last_term + (1.0 - traffic_erlang / (double)servers) * sum);
    return erlang_c;
}

double erlang_c_avg_wait_time_ms(int servers, double traffic_erlang,
                                  double mean_service_time_ms) {
    double pw = erlang_c_waiting_prob(servers, traffic_erlang);
    return pw * mean_service_time_ms / ((double)servers - traffic_erlang);
}

/* ================================================================
 * L6: Cell Capacity Dimensioning
 * ================================================================ */

typedef struct {
    double area_sqkm;
    double user_density_per_sqkm;
    double avg_traffic_per_user_erlang;
    double target_blocking_prob;
    int    channels_per_cell;
    double cell_area_sqkm;
} capacity_input_t;

typedef struct {
    double total_traffic_erlang;
    double traffic_per_cell_erlang;
    int    required_channels;
    int    required_cells;
    double cell_radius_km;
    double total_throughput_mbps;
    int    num_users_supported;
} capacity_output_t;

capacity_output_t dimension_cell_capacity(const capacity_input_t *inp) {
    capacity_output_t out;
    memset(&out, 0, sizeof(out));
    if (!inp || inp->area_sqkm <= 0.0 || inp->cell_area_sqkm <= 0.0) return out;

    out.total_traffic_erlang = inp->area_sqkm * inp->user_density_per_sqkm
                              * inp->avg_traffic_per_user_erlang;
    double num_cells_float = inp->area_sqkm / inp->cell_area_sqkm;
    out.required_cells = (int)ceil(num_cells_float);
    out.traffic_per_cell_erlang = out.total_traffic_erlang / (double)out.required_cells;
    out.required_channels = erlang_b_required_channels(
        out.traffic_per_cell_erlang, inp->target_blocking_prob);
    out.cell_radius_km = sqrt(inp->cell_area_sqkm / (2.598));
    out.num_users_supported = (int)(inp->user_density_per_sqkm * inp->area_sqkm);
    return out;
}

/* ================================================================
 * L6: Throughput Capacity (Shannon-based)
 * ================================================================ */

double cell_throughput_mbps(double bandwidth_mhz, double avg_sinr_db,
                             double cell_load) {
    double sinr_lin = pow(10.0, avg_sinr_db / 10.0);
    double eff = log2(1.0 + sinr_lin);
    return bandwidth_mhz * eff * cell_load;
}

double area_capacity_mbps_per_sqkm(double cell_throughput_mbps,
                                    double cell_area_sqkm) {
    if (cell_area_sqkm <= 0.0) return 0.0;
    return cell_throughput_mbps / cell_area_sqkm;
}

double num_users_per_cell(double cell_throughput_mbps,
                           double avg_user_rate_mbps, double overbooking) {
    if (avg_user_rate_mbps <= 0.0) return 0.0;
    return (cell_throughput_mbps / avg_user_rate_mbps) * overbooking;
}

/* ================================================================
 * L6: M/D/1 Queuing Model for Packet Scheduling
 * ================================================================ */

double md1_avg_queue_length(double arrival_rate, double service_rate) {
    if (service_rate <= arrival_rate) return 1e9;  /* Unsustainable */
    double rho = arrival_rate / service_rate;
    return rho * rho / (2.0 * (1.0 - rho));
}

double md1_avg_wait_time_ms(double arrival_rate, double service_rate,
                              double service_time_ms) {
    (void)service_time_ms;
    if (service_rate <= arrival_rate) return 1e9;
    double rho = arrival_rate / service_rate;
    return (rho / (2.0 * service_rate * (1.0 - rho))) * 1000.0;
}

double md1_prob_delay_exceeds(double arrival_rate, double service_rate,
                               double t_seconds) {
    if (service_rate <= arrival_rate) return 1.0;
    double rho = arrival_rate / service_rate;
    double exponent = -2.0 * (1.0 - rho) * service_rate * t_seconds / rho;
    if (exponent < -700.0) return 0.0;  /* Avoid underflow */
    return rho * exp(exponent);
}
