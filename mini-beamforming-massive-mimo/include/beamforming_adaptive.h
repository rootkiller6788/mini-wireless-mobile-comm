/**
 * beamforming_adaptive.h ? Adaptive Beamforming Algorithms
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: lms_beamformer, rls_beamformer, cma_beamformer
 *   L2 Core Concepts: Adaptive weight update, error signal, convergence,
 *                     step-size tradeoff, misadjustment, forgetting factor
 *   L3 Mathematical Structures: Stochastic gradient descent, Wiener solution,
 *                               matrix inversion lemma (Woodbury)
 *   L5 Algorithms: LMS, NLMS, RLS, CMA, SMI, Kalman beamforming
 *   L6 Canonical Problems: Interference rejection, tracking moving sources,
 *                          blind beamforming (CMA)
 *   L7 Applications: GPS anti-jam, radar jammer suppression,
 *                    5G beam tracking, WiFi beamforming
 *
 * Reference: Widrow & Stearns (1985) Adaptive Signal Processing
 *            Haykin (2002) Adaptive Filter Theory, 4th Ed.
 *            Godard (1980) "Self-Recovering Equalization," IEEE TCOM (CMA)
 *            Reed et al. (1974) "SMI," IEEE TASSP
 *            Monzingo & Miller (2004) Adaptive Arrays
 */

#ifndef BEAMFORMING_ADAPTIVE_H
#define BEAMFORMING_ADAPTIVE_H

#include "beamforming_types.h"
#include "beamforming_array.h"

/* ================================================================
 * L1: Adaptive Beamformer Types
 * ================================================================ */

/** LMS (Least Mean Square) beamformer state.
 *  L5 Algorithm: Stochastic gradient descent with instantaneous gradient estimate.
 *  Simplest and most widely used adaptive algorithm.
 *  Weight update: w[n+1] = w[n] + mu * e*[n] * x[n].
 *  Complexity: O(M) per iteration (M = number of elements).
 *  Reference: Widrow & Hoff (1960) "Adaptive Switching Circuits." */
typedef struct {
    complex_vector weights;     /* Current beamforming weights w   */
    double mu;                  /* Step size (learning rate)       */
    double leakage;             /* Leakage factor for robust LMS   */
    size_t num_elements;        /* Array size M                    */
    size_t iteration;           /* Iteration counter               */
    double e_history;           /* Latest error magnitude          */
    double mse_estimate;        /* Running MSE estimate            */
} lms_beamformer;

/** NLMS (Normalized LMS) beamformer state.
 *  L5 Algorithm: Normalized LMS ? step size normalized by input power.
 *  mu[n] = mu_0 / (epsilon + ||x[n]||^2).
 *  Advantage: Faster and more stable convergence than LMS.
 *  Complexity: O(M) per iteration. */
typedef struct {
    complex_vector weights;
    double mu_0;                /* Normalized step size parameter  */
    double epsilon;             /* Regularization (prevents div-by-0) */
    size_t num_elements;
    size_t iteration;
} nlms_beamformer;

/** RLS (Recursive Least Squares) beamformer state.
 *  L5 Algorithm: Recursive least squares with exponential forgetting.
 *  Minimizes sum_{i=0}^n lambda^{n-i} |e[i]|^2 (weighted LS).
 *  Update uses matrix inversion lemma (Woodbury identity) for O(M^2).
 *  Advantage: Much faster convergence than LMS (order of magnitude).
 *  Disadvantage: O(M^2) complexity vs O(M) for LMS.
 *  Reference: Plackett (1950) "RLS," Biometrika.
 *  Complexity: O(M^2) per iteration. */
typedef struct {
    complex_vector weights;     /* Current beamforming weights     */
    complex_matrix P;           /* Inverse correlation matrix (MxM) */
    double lambda;              /* Forgetting factor (0 < lambda <= 1) */
    double delta;               /* Initialization constant         */
    size_t num_elements;        /* Array size M                    */
    size_t iteration;
    double *gain_vector_norm;   /* Trace for monitoring            */
} rls_beamformer;

/** CMA (Constant Modulus Algorithm) blind beamformer state.
 *  L5 Algorithm: Blind adaptive beamforming ? no reference signal needed.
 *  Cost function: J = E[(|y[n]|^2 - R_2)^2], R_2 = E[|s|^4]/E[|s|^2].
 *  Weight update: w[n+1] = w[n] - mu * (|y|^2 - R_2) * y*[n] * x[n].
 *  Exploits constant modulus property of PSK/FSK signals.
 *  Reference: Godard (1980), Treichler & Agee (1983). */
typedef struct {
    complex_vector weights;
    double mu;                  /* Step size                       */
    double R2;                  /* Godard dispersion constant      */
    size_t num_elements;
    size_t iteration;
    double modulus_error;       /* |y|^2 - R2 error               */
} cma_beamformer;

/** SMI (Sample Matrix Inversion) beamformer.
 *  L5 Algorithm: Direct computation of Wiener solution from sample covariance.
 *  w_SMI = R_xx^{-1} r_xd (Wiener-Hopf solution).
 *  For MVDR: w = R_xx^{-1} a(theta) / (a^H R_xx^{-1} a(theta)).
 *  Advantage: Fastest convergence (block-based), no iteration.
 *  Disadvantage: O(M^3) for matrix inversion, requires N >= 2M snapshots.
 *  Reference: Reed, Mallett, Brennan (1974) IEEE TASSP. */
typedef struct {
    complex_vector weights;
    size_t num_elements;
    unsigned int snapshots_used;
    double converged;           /* 1 if loss < 3dB from optimal   */
} smi_beamformer;

/** Kalman-based beamformer state.
 *  L5 Algorithm: Kalman filter for time-varying optimal weight tracking.
 *  State-space model: w[n+1] = w[n] + v[n] (process noise).
 *  Observation: d[n] = w^H[n] x[n] + e[n] (measurement noise).
 *  Advantage: Optimal tracking of non-stationary channels.
 *  Complexity: O(M^2) per iteration.
 *  Reference: Baird (1974) "Kalman Adaptive Arrays," IEEE AES. */
typedef struct {
    complex_vector weights;
    complex_matrix P;           /* State error covariance (MxM)   */
    complex_matrix Q;           /* Process noise covariance (MxM) */
    double R;                   /* Measurement noise variance     */
    size_t num_elements;
    size_t iteration;
} kalman_beamformer;

/* ================================================================
 * L5: LMS Family Algorithms
 * ================================================================ */

/** Initialize LMS beamformer.
 *  mu: step size (typical: 0 < mu < 2/lambda_max).
 *  leakage: 0 = standard LMS, >0 = leaky LMS for robustness.
 *  initial_weights: can be NULL (initialized to steering vector),
 *                   or provided for warm start. */
void lms_init(lms_beamformer *bmf, size_t M, double mu, double leakage,
              const complex_vector *initial_weights);

/** LMS weight update (one snapshot).
 *  w[n+1] = (1-mu*leak)*w[n] + mu * conj(e) * x[n].
 *  Returns: beamformer output y = w^H x.
 *  Reference: Widrow et al. (1975) Proc. IEEE. */
complex_double lms_update(lms_beamformer *bmf,
                          const complex_vector *snapshot,
                          complex_double desired_signal);

/** NLMS weight update.
 *  mu_eff = mu_0 / (epsilon + ||x||^2).
 *  w[n+1] = w[n] + mu_eff * conj(e) * x[n].
 *  Returns: beamformer output. */
complex_double nlms_update(nlms_beamformer *bmf,
                           const complex_vector *snapshot,
                           complex_double desired_signal);

/** Initialize NLMS beamformer. */
void nlms_init(nlms_beamformer *bmf, size_t M, double mu_0,
               double epsilon, const complex_vector *initial_weights);

/** Leaky LMS with momentum (accelerated convergence).
 *  v[n+1] = alpha * v[n] + mu * conj(e) * x[n].
 *  w[n+1] = w[n] + v[n+1].
 *  L5 Algorithm: Momentum-accelerated LMS for faster convergence. */
complex_double lms_momentum_update(lms_beamformer *bmf,
                                   const complex_vector *snapshot,
                                   complex_double desired_signal,
                                   double momentum);

/* ================================================================
 * L5: RLS Algorithm
 * ================================================================ */

/** Initialize RLS beamformer.
 *  lambda: forgetting factor (0.95-0.999 typical).
 *  delta: initial diagonal loading of P (delta ~ 1/snr). */
void rls_init(rls_beamformer *bmf, size_t M, double lambda,
              double delta, const complex_vector *initial_weights);

/** RLS weight update (one snapshot).
 *  k = P x / (lambda + x^H P x)   [Kalman gain]
 *  P = (P - k x^H P) / lambda      [Ricatti update]
 *  w = w + k * conj(e)
 *  Returns: beamformer output.
 *  Complexity: O(M^2). */
complex_double rls_update(rls_beamformer *bmf,
                          const complex_vector *snapshot,
                          complex_double desired_signal);

/** RLS with QR-decomposition (QRD-RLS) ? numerically stable variant.
 *  L5 Algorithm: QRD-RLS using Givens rotations for better numerical stability.
 *  Complexity: O(M^2) but more robust to roundoff. */
complex_double rls_qr_update(rls_beamformer *bmf,
                             const complex_vector *snapshot,
                             complex_double desired_signal);

/* ================================================================
 * L5: Blind Beamforming (CMA)
 * ================================================================ */

/** Initialize CMA beamformer.
 *  R2: Godard constant (depends on constellation).
 *  For BPSK: R2 = 1; QPSK: R2 = 1; 16-QAM: R2 = 1.32. */
void cma_init(cma_beamformer *bmf, size_t M, double mu,
              double R2, const complex_vector *initial_weights);

/** CMA weight update (one snapshot).
 *  y = w^H x.
 *  e = y * (|y|^2 - R2)   [CMA error].
 *  w = w - mu * conj(e) * x.
 *  Returns: beamformer output y.
 *  Reference: Treichler & Agee (1983) IEEE TASSP. */
complex_double cma_update(cma_beamformer *bmf,
                          const complex_vector *snapshot);

/** Multi-modulus algorithm (MMA) ? CMA variant for QAM.
 *  L5 Algorithm: Extension of CMA using separate real/imag cost functions.
 *  Better for cross-QAM constellations. */
complex_double mma_update(cma_beamformer *bmf,
                          const complex_vector *snapshot);

/* ================================================================
 * L5: SMI Beamforming
 * ================================================================ */

/** SMI weight computation (block-based).
 *  w = R_xx^{-1} a(theta_steer).
 *  L5 Algorithm: Direct sample matrix inversion ? optimal in stationary case.
 *  Requires: N >= 2M samples for 3dB loss from optimal.
 *  Reference: Reed, Mallett, Brennan (1974). */
int smi_compute_weights(const array_snapshot_buffer *snapshots,
                        const ula_geometry *array,
                        steering_direction_1d steer_dir,
                        smi_beamformer *bmf);

/** Diagonal loaded SMI for robustness.
 *  w = (R_xx + gamma * I)^{-1} a(theta_steer).
 *  gamma: loading level (typically 10*sigma^2). */
int loaded_smi_compute(const array_snapshot_buffer *snapshots,
                       const ula_geometry *array,
                       steering_direction_1d steer_dir,
                       double gamma,
                       smi_beamformer *bmf);

/* ================================================================
 * L5: Kalman Beamforming
 * ================================================================ */

/** Initialize Kalman beamformer.
 *  q: process noise power, r: measurement noise variance. */
void kalman_init(kalman_beamformer *bmf, size_t M,
                 double q, double r,
                 const complex_vector *initial_weights);

/** Kalman filter weight update.
 *  Predict: P = P + Q.
 *  Update: k = P x / (r + x^H P x).
 *          P = (I - k x^H) P.
 *          w = w + k * conj(e).
 *  Complexity: O(M^2).
 *  Reference: Haykin (2002) Adaptive Filter Theory, Ch.14. */
complex_double kalman_update(kalman_beamformer *bmf,
                             const complex_vector *snapshot,
                             complex_double desired_signal);

/* ================================================================
 * L6: Adaptive Beamforming Performance Metrics
 * ================================================================ */

/** Output SINR computation.
 *  SINR = |w^H a(theta_s)|^2 * P_s / (w^H R_interf w + sigma^2).
 *  L6 Canonical Problem: Evaluate adaptive beamformer performance.
 *  Reference: Van Trees (2002) Optimum Array Processing, ?6.3. */
double compute_adaptive_sinr(const complex_vector *weights,
                             const ula_geometry *array,
                             double signal_theta,
                             double interferer_angle,
                             double inr_linear,
                             double snr_linear);

/** Learning curve computation (MSE vs iterations).
 *  L6 Canonical Problem: Adaptive filter convergence analysis.
 *  Returns array of MSE values for plotting. */
real_vector compute_learning_curve(lms_beamformer *bmf,
                                   const array_snapshot_buffer *data,
                                   const complex_vector *desired_sequence,
                                   size_t num_iterations);

/** Misadjustment estimation.
 *  M = (mu/2) * trace(R_xx) ? excess MSE over Wiener solution.
 *  L2 Concept: Steady-state excess error due to gradient noise.
 *  Reference: Widrow & Stearns (1985), ?6.4. */
double estimate_misadjustment_lms(double mu, const complex_matrix *R_xx);

/** Convergence time constant.
 *  tau ~ 1 / (4 * mu * lambda_min) for LMS.
 *  L2 Concept: Number of iterations to reach (1-1/e) of steady state. */
double lms_convergence_time_constant(double mu, double lambda_min);

/* ================================================================
 * Utility Functions
 * ================================================================ */

/** Free LMS beamformer internal allocations. */
void lms_free(lms_beamformer *bmf);

/** Free NLMS beamformer. */
void nlms_free(nlms_beamformer *bmf);

/** Free RLS beamformer. */
void rls_free(rls_beamformer *bmf);

/** Free CMA beamformer. */
void cma_free(cma_beamformer *bmf);

/** Free SMI beamformer. */
void smi_free(smi_beamformer *bmf);

/** Free Kalman beamformer. */
void kalman_free(kalman_beamformer *bmf);

#endif /* BEAMFORMING_ADAPTIVE_H */
