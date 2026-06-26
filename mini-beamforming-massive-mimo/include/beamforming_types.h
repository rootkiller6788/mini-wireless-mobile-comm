/**
 * beamforming_types.h ? Core Types for Beamforming & Massive MIMO
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: complex_double, complex_vector, complex_matrix
 *   L3 Mathematical Structures: complex algebra, matrix decomposition types
 *
 * Reference: Molisch (2011) Wireless Communications, Ch.20
 *            Heath et al. (2016) Foundations of MIMO Communication
 *            Bj?rnson et al. (2017) Massive MIMO Networks
 *
 * Design: Each type directly maps to a physical/engineering concept.
 *         No filler types ? every struct has operational meaning.
 */

#ifndef BEAMFORMING_TYPES_H
#define BEAMFORMING_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * L1: Complex Number Types
 * ================================================================ */

/** Complex double ? fundamental building block for baseband signals.
 *  Maps to: complex envelope representation of RF signals.
 *  Each beamforming weight = one complex_double per antenna element. */
typedef struct {
    double real;
    double imag;
} complex_double;

/** Complex float ? memory-efficient variant for large MIMO arrays.
 *  Used when double precision not required (SNR < 40 dB). */
typedef struct {
    float real;
    float imag;
} complex_float;

/* ================================================================
 * L1: Vector Types
 * ================================================================ */

/** Complex vector ? represents a beamforming weight vector w in C^N
 *  or a received signal vector y in C^M at the antenna array.
 *  length = number of antenna elements.
 *  Maps to: steering vector a(theta), precoding vector w_k, received signal y. */
typedef struct {
    complex_double *data;
    size_t length;
    int owner;  /* 1 = owns memory (must free), 0 = view into matrix */
} complex_vector;

/** Real vector ? for real-valued operations like DOA search grid.
 *  Maps to: angular scan grid theta in R^K. */
typedef struct {
    double *data;
    size_t length;
} real_vector;

/* ================================================================
 * L1: Matrix Types
 * ================================================================ */

/** Complex matrix ? represents MIMO channel matrix H in C^(MxN),
 *  precoding matrix W in C^(NxK), or covariance matrix R in C^(MxM).
 *  rows = number of receive antennas, cols = number of transmit antennas.
 *  Maps to: MIMO channel, beamforming matrix, spatial covariance. */
typedef struct {
    complex_double *data;   /* row-major storage: data[i*cols + j] */
    size_t rows;
    size_t cols;
    int owner;
} complex_matrix;

/** Real matrix ? for DOA spectrum, real-valued transforms.
 *  Maps to: MUSIC pseudo-spectrum P(theta) stored as matrix rows. */
typedef struct {
    double *data;
    size_t rows;
    size_t cols;
} real_matrix;

/* ================================================================
 * L1: Eigenvalue Decomposition Result
 * ================================================================ */

/** Eigenvalue decomposition result ? Sigma (singular values) and V (eigenvectors).
 *  Maps to: SVD of channel matrix H = U Sigma V^H.
 *  eigenvalues[i] = sigma_i^2 / lambda_i (squared singular value = eigenvalue of H^H H).
 *  Used by: waterfilling (L4), MUSIC noise subspace (L5), capacity formulas (L4). */
typedef struct {
    double *eigenvalues;        /* length = min(rows, cols) */
    complex_matrix eigenvectors; /* right singular vectors V    */
    size_t rank;
} eigendecomp_result;

/* ================================================================
 * L1: SVD Decomposition Result
 * ================================================================ */

/** SVD result ? H = U Sigma V^H.
 *  U: left singular vectors (MxM)
 *  Sigma: singular values (min(M,N))
 *  V: right singular vectors (NxN)
 *  Essential for MIMO capacity computation and precoder design. */
typedef struct {
    complex_matrix U;
    double *sigma;              /* singular values, descending  */
    complex_matrix V;
    size_t M;                   /* rows of original matrix      */
    size_t N;                   /* cols of original matrix      */
} svd_result;

/* ================================================================
 * L2: Beam Pattern Result
 * ================================================================ */

/** Beam pattern result ? |AF(theta)|^2 for azimuth angles.
 *  Maps to: radiation pattern of the antenna array.
 *  gain_db[i] = 10*log10(|AF(theta_i)|^2)
 *  Used by: beam pattern visualization, null depth verification. */
typedef struct {
    double *angles_rad;         /* scan angles (radians)        */
    double *gain_linear;        /* array factor magnitude^2     */
    double *gain_db;            /* gain in dB                  */
    size_t num_angles;
    double steering_angle;      /* main lobe direction          */
    double half_power_bw;       /* 3dB beamwidth                */
    double max_sidelobe_db;     /* peak side lobe level         */
} beam_pattern;

/* ================================================================
 * L3: SVD Configuration (for iterative SVD algorithms)
 * ================================================================ */

typedef struct {
    int max_iterations;
    double tolerance;
    int use_qr_iteration;       /* 1 = QR, 0 = power method    */
} svd_config;

/* ================================================================
 * Complex Number Operations  (L3: Mathematical Structures)
 * ================================================================ */

/** Complex addition: c = a + b. The simplest operation in C. */
complex_double cadd(complex_double a, complex_double b);

/** Complex subtraction: c = a - b */
complex_double csub(complex_double a, complex_double b);

/** Complex multiplication: c = a * b = (a_r b_r - a_i b_i) + j(a_r b_i + a_i b_r).
 *  Essential for: steering vector phase rotation e^{jphi}. */
complex_double cmul(complex_double a, complex_double b);

/** Complex division: c = a / b. Used for normalization. */
complex_double cdiv(complex_double a, complex_double b);

/** Complex conjugate: a* = a_r - j a_i.
 *  Used for: Hermitian transpose computation. */
complex_double cconj(complex_double a);

/** Complex magnitude: |a| = sqrt(a_r^2 + a_i^2).
 *  Used for: beam pattern amplitude, signal power. */
double complex_abs(complex_double a);

/** Complex phase angle: angle(a) = atan2(a_i, a_r).
 *  Used for: phase alignment in coherent combining. */
double complex_arg(complex_double a);

/** Complex exponential: e^{jtheta} = cos theta + j sin theta.
 *  Critical for: steering vector generation a(theta) = [1, e^{jphi}, e^{j2phi}, ...]. */
complex_double cexpj(double theta);

/** Initialize a complex number from real and imaginary parts. */
complex_double make_complex(double real, double imag);

/* ================================================================
 * Vector Operations  (L3: Mathematical Structures)
 * ================================================================ */

/** Allocate a complex vector of given length, zero-initialized. */
complex_vector cvec_alloc(size_t length);

/** Free a complex vector (only if owner=1). */
void cvec_free(complex_vector *v);

/** Allocate a real vector of given length. */
real_vector rvec_alloc(size_t length);

/** Free a real vector. */
void rvec_free(real_vector *v);

/** Dot product of two complex vectors: sum conj(a_i) * b_i.
 *  Used for: inner product in C^N spaces, beamformer output y = w^H x. */
complex_double cvec_dot(const complex_vector *a, const complex_vector *b);

/** Real dot product: sum a_i * b_i.
 *  Used for: real vector inner products, power calculations. */
double rvec_dot(const real_vector *a, const real_vector *b);

/** L2 norm of complex vector: sqrt(sum |a_i|^2).
 *  Used for: normalization of beamforming weights. */
double cvec_norm(const complex_vector *v);

/** Normalize complex vector to unit norm: v_hat = v / ||v||.
 *  Important for: power constraint in precoding. */
void cvec_normalize(complex_vector *v);

/** Complex vector scaling: v *= alpha.
 *  Used for: power allocation in precoding. */
void cvec_scale(complex_vector *v, complex_double alpha);

/** Copy complex vector: dst = src. */
void cvec_copy(const complex_vector *src, complex_vector *dst);

/** Create a view (non-owning) into a complex vector. */
complex_vector cvec_view(complex_double *data, size_t length);

/* ================================================================
 * Matrix Operations  (L3: Mathematical Structures)
 * ================================================================ */

/** Allocate a complex matrix of given dimensions. */
complex_matrix cmat_alloc(size_t rows, size_t cols);

/** Free a complex matrix (only if owner=1). */
void cmat_free(complex_matrix *m);

/** Allocate a real matrix. */
real_matrix rmat_alloc(size_t rows, size_t cols);

/** Free a real matrix. */
void rmat_free(real_matrix *m);

/** Get element at (row, col) ? bounds-checked. */
complex_double cmat_get(const complex_matrix *m, size_t row, size_t col);

/** Set element at (row, col) ? bounds-checked. */
void cmat_set(complex_matrix *m, size_t row, size_t col, complex_double val);

/** Matrix-vector multiply: y = A x.
 *  y[i] = sum_j A[i,j] * x[j].
 *  Used for: received signal y = H x + n. */
void cmat_mul_vec(const complex_matrix *A, const complex_vector *x,
                  complex_vector *y);

/** Matrix-matrix multiply: C = A B.
 *  C[i,j] = sum_k A[i,k] * B[k,j].
 *  Used for: channel product H_eff = W_rx^H H W_tx. */
void cmat_mul_mat(const complex_matrix *A, const complex_matrix *B,
                  complex_matrix *C);

/** Hermitian transpose: A^H, where (A^H)[i,j] = conj(A[j,i]).
 *  Critical for: precoding W = H^H (HH^H)^{-1}, MRT W = H^H/||H||. */
void cmat_hermitian(const complex_matrix *A, complex_matrix *AH);

/** Frobenius norm: ||A||_F = sqrt(sum_{i,j} |a_ij|^2).
 *  Used for: channel normalization, error metrics. */
double cmat_frobenius_norm(const complex_matrix *A);

/** Spectral norm (approximation via power iteration): ||A||_2.
 *  Used for: condition number estimation. */
double cmat_spectral_norm(const complex_matrix *A);

/** Matrix identity: A = I_n (nxn identity).
 *  Used for: regularization in MMSE, initialization. */
void cmat_set_identity(complex_matrix *A);

/** Matrix zero: A = 0.
 *  Used for: initialization of covariance matrices. */
void cmat_set_zero(complex_matrix *A);

/** Copy matrix: dst = src. */
void cmat_copy(const complex_matrix *src, complex_matrix *dst);

/** Extract column vector from matrix. */
complex_vector cmat_get_column(const complex_matrix *m, size_t col);

/** Extract row vector from matrix. */
complex_vector cmat_get_row(const complex_matrix *m, size_t row);

/** Create a view (non-owning) into a complex matrix. */
complex_matrix cmat_view(complex_double *data, size_t rows, size_t cols);

/** Check if matrix is Hermitian (A == A^H within tolerance).
 *  Used for: verifying covariance matrix properties. */
int cmat_is_hermitian(const complex_matrix *A, double tol);

/** Check if matrix is positive semi-definite.
 *  Used for: verifying channel covariance matrices. */
int cmat_is_psd(const complex_matrix *A, double tol);

/* ================================================================
 * SVD Operations  (L3: Mathematical Structures)
 * ================================================================ */

/** Allocate SVD result given matrix dimensions. */
svd_result svd_result_alloc(size_t M, size_t N);

/** Free SVD result. */
void svd_result_free(svd_result *svd);

/** Compute SVD: A = U Sigma V^H using Golub-Reinsch algorithm.
 *  L5 Algorithm: Two-phase SVD (bidiagonalization + QR iteration).
 *  Complexity: O(M N min(M,N)).
 *  Reference: Golub & Van Loan (2013) Matrix Computations, Sec.8.6. */
int svd_compute(const complex_matrix *A, svd_result *result,
                const svd_config *cfg);

/** SVD-based low-rank approximation: A ~ U_k Sigma_k V_k^H kept k largest singular values.
 *  L5 Algorithm: Truncated SVD for channel compression.
 *  Used for: massive MIMO dimensionality reduction. */
int svd_low_rank_approx(const complex_matrix *A, size_t k,
                        complex_matrix *approx);

/** Compute pseudo-inverse via SVD: A^+ = V Sigma^+ U^H.
 *  L5 Algorithm: Moore-Penrose pseudo-inverse.
 *  Critical for: ZF precoding W_ZF = H^+. */
int svd_pinv(const complex_matrix *A, complex_matrix *pinv, double tol);

/* ================================================================
 * Eigenvalue Decomposition
 * ================================================================ */

/** Allocate eigenvalue decomposition result. */
eigendecomp_result eigen_alloc(size_t size);

/** Free eigenvalue decomposition. */
void eigen_free(eigendecomp_result *evd);

/** Compute eigenvalues/eigenvectors of Hermitian matrix using Jacobi method.
 *  L5 Algorithm: Cyclic Jacobi eigenvalue algorithm.
 *  Complexity: O(n^3).
 *  Used for: MUSIC noise subspace, covariance matrix analysis. */
int eigen_sym_decomp(const complex_matrix *A, eigendecomp_result *result,
                     int max_iter, double tol);

#endif /* BEAMFORMING_TYPES_H */
