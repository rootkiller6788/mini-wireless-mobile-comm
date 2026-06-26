/**
 * @file mimo_channel.c
 * @brief MIMO Channel Matrix Implementations (L3, L4, L5, L6, L8)
 *
 * Implements MIMO channel matrix generation (i.i.d., Kronecker correlated,
 * massive MIMO), capacity computation (equal power, water-filling), and
 * channel quality metrics (condition number, rank, diversity order).
 *
 * Theorem Coverage:
 *   L3: MIMO channel matrix operations (Frobenius norm, element access)
 *   L4: Telatar/Foschini MIMO capacity formula
 *   L5: Cholesky-based Kronecker correlated MIMO generation
 *   L5: Water-filling power allocation algorithm
 *   L5: Exponential spatial correlation model
 *   L6: MIMO capacity evaluation (ergodic capacity)
 *   L8: Massive MIMO channel hardening
 *   L8: 3GPP 3D spatial channel model
 *
 * Reference:
 *   Telatar, "Capacity of Multi-antenna Gaussian Channels", 1999
 *   Foschini & Gans, "On Limits of Wireless Communications...", 1998
 *   Paulraj, Nabar, Gore, "Introduction to Space-Time Wireless
 *     Communications", 2003
 *   3GPP TR 38.901 v16.1.0 (2020)
 */

#include "mimo_channel.h"
#include "fading.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*============================================================================
 * L3: MIMO Channel Matrix Operations
 *============================================================================*/

mimo_channel_matrix_t *mimo_channel_alloc(size_t num_rx, size_t num_tx)
{
    if (num_rx == 0 || num_tx == 0) return NULL;

    mimo_channel_matrix_t *mimo =
        (mimo_channel_matrix_t *)malloc(sizeof(mimo_channel_matrix_t));
    if (!mimo) return NULL;

    mimo->num_rx = num_rx;
    mimo->num_tx = num_tx;
    mimo->h = (double complex *)calloc(num_rx * num_tx, sizeof(double complex));

    if (!mimo->h) {
        free(mimo);
        return NULL;
    }

    return mimo;
}

void mimo_channel_free(mimo_channel_matrix_t *mimo)
{
    if (!mimo) return;
    free(mimo->h);
    free(mimo);
}

int mimo_channel_copy(mimo_channel_matrix_t *dst,
                       const mimo_channel_matrix_t *src)
{
    if (!dst || !src) return -1;
    if (dst->num_rx != src->num_rx || dst->num_tx != src->num_tx) return -1;

    size_t n_elements = src->num_rx * src->num_tx;
    memcpy(dst->h, src->h, n_elements * sizeof(double complex));
    return 0;
}

double complex mimo_get_element(const mimo_channel_matrix_t *mimo,
                                 size_t rx_idx, size_t tx_idx)
{
    if (!mimo || rx_idx >= mimo->num_rx || tx_idx >= mimo->num_tx) {
        return 0.0 + 0.0 * I;
    }
    return mimo->h[rx_idx * mimo->num_tx + tx_idx];
}

void mimo_set_element(mimo_channel_matrix_t *mimo,
                       size_t rx_idx, size_t tx_idx, double complex value)
{
    if (!mimo || rx_idx >= mimo->num_rx || tx_idx >= mimo->num_tx) return;
    mimo->h[rx_idx * mimo->num_tx + tx_idx] = value;
}

double mimo_frobenius_norm(const mimo_channel_matrix_t *mimo)
{
    if (!mimo) return 0.0;

    double sum_sq = 0.0;
    size_t n = mimo->num_rx * mimo->num_tx;

    for (size_t i = 0; i < n; i++) {
        double mag_sq = creal(mimo->h[i] * conj(mimo->h[i]));
        sum_sq += mag_sq;
    }

    return sqrt(sum_sq);
}

/*============================================================================
 * L5: MIMO Channel Matrix Generation
 *============================================================================*/

int mimo_generate_iid_rayleigh(mimo_channel_matrix_t *mimo)
{
    if (!mimo) return -1;

    size_t n = mimo->num_rx * mimo->num_tx;
    /* CN(0,1): variance 1 per complex dimension */

    for (size_t i = 0; i < n; i++) {
        mimo->h[i] = fading_rand_complex_gaussian(1.0);
    }

    return 0;
}

int mimo_generate_iid_rician(mimo_channel_matrix_t *mimo, double k_factor_db)
{
    if (!mimo) return -1;

    double k_linear = pow(10.0, k_factor_db / 10.0);
    /* K = nu^2/(2*sigma^2). For unit power: 2*sigma^2 = 1/(K+1), nu = sqrt(K/(K+1)) */
    double sigma = sqrt(1.0 / (2.0 * (k_linear + 1.0)));
    double nu = sqrt(k_linear / (k_linear + 1.0));

    size_t n = mimo->num_rx * mimo->num_tx;

    for (size_t i = 0; i < n; i++) {
        /* Deterministic LOS component:
         * Phase = 0 (same for all elements in simplest model) */
        double complex los_component = nu + 0.0 * I;

        /* Diffuse NLOS component: CN(0, 2*sigma^2) */
        double complex nlos_component = fading_rand_complex_gaussian(
            sigma * sqrt(2.0));

        mimo->h[i] = los_component + nlos_component;
    }

    return 0;
}

int mimo_generate_kronecker(mimo_channel_matrix_t *mimo,
                             const double *corr_rx,
                             const double *corr_tx)
{
    if (!mimo || !corr_rx || !corr_tx) return -1;

    size_t N_rx = mimo->num_rx;
    size_t N_tx = mimo->num_tx;

    /* Cholesky decompose correlation matrices: R = L*L^T */
    double *L_rx = (double *)malloc(N_rx * N_rx * sizeof(double));
    double *L_tx = (double *)malloc(N_tx * N_tx * sizeof(double));

    if (!L_rx || !L_tx) {
        free(L_rx);
        free(L_tx);
        return -1;
    }

    if (fading_cholesky_decomp(corr_rx, N_rx, L_rx) != 0 ||
        fading_cholesky_decomp(corr_tx, N_tx, L_tx) != 0) {
        free(L_rx);
        free(L_tx);
        return -1;
    }

    /* Generate i.i.d. H_iid */
    double complex *h_iid = (double complex *)malloc(
        N_rx * N_tx * sizeof(double complex));
    if (!h_iid) {
        free(L_rx);
        free(L_tx);
        return -1;
    }

    for (size_t i = 0; i < N_rx * N_tx; i++) {
        h_iid[i] = fading_rand_complex_gaussian(1.0);
    }

    /* H_corr = L_rx * H_iid * L_tx'
     * Implementation: temp = H_iid * L_tx', then H = L_rx * temp
     * Since L_tx is real and lower triangular, L_tx' = L_tx^T */
    double complex *temp = (double complex *)calloc(
        N_rx * N_tx, sizeof(double complex));
    if (!temp) {
        free(h_iid);
        free(L_rx);
        free(L_tx);
        return -1;
    }

    /* temp = H_iid * L_tx^T */
    for (size_t i = 0; i < N_rx; i++) {
        for (size_t j = 0; j < N_tx; j++) {
            double complex sum = 0.0 + 0.0 * I;
            for (size_t k = 0; k < N_tx; k++) {
                sum += h_iid[i * N_tx + k] * L_tx[j * N_tx + k];
            }
            temp[i * N_tx + j] = sum;
        }
    }

    /* H = L_rx * temp (L_rx is lower triangular) */
    for (size_t i = 0; i < N_rx; i++) {
        for (size_t j = 0; j < N_tx; j++) {
            double complex sum = 0.0 + 0.0 * I;
            for (size_t k = 0; k < N_rx; k++) {
                sum += L_rx[i * N_rx + k] * temp[k * N_tx + j];
            }
            mimo->h[i * N_tx + j] = sum;
        }
    }

    free(h_iid);
    free(temp);
    free(L_rx);
    free(L_tx);
    return 0;
}

void mimo_exponential_correlation(double *corr_matrix, size_t n, double rho)
{
    if (!corr_matrix || n == 0) return;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            int diff = (int)i - (int)j;
            if (diff < 0) diff = -diff;
            corr_matrix[i * n + j] = pow(rho, (double)diff);
        }
    }
}

/*============================================================================
 * L5: Power Iteration for Dominant Eigenvalue
 *
 * Used internally for singular value estimation and condition number.
 * The power method converges to the dominant eigenvalue:
 *   v_{k+1} = A*v_k / ||A*v_k||
 *   lambda_max ~ v^T*A*v / v^T*v
 *============================================================================*/

static double power_iteration_dominant_eigenvalue(
    const double *A, size_t n, int max_iter, double tol)
{
    if (n == 0 || !A) return 0.0;

    double *v = (double *)malloc(n * sizeof(double));
    double *Av = (double *)malloc(n * sizeof(double));
    if (!v || !Av) {
        free(v);
        free(Av);
        return 0.0;
    }

    /* Initialize with ones */
    for (size_t i = 0; i < n; i++) v[i] = 1.0;

    double lambda_old = 0.0, lambda_new = 0.0;

    for (int iter = 0; iter < max_iter; iter++) {
        /* Av = A * v */
        double norm = 0.0;
        for (size_t i = 0; i < n; i++) {
            Av[i] = 0.0;
            for (size_t j = 0; j < n; j++) {
                Av[i] += A[i * n + j] * v[j];
            }
            norm += Av[i] * Av[i];
        }

        norm = sqrt(norm);
        if (norm < 1e-15) break;

        /* Normalize and estimate eigenvalue */
        lambda_new = 0.0;
        for (size_t i = 0; i < n; i++) {
            v[i] = Av[i] / norm;
            lambda_new += v[i] * Av[i];
        }
        lambda_new /= (norm > 0 ? 1.0 : 1.0);

        if (fabs(lambda_new - lambda_old) < tol) break;
        lambda_old = lambda_new;
    }

    /* Verify: v^T*A*v / v^T*v gives the Rayleigh quotient */
    for (size_t i = 0; i < n; i++) {
        Av[i] = 0.0;
        for (size_t j = 0; j < n; j++) {
            Av[i] += A[i * n + j] * v[j];
        }
    }

    double rayleigh_num = 0.0, rayleigh_den = 0.0;
    for (size_t i = 0; i < n; i++) {
        rayleigh_num += v[i] * Av[i];
        rayleigh_den += v[i] * v[i];
    }

    double result = (rayleigh_den > 0) ? rayleigh_num / rayleigh_den : lambda_new;

    free(v);
    free(Av);
    return result;
}

/*============================================================================
 * L4: MIMO Channel Capacity
 *============================================================================*/

int mimo_capacity_equal_power(const mimo_channel_matrix_t *mimo,
                               double snr_db, double bandwidth_hz,
                               channel_capacity_t *capacity)
{
    if (!mimo || !capacity) return -1;

    size_t N_rx = mimo->num_rx;
    size_t N_tx = mimo->num_tx;

    double snr_linear = pow(10.0, snr_db / 10.0);
    double rho = snr_linear / (double)N_tx;  /* SNR per transmit antenna */

    /* Build H*H^H matrix (N_rx x N_rx).
     * For capacity: det(I + rho*H*H^H)
     * If N_rx > N_tx, use det(I + rho*H^H*H) which is N_tx x N_tx. */

    size_t n = (N_rx <= N_tx) ? N_rx : N_tx;
    double *gram = (double *)calloc(n * n, sizeof(double));
    if (!gram) return -1;

    if (N_rx <= N_tx) {
        /* H*H^H: N_rx x N_rx (but we use n_min) */
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                double complex sum = 0.0 + 0.0 * I;
                for (size_t k = 0; k < N_tx; k++) {
                    sum += mimo->h[i * N_tx + k] *
                           conj(mimo->h[j * N_tx + k]);
                }
                gram[i * n + j] = creal(sum);
            }
        }
    } else {
        /* H^H*H: N_tx x N_tx */
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                double complex sum = 0.0 + 0.0 * I;
                for (size_t k = 0; k < N_rx; k++) {
                    sum += conj(mimo->h[k * N_tx + i]) *
                           mimo->h[k * N_tx + j];
                }
                gram[i * n + j] = creal(sum);
            }
        }
    }

    /* Eigendecomposition via power iteration + deflation for n <= 4.
     * For larger n, use power iteration estimate (approximate). */
    capacity->singular_values = (double *)malloc(n * sizeof(double));
    if (!capacity->singular_values) {
        free(gram);
        return -1;
    }

    double *lambda = capacity->singular_values;
    if (n <= 4) {
        /* Simple: for small n use iterative method.
         * Estimate largest eigenvalue, then deflate for next.
         * In production code, use LAPACK DSYEV. */
        double *A_copy = (double *)malloc(n * n * sizeof(double));
        if (!A_copy) {
            free(gram);
            free(capacity->singular_values);
            capacity->singular_values = NULL;
            return -1;
        }

        memcpy(A_copy, gram, n * n * sizeof(double));

        for (size_t k = 0; k < n; k++) {
            lambda[k] = power_iteration_dominant_eigenvalue(
                A_copy, n, 100, 1e-6);

            /* Deflate: A = A - lambda*v*v^T (simplified) */
            if (lambda[k] > 1e-10) {
                for (size_t i = 0; i < n; i++) {
                    A_copy[i * n + i] -= lambda[k] / (double)n;
                }
            }
        }

        /* Sort descending */
        for (size_t i = 0; i < n - 1; i++) {
            for (size_t j = i + 1; j < n; j++) {
                if (lambda[j] > lambda[i]) {
                    double tmp = lambda[i];
                    lambda[i] = lambda[j];
                    lambda[j] = tmp;
                }
            }
        }

        free(A_copy);
    } else {
        /* Approximate: use power iteration for largest,
         * assume equal spread for others */
        lambda[0] = power_iteration_dominant_eigenvalue(gram, n, 100, 1e-6);
        double trace = 0.0;
        for (size_t i = 0; i < n; i++) trace += gram[i * n + i];
        double remaining = trace - lambda[0];
        double avg = remaining / (double)(n - 1);
        for (size_t i = 1; i < n; i++) lambda[i] = avg;
    }

    /* Capacity: C = B * sum_{i=1}^{n} log2(1 + rho*lambda_i) */
    double sum_log = 0.0;
    capacity->rank = 0;

    for (size_t i = 0; i < n; i++) {
        if (lambda[i] < 1e-12) lambda[i] = 0.0;
        if (lambda[i] > 1e-12) capacity->rank++;
        double arg = 1.0 + rho * lambda[i];
        if (arg > 1.0) {
            sum_log += log2(arg);
        }
    }

    capacity->capacity_bps = bandwidth_hz * sum_log;
    capacity->spectral_efficiency = sum_log;
    capacity->num_streams = capacity->rank;

    free(gram);
    return 0;
}

int mimo_capacity_waterfilling(const mimo_channel_matrix_t *mimo,
                                double snr_db, double bandwidth_hz,
                                channel_capacity_t *capacity)
{
    if (!mimo || !capacity) return -1;

    /* First compute equal-power capacity to get eigenvalues */
    if (mimo_capacity_equal_power(mimo, snr_db, bandwidth_hz, capacity) != 0) {
        return -1;
    }

    size_t N_tx = mimo->num_tx;
    size_t n = (mimo->num_rx < N_tx) ? mimo->num_rx : N_tx;
    double snr_linear = pow(10.0, snr_db / 10.0);

    /* Water-filling:
     * P_i = max(0, mu - 1/((SNR/N_t)*lambda_i))
     * where mu is chosen so that sum(P_i) = 1 (normalized to total power) */

    double *lambda = capacity->singular_values;
    if (!lambda || n == 0) return -1;

    /* Sort eigenvalues ascending for water-filling */
    for (size_t i = 0; i < n - 1; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (lambda[j] < lambda[i]) {
                double tmp = lambda[i];
                lambda[i] = lambda[j];
                lambda[j] = tmp;
            }
        }
    }

    /* Water-filling iterative algorithm:
     * Start with all streams active, compute water level,
     * remove streams with negative power, repeat. */

    double *noise_to_gain = (double *)malloc(n * sizeof(double));
    if (!noise_to_gain) return -1;

    for (size_t i = 0; i < n; i++) {
        if (lambda[i] > 1e-12) {
            noise_to_gain[i] = (double)N_tx / (snr_linear * lambda[i]);
        } else {
            noise_to_gain[i] = 1e15;  /* effectively infinite, zero power */
        }
    }

    int num_active = (int)n;
    double mu = 0.0;  /* water level */

    while (num_active > 0) {
        /* Compute water level: mu = (1/num_active) * (1 + sum(N0/lambda_i)) */
        double sum_ntg = 0.0;
        for (size_t i = 0; i < (size_t)num_active; i++) {
            sum_ntg += noise_to_gain[i];
        }
        mu = (1.0 + sum_ntg) / (double)num_active;

        /* Check if last active channel gets negative power */
        if (mu <= noise_to_gain[num_active - 1]) {
            num_active--;
        } else {
            break;
        }
    }

    /* Compute capacity with water-filling powers */
    double sum_log_wf = 0.0;
    for (size_t i = 0; i < (size_t)num_active; i++) {
        double power_alloc = mu - noise_to_gain[i];
        if (power_alloc > 0.0) {
            double arg = 1.0 + power_alloc * snr_linear *
                         lambda[i] / (double)N_tx;
            if (arg > 1.0) {
                sum_log_wf += log2(arg);
            }
        }
    }

    capacity->capacity_bps = bandwidth_hz * sum_log_wf;
    capacity->spectral_efficiency = sum_log_wf;

    free(noise_to_gain);
    return 0;
}

void mimo_capacity_free(channel_capacity_t *capacity)
{
    if (!capacity) return;
    free(capacity->singular_values);
    capacity->singular_values = NULL;
    capacity->rank = 0;
}

/*============================================================================
 * L5: MIMO Channel Metrics
 *============================================================================*/

double mimo_condition_number(const mimo_channel_matrix_t *mimo)
{
    if (!mimo) return 1.0;

    /* Use capacity computation to get singular values */
    channel_capacity_t cap = {0};
    if (mimo_capacity_equal_power(mimo, 0.0, 1.0, &cap) != 0) {
        return INFINITY;
    }

    size_t n = (mimo->num_rx < mimo->num_tx) ? mimo->num_rx : mimo->num_tx;
    if (!cap.singular_values || n == 0) {
        mimo_capacity_free(&cap);
        return INFINITY;
    }

    /* Find min and max singular values */
    double sig_max = cap.singular_values[0];
    double sig_min = cap.singular_values[0];

    for (size_t i = 1; i < n; i++) {
        if (cap.singular_values[i] > sig_max) sig_max = cap.singular_values[i];
        if (cap.singular_values[i] < sig_min) sig_min = cap.singular_values[i];
    }

    mimo_capacity_free(&cap);

    if (sig_min < 1e-15) return INFINITY;
    return sig_max / sig_min;
}

size_t mimo_rank(const mimo_channel_matrix_t *mimo, double threshold_db)
{
    if (!mimo) return 0;

    channel_capacity_t cap = {0};
    if (mimo_capacity_equal_power(mimo, 0.0, 1.0, &cap) != 0) {
        return 0;
    }

    size_t n = (mimo->num_rx < mimo->num_tx) ? mimo->num_rx : mimo->num_tx;
    if (!cap.singular_values || n == 0) {
        mimo_capacity_free(&cap);
        return 0;
    }

    /* Find max singular value */
    double sig_max = cap.singular_values[0];
    for (size_t i = 1; i < n; i++) {
        if (cap.singular_values[i] > sig_max) sig_max = cap.singular_values[i];
    }

    double threshold_linear = pow(10.0, threshold_db / 10.0);
    size_t rank = 0;

    for (size_t i = 0; i < n; i++) {
        if (cap.singular_values[i] >= sig_max * threshold_linear) {
            rank++;
        }
    }

    mimo_capacity_free(&cap);
    return rank;
}

int mimo_spatial_correlation(const mimo_channel_matrix_t *realizations,
                              size_t num_realizations,
                              spatial_corr_matrix_t *corr_matrix)
{
    if (!realizations || num_realizations == 0 || !corr_matrix) return -1;

    size_t N = realizations[0].num_rx * realizations[0].num_tx;

    /* Sample covariance: R = (1/K) * sum(vec(H_k) * vec(H_k)^H) */
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < N; j++) {
            double complex sum = 0.0 + 0.0 * I;
            for (size_t k = 0; k < num_realizations; k++) {
                sum += realizations[k].h[i] *
                       conj(realizations[k].h[j]);
            }
            corr_matrix->matrix[i * N + j] =
                sum / (double)num_realizations;
        }
    }

    return 0;
}

double mimo_diversity_order(const mimo_channel_matrix_t *mimo)
{
    if (!mimo) return 1.0;

    /* Diversity order = effective number of independent channels.
     * Estimated from eigenvalue spread of HH^H.
     * For full-rank i.i.d. Rayleigh: diversity = N_rx * N_tx. */
    channel_capacity_t cap = {0};
    if (mimo_capacity_equal_power(mimo, 0.0, 1.0, &cap) != 0) {
        return 1.0;
    }

    size_t n = (mimo->num_rx < mimo->num_tx) ? mimo->num_rx : mimo->num_tx;
    if (!cap.singular_values || n == 0) {
        mimo_capacity_free(&cap);
        return 1.0;
    }

    /* Diversity from eigenvalue distribution:
     * d = (sum lambda_i)^2 / sum(lambda_i^2) */
    double sum_lam = 0.0, sum_lam_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum_lam += cap.singular_values[i];
        sum_lam_sq += cap.singular_values[i] * cap.singular_values[i];
    }

    mimo_capacity_free(&cap);

    if (sum_lam_sq < 1e-15) return 1.0;
    double diversity = (sum_lam * sum_lam) / sum_lam_sq;

    if (diversity < 1.0) diversity = 1.0;
    return diversity;
}

/*============================================================================
 * L8: Massive MIMO
 *============================================================================*/

int mimo_generate_massive_iid(mimo_channel_matrix_t *mimo)
{
    /* Massive MIMO: N_tx >> N_rx. Channel columns become asymptotically
     * orthogonal. Generate i.i.d. CN(0,1) elements.
     * Channel hardening: (1/N_tx)*HH^H -> I as N_tx -> infinity. */
    if (!mimo) return -1;

    size_t n = mimo->num_rx * mimo->num_tx;

    for (size_t i = 0; i < n; i++) {
        mimo->h[i] = fading_rand_complex_gaussian(1.0);
    }

    return 0;
}

double mimo_channel_hardening_metric(const mimo_channel_matrix_t *mimo)
{
    if (!mimo || mimo->num_tx == 0) return 1.0;

    size_t N_rx = mimo->num_rx;
    size_t N_tx = mimo->num_tx;

    /* Compute (1/N_tx) * H * H^H and compare to I */
    double *gram = (double *)calloc(N_rx * N_rx, sizeof(double));
    if (!gram) return 1.0;

    for (size_t i = 0; i < N_rx; i++) {
        for (size_t j = 0; j < N_rx; j++) {
            double complex sum = 0.0 + 0.0 * I;
            for (size_t k = 0; k < N_tx; k++) {
                sum += mimo->h[i * N_tx + k] *
                       conj(mimo->h[j * N_tx + k]);
            }
            gram[i * N_rx + j] = creal(sum) / (double)N_tx;
        }
    }

    /* Frobenius norm of (gram - I) divided by norm of I = sqrt(N_rx) */
    double frob_diff = 0.0;
    for (size_t i = 0; i < N_rx; i++) {
        for (size_t j = 0; j < N_rx; j++) {
            double diff = gram[i * N_rx + j] - ((i == j) ? 1.0 : 0.0);
            frob_diff += diff * diff;
        }
    }

    free(gram);
    return sqrt(frob_diff) / sqrt((double)N_rx);
}

int mimo_generate_3gpp_3d(mimo_channel_matrix_t *mimo,
                           size_t num_clusters,
                           double angular_spread_deg,
                           double elevation_spread_deg,
                           double carrier_freq_hz,
                           double ant_spacing_rx,
                           double ant_spacing_tx)
{
    if (!mimo || num_clusters == 0) return -1;

    size_t N_rx = mimo->num_rx;
    size_t N_tx = mimo->num_tx;
    double lambda = CHANNEL_C0 / carrier_freq_hz;

    /* Clear channel matrix */
    memset(mimo->h, 0, N_rx * N_tx * sizeof(double complex));

    for (size_t c = 0; c < num_clusters; c++) {
        /* Random cluster angles (uniformly distributed) */
        double aod_az = (fading_rand_uniform() - 0.5) * angular_spread_deg *
                        M_PI / 180.0;
        double aoa_az = (fading_rand_uniform() - 0.5) * angular_spread_deg *
                        M_PI / 180.0;
        double zod = (fading_rand_uniform() - 0.5) * elevation_spread_deg *
                     M_PI / 180.0 + M_PI / 2.0;
        double zoa = (fading_rand_uniform() - 0.5) * elevation_spread_deg *
                     M_PI / 180.0 + M_PI / 2.0;

        /* Cluster power (exponential decay with cluster index) */
        double cluster_power = exp(-(double)c * 0.5);
        double cluster_gain = sqrt(cluster_power);

        /* Random phase per cluster */
        double phase = 2.0 * M_PI * fading_rand_uniform();

        for (size_t i = 0; i < N_rx; i++) {
            for (size_t j = 0; j < N_tx; j++) {
                /* Array response: steering vector phase
                 * phi_rx = 2*pi*d_rx*(i*sin(zoa)*cos(aoa_az)) / lambda
                 * phi_tx = 2*pi*d_tx*(j*sin(zod)*cos(aod_az)) / lambda */
                double phi_rx = 2.0 * M_PI * ant_spacing_rx * (double)i *
                                sin(zoa) * cos(aoa_az) / lambda;
                double phi_tx = 2.0 * M_PI * ant_spacing_tx * (double)j *
                                sin(zod) * cos(aod_az) / lambda;

                double total_phase = phase + phi_rx + phi_tx;
                mimo->h[i * N_tx + j] +=
                    cluster_gain * (cos(total_phase) + sin(total_phase) * I);
            }
        }
    }

    /* Normalize to unit Frobenius norm */
    double norm = mimo_frobenius_norm(mimo);
    if (norm > 1e-15) {
        double inv_norm = 1.0 / norm;
        for (size_t i = 0; i < N_rx * N_tx; i++) {
            mimo->h[i] *= inv_norm;
        }
    }

    return 0;
}
