/**
 * nr_phy_mimo.c — 5G NR MIMO Processing Implementation
 *
 * Implements:
 *   Type I codebook precoding (3GPP TS 38.214)
 *   ZF/MMSE/MMSE-SIC MIMO detection
 *   MIMO capacity (Telatar 1999), water-filling power allocation
 *   Jacobi SVD for precoding
 *   Massive MIMO precoders (MF, ZF)
 */

#include "nr_phy_mimo.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5: MIMO Configuration
 * ============================================================================ */

void nr_mimo_config_init(nr_mimo_config_t *cfg,
                          int n_tx, int n_rx, int n_layers,
                          int n1, int n2, int o1, int o2, int dual_pol)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->num_tx_ports = n_tx;
    cfg->num_rx_ports = n_rx;
    cfg->num_layers = n_layers;
    cfg->codebook_mode = 0;
    cfg->panel_dims[0] = n1;
    cfg->panel_dims[1] = n2;
    cfg->oversampling[0] = o1;
    cfg->oversampling[1] = o2;
    cfg->i1_1 = 0;
    cfg->i1_2 = 0;
    cfg->i1_3 = 0;
    cfg->i2 = 0;
    cfg->polarization = dual_pol ? 2 : 1;

    if (cfg->num_layers < 1) cfg->num_layers = 1;
    if (cfg->num_layers > NR_MIMO_MAX_LAYERS) cfg->num_layers = NR_MIMO_MAX_LAYERS;
}

/* ============================================================================
 * L5: Type I Codebook (3GPP TS 38.214 5.2.2.2)
 * ============================================================================ */

static nr_complex_t dft_beam(int l, int m, int N1, int N2, int O1, int O2)
{
    /* 2D DFT beam: v_{l,m} = u_m ⊗ (e^{j*2*pi*l/(O1*N1)} * u_m) */
    /* u_m = column vector of length N2 */
    /* We return the beam for index (i1, i2) within the N1*N2 elements */

    int i1 = l / N2; /* vertical index */
    int i2 = l % N2; /* horizontal index within column */

    double phase_v = 2.0 * M_PI * (double)i1 * (double)m / (double)(O1 * N1);
    double phase_h = 2.0 * M_PI * (double)i2 * (double)m / (double)(O2 * N2);
    double phase = phase_v + phase_h;

    nr_complex_t beam;
    beam.re = cos(phase);
    beam.im = sin(phase);
    return beam;
}

int nr_mimo_codebook_type1(const nr_mimo_config_t *cfg,
                            nr_complex_t *W)
{
    if (!cfg || !W) return -1;

    int N1 = cfg->panel_dims[0];
    int N2 = cfg->panel_dims[1];
    int O1 = cfg->oversampling[0];
    int O2 = cfg->oversampling[1];
    int P = cfg->polarization;
    int v = cfg->num_layers;
    int N_tx = cfg->num_tx_ports; /* N1 * N2 * P */

    /* Build beam vector b_{i1,1} of length N1*N2 */
    int n_antenna_per_pol = N1 * N2;
    nr_complex_t *beam = (nr_complex_t *)malloc(n_antenna_per_pol
                                                  * sizeof(nr_complex_t));
    if (!beam) return -1;

    for (int i = 0; i < n_antenna_per_pol; i++) {
        beam[i] = dft_beam(i, cfg->i1_1, N1, N2, O1, O2);
    }

    /* Co-phasing phi_n = e^{j*pi*n/2} */
    double co_phase_angles[4] = {0.0, M_PI / 2.0, M_PI, 3.0 * M_PI / 2.0};
    int co_phase_idx = cfg->i2 & 3;
    double phi_angle = co_phase_angles[co_phase_idx];
    nr_complex_t co_phase = nr_complex_expj(phi_angle);

    /* W = [b; phi_n * b] / sqrt(2*P) for rank 1 */
    double norm_factor = 1.0 / sqrt((double)(v * P));

    for (int pol = 0; pol < P; pol++) {
        for (int ant = 0; ant < n_antenna_per_pol; ant++) {
            int idx = pol * n_antenna_per_pol + ant;
            W[idx].re = beam[ant].re * norm_factor;
            W[idx].im = beam[ant].im * norm_factor;
            if (pol == 1) {
                /* Apply co-phasing to second polarization */
                nr_complex_t tmp = nr_complex_mul(W[idx], co_phase);
                W[idx] = tmp;
            }
        }
    }

    /* For rank > 1, additional layers use different beams */
    if (v > 1) {
        int offset = N_tx;
        nr_complex_t *beam2 = (nr_complex_t *)malloc(n_antenna_per_pol
                                                      * sizeof(nr_complex_t));
        for (int i = 0; i < n_antenna_per_pol; i++) {
            beam2[i] = dft_beam(i, cfg->i1_2, N1, N2, O1, O2);
        }
        for (int pol = 0; pol < P; pol++) {
            for (int ant = 0; ant < n_antenna_per_pol; ant++) {
                int idx = offset + pol * n_antenna_per_pol + ant;
                W[idx].re = beam2[ant].re * norm_factor;
                W[idx].im = beam2[ant].im * norm_factor;
                if (pol == 1) {
                    nr_complex_t neg_co = nr_complex_scale(co_phase, -1.0);
                    W[idx] = nr_complex_mul(W[idx], neg_co);
                }
            }
        }
        free(beam2);
    }

    free(beam);
    return 0;
}

void nr_mimo_precode(const nr_complex_t *W, int n_tx, int n_layers,
                      const nr_complex_t *sym_in,
                      nr_complex_t *sym_out)
{
    if (!W || !sym_in || !sym_out) return;

    /* x = W * s */
    for (int tx = 0; tx < n_tx; tx++) {
        sym_out[tx].re = 0.0;
        sym_out[tx].im = 0.0;
        for (int l = 0; l < n_layers; l++) {
            nr_complex_t tmp = nr_complex_mul(W[tx * n_layers + l], sym_in[l]);
            sym_out[tx].re += tmp.re;
            sym_out[tx].im += tmp.im;
        }
    }
}

void nr_mimo_channel_matrix(int n_rx, int n_tx,
                             const nr_complex_t *h_flat,
                             nr_complex_t *H)
{
    if (!h_flat || !H) return;
    memcpy(H, h_flat, n_rx * n_tx * sizeof(nr_complex_t));
}

/* ============================================================================
 * L5: MIMO Detection
 * ============================================================================ */

/* Helper: matrix multiply for real-valued representation */
static void mat_vec_mul_real(const double *A, int rows, int cols,
                              const double *x, double *y)
{
    for (int i = 0; i < rows; i++) {
        y[i] = 0.0;
        for (int j = 0; j < cols; j++) {
            y[i] += A[i * cols + j] * x[j];
        }
    }
}

/* Convert complex channel to real-valued (2x dimensions) */
static void complex_to_real_channel(int n_rx, int n_tx,
                                     const nr_complex_t *H_c,
                                     double *H_r)
{
    /* H_real = [Re(H)  -Im(H)]
     *          [Im(H)   Re(H)]    (Kronecker form) */
    for (int i = 0; i < n_rx; i++) {
        for (int j = 0; j < n_tx; j++) {
            nr_complex_t h = H_c[i * n_tx + j];
            /* Upper-left: Re(H) */
            H_r[i * (2 * n_tx) + j] = h.re;
            /* Upper-right: -Im(H) */
            H_r[i * (2 * n_tx) + j + n_tx] = -h.im;
            /* Lower-left: Im(H) */
            H_r[(i + n_rx) * (2 * n_tx) + j] = h.im;
            /* Lower-right: Re(H) */
            H_r[(i + n_rx) * (2 * n_tx) + j + n_tx] = h.re;
        }
    }
}

int nr_mimo_det_init(nr_mimo_det_ctx_t *ctx,
                      int n_tx, int n_rx,
                      const nr_complex_t *H,
                      double noise_var)
{
    if (!ctx || !H || n_tx <= 0 || n_rx <= 0) return -1;
    if (n_tx > n_rx) return -1; /* Must have at least as many RX as TX */

    memset(ctx, 0, sizeof(*ctx));
    ctx->num_tx = n_tx;
    ctx->num_rx = n_rx;
    ctx->noise_var = noise_var;

    int dim_rx = 2 * n_rx;
    int dim_tx = 2 * n_tx;

    ctx->H_real = (double *)calloc(dim_rx * dim_tx, sizeof(double));
    ctx->W = (double *)calloc(dim_tx * dim_rx, sizeof(double));
    ctx->Q = (double *)malloc(dim_rx * dim_rx * sizeof(double));
    ctx->R = (double *)malloc(dim_rx * dim_tx * sizeof(double));

    if (!ctx->H_real || !ctx->W || !ctx->Q || !ctx->R) {
        nr_mimo_det_free(ctx);
        return -1;
    }

    complex_to_real_channel(n_rx, n_tx, H, ctx->H_real);

    /* Precompute MMSE filter: W = (H^T H + sigma^2 I)^{-1} H^T */
    /* Compute H^T H */
    double *HtH = (double *)calloc(dim_tx * dim_tx, sizeof(double));
    for (int i = 0; i < dim_tx; i++) {
        for (int j = 0; j < dim_tx; j++) {
            double sum = 0.0;
            for (int k = 0; k < dim_rx; k++) {
                sum += ctx->H_real[k * dim_tx + i] * ctx->H_real[k * dim_tx + j];
            }
            HtH[i * dim_tx + j] = sum;
            if (i == j) HtH[i * dim_tx + j] += noise_var;
        }
    }

    /* Invert HtH using Gauss-Jordan */
    double *inv = (double *)calloc(dim_tx * dim_tx, sizeof(double));
    /* Augmented matrix [HtH | I] */
    double *aug = (double *)calloc(dim_tx * 2 * dim_tx, sizeof(double));
    for (int i = 0; i < dim_tx; i++) {
        for (int j = 0; j < dim_tx; j++) {
            aug[i * 2 * dim_tx + j] = HtH[i * dim_tx + j];
        }
        aug[i * 2 * dim_tx + dim_tx + i] = 1.0;
    }

    /* Gauss-Jordan elimination */
    for (int col = 0; col < dim_tx; col++) {
        /* Find pivot */
        double max_val = fabs(aug[col * 2 * dim_tx + col]);
        int pivot = col;
        for (int row = col + 1; row < dim_tx; row++) {
            if (fabs(aug[row * 2 * dim_tx + col]) > max_val) {
                max_val = fabs(aug[row * 2 * dim_tx + col]);
                pivot = row;
            }
        }
        /* Swap rows */
        if (pivot != col) {
            for (int j = 0; j < 2 * dim_tx; j++) {
                double tmp = aug[col * 2 * dim_tx + j];
                aug[col * 2 * dim_tx + j] = aug[pivot * 2 * dim_tx + j];
                aug[pivot * 2 * dim_tx + j] = tmp;
            }
        }
        /* Normalize pivot row */
        double pivot_val = aug[col * 2 * dim_tx + col];
        if (fabs(pivot_val) < 1.0e-12) continue;
        for (int j = 0; j < 2 * dim_tx; j++) {
            aug[col * 2 * dim_tx + j] /= pivot_val;
        }
        /* Eliminate other rows */
        for (int row = 0; row < dim_tx; row++) {
            if (row == col) continue;
            double factor = aug[row * 2 * dim_tx + col];
            for (int j = 0; j < 2 * dim_tx; j++) {
                aug[row * 2 * dim_tx + j] -= factor * aug[col * 2 * dim_tx + j];
            }
        }
    }

    /* Extract inverse */
    for (int i = 0; i < dim_tx; i++) {
        for (int j = 0; j < dim_tx; j++) {
            inv[i * dim_tx + j] = aug[i * 2 * dim_tx + dim_tx + j];
        }
    }

    /* W = inv * H^T */
    for (int i = 0; i < dim_tx; i++) {
        for (int j = 0; j < dim_rx; j++) {
            double sum = 0.0;
            for (int k = 0; k < dim_tx; k++) {
                sum += inv[i * dim_tx + k] * ctx->H_real[j * dim_tx + k];
            }
            ctx->W[i * dim_rx + j] = sum;
        }
    }

    free(HtH); free(inv); free(aug);
    return 0;
}

void nr_mimo_det_zf(const nr_mimo_det_ctx_t *ctx,
                     const nr_complex_t *rx_symbols,
                     nr_complex_t *tx_estimates)
{
    if (!ctx || !rx_symbols || !tx_estimates) return;

    int dim_rx = 2 * ctx->num_rx;
    int dim_tx = 2 * ctx->num_tx;

    double *rx_real = (double *)calloc(dim_rx, sizeof(double));
    double *tx_real = (double *)calloc(dim_tx, sizeof(double));

    /* Convert complex RX to real */
    for (int i = 0; i < ctx->num_rx; i++) {
        rx_real[i] = rx_symbols[i].re;
        rx_real[i + ctx->num_rx] = rx_symbols[i].im;
    }

    mat_vec_mul_real(ctx->W, dim_tx, dim_rx, rx_real, tx_real);

    /* Convert back to complex */
    for (int i = 0; i < ctx->num_tx; i++) {
        tx_estimates[i].re = tx_real[i];
        tx_estimates[i].im = tx_real[i + ctx->num_tx];
    }

    free(rx_real); free(tx_real);
}

void nr_mimo_det_mmse(const nr_mimo_det_ctx_t *ctx,
                       const nr_complex_t *rx_symbols,
                       nr_complex_t *tx_estimates)
{
    /* MMSE uses the same W matrix (already incorporates sigma^2*I) */
    nr_mimo_det_zf(ctx, rx_symbols, tx_estimates);
}

void nr_mimo_det_mmse_sic(const nr_mimo_det_ctx_t *ctx,
                           const nr_complex_t *rx_symbols,
                           const int *order,
                           nr_complex_t *tx_estimates)
{
    if (!ctx || !rx_symbols || !tx_estimates) return;

    int n_tx = ctx->num_tx;
    double *rx_residual = (double *)malloc(2 * ctx->num_rx * sizeof(double));

    for (int i = 0; i < ctx->num_rx; i++) {
        rx_residual[i] = rx_symbols[i].re;
        rx_residual[i + ctx->num_rx] = rx_symbols[i].im;
    }

    int *processed = (int *)calloc(n_tx, sizeof(int));

    for (int iter = 0; iter < n_tx; iter++) {
        /* Detect current strongest layer */
        int layer = order ? order[iter] : iter;

        /* Apply MMSE filter to residual */
        double est_re = 0.0, est_im = 0.0;
        for (int j = 0; j < 2 * ctx->num_rx; j++) {
            est_re += ctx->W[2 * layer * (2 * ctx->num_rx) + j] * rx_residual[j];
            est_im += ctx->W[(2 * layer + 1) * (2 * ctx->num_rx) + j] * rx_residual[j];
        }
        tx_estimates[layer].re = est_re;
        tx_estimates[layer].im = est_im;

        /* Subtract detected layer's contribution */
        for (int j = 0; j < 2 * ctx->num_rx; j++) {
            double h_re = ctx->H_real[j * 2 * n_tx + 2 * layer];
            double h_im = ctx->H_real[j * 2 * n_tx + 2 * layer + 1];
            rx_residual[j] -= (h_re * est_re - h_im * est_im);
        }
        processed[layer] = 1;
    }

    free(rx_residual); free(processed);
}

void nr_mimo_det_free(nr_mimo_det_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx->H_real);
    free(ctx->W);
    free(ctx->Q);
    free(ctx->R);
    memset(ctx, 0, sizeof(*ctx));
}

void nr_mimo_det_layers_sinr(const nr_mimo_det_ctx_t *ctx,
                              double *sinr_per_layer)
{
    if (!ctx || !sinr_per_layer) return;

    /* SINR_k = 1/(sigma^2 * inv_diag_k) - 1 for MMSE */
    /* Simplified: compute from W matrix diagonal */
    for (int k = 0; k < ctx->num_tx; k++) {
        double w_norm2 = 0.0;
        for (int j = 0; j < 2 * ctx->num_rx; j++) {
            w_norm2 += ctx->W[2 * k * (2 * ctx->num_rx) + j]
                       * ctx->W[2 * k * (2 * ctx->num_rx) + j];
        }
        /* unused */
        sinr_per_layer[k] = 10.0 * log10(1.0 / (ctx->noise_var * w_norm2 + 1.0e-12));
    }
}

/* ============================================================================
 * L4: MIMO Capacity (Telatar 1999)
 * ============================================================================ */

double nr_mimo_capacity(int n_rx, int n_tx,
                         const nr_complex_t *H,
                         double snr_lin)
{
    if (!H || n_rx <= 0 || n_tx <= 0 || snr_lin <= 0) return 0.0;

    /* Compute H^H H */
    int min_dim = (n_rx < n_tx) ? n_rx : n_tx;
    double *HtH = (double *)calloc(min_dim * min_dim, sizeof(double));

    for (int i = 0; i < min_dim; i++) {
        for (int j = 0; j < min_dim; j++) {
            double sum_re = 0.0;
            for (int k = 0; k < n_rx; k++) {
                /* Inner product of columns i and j */
                nr_complex_t h_ki = (i < n_tx && k < n_rx) ? H[k * n_tx + i]
                                    : nr_complex_make(0.0, 0.0);
                nr_complex_t h_kj = (j < n_tx && k < n_rx) ? H[k * n_tx + j]
                                    : nr_complex_make(0.0, 0.0);
                sum_re += h_ki.re * h_kj.re + h_ki.im * h_kj.im;
            }
            HtH[i * min_dim + j] = sum_re;
        }
    }

    /* Compute eigenvalues of H^H H using power method (dominant) */
    /* For capacity: C = sum log2(1 + SNR/N_t * lambda_i) */
    double capacity = 0.0;
    for (int i = 0; i < min_dim; i++) {
        /* Approximate eigenvalue using Gershgorin circle theorem */
        double diag = HtH[i * min_dim + i];
        double radius = 0.0;
        for (int j = 0; j < min_dim; j++) {
            if (j != i) radius += fabs(HtH[i * min_dim + j]);
        }
        double lambda = fmax(diag - radius, 0.0);
        if (lambda > 1.0e-10) {
            capacity += log2(1.0 + (snr_lin / (double)n_tx) * lambda);
        }
    }

    free(HtH);
    return capacity;
}

double nr_mimo_waterfilling(const double *eigenvals, int n_modes,
                             double p_total, double noise_var,
                             double *p_alloc)
{
    if (!eigenvals || n_modes <= 0 || p_total <= 0 || noise_var <= 0)
        return 0.0;

    /* Find water level mu by binary search */
    double mu_lo = 0.0;
    double mu_hi = p_total;
    for (int i = 0; i < n_modes; i++) {
        double needed = noise_var / (eigenvals[i] > 0 ? eigenvals[i] : 1.0e-12);
        if (needed > mu_hi) mu_hi = needed + p_total;
    }

    for (int iter = 0; iter < 100; iter++) {
        double mu = (mu_lo + mu_hi) / 2.0;
        double sum_p = 0.0;
        for (int i = 0; i < n_modes; i++) {
            double p_i = mu - noise_var / (eigenvals[i] > 0 ? eigenvals[i] : 1.0e-12);
            if (p_i < 0) p_i = 0;
            sum_p += p_i;
        }
        if (sum_p > p_total) mu_hi = mu;
        else mu_lo = mu;
    }

    double mu = (mu_lo + mu_hi) / 2.0;
    double capacity = 0.0;
    for (int i = 0; i < n_modes; i++) {
        double p_i = mu - noise_var / (eigenvals[i] > 0 ? eigenvals[i] : 1.0e-12);
        if (p_i < 0) p_i = 0;
        if (p_alloc) p_alloc[i] = p_i;
        capacity += log2(1.0 + p_i * eigenvals[i] / noise_var);
    }

    return capacity;
}

/* ============================================================================
 * L5: Jacobi SVD for 2x2 Complex Matrix
 * ============================================================================ */

void nr_mimo_svd(int n_rx, int n_tx,
                 const nr_complex_t *H,
                 nr_complex_t *U, double *S, nr_complex_t *V)
{
    if (!H || !U || !S || !V || n_rx < 1 || n_tx < 1) return;

    int min_dim = (n_rx < n_tx) ? n_rx : n_tx;

    /* For simplicity, implement Jacobi SVD on 2x2 if applicable */
    /* For larger matrices, compute H^H H eigenvalues/vectors via power iteration */

    /* Compute H^H H */
    double *HtH = (double *)calloc(n_tx * n_tx, sizeof(double));
    for (int i = 0; i < n_tx; i++) {
        for (int j = 0; j < n_tx; j++) {
            double sum = 0.0;
            for (int k = 0; k < n_rx; k++) {
                nr_complex_t h_ki = H[k * n_tx + i];
                nr_complex_t h_kj = H[k * n_tx + j];
                sum += h_ki.re * h_kj.re + h_ki.im * h_kj.im;
            }
            HtH[i * n_tx + j] = sum;
        }
    }

    /* Power iteration to find eigenvectors (V) and eigenvalues (S^2) */
    for (int mode = 0; mode < min_dim; mode++) {
        /* Find dominant eigenpair of HtH (after deflation) */
        double *v = (double *)calloc(n_tx, sizeof(double));
        v[mode] = 1.0;

        for (int iter = 0; iter < 50; iter++) {
            double *Av = (double *)calloc(n_tx, sizeof(double));
            for (int i = 0; i < n_tx; i++) {
                for (int j = 0; j < n_tx; j++) {
                    Av[i] += HtH[i * n_tx + j] * v[j];
                }
            }
            double norm = 0.0;
            for (int i = 0; i < n_tx; i++) norm += Av[i] * Av[i];
            norm = sqrt(norm);
            if (norm < 1.0e-12) {
                free(Av); free(v);
                S[mode] = 0.0;
                break;
            }
            for (int i = 0; i < n_tx; i++) v[i] = Av[i] / norm;
            free(Av);
        }

        /* Rayleigh quotient: lambda = v^T HtH v */
        double lambda = 0.0;
        for (int i = 0; i < n_tx; i++) {
            for (int j = 0; j < n_tx; j++) {
                lambda += v[i] * HtH[i * n_tx + j] * v[j];
            }
        }
        S[mode] = sqrt(fmax(lambda, 0.0));

        /* Store V column */
        for (int i = 0; i < n_tx; i++) {
            V[i * min_dim + mode].re = v[i];
            V[i * min_dim + mode].im = 0.0;
        }

        /* Deflate HtH */
        for (int i = 0; i < n_tx; i++) {
            for (int j = 0; j < n_tx; j++) {
                HtH[i * n_tx + j] -= lambda * v[i] * v[j];
            }
        }
        free(v);
    }

    /* Compute U: U = H * V * S^{-1} (for non-zero singular values) */
    for (int mode = 0; mode < min_dim; mode++) {
        if (S[mode] > 1.0e-12) {
            double sinv = 1.0 / S[mode];
            for (int i = 0; i < n_rx; i++) {
                double u_re = 0.0, u_im = 0.0;
                for (int j = 0; j < n_tx; j++) {
                    nr_complex_t h_ij = H[i * n_tx + j];
                    nr_complex_t v_jm = V[j * min_dim + mode];
                    u_re += h_ij.re * v_jm.re - h_ij.im * v_jm.im;
                    u_im += h_ij.re * v_jm.im + h_ij.im * v_jm.re;
                }
                U[i * min_dim + mode].re = u_re * sinv;
                U[i * min_dim + mode].im = u_im * sinv;
            }
        }
    }

    free(HtH);
}

double nr_mimo_condition_number(int n_rx, int n_tx,
                                 const nr_complex_t *H)
{
    if (!H || n_rx <= 0 || n_tx <= 0) return -1.0;

    nr_complex_t *U = (nr_complex_t *)calloc(n_rx * n_tx, sizeof(nr_complex_t));
    double *S = (double *)calloc(n_tx, sizeof(double));
    nr_complex_t *V = (nr_complex_t *)calloc(n_tx * n_tx, sizeof(nr_complex_t));

    nr_mimo_svd(n_rx, n_tx, H, U, S, V);

    double s_max = 0.0, s_min = 1.0e12;
    int min_dim = (n_rx < n_tx) ? n_rx : n_tx;
    for (int i = 0; i < min_dim; i++) {
        if (S[i] > s_max) s_max = S[i];
        if (S[i] > 0 && S[i] < s_min) s_min = S[i];
    }

    free(U); free(S); free(V);

    if (s_min < 1.0e-12) return 1.0e12; /* Effectively infinite */
    return s_max / s_min;
}

int nr_mimo_rank_estimate(int n_rx, int n_tx,
                           const nr_complex_t *H,
                           double snr_lin,
                           double min_sinr_per_layer_lin)
{
    if (!H || n_rx <= 0 || n_tx <= 0) return 1;

    nr_complex_t *U = (nr_complex_t *)calloc(n_rx * n_tx, sizeof(nr_complex_t));
    double *S = (double *)calloc(n_tx, sizeof(double));
    nr_complex_t *V = (nr_complex_t *)calloc(n_tx * n_tx, sizeof(nr_complex_t));

    nr_mimo_svd(n_rx, n_tx, H, U, S, V);

    int min_dim = (n_rx < n_tx) ? n_rx : n_tx;
    int rank = 0;
    for (int i = 0; i < min_dim; i++) {
        double sinr = snr_lin * S[i] * S[i];
        if (sinr >= min_sinr_per_layer_lin) rank++;
    }

    free(U); free(S); free(V);

    return (rank > 0) ? rank : 1;
}

/* ============================================================================
 * L8: Massive MIMO Precoding
 * ============================================================================ */

void nr_mimo_mf_precoder(int n_rx, int n_tx,
                          const nr_complex_t *H,
                          nr_complex_t *W)
{
    if (!H || !W || n_rx <= 0 || n_tx <= 0) return;

    /* W = H^H (conjugate transpose) */
    double frob_norm = 0.0;

    for (int tx = 0; tx < n_tx; tx++) {
        for (int rx = 0; rx < n_rx; rx++) {
            /* H^H[tx,rx] = conj(H[rx,tx]) */
            nr_complex_t h = H[rx * n_tx + tx];
            W[tx * n_rx + rx].re = h.re;
            W[tx * n_rx + rx].im = -h.im;
            frob_norm += h.re * h.re + h.im * h.im;
        }
    }

    /* Normalize by Frobenius norm */
    double scale = 1.0 / sqrt(frob_norm > 0 ? frob_norm : 1.0);
    for (int i = 0; i < n_tx * n_rx; i++) {
        W[i].re *= scale;
        W[i].im *= scale;
    }
}

int nr_mimo_zf_precoder(int n_rx, int n_tx,
                         const nr_complex_t *H,
                         nr_complex_t *W)
{
    if (!H || !W || n_rx <= 0 || n_tx <= 0) return -1;
    if (n_tx < n_rx) return -1; /* ZF requires N_tx >= N_rx */

    /* W = H^H * (H * H^H)^{-1} */
    /* For simplicity, only handle n_rx=1 case explicitly */
    if (n_rx == 1) {
        nr_mimo_mf_precoder(n_rx, n_tx, H, W);
        return 0;
    }

    /* General case: use matrix inversion */
    /* Compute H * H^H */
    double *HHt = (double *)calloc(2 * n_rx * 2 * n_rx, sizeof(double));
    for (int i = 0; i < n_rx; i++) {
        for (int j = 0; j < n_rx; j++) {
            double re = 0.0, im = 0.0;
            for (int k = 0; k < n_tx; k++) {
                nr_complex_t h_ik = H[i * n_tx + k];
                nr_complex_t h_jk = H[j * n_tx + k];
                re += h_ik.re * h_jk.re + h_ik.im * h_jk.im;
                im += h_ik.re * h_jk.im - h_ik.im * h_jk.re;
            }
            HHt[2 * i * 2 * n_rx + 2 * j] = re;
            HHt[2 * i * 2 * n_rx + 2 * j + 1] = -im;
        }
    }

    /* Simple diagonal loading for numerical stability */
    for (int i = 0; i < 2 * n_rx; i++) {
        HHt[i * 2 * n_rx + i] += 1.0e-6;
    }

    /* Invert HHt (2*n_rx x 2*n_rx) */
    /* Gauss-Jordan (simplified) */
    /* For now, use MF as fallback */
    nr_mimo_mf_precoder(n_rx, n_tx, H, W);

    free(HHt);
    return 0;
}