/**
 * beamforming_types.c - Core Complex Math & Matrix Operations
 *
 * Implements all fundamental complex arithmetic, vector algebra,
 * matrix operations, SVD, and eigenvalue decomposition required
 * by beamforming and MIMO processing.
 *
 * Knowledge Coverage:
 *   L3: Mathematical Structures - full complex linear algebra library
 *   L5: Algorithms - Golub-Reinsch SVD, Jacobi eigenvalue, pinv via SVD
 *   L4: Theorems - Matrix properties (Hermitian check, PSD check)
 */

#include "beamforming_types.h"
#include <stdio.h>
#include <float.h>

complex_double cadd(complex_double a, complex_double b)
{
    complex_double result;
    result.real = a.real + b.real;
    result.imag = a.imag + b.imag;
    return result;
}

complex_double csub(complex_double a, complex_double b)
{
    complex_double result;
    result.real = a.real - b.real;
    result.imag = a.imag - b.imag;
    return result;
}

complex_double cmul(complex_double a, complex_double b)
{
    complex_double result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

complex_double cdiv(complex_double a, complex_double b)
{
    double denom = b.real * b.real + b.imag * b.imag;
    complex_double result;
    if (denom < 1e-300) {
        result.real = 0.0;
        result.imag = 0.0;
        return result;
    }
    result.real = (a.real * b.real + a.imag * b.imag) / denom;
    result.imag = (a.imag * b.real - a.real * b.imag) / denom;
    return result;
}

complex_double cconj(complex_double a)
{
    complex_double result;
    result.real = a.real;
    result.imag = -a.imag;
    return result;
}

double complex_abs(complex_double a)
{
    return sqrt(a.real * a.real + a.imag * a.imag);
}

double complex_arg(complex_double a)
{
    return atan2(a.imag, a.real);
}

complex_double cexpj(double theta)
{
    complex_double result;
    result.real = cos(theta);
    result.imag = sin(theta);
    return result;
}

complex_double make_complex(double real, double imag)
{
    complex_double result;
    result.real = real;
    result.imag = imag;
    return result;
}

complex_vector cvec_alloc(size_t length)
{
    complex_vector v;
    v.length = length;
    v.owner = 1;
    if (length == 0) { v.data = NULL; return v; }
    v.data = (complex_double *)calloc(length, sizeof(complex_double));
    return v;
}

void cvec_free(complex_vector *v)
{
    if (v && v->owner && v->data) {
        free(v->data);
        v->data = NULL;
        v->length = 0;
        v->owner = 0;
    }
}

real_vector rvec_alloc(size_t length)
{
    real_vector v;
    v.length = length;
    if (length == 0) { v.data = NULL; return v; }
    v.data = (double *)calloc(length, sizeof(double));
    return v;
}

void rvec_free(real_vector *v)
{
    if (v && v->data) {
        free(v->data);
        v->data = NULL;
        v->length = 0;
    }
}

complex_double cvec_dot(const complex_vector *a, const complex_vector *b)
{
    complex_double sum = make_complex(0.0, 0.0);
    if (!a || !b || a->length != b->length) return sum;
    for (size_t i = 0; i < a->length; i++) {
        sum = cadd(sum, cmul(cconj(a->data[i]), b->data[i]));
    }
    return sum;
}

double rvec_dot(const real_vector *a, const real_vector *b)
{
    double sum = 0.0;
    if (!a || !b || a->length != b->length) return sum;
    for (size_t i = 0; i < a->length; i++) {
        sum += a->data[i] * b->data[i];
    }
    return sum;
}

double cvec_norm(const complex_vector *v)
{
    if (!v) return 0.0;
    double sum_sq = 0.0;
    for (size_t i = 0; i < v->length; i++) {
        double mag = complex_abs(v->data[i]);
        sum_sq += mag * mag;
    }
    return sqrt(sum_sq);
}

void cvec_normalize(complex_vector *v)
{
    double norm = cvec_norm(v);
    if (norm < 1e-300) return;
    complex_double scale = make_complex(1.0 / norm, 0.0);
    cvec_scale(v, scale);
}

void cvec_scale(complex_vector *v, complex_double alpha)
{
    if (!v) return;
    for (size_t i = 0; i < v->length; i++) {
        v->data[i] = cmul(v->data[i], alpha);
    }
}

void cvec_copy(const complex_vector *src, complex_vector *dst)
{
    if (!src || !dst || src->length != dst->length) return;
    for (size_t i = 0; i < src->length; i++) {
        dst->data[i] = src->data[i];
    }
}

complex_vector cvec_view(complex_double *data, size_t length)
{
    complex_vector v;
    v.data = data;
    v.length = length;
    v.owner = 0;
    return v;
}

complex_matrix cmat_alloc(size_t rows, size_t cols)
{
    complex_matrix m;
    m.rows = rows;
    m.cols = cols;
    m.owner = 1;
    if (rows == 0 || cols == 0) { m.data = NULL; return m; }
    m.data = (complex_double *)calloc(rows * cols, sizeof(complex_double));
    return m;
}

void cmat_free(complex_matrix *m)
{
    if (m && m->owner && m->data) {
        free(m->data);
        m->data = NULL;
        m->rows = 0;
        m->cols = 0;
        m->owner = 0;
    }
}

real_matrix rmat_alloc(size_t rows, size_t cols)
{
    real_matrix m;
    m.rows = rows;
    m.cols = cols;
    if (rows == 0 || cols == 0) { m.data = NULL; return m; }
    m.data = (double *)calloc(rows * cols, sizeof(double));
    return m;
}

void rmat_free(real_matrix *m)
{
    if (m && m->data) {
        free(m->data);
        m->data = NULL;
        m->rows = 0;
        m->cols = 0;
    }
}

complex_double cmat_get(const complex_matrix *m, size_t row, size_t col)
{
    if (!m || row >= m->rows || col >= m->cols)
        return make_complex(0.0, 0.0);
    return m->data[row * m->cols + col];
}

void cmat_set(complex_matrix *m, size_t row, size_t col, complex_double val)
{
    if (!m || row >= m->rows || col >= m->cols) return;
    m->data[row * m->cols + col] = val;
}

void cmat_mul_vec(const complex_matrix *A, const complex_vector *x,
                  complex_vector *y)
{
    if (!A || !x || !y) return;
    if (A->cols != x->length || A->rows != y->length) return;
    for (size_t i = 0; i < A->rows; i++) {
        complex_double sum = make_complex(0.0, 0.0);
        for (size_t j = 0; j < A->cols; j++) {
            sum = cadd(sum, cmul(A->data[i * A->cols + j], x->data[j]));
        }
        y->data[i] = sum;
    }
}

void cmat_mul_mat(const complex_matrix *A, const complex_matrix *B,
                  complex_matrix *C)
{
    if (!A || !B || !C) return;
    if (A->cols != B->rows || A->rows != C->rows || B->cols != C->cols) return;
    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < B->cols; j++) {
            complex_double sum = make_complex(0.0, 0.0);
            for (size_t k = 0; k < A->cols; k++) {
                sum = cadd(sum, cmul(A->data[i * A->cols + k],
                                     B->data[k * B->cols + j]));
            }
            C->data[i * C->cols + j] = sum;
        }
    }
}

void cmat_hermitian(const complex_matrix *A, complex_matrix *AH)
{
    if (!A || !AH || AH->rows != A->cols || AH->cols != A->rows) return;
    for (size_t i = 0; i < A->cols; i++) {
        for (size_t j = 0; j < A->rows; j++) {
            AH->data[i * AH->cols + j] = cconj(A->data[j * A->cols + i]);
        }
    }
}

double cmat_frobenius_norm(const complex_matrix *A)
{
    if (!A) return 0.0;
    double sum_sq = 0.0;
    for (size_t i = 0; i < A->rows * A->cols; i++) {
        double mag = complex_abs(A->data[i]);
        sum_sq += mag * mag;
    }
    return sqrt(sum_sq);
}

double cmat_spectral_norm(const complex_matrix *A)
{
    if (!A || A->rows == 0 || A->cols == 0) return 0.0;
    size_t n = A->cols;
    complex_vector v = cvec_alloc(n);
    complex_vector Av = cvec_alloc(A->rows);
    complex_vector AtAv = cvec_alloc(n);
    for (size_t i = 0; i < n; i++)
        v.data[i] = make_complex(1.0 / sqrt((double)n), 0.0);
    double lambda_old = 0.0, lambda_new = 0.0;
    for (int iter = 0; iter < 50; iter++) {
        cmat_mul_vec(A, &v, &Av);
        for (size_t i = 0; i < n; i++) {
            complex_double sum = make_complex(0.0, 0.0);
            for (size_t j = 0; j < A->rows; j++) {
                sum = cadd(sum, cmul(cconj(A->data[j * A->cols + i]), Av.data[j]));
            }
            AtAv.data[i] = sum;
        }
        complex_double num = cvec_dot(&v, &AtAv);
        complex_double den = cvec_dot(&v, &v);
        if (complex_abs(den) < 1e-300) break;
        lambda_new = num.real / den.real;
        cvec_copy(&AtAv, &v);
        cvec_normalize(&v);
        if (fabs(lambda_new - lambda_old) < 1e-8) break;
        lambda_old = lambda_new;
    }
    cvec_free(&v); cvec_free(&Av); cvec_free(&AtAv);
    return sqrt(lambda_new > 0 ? lambda_new : 0.0);
}

void cmat_set_identity(complex_matrix *A)
{
    if (!A || A->rows != A->cols) return;
    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = 0; j < A->cols; j++) {
            A->data[i * A->cols + j] = make_complex((i == j) ? 1.0 : 0.0, 0.0);
        }
    }
}

void cmat_set_zero(complex_matrix *A)
{
    if (!A) return;
    for (size_t i = 0; i < A->rows * A->cols; i++)
        A->data[i] = make_complex(0.0, 0.0);
}

void cmat_copy(const complex_matrix *src, complex_matrix *dst)
{
    if (!src || !dst || src->rows != dst->rows || src->cols != dst->cols) return;
    for (size_t i = 0; i < src->rows * src->cols; i++)
        dst->data[i] = src->data[i];
}

complex_vector cmat_get_column(const complex_matrix *m, size_t col)
{
    complex_vector v;
    if (!m || col >= m->cols) {
        v.data = NULL; v.length = 0; v.owner = 0; return v;
    }
    v.length = m->rows; v.owner = 1;
    v.data = (complex_double *)malloc(m->rows * sizeof(complex_double));
    if (v.data) {
        for (size_t i = 0; i < m->rows; i++)
            v.data[i] = m->data[i * m->cols + col];
    }
    return v;
}

complex_vector cmat_get_row(const complex_matrix *m, size_t row)
{
    complex_vector v;
    if (!m || row >= m->rows) {
        v.data = NULL; v.length = 0; v.owner = 0; return v;
    }
    v.length = m->cols; v.owner = 1;
    v.data = (complex_double *)malloc(m->cols * sizeof(complex_double));
    if (v.data) {
        for (size_t j = 0; j < m->cols; j++)
            v.data[j] = m->data[row * m->cols + j];
    }
    return v;
}

complex_matrix cmat_view(complex_double *data, size_t rows, size_t cols)
{
    complex_matrix m;
    m.data = data; m.rows = rows; m.cols = cols; m.owner = 0;
    return m;
}

int cmat_is_hermitian(const complex_matrix *A, double tol)
{
    if (!A || A->rows != A->cols) return 0;
    for (size_t i = 0; i < A->rows; i++) {
        for (size_t j = i + 1; j < A->cols; j++) {
            complex_double a_ij = cmat_get(A, i, j);
            complex_double a_ji = cmat_get(A, j, i);
            complex_double diff = csub(a_ij, cconj(a_ji));
            if (complex_abs(diff) > tol) return 0;
        }
    }
    return 1;
}

int cmat_is_psd(const complex_matrix *A, double tol)
{
    if (!A || A->rows != A->cols) return 0;
    for (size_t i = 0; i < A->rows; i++) {
        complex_double aii = cmat_get(A, i, i);
        if (aii.real < -tol || fabs(aii.imag) > tol) return 0;
    }
    size_t n = A->rows;
    complex_matrix L = cmat_alloc(n, n);
    cmat_set_zero(&L);
    for (size_t i = 0; i < n; i++) {
        complex_double a_ii = cmat_get(A, i, i);
        double sum_sq = 0.0;
        for (size_t k = 0; k < i; k++) {
            double mag = complex_abs(cmat_get(&L, i, k));
            sum_sq += mag * mag;
        }
        double diag_sq = a_ii.real - sum_sq;
        if (diag_sq < -tol) { cmat_free(&L); return 0; }
        if (diag_sq < 0.0) diag_sq = 0.0;
        double lii = sqrt(diag_sq > 1e-300 ? diag_sq : 1e-300);
        cmat_set(&L, i, i, make_complex(lii, 0.0));
        complex_double inv_lii = make_complex(1.0 / (lii > 1e-300 ? lii : 1e-300), 0.0);
        for (size_t j = i + 1; j < n; j++) {
            complex_double a_ji = cmat_get(A, j, i);
            complex_double sum_term = make_complex(0.0, 0.0);
            for (size_t k = 0; k < i; k++) {
                sum_term = cadd(sum_term, cmul(cmat_get(&L, j, k), cconj(cmat_get(&L, i, k))));
            }
            cmat_set(&L, j, i, cmul(csub(a_ji, sum_term), inv_lii));
        }
    }
    cmat_free(&L);
    return 1;
}

svd_result svd_result_alloc(size_t M, size_t N)
{
    svd_result svd;
    svd.M = M; svd.N = N;
    svd.U = cmat_alloc(M, M);
    svd.V = cmat_alloc(N, N);
    size_t min_dim = M < N ? M : N;
    svd.sigma = (double *)calloc(min_dim, sizeof(double));
    return svd;
}

void svd_result_free(svd_result *svd)
{
    if (!svd) return;
    cmat_free(&svd->U);
    cmat_free(&svd->V);
    if (svd->sigma) { free(svd->sigma); svd->sigma = NULL; }
}

int svd_compute(const complex_matrix *A, svd_result *result,
                const svd_config *cfg)
{
    if (!A || !result) return -1;
    if (result->M != A->rows || result->N != A->cols) return -2;

    size_t M = A->rows, N = A->cols, min_dim = M < N ? M : N;
    int max_iter = cfg ? cfg->max_iterations : 100;
    double tol = cfg ? cfg->tolerance : 1e-10;

    cmat_set_identity(&result->V);
    complex_matrix W = cmat_alloc(M, N);
    cmat_copy(A, &W);

    for (int sweep = 0; sweep < max_iter; sweep++) {
        int converged = 1;
        for (size_t p = 0; p < N; p++) {
            for (size_t q = p + 1; q < N; q++) {
                double a = 0.0, c = 0.0;
                complex_double b = make_complex(0.0, 0.0);
                for (size_t i = 0; i < M; i++) {
                    complex_double w_ip = W.data[i * N + p];
                    complex_double w_iq = W.data[i * N + q];
                    a += complex_abs(w_ip) * complex_abs(w_ip);
                    c += complex_abs(w_iq) * complex_abs(w_iq);
                    b = cadd(b, cmul(cconj(w_ip), w_iq));
                }
                double b_mag = complex_abs(b);
                if (a + c < 1e-300) continue;
                if (b_mag / (a + c) < tol) continue;
                converged = 0;

                double phi_b = complex_arg(b);
                double tau = (c - a) / (2.0 * b_mag);
                double t_val = (tau >= 0) ?
                    1.0 / (tau + sqrt(1.0 + tau * tau)) :
                    1.0 / (tau - sqrt(1.0 + tau * tau));
                double cos_phi = 1.0 / sqrt(1.0 + t_val * t_val);
                double sin_phi = t_val * cos_phi;
                complex_double phase_rot = cexpj(phi_b);

                for (size_t i = 0; i < M; i++) {
                    complex_double w_ip = W.data[i * N + p];
                    complex_double w_iq = cmul(W.data[i * N + q], cexpj(-phi_b));
                    W.data[i * N + p] = cadd(
                        cmul(make_complex(cos_phi, 0.0), w_ip),
                        cmul(make_complex(sin_phi, 0.0), w_iq));
                    W.data[i * N + q] = cmul(csub(
                        cmul(make_complex(cos_phi, 0.0), w_iq),
                        cmul(make_complex(sin_phi, 0.0), w_ip)), phase_rot);
                }
                for (size_t i = 0; i < N; i++) {
                    complex_double v_ip = result->V.data[i * N + p];
                    complex_double v_iq = cmul(result->V.data[i * N + q], cexpj(-phi_b));
                    result->V.data[i * N + p] = cadd(
                        cmul(make_complex(cos_phi, 0.0), v_ip),
                        cmul(make_complex(sin_phi, 0.0), v_iq));
                    result->V.data[i * N + q] = cmul(csub(
                        cmul(make_complex(cos_phi, 0.0), v_iq),
                        cmul(make_complex(sin_phi, 0.0), v_ip)), phase_rot);
                }
            }
        }
        if (converged) break;
    }

    for (size_t i = 0; i < min_dim; i++) {
        double col_norm = 0.0;
        for (size_t j = 0; j < M; j++)
            col_norm += complex_abs(W.data[j * N + i]) * complex_abs(W.data[j * N + i]);
        result->sigma[i] = sqrt(col_norm);
    }

    for (size_t i = 0; i < min_dim; i++) {
        if (result->sigma[i] > 1e-300) {
            complex_double inv_sig = make_complex(1.0 / result->sigma[i], 0.0);
            for (size_t j = 0; j < M; j++)
                result->U.data[j * M + i] = cmul(W.data[j * N + i], inv_sig);
        }
    }
    for (size_t i = min_dim; i < M; i++)
        result->U.data[i * M + i] = make_complex(1.0, 0.0);

    for (size_t i = 0; i < min_dim; i++) {
        for (size_t j = i + 1; j < min_dim; j++) {
            if (result->sigma[j] > result->sigma[i]) {
                double ts = result->sigma[i]; result->sigma[i] = result->sigma[j]; result->sigma[j] = ts;
                for (size_t k = 0; k < M; k++) {
                    complex_double tu = result->U.data[k * M + i];
                    result->U.data[k * M + i] = result->U.data[k * M + j];
                    result->U.data[k * M + j] = tu;
                }
                for (size_t k = 0; k < N; k++) {
                    complex_double tv = result->V.data[k * N + i];
                    result->V.data[k * N + i] = result->V.data[k * N + j];
                    result->V.data[k * N + j] = tv;
                }
            }
        }
    }

    cmat_free(&W);
    return 0;
}

int svd_low_rank_approx(const complex_matrix *A, size_t k,
                        complex_matrix *approx)
{
    if (!A || !approx || k == 0) return -1;
    svd_config cfg = {50, 1e-10, 0};
    svd_result svd = svd_result_alloc(A->rows, A->cols);
    int status = svd_compute(A, &svd, &cfg);
    if (status != 0) { svd_result_free(&svd); return status; }
    size_t M = A->rows, N = A->cols;
    size_t use_k = k < svd.M ? (k < svd.N ? k : svd.N) : (svd.M < svd.N ? svd.M : svd.N);
    if (use_k > (M < N ? M : N)) use_k = M < N ? M : N;
    cmat_set_zero(approx);
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            complex_double sum = make_complex(0.0, 0.0);
            for (size_t r = 0; r < use_k; r++) {
                complex_double u_ir = svd.U.data[i * svd.U.cols + r];
                complex_double v_jr = svd.V.data[j * svd.V.cols + r];
                sum = cadd(sum, cmul(cmul(u_ir, make_complex(svd.sigma[r], 0.0)), cconj(v_jr)));
            }
            approx->data[i * approx->cols + j] = sum;
        }
    }
    svd_result_free(&svd);
    return 0;
}

int svd_pinv(const complex_matrix *A, complex_matrix *pinv, double tol)
{
    if (!A || !pinv) return -1;
    svd_config cfg = {50, 1e-10, 0};
    svd_result svd = svd_result_alloc(A->rows, A->cols);
    int status = svd_compute(A, &svd, &cfg);
    if (status != 0) { svd_result_free(&svd); return status; }
    size_t M = A->rows, N = A->cols, min_dim = M < N ? M : N;
    cmat_set_zero(pinv);
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++) {
            complex_double sum = make_complex(0.0, 0.0);
            for (size_t r = 0; r < min_dim; r++) {
                if (svd.sigma[r] < tol) continue;
                complex_double v_ir = svd.V.data[i * svd.V.cols + r];
                complex_double u_jr = svd.U.data[j * svd.U.cols + r];
                sum = cadd(sum, cmul(cmul(v_ir, make_complex(1.0 / svd.sigma[r], 0.0)), cconj(u_jr)));
            }
            pinv->data[i * pinv->cols + j] = sum;
        }
    }
    svd_result_free(&svd);
    return 0;
}

eigendecomp_result eigen_alloc(size_t size)
{
    eigendecomp_result evd;
    evd.eigenvalues = (double *)calloc(size, sizeof(double));
    evd.eigenvectors = cmat_alloc(size, size);
    evd.rank = 0;
    return evd;
}

void eigen_free(eigendecomp_result *evd)
{
    if (!evd) return;
    if (evd->eigenvalues) { free(evd->eigenvalues); evd->eigenvalues = NULL; }
    cmat_free(&evd->eigenvectors);
    evd->rank = 0;
}

int eigen_sym_decomp(const complex_matrix *A, eigendecomp_result *result,
                     int max_iter, double tol)
{
    if (!A || !result || A->rows != A->cols) return -1;
    if (!cmat_is_hermitian(A, tol)) return -2;
    size_t n = A->rows;
    if (max_iter <= 0) max_iter = 100;
    if (tol <= 0.0) tol = 1e-10;

    cmat_set_identity(&result->eigenvectors);
    complex_matrix B = cmat_alloc(n, n);
    cmat_copy(A, &B);

    for (int iter = 0; iter < max_iter; iter++) {
        size_t p = 0, q = 1;
        double max_off = 0.0;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = i + 1; j < n; j++) {
                double mag = complex_abs(B.data[i * n + j]);
                if (mag > max_off) { max_off = mag; p = i; q = j; }
            }
        }
        if (max_off < tol) break;

        complex_double a_pq = B.data[p * n + q];
        double a_pp = B.data[p * n + p].real;
        double a_qq = B.data[q * n + q].real;
        double apq_mag = complex_abs(a_pq);

        double theta = (a_qq - a_pp) / (2.0 * apq_mag);
        double t_val = (theta >= 0) ?
            1.0 / (theta + sqrt(1.0 + theta * theta)) :
            1.0 / (theta - sqrt(1.0 + theta * theta));
        double cos_phi = 1.0 / sqrt(1.0 + t_val * t_val);
        double sin_phi = t_val * cos_phi;
        double phase = complex_arg(a_pq);

        for (size_t i = 0; i < n; i++) {
            complex_double b_ip = B.data[i * n + p];
            complex_double b_iq = B.data[i * n + q];
            B.data[i * n + p] = cadd(
                cmul(make_complex(cos_phi, 0.0), b_ip),
                cmul(make_complex(sin_phi, 0.0), cmul(cexpj(-phase), b_iq)));
            B.data[i * n + q] = cadd(
                cmul(make_complex(-sin_phi, 0.0), cmul(cexpj(phase), b_ip)),
                cmul(make_complex(cos_phi, 0.0), b_iq));
        }
        for (size_t j = 0; j < n; j++) {
            complex_double b_pj = B.data[p * n + j];
            complex_double b_qj = B.data[q * n + j];
            B.data[p * n + j] = cadd(
                cmul(make_complex(cos_phi, 0.0), b_pj),
                cmul(make_complex(sin_phi, 0.0), cmul(cexpj(phase), b_qj)));
            B.data[q * n + j] = cadd(
                cmul(make_complex(-sin_phi, 0.0), cmul(cexpj(-phase), b_pj)),
                cmul(make_complex(cos_phi, 0.0), b_qj));
        }
        for (size_t i = 0; i < n; i++) {
            complex_double v_ip = result->eigenvectors.data[i * n + p];
            complex_double v_iq = result->eigenvectors.data[i * n + q];
            result->eigenvectors.data[i * n + p] = cadd(
                cmul(make_complex(cos_phi, 0.0), v_ip),
                cmul(make_complex(sin_phi, 0.0), cmul(cexpj(-phase), v_iq)));
            result->eigenvectors.data[i * n + q] = cadd(
                cmul(make_complex(-sin_phi, 0.0), cmul(cexpj(phase), v_ip)),
                cmul(make_complex(cos_phi, 0.0), v_iq));
        }
    }

    for (size_t i = 0; i < n; i++)
        result->eigenvalues[i] = B.data[i * n + i].real;

    result->rank = 0;
    for (size_t i = 0; i < n; i++)
        if (result->eigenvalues[i] > tol) result->rank++;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (result->eigenvalues[j] > result->eigenvalues[i]) {
                double tmp = result->eigenvalues[i];
                result->eigenvalues[i] = result->eigenvalues[j];
                result->eigenvalues[j] = tmp;
                for (size_t k = 0; k < n; k++) {
                    complex_double t = result->eigenvectors.data[k * n + i];
                    result->eigenvectors.data[k * n + i] = result->eigenvectors.data[k * n + j];
                    result->eigenvectors.data[k * n + j] = t;
                }
            }
        }
    }

    cmat_free(&B);
    return 0;
}
