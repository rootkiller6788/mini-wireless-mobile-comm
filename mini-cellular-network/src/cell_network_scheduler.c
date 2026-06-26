#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "cell_network_scheduler.h"
#include "cell_network_link.h"

void sched_init(sched_context_t *ctx, scheduler_type_t type,
                int num_rbs, double tti_ms, double alpha_pf) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    ctx->num_rbs = num_rbs > 0 ? num_rbs : 100;
    ctx->tti_ms = tti_ms > 0.0 ? tti_ms : 1.0;
    ctx->alpha_pf = alpha_pf > 0.0 ? alpha_pf : 0.01;
}

int sched_add_ue(sched_context_t *ctx, uint32_t ue_id,
                 sinr_db_t sinr, int buffer_bytes) {
    if (!ctx || ctx->num_ues >= MAX_UE_PER_GNB) return -1;
    int i = ctx->num_ues++;
    ctx->ues[i].ue_id = ue_id; ctx->ues[i].sinr = sinr;
    ctx->ues[i].cqi = sinr_to_cqi(sinr);
    ctx->ues[i].spectral_eff = cqi_to_efficiency(ctx->ues[i].cqi);
    ctx->ues[i].buffer_bytes = buffer_bytes;
    ctx->ues[i].is_active = 1;
    ctx->ues[i].avg_rate_mbps = 1.0;
    ctx->ues[i].qos_weight = 1.0;
    return i;
}

void sched_update_sinr(sched_context_t *ctx, uint32_t ue_id, sinr_db_t new_sinr) {
    if (!ctx) return;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ctx->ues[i].ue_id == ue_id) {
            ctx->ues[i].sinr = new_sinr;
            ctx->ues[i].cqi = sinr_to_cqi(new_sinr);
            ctx->ues[i].spectral_eff = cqi_to_efficiency(ctx->ues[i].cqi);
            return;
        }
    }
}

void sched_remove_ue(sched_context_t *ctx, uint32_t ue_id) {
    if (!ctx) return;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ctx->ues[i].ue_id == ue_id) {
            ctx->ues[i].is_active = 0;
            ctx->ues[i] = ctx->ues[ctx->num_ues - 1];
            ctx->num_ues--;
            return;
        }
    }
}

sched_decision_t sched_round_robin(sched_context_t *ctx) {
    sched_decision_t dec; memset(&dec, 0, sizeof(dec));
    if (!ctx || ctx->num_ues == 0) return dec;
    int n_active = 0;
    for (int i = 0; i < ctx->num_ues; i++)
        if (ctx->ues[i].is_active) n_active++;
    if (n_active == 0) return dec;
    int rbs_per_ue = ctx->num_rbs / n_active;
    int remainder = ctx->num_rbs % n_active;
    double total_tput = 0.0; int alloc_idx = 0;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (!ctx->ues[i].is_active) continue;
        int rbs = rbs_per_ue + (alloc_idx < remainder ? 1 : 0);
        double rate = cqi_to_efficiency(ctx->ues[i].cqi) * rbs * 180000.0 / 1e6;
        dec.ue_allocations[alloc_idx].ue_id = ctx->ues[i].ue_id;
        dec.ue_allocations[alloc_idx].rbs_allocated = rbs;
        dec.ue_allocations[alloc_idx].rate_allocated_mbps = rate;
        dec.ue_allocations[alloc_idx].throughput_actual_bps = rate * 1e6;
        total_tput += rate * 1e6; alloc_idx++;
    }
    dec.num_allocated = alloc_idx;
    dec.total_cell_throughput_bps = total_tput;
    dec.tti_ms = (uint32_t)(ctx->tti_ms * 1000.0);
    return dec;
}

sched_decision_t sched_max_ci(sched_context_t *ctx) {
    sched_decision_t dec; memset(&dec, 0, sizeof(dec));
    if (!ctx || ctx->num_ues == 0 || ctx->num_rbs == 0) return dec;
    int n_active = 0;
    for (int i = 0; i < ctx->num_ues; i++)
        if (ctx->ues[i].is_active) n_active++;
    if (n_active == 0) return dec;
    int ue_rbs[MAX_UE_PER_GNB] = {0};
    for (int rb = 0; rb < ctx->num_rbs; rb++) {
        int best_ue = -1; double best_sinr = -999.0;
        for (int i = 0; i < ctx->num_ues; i++) {
            if (!ctx->ues[i].is_active) continue;
            if (ctx->ues[i].sinr > best_sinr) { best_sinr = ctx->ues[i].sinr; best_ue = i; }
        }
        if (best_ue >= 0) ue_rbs[best_ue]++;
    }
    double total_tput = 0.0; int alloc_idx = 0;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ue_rbs[i] > 0) {
            double rate = cqi_to_efficiency(ctx->ues[i].cqi) * ue_rbs[i] * 180000.0 / 1e6;
            dec.ue_allocations[alloc_idx].ue_id = ctx->ues[i].ue_id;
            dec.ue_allocations[alloc_idx].rbs_allocated = ue_rbs[i];
            dec.ue_allocations[alloc_idx].rate_allocated_mbps = rate;
            dec.ue_allocations[alloc_idx].throughput_actual_bps = rate * 1e6;
            total_tput += rate * 1e6; alloc_idx++;
        }
    }
    dec.num_allocated = alloc_idx;
    dec.total_cell_throughput_bps = total_tput;
    return dec;
}

sched_decision_t sched_proportional_fair(sched_context_t *ctx) {
    sched_decision_t dec; memset(&dec, 0, sizeof(dec));
    if (!ctx || ctx->num_ues == 0 || ctx->num_rbs == 0) return dec;
    int ue_rbs[MAX_UE_PER_GNB] = {0};
    double ue_rate_sum[MAX_UE_PER_GNB] = {0.0};
    for (int rb = 0; rb < ctx->num_rbs; rb++) {
        int best_ue = -1; double best_metric = -1.0;
        for (int i = 0; i < ctx->num_ues; i++) {
            if (!ctx->ues[i].is_active) continue;
            double avg = ctx->ues[i].avg_rate_mbps;
            if (avg < 0.001) avg = 0.001;
            double inst_rate = ctx->ues[i].spectral_eff * 0.180;
            double metric = inst_rate / avg;
            if (metric > best_metric) { best_metric = metric; best_ue = i; }
        }
        if (best_ue >= 0) {
            ue_rbs[best_ue]++;
            ue_rate_sum[best_ue] += ctx->ues[best_ue].spectral_eff * 0.180;
        }
    }
    double total_tput = 0.0; int alloc_idx = 0;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ue_rbs[i] > 0) {
            dec.ue_allocations[alloc_idx].ue_id = ctx->ues[i].ue_id;
            dec.ue_allocations[alloc_idx].rbs_allocated = ue_rbs[i];
            dec.ue_allocations[alloc_idx].rate_allocated_mbps = ue_rate_sum[i];
            dec.ue_allocations[alloc_idx].throughput_actual_bps = ue_rate_sum[i] * 1e6;
            total_tput += ue_rate_sum[i] * 1e6;
            ctx->ues[i].avg_rate_mbps = sched_update_avg_throughput(
                ctx->ues[i].avg_rate_mbps, ue_rate_sum[i], ctx->alpha_pf);
            alloc_idx++;
        }
    }
    dec.num_allocated = alloc_idx;
    dec.total_cell_throughput_bps = total_tput;
    return dec;
}

sched_decision_t sched_exp_pf(sched_context_t *ctx, double delay_weight) {
    sched_decision_t dec; memset(&dec, 0, sizeof(dec));
    if (!ctx || ctx->num_ues == 0 || ctx->num_rbs == 0) return dec;
    double avg_delay = 0.0; int n = 0;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ctx->ues[i].is_active) {
            avg_delay += (ctx->epoch_ms - ctx->ues[i].last_scheduled_ms); n++;
        }
    }
    if (n > 0) avg_delay /= (double)n;
    int ue_rbs[MAX_UE_PER_GNB] = {0};
    for (int rb = 0; rb < ctx->num_rbs; rb++) {
        int best_ue = -1; double best_metric = -1.0;
        for (int i = 0; i < ctx->num_ues; i++) {
            if (!ctx->ues[i].is_active) continue;
            double w_i = ctx->ues[i].qos_weight;
            double delay = ctx->epoch_ms - ctx->ues[i].last_scheduled_ms;
            double exp_factor = exp((delay_weight * w_i * delay - delay_weight * avg_delay)
                                    / (1.0 + sqrt(delay_weight * avg_delay)));
            double avg = ctx->ues[i].avg_rate_mbps;
            if (avg < 0.001) avg = 0.001;
            double metric = exp_factor * ctx->ues[i].spectral_eff * 0.180 / avg;
            if (metric > best_metric) { best_metric = metric; best_ue = i; }
        }
        if (best_ue >= 0) ue_rbs[best_ue]++;
    }
    double total_tput = 0.0; int alloc_idx = 0;
    for (int i = 0; i < ctx->num_ues; i++) {
        if (ue_rbs[i] > 0) {
            double rate = ctx->ues[i].spectral_eff * ue_rbs[i] * 0.180;
            dec.ue_allocations[alloc_idx].ue_id = ctx->ues[i].ue_id;
            dec.ue_allocations[alloc_idx].rbs_allocated = ue_rbs[i];
            dec.ue_allocations[alloc_idx].rate_allocated_mbps = rate;
            dec.ue_allocations[alloc_idx].throughput_actual_bps = rate * 1e6;
            total_tput += rate * 1e6; alloc_idx++;
        }
    }
    dec.num_allocated = alloc_idx;
    dec.total_cell_throughput_bps = total_tput;
    return dec;
}

double sched_rb_rate_bps(double sinr_db, double rb_bandwidth_hz,
                          double alpha_attenuation) {
    double sinr_lin = pow(10.0, sinr_db / 10.0);
    double cap = rb_bandwidth_hz * log2(1.0 + sinr_lin);
    return cap * alpha_attenuation;
}

double sched_update_avg_throughput(double old_avg, double current_rate,
                                    double alpha) {
    return (1.0 - alpha) * old_avg + alpha * current_rate;
}

double sched_jain_fairness(const double *throughputs, int n) {
    if (!throughputs || n <= 0) return 0.0;
    double sum = 0.0, sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        sum += throughputs[i];
        sum_sq += throughputs[i] * throughputs[i];
    }
    if (sum_sq <= 0.0) return 0.0;
    return (sum * sum) / ((double)n * sum_sq);
}

double sched_cell_spectral_efficiency_bps_per_hz(
    double total_throughput_bps, double total_bandwidth_hz) {
    if (total_bandwidth_hz <= 0.0) return 0.0;
    return total_throughput_bps / total_bandwidth_hz;
}

double sched_cell_edge_throughput_bps(double *throughputs, int n,
                                       double percentile) {
    if (!throughputs || n <= 0) return 0.0;
    double *sorted = (double *)malloc((size_t)n * sizeof(double));
    if (!sorted) return 0.0;
    for (int i = 0; i < n; i++) sorted[i] = throughputs[i];
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (sorted[i] > sorted[j]) {
                double tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
            }
        }
    }
    int idx = (int)((percentile / 100.0) * (double)n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    double result = sorted[idx];
    free(sorted);
    return result;
}

void sched_throughput_percentiles(const double *throughputs, int n,
                                   double *p5, double *p50, double *p95) {
    double *sorted = (double *)malloc((size_t)n * sizeof(double));
    if (!sorted) { *p5 = *p50 = *p95 = 0.0; return; }
    for (int i = 0; i < n; i++) sorted[i] = throughputs[i];
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[i] > sorted[j]) {
                double tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
            }
    *p5  = sorted[(int)(0.05 * (n - 1))];
    *p50 = sorted[(int)(0.50 * (n - 1))];
    *p95 = sorted[(int)(0.95 * (n - 1))];
    free(sorted);
}
