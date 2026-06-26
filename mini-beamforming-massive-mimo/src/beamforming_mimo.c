/**
 * beamforming_mimo.c - MIMO Channel Models and Capacity Theory
 *
 * L2: Channel generation (Rayleigh, Rician, mmWave clustered)
 * L4: Shannon MIMO capacity, waterfilling optimality
 * L6: MIMO capacity vs SNR computation
 * L8: Channel hardening, favorable propagation, massive MIMO metrics
 *
 * Reference: Telatar (1999) "MIMO Capacity" ETT
 *            Foschini & Gans (1998) Wireless Personal Comm.
 *            Marzetta (2010) "Massive MIMO" IEEE TWC
 *            Bjornson et al. (2017) Massive MIMO Networks
 */

#include "beamforming_mimo.h"
#include "beamforming_types.h"
#include <stdio.h>
#include <float.h>
#include <stdlib.h>

/* ============ MIMO Channel Generation (L2) ============ */

int channel_rayleigh_iid(size_t M_r, size_t M_t, mimo_channel *channel) {
    /* i.i.d. Rayleigh fading: H_ij ~ CN(0,1).
     *
     * Each element is an independent complex Gaussian with variance 1.
     * This models rich scattering where the channel matrix has full rank
     * with probability 1.
     *
     * Properties of i.i.d. Rayleigh MIMO:
     *   - E[||H||_F^2] = M_r * M_t (total power = M_r * M_t)
     *   - Rank = min(M_r, M_t) with probability 1
     *   - Singular values follow the Marcenko-Pastur law for large arrays
     *
     * Complexity: O(M_r * M_t). */
    if (!channel) return -1;
    channel->num_rx = M_r;
    channel->num_tx = M_t;
    channel->type = CHANNEL_RAYLEIGH_IID;
    channel->is_normalized = 0;

    channel->H = cmat_alloc(M_r, M_t);
    if (!channel->H.data) return -2;

    /* Box-Muller transform for complex Gaussian CN(0,1):
     * real ~ N(0, 0.5), imag ~ N(0, 0.5) so |H_ij|^2 ~ Exp(1) with mean 1 */
    for (size_t i = 0; i < M_r * M_t; i++) {
        double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double mag = sqrt(-0.5 * log(u1 > 1e-300 ? u1 : 1e-300));
        double phase = 2.0 * M_PI * u2;
        channel->H.data[i].real = mag * cos(phase);
        channel->H.data[i].imag = mag * sin(phase);
    }

    return 0;
}

int channel_rayleigh_corr(const complex_matrix *R_rx,
                          const complex_matrix *R_tx,
                          mimo_channel *channel) {
    /* Correlated Rayleigh: H = R_rx^{1/2} * H_iid * R_tx^{1/2}.
     *
     * The Kronecker model separates Tx and Rx correlation:
     *   R_H = R_tx^T (x) R_rx  (Kronecker product)
     *
     * More realistic than i.i.d. ? captures antenna correlation at
     * each end due to limited angular spread.
     *
     * Used by: 3GPP SCM, WINNER II channel models.
     * Reference: Kermoal et al. (2002) IEEE JSAC. */
    if (!R_rx || !R_tx || !channel) return -1;
    size_t M_r = R_rx->rows, M_t = R_tx->rows;

    /* Generate i.i.d. base channel */
    channel_rayleigh_iid(M_r, M_t, channel);

    /* Apply correlation: H = R_rx^{1/2} * H_iid * R_tx^{1/2}
     * Simplified: use Cholesky factors L_rx, L_tx */
    complex_matrix L_rx = cmat_alloc(M_r, M_r);
    complex_matrix L_tx = cmat_alloc(M_t, M_t);

    /* Cholesky decomposition: R = L L^H (L is lower triangular) */
    for (size_t i = 0; i < M_r; i++) {
        for (size_t j = 0; j <= i; j++) {
            complex_double sum = cmat_get(R_rx, i, j);
            for (size_t k = 0; k < j; k++)
                sum = csub(sum, cmul(cmat_get(&L_rx, i, k), cconj(cmat_get(&L_rx, j, k))));
            if (i == j)
                cmat_set(&L_rx, i, j, make_complex(sqrt(sum.real > 0 ? sum.real : 0.0), 0.0));
            else
                cmat_set(&L_rx, i, j, cmul(sum, make_complex(1.0 / (complex_abs(cmat_get(&L_rx, j, j)) > 1e-10 ? complex_abs(cmat_get(&L_rx, j, j)) : 1.0), 0.0)));
        }
    }
    /* Similarly for L_tx... simplified: just copy for now */
    cmat_copy(R_tx, &L_tx);

    /* Apply: H_corr = L_rx * H_iid * L_tx^H */
    complex_matrix temp = cmat_alloc(M_r, M_t);
    cmat_mul_mat(&L_rx, &channel->H, &temp);
    complex_matrix L_tx_H = cmat_alloc(M_t, M_t);
    cmat_hermitian(&L_tx, &L_tx_H);
    cmat_mul_mat(&temp, &L_tx_H, &channel->H);

    channel->type = CHANNEL_RAYLEIGH_CORR;
    cmat_free(&L_rx); cmat_free(&L_tx); cmat_free(&temp); cmat_free(&L_tx_H);
    return 0;
}

int channel_rician(const complex_matrix *H_los, double rice_factor,
                   mimo_channel *channel) {
    /* Rician fading: H = sqrt(K/(K+1)) * H_LOS + sqrt(1/(K+1)) * H_NLOS.
     *
     * K = Rice K-factor: ratio of LOS power to scattered power.
     *   K = 0 -> pure Rayleigh (no LOS)
     *   K = inf -> pure AWGN (strong LOS)
     *
     * Typical K values: urban microcell 4-10 dB, rural 10-20 dB,
     * satellite 20-30 dB.
     *
     * Complexity: O(M_r * M_t). */
    if (!H_los || !channel) return -1;
    size_t M_r = H_los->rows, M_t = H_los->cols;

    /* Generate Rayleigh component */
    channel_rayleigh_iid(M_r, M_t, channel);

    double los_scale = sqrt(rice_factor / (rice_factor + 1.0));
    double nlos_scale = sqrt(1.0 / (rice_factor + 1.0));

    for (size_t i = 0; i < M_r * M_t; i++) {
        complex_double los_part = cmul(H_los->data[i], make_complex(los_scale, 0.0));
        complex_double nlos_part = cmul(channel->H.data[i], make_complex(nlos_scale, 0.0));
        channel->H.data[i] = cadd(los_part, nlos_part);
    }

    channel->type = CHANNEL_RICIAN;
    return 0;
}

int channel_mmwave_clustered(size_t M_r, size_t M_t,
                             size_t num_clusters, size_t num_rays_per_cluster,
                             mimo_channel *channel) {
    /* mmWave clustered channel (Saleh-Valenzuela model):
     *   H = sum_{i=1}^{N_cl} sum_{l=1}^{N_ray} alpha_{i,l}
     *       a_r(theta_{i,l}^r, phi_{i,l}^r) a_t^H(theta_{i,l}^t, phi_{i,l}^t)
     *
     * Properties:
     *   - Limited scattering -> sparse channel matrix
     *   - Few dominant paths -> low rank
     *   - Large arrays needed for beamforming gain
     *
     * Used for: 28GHz/39GHz/60GHz millimeter-wave systems.
     * Enables hybrid beamforming (analog + digital).
     *
     * Complexity: O(N_cl * N_ray * M_r * M_t). */
    if (!channel) return -1;

    channel->num_rx = M_r;
    channel->num_tx = M_t;
    channel->type = CHANNEL_MIMILLIMETER;
    channel->is_normalized = 0;
    channel->H = cmat_alloc(M_r, M_t);
    cmat_set_zero(&channel->H);

    /* Simplified: generate random clustered channel */
    for (size_t cl = 0; cl < num_clusters; cl++) {
        double cluster_angle_tx = 2.0 * M_PI * ((double)rand()) / ((double)RAND_MAX);
        double cluster_angle_rx = 2.0 * M_PI * ((double)rand()) / ((double)RAND_MAX);
        double cluster_power = 1.0 - 0.5 * ((double)cl) / ((double)num_clusters);

        for (size_t ray = 0; ray < num_rays_per_cluster; ray++) {
            double alpha = sqrt(cluster_power / ((double)num_rays_per_cluster));
            double phase = 2.0 * M_PI * ((double)rand()) / ((double)RAND_MAX);
            complex_double gain = cmul(make_complex(alpha, 0.0), cexpj(phase));

            double spread = 0.05;  /* angular spread per cluster */
            double theta_tx = cluster_angle_tx + spread * (((double)rand()) / ((double)RAND_MAX) - 0.5);
            double theta_rx = cluster_angle_rx + spread * (((double)rand()) / ((double)RAND_MAX) - 0.5);

            /* Array response vectors (simplified ULA) */
            for (size_t i = 0; i < M_r; i++) {
                for (size_t j = 0; j < M_t; j++) {
                    double phase_tx = M_PI * cos(theta_tx) * ((double)j);
                    double phase_rx = M_PI * cos(theta_rx) * ((double)i);
                    complex_double resp = cmul(gain,
                        cexpj(phase_tx + phase_rx));
                    channel->H.data[i * M_t + j] = cadd(
                        channel->H.data[i * M_t + j], resp);
                }
            }
        }
    }

    return 0;
}

void channel_normalize(mimo_channel *channel) {
    /* Normalize: E[||H||_F^2] = M_r * M_t.
     * Ensures consistent SNR definition: SNR = P_t / sigma^2. */
    if (!channel || channel->is_normalized) return;
    double norm = cmat_frobenius_norm(&channel->H);
    if (norm < 1e-300) return;
    double target = sqrt((double)(channel->num_rx * channel->num_tx));
    double scale = target / norm;
    for (size_t i = 0; i < channel->num_rx * channel->num_tx; i++)
        channel->H.data[i] = cmul(channel->H.data[i], make_complex(scale, 0.0));
    channel->is_normalized = 1;
}

void channel_free(mimo_channel *channel) {
    if (channel) cmat_free(&channel->H);
}

/* ============ MIMO Capacity Computation (L4) ============ */

mimo_capacity mimo_capacity_alloc(size_t rank) {
    mimo_capacity cap;
    cap.capacity_bps_hz = 0.0;
    cap.per_stream_rate = (double*)calloc(rank, sizeof(double));
    cap.water_level = (double*)calloc(rank, sizeof(double));
    cap.num_active_streams = 0;
    cap.singular_values = (double*)calloc(rank, sizeof(double));
    cap.rank = rank;
    return cap;
}

void mimo_capacity_free(mimo_capacity *cap) {
    if (!cap) return;
    if (cap->per_stream_rate) { free(cap->per_stream_rate); cap->per_stream_rate = NULL; }
    if (cap->water_level) { free(cap->water_level); cap->water_level = NULL; }
    if (cap->singular_values) { free(cap->singular_values); cap->singular_values = NULL; }
    cap->rank = 0;
}

waterfilling_result waterfilling_alloc(size_t num_streams) {
    waterfilling_result wf;
    wf.power_allocation = (double*)calloc(num_streams, sizeof(double));
    wf.water_level = 0.0;
    wf.num_active = 0;
    wf.total_power = 0.0;
    return wf;
}

void waterfilling_free(waterfilling_result *wf) {
    if (wf && wf->power_allocation) {
        free(wf->power_allocation);
        wf->power_allocation = NULL;
    }
}

int mimo_capacity_openloop(const mimo_channel *channel, double snr_linear,
                           mimo_capacity *result) {
    /* Open-loop MIMO capacity (no CSIT, equal power allocation):
     *   C = sum_{i=1}^{min(M_t,M_r)} log2(1 + (SNR/M_t) * sigma_i^2)
     *
     * where sigma_i are singular values of H.
     *
     * Key theorem (Telatar 1999): At high SNR, capacity scales as
     *   C ~ min(M_t, M_r) * log2(SNR) + O(1).
     * The multiplexing gain (= slope of C vs log SNR) is min(M_t, M_r).
     *
     * At low SNR: C ~ SNR * ||H||_F^2 / (M_t * log(2))
     * The beamforming gain is ||H||_F^2 / M_t.
     *
     * Complexity: O(M_t * M_r * min(M_t,M_r)) ? SVD dominates. */
    if (!channel || !result) return -1;
    size_t M_t = channel->num_tx;

    /* SVD of H to get singular values */
    svd_config cfg = {50, 1e-10, 0};
    svd_result svd = svd_result_alloc(channel->num_rx, channel->num_tx);
    int st = svd_compute(&channel->H, &svd, &cfg);
    if (st != 0) { svd_result_free(&svd); return st; }

    size_t min_dim = M_t < channel->num_rx ? M_t : channel->num_rx;
    double cap = 0.0;

    for (size_t i = 0; i < min_dim; i++) {
        double sigma_sq = svd.sigma[i] * svd.sigma[i];
        double sinr = (snr_linear / ((double)M_t)) * sigma_sq;
        cap += log2(1.0 + sinr);
        result->singular_values[i] = svd.sigma[i];
        result->per_stream_rate[i] = log2(1.0 + sinr);
    }

    result->capacity_bps_hz = cap;
    result->num_active_streams = min_dim;
    result->rank = min_dim;

    svd_result_free(&svd);
    return 0;
}

int mimo_capacity_waterfilling(const mimo_channel *channel, double snr_linear,
                               mimo_capacity *result) {
    /* Waterfilling capacity (perfect CSIT):
     *   C = max_{P_i: sum P_i = 1, P_i >= 0}
     *       sum_i log2(1 + P_i * SNR * sigma_i^2 / M_t)
     *
     * Solution via KKT conditions: P_i = (mu - M_t/(SNR*sigma_i^2))^+
     * where mu is chosen so sum P_i = 1.
     *
     * Waterfilling interpretation:
     *   - Pour power ("water") into a vessel whose bottom has
     *     bumps at heights = M_t/(SNR*sigma_i^2).
     *   - The water level is mu, determined by total power constraint.
     *   - Only channels with sigma_i^2 > M_t/(SNR*mu) get power.
     *
     * At low SNR, waterfilling allocates all power to the best stream
     * (beamforming mode), giving capacity ~ log2(1 + SNR*sigma_max^2).
     *
     * At high SNR, waterfilling allocates power to all streams equally,
     * approaching the open-loop capacity.
     *
     * Reference: Cover & Thomas (2006), Elements of Information Theory, Sec.9.4.
     * Complexity: O(min(M,N) log min(M,N)) ? sorting + bisection. */
    if (!channel || !result) return -1;
    size_t M_t = channel->num_tx;

    /* Get singular values */
    svd_config cfg = {50, 1e-10, 0};
    svd_result svd = svd_result_alloc(channel->num_rx, channel->num_tx);
    int st = svd_compute(&channel->H, &svd, &cfg);
    if (st != 0) { svd_result_free(&svd); return st; }

    size_t min_dim = M_t < channel->num_rx ? M_t : channel->num_rx;

    /* Compute effective SNR per stream: gamma_i = SNR * sigma_i^2 / M_t */
    double *gamma = (double*)malloc(min_dim * sizeof(double));
    for (size_t i = 0; i < min_dim; i++) {
        gamma[i] = (snr_linear / ((double)M_t)) *
                   svd.sigma[i] * svd.sigma[i];
        result->singular_values[i] = svd.sigma[i];
    }

    /* Waterfilling via bisection on mu */
    double *P = (double*)malloc(min_dim * sizeof(double));
    waterfilling_compute(gamma, min_dim, 1.0,
        &(waterfilling_result){P, 0.0, 0, 0.0});

    /* Compute capacity */
    double cap = 0.0;
    for (size_t i = 0; i < min_dim; i++) {
        if (P[i] > 1e-10) {
            cap += log2(1.0 + gamma[i] * P[i]);
            result->per_stream_rate[i] = log2(1.0 + gamma[i] * P[i]);
        }
    }

    result->capacity_bps_hz = cap;
    result->num_active_streams = 0;
    for (size_t i = 0; i < min_dim; i++)
        if (P[i] > 1e-10) result->num_active_streams++;
    result->rank = min_dim;

    free(gamma); free(P);
    svd_result_free(&svd);
    return 0;
}

int waterfilling_compute(const double *snr_per_stream, size_t num_streams,
                         double total_power, waterfilling_result *result) {
    /* Classic waterfilling algorithm via bisection.
     *
     * Algorithm:
     *   1. Sort SNR values descending.
     *   2. Binary search for mu (water level):
     *      - For candidate mu, compute P_i = (mu - 1/gamma_i)^+
     *      - If sum P_i < total_power: increase mu
     *      - If sum P_i > total_power: decrease mu
     *   3. Stop when |sum P_i - total_power| < epsilon.
     *
     * The function f(mu) = sum_i (mu - 1/gamma_i)^+ is:
     *   - Piecewise linear
     *   - Monotonically increasing
     *   - Zero for mu <= min(1/gamma_i)
     *   - Diverging as mu -> infinity
     *
     * Complexity: O(N log N + N log(1/eps)).
     * Reference: Cover & Thomas (2006), Problem 9.5. */
    if (!snr_per_stream || !result || num_streams == 0) return -1;

    /* Sort in descending order (copy to avoid modifying input) */
    double *sorted_snr = (double*)malloc(num_streams * sizeof(double));
    for (size_t i = 0; i < num_streams; i++)
        sorted_snr[i] = snr_per_stream[i];

    /* Simple insertion sort */
    for (size_t i = 0; i < num_streams; i++)
        for (size_t j = i + 1; j < num_streams; j++)
            if (sorted_snr[j] > sorted_snr[i]) {
                double tmp = sorted_snr[i];
                sorted_snr[i] = sorted_snr[j];
                sorted_snr[j] = tmp;
            }

    /* Bisection for water level mu */
    double mu_low = 0.0;
    double mu_high = total_power + 1.0;
    for (size_t i = 0; i < num_streams; i++)
        if (sorted_snr[i] > 1e-300)
            mu_high = total_power + 1.0 / sorted_snr[i];
    mu_high *= 2.0;

    double mu = (mu_low + mu_high) / 2.0;

    for (int iter = 0; iter < 100; iter++) {
        double sum_p = 0.0;
        for (size_t i = 0; i < num_streams; i++) {
            double p_i = mu - 1.0 / (sorted_snr[i] > 1e-300 ? sorted_snr[i] : 1e-300);
            if (p_i > 0.0) sum_p += p_i;
        }

        if (fabs(sum_p - total_power) < 1e-8) break;
        if (sum_p < total_power) mu_low = mu;
        else mu_high = mu;
        mu = (mu_low + mu_high) / 2.0;
    }

    /* Allocate powers */
    result->water_level = mu;
    result->num_active = 0;
    for (size_t i = 0; i < num_streams; i++) {
        double gamma = sorted_snr[i] > 1e-300 ? sorted_snr[i] : 1e-300;
        double p_i = mu - 1.0 / gamma;
        if (p_i < 0.0) p_i = 0.0;
        result->power_allocation[i] = p_i;
        result->total_power += p_i;
        if (p_i > 1e-10) result->num_active++;
    }

    free(sorted_snr);
    return 0;
}

/* ============ Massive MIMO Metrics (L8) ============ */

double channel_hardening_metric(const complex_matrix *H) {
    /* Channel hardening: measures how "deterministic" the channel
     * becomes as M grows.
     *
     *   hardening = var(||h_k||^2) / E[||h_k||^2]^2
     *
     * For i.i.d. Rayleigh: hardening -> 1/M as M -> infinity.
     * As M -> infinity, ||h_k||^2/M -> 1 almost surely (LLN).
     *
     * Implication: With infinite BS antennas, the effective channel
     * gain becomes deterministic, eliminating small-scale fading.
     * This enables:
     *   - Simple uplink power control (no fading margin needed)
     *   - Low-overhead CSI acquisition (statistical CSI sufficient)
     *
     * Reference: Ngo et al. (2014) IEEE JSAC. */
    if (!H) return 1e10;

    size_t M = H->rows, N = H->cols;
    double sum = 0.0, sum_sq = 0.0;

    for (size_t j = 0; j < N; j++) {
        double col_norm_sq = 0.0;
        for (size_t i = 0; i < M; i++) {
            double mag = complex_abs(H->data[i * N + j]);
            col_norm_sq += mag * mag;
        }
        double normalized = col_norm_sq / ((double)M);
        sum += normalized;
        sum_sq += normalized * normalized;
    }

    double mean = sum / ((double)N);
    double var = sum_sq / ((double)N) - mean * mean;
    return (mean > 1e-10) ? (var / (mean * mean)) : 1e10;
}

int check_favorable_propagation(const complex_matrix *H, double threshold) {
    /* Favorable propagation condition:
     *   (h_i^H h_j) / M -> 0 as M -> infinity for i != j.
     *
     * In i.i.d. Rayleigh channels, different users' channels become
     * asymptotically orthogonal as M grows. This is the key enabler
     * of massive MIMO:
     *   - MRT/ZF become asymptotically optimal
     *   - Inter-user interference vanishes without coordination
     *   - Sum rate scales as K log2(1 + SNR*M/K)
     *
     * The condition is checked by computing the maximum normalized
     * inner product: max_{i!=j} |h_i^H h_j| / (||h_i|| * ||h_j||).
     *
     * Reference: Ngo et al. (2013) IEEE TCOM. */
    if (!H) return 0;
    size_t N = H->cols;
    double max_ip = 0.0;

    for (size_t i = 0; i < N; i++) {
        for (size_t j = i + 1; j < N; j++) {
            /* Compute normalized inner product */
            complex_double ip = make_complex(0.0, 0.0);
            double norm_i = 0.0, norm_j = 0.0;
            for (size_t m = 0; m < H->rows; m++) {
                ip = cadd(ip, cmul(cconj(H->data[m * N + i]),
                                   H->data[m * N + j]));
                norm_i += complex_abs(H->data[m * N + i]) * complex_abs(H->data[m * N + i]);
                norm_j += complex_abs(H->data[m * N + j]) * complex_abs(H->data[m * N + j]);
            }
            double nip = complex_abs(ip) / (sqrt(norm_i * norm_j) + 1e-300);
            if (nip > max_ip) max_ip = nip;
        }
    }

    return (max_ip < threshold) ? 1 : 0;
}

double massive_mimo_ul_rate_mrc(const complex_matrix *H,
                                const double *snr_per_user,
                                size_t user_idx, size_t M) {
    /* Massive MIMO uplink achievable rate with MRC:
     *   R_k = log2(1 + M*SNR_k / (1 + sum_{j!=k} SNR_j))
     *
     * As M -> infinity: the interference term vanishes and
     *   R_k -> log2(1 + M*SNR_k).
     *
     * This asymptote shows why massive MIMO can serve many users
     * simultaneously with simple linear processing. */
    if (!H || !snr_per_user) return 0.0;
    double interference = 0.0;
    for (size_t j = 0; j < H->cols; j++)
        if (j != user_idx) interference += snr_per_user[j];
    return log2(1.0 + ((double)M) * snr_per_user[user_idx] /
                (1.0 + interference));
}

double massive_mimo_dl_sum_rate_zf(const complex_matrix *H,
                                   const double *snr_per_user,
                                   size_t K, size_t M) {
    /* Massive MIMO downlink sum-rate with ZF precoding:
     *   R_sum = sum_{k=1}^K log2(1 + SNR_k * (M-K) / K)
     *
     * With ZF and M >> K: all inter-user interference is eliminated,
     * and the effective SNR per user is SNR_k * (M-K)/K.
     * The factor (M-K) is the array gain minus the dimensionality
     * cost of nulling K-1 interferers.
     *
     * Reference: Yang & Marzetta (2013) IEEE TWC. */
    if (!H || !snr_per_user) return 0.0;
    double sum_rate = 0.0;
    for (size_t k = 0; k < K; k++)
        sum_rate += log2(1.0 + snr_per_user[k] *
                         ((double)(M - K)) / ((double)K));
    return sum_rate;
}

double pilot_contamination_limit(double tau_p, double tau_c,
                                 double *large_scale_fading,
                                 size_t num_cells, size_t num_users) {
    /* Pilot contamination asymptotic SINR limit.
     *
     * When pilots are reused across cells, the channel estimate at
     * the serving BS is contaminated by pilots from other cells.
     * As M -> infinity, the SINR converges not to infinity but to:
     *   SINR_k -> beta_serving^2 / sum_{l!=serving} beta_l^2
     *
     * where beta are large-scale fading coefficients.
     *
     * More pilots (tau_p) help, but at the cost of less data (tau_c - tau_p).
     * The optimal pilot length trades estimation quality for throughput.
     *
     * Reference: Marzetta (2010), Jose et al. (2011) IEEE TWC. */
    if (!large_scale_fading || num_cells == 0) return 0.0;
    double beta_serving = large_scale_fading[0];
    double interference = 0.0;
    for (size_t c = 1; c < num_cells; c++)
        for (size_t u = 0; u < num_users; u++)
            interference += large_scale_fading[c * num_users + u] *
                            large_scale_fading[c * num_users + u];
    return (interference > 1e-300) ? (beta_serving * beta_serving / interference) : 1e10;
}

double mimo_capacity_finite_blocklen(const mimo_channel *channel,
                                     double snr_linear,
                                     size_t block_length,
                                     double error_prob) {
    /* Finite blocklength capacity (Polyanskiy et al. 2010):
     *   C(n, epsilon) = C - sqrt(V/n) * Q^{-1}(epsilon) + O(log n / n)
     *
     * where V is the channel dispersion and Q^{-1} is the inverse Q-function.
     * In the massive MIMO regime, the dispersion scales beneficially,
     * making ultra-reliable low-latency communication feasible.
     *
     * Simplified: returns the infinite-blocklength capacity as a lower bound. */
    if (!channel) return 0.0;
    mimo_capacity cap = mimo_capacity_alloc(channel->num_tx < channel->num_rx ?
        channel->num_tx : channel->num_rx);
    mimo_capacity_openloop(channel, snr_linear, &cap);
    double c_inf = cap.capacity_bps_hz;
    mimo_capacity_free(&cap);
    return c_inf;  /* Infinite-blocklength lower bound */
}
