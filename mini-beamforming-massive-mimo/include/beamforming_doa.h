/**
 * beamforming_doa.h ? Direction-of-Arrival (DOA) Estimation
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: doa_result, music_config, esprit_result
 *   L2 Core Concepts: Spatial spectrum, noise subspace, signal subspace,
 *                     array manifold, spatial frequency
 *   L3 Mathematical Structures: Eigendecomposition of covariance matrix,
 *                               Vandermonde structure of steering matrix
 *   L4 Fundamental Laws: Cramer-Rao Lower Bound for DOA estimation
 *   L5 Algorithms: MUSIC, ESPRIT, Capon/MVDR, Bartlett, Root-MUSIC
 *   L6 Canonical Problems: Two-source resolution, DOA estimation accuracy vs SNR
 *   L7 Applications: Radar angular resolution, 5G beam management
 *
 * Reference: Schmidt (1986) "MUSIC Algorithm" IEEE TAP
 *            Roy & Kailath (1989) "ESPRIT" IEEE TASSP
 *            Capon (1969) "MVDR" Proc. IEEE
 *            Van Trees (2002) Optimum Array Processing, Ch.8-9
 */

#ifndef BEAMFORMING_DOA_H
#define BEAMFORMING_DOA_H

#include "beamforming_types.h"
#include "beamforming_array.h"

/* ================================================================
 * L1: DOA Result Types
 * ================================================================ */

/** DOA estimation result for a single source.
 *  Maps to: estimated angle of arrival with confidence metric. */
typedef struct {
    double angle_rad;           /* Estimated DOA (radians)         */
    double confidence;          /* MUSIC spectral peak height      */
} doa_estimate;

/** Multi-source DOA result.
 *  Maps to: simultaneous estimation of K sources. */
typedef struct {
    doa_estimate *sources;      /* Array of K estimates            */
    size_t num_sources;         /* Number of detected sources      */
    double *spectrum;           /* Full spatial spectrum P(theta)  */
    double *angle_grid;         /* Grid of scanned angles          */
    size_t spectrum_length;     /* Number of grid points           */
} doa_result;

/** MUSIC algorithm configuration.
 *  L5 Algorithm: Multiple Signal Classification. */
typedef struct {
    size_t num_elements;        /* Array elements M               */
    size_t num_snapshots;       /* Time samples N_s               */
    size_t num_sources;         /* Known/via MDL number of sources*/
    double angle_min;           /* Scan range min (radians)       */
    double angle_max;           /* Scan range max (radians)       */
    size_t angle_grid_size;     /* Number of scan points          */
    int use_forward_backward;   /* 1 = FB averaging for coherent */
} music_config;

/** ESPRIT result ? signal parameter estimation via rotational invariance.
 *  L5 Algorithm: ESPRIT uses two identical subarrays with known displacement.
 *  Advantage: no spectral search needed ? closed-form solution.
 *  Maps to: DOA estimates from signal subspace rotation. */
typedef struct {
    double *angles_rad;         /* Estimated DOAs                 */
    size_t num_sources;         /* Number of sources found        */
    double *eigenvalues;        /* LS/ESPRIT eigenvalues          */
    double fitting_error;       /* LS fitting residual            */
} esprit_result;

/** MDL (Minimum Description Length) source enumeration result.
 *  L5 Algorithm: Information-theoretic criterion for model order selection.
 *  Reference: Wax & Kailath (1985) "Detection of Signals by MDL," IEEE TASSP. */
typedef struct {
    size_t estimated_sources;   /* MDL estimate of K              */
    double *mdl_values;         /* MDL criterion for each k       */
    size_t max_sources;         /* Maximum tested                 */
} mdl_result;

/* ================================================================
 * L5: Classical DOA Algorithms
 * ================================================================ */

/** MUSIC (Multiple Signal Classification) pseudo-spectrum.
 *  P_MUSIC(theta) = 1 / (a^H(theta) E_n E_n^H a(theta)).
 *  L5 Algorithm: Subspace-based DOA estimation.
 *  Principle: separate signal subspace (K largest eigenvectors of R_yy)
 *             from noise subspace (M-K smallest eigenvectors).
 *             Steering vectors of true sources are orthogonal to noise subspace.
 *  Complexity: O(M^3 + M^2 * N_theta) ? eigendecomp dominates.
 *  Reference: Schmidt (1986) IEEE TAP, vol.34, no.3.
 *  Note: Fails for coherent sources (use spatial smoothing first). */
int doa_music(const array_snapshot_buffer *snapshots,
              const ula_geometry *array,
              const music_config *config,
              doa_result *result);

/** Root-MUSIC ? polynomial rooting version for ULA.
 *  L5 Algorithm: Converts spectral search to polynomial root-finding.
 *  Advantage: Exact DOA estimates without grid quantization error.
 *  Finds roots of a^T(1/z) E_n E_n^H a(z) = 0 near unit circle.
 *  Complexity: O(M^3 + M^2) ? avoids search grid.
 *  Reference: Barabell (1983) "Root-MUSIC," ICASSP. */
int doa_root_music(const array_snapshot_buffer *snapshots,
                   const ula_geometry *array,
                   size_t num_sources,
                   double *angles_rad);

/** ESPRIT (Estimation of Signal Parameters via Rotational Invariance).
 *  L5 Algorithm: Closed-form DOA from signal subspace rotation.
 *  Uses two overlapping subarrays: X_1 = first M-1 elements, X_2 = last M-1.
 *  The rotation matrix Phi = (E_s1)^+ E_s2 has eigenvalues e^{j k d cos(theta)}.
 *  Complexity: O(M^3) ? no spectral search needed!
 *  Advantage: Computationally efficient, no grid quantization.
 *  Reference: Roy & Kailath (1989) IEEE TASSP, vol.37, no.7. */
int doa_esprit(const array_snapshot_buffer *snapshots,
               const ula_geometry *array,
               size_t num_sources,
               esprit_result *result);

/** Capon (MVDR ? Minimum Variance Distortionless Response) beamformer.
 *  P_Capon(theta) = 1 / (a^H(theta) R_yy^{-1} a(theta)).
 *  L5 Algorithm: Adaptive beamformer used as spatial spectrum estimator.
 *  Constraint: w^H a(theta_0) = 1 (distortionless), min w^H R w (min variance).
 *  Solution: w_opt = R^{-1} a / (a^H R^{-1} a), output power = 1/(a^H R^{-1} a).
 *  Advantage: Better resolution than Bartlett, adaptive to interference.
 *  Complexity: O(M^3 + M * N_theta). */
int doa_capon(const array_snapshot_buffer *snapshots,
              const ula_geometry *array,
              double angle_min, double angle_max,
              size_t angle_grid_size,
              doa_result *result);

/** Bartlett (conventional) beamformer.
 *  P_Bartlett(theta) = a^H(theta) R_yy a(theta) / M^2.
 *  L5 Algorithm: Classical delay-and-sum spatial spectrum.
 *  Equivalent to Fourier-based DOA estimation.
 *  Resolution limited by Rayleigh criterion: ~ lambda/(M*d).
 *  Complexity: O(M^2 * N_theta).
 *  Reference: Bartlett (1948) "Smoothing Periodograms," Nature. */
int doa_bartlett(const array_snapshot_buffer *snapshots,
                 const ula_geometry *array,
                 double angle_min, double angle_max,
                 size_t angle_grid_size,
                 doa_result *result);

/* ================================================================
 * L3: Covariance Matrix Estimation
 * ================================================================ */

/** Sample covariance matrix: R_yy = (1/N) sum_{n=1}^N y[n] y^H[n].
 *  L3 Mathematical Structure: Maximum likelihood covariance estimate.
 *  R_yy is Hermitian, PSD, of size M x M for M-element array.
 *  Complexity: O(M^2 * N).
 *  Reference: Van Trees (2002) Optimum Array Processing, ?2.4. */
int estimate_covariance(const array_snapshot_buffer *snapshots,
                        complex_matrix *R_yy);

/** Forward-backward averaging (for coherent sources).
 *  R_FB = (R + J R^* J) / 2, where J is the exchange matrix.
 *  L3 Mathematical Structure: Decorrelation via spatial smoothing.
 *  Doubles the effective number of snapshots for Toeplitz covariance.
 *  Complexity: O(M^2). */
int covariance_fb_averaging(const complex_matrix *R, complex_matrix *R_fb);

/** Spatial smoothing (decorrelation of coherent sources).
 *  Divides M-element array into L = M-L_sub+1 overlapping subarrays.
 *  R_ss = (1/L) sum_{l=1}^L R_l where R_l is l-th subarray covariance.
 *  L3 Mathematical Structure: Restores full rank of signal subspace.
 *  Essential for MUSIC/ESPRIT with coherent sources.
 *  Complexity: O(M * L_sub^2 * N). */
int covariance_spatial_smoothing(const complex_matrix *R,
                                 size_t M, size_t L_sub,
                                 complex_matrix *R_ss);

/* ================================================================
 * L4: Performance Bounds
 * ================================================================ */

/** Cramer-Rao Lower Bound for DOA estimation of a single source.
 *  CRLB(theta) = 6 / (N * SNR * M * (M^2-1) * (k*d*sin(theta))^2).
 *  L4 Fundamental Law: Fundamental limit on DOA estimation accuracy.
 *  Shows CRLB inversely proportional to SNR, N, and M^3.
 *  Reference: Stoica & Nehorai (1989) "MUSIC, ML, and CRB," IEEE TASSP.
 *  Complexity: O(1). */
double crlb_doa_single(double snr_linear, size_t M, size_t N_snapshots,
                       const ula_geometry *array, double theta_rad);

/** CRLB for K uncorrelated sources.
 *  Full Fisher information matrix inversion.
 *  L4 Fundamental Law: General CRLB for multi-source DOA.
 *  Complexity: O(K^3). */
double crlb_doa_multi(const complex_matrix *R_yy,
                      const ula_geometry *array,
                      const double *true_angles,
                      size_t K, size_t N_snapshots);

/* ================================================================
 * L5: Source Number Detection
 * ================================================================ */

/** MDL (Minimum Description Length) criterion for source enumeration.
 *  MDL(k) = -N*(M-k)*log(gmean/amean) + 0.5*k*(2M-k)*log(N).
 *  where gmean/amean = geometric/arithmetic mean of M-k smallest eigenvalues.
 *  Estimated K = argmin_k MDL(k).
 *  L5 Algorithm: Information-theoretic model order selection.
 *  Complexity: O(M^3 + M^2). */
int source_detection_mdl(const array_snapshot_buffer *snapshots,
                         size_t max_sources, mdl_result *result);

/** AIC (Akaike Information Criterion) source enumeration.
 *  AIC(k) = -N*(M-k)*log(gmean/amean) + k*(2M-k).
 *  L5 Algorithm: Alternative to MDL (tends to overestimate).
 *  Complexity: O(M^2). */
int source_detection_aic(const array_snapshot_buffer *snapshots,
                         size_t max_sources, mdl_result *result);

/* ================================================================
 * Utility Functions
 * ================================================================ */

/** Allocate DOA result for given parameters. */
doa_result doa_result_alloc(size_t num_sources, size_t spectrum_length);

/** Free DOA result. */
void doa_result_free(doa_result *result);

/** Find spectral peaks (above threshold) for DOA extraction.
 *  L3 Mathematical Structure: Peak detection in spatial spectrum.
 *  Simple gradient-based peak finder. */
int doa_find_peaks(const double *spectrum, const double *angles,
                   size_t length, double threshold,
                   size_t max_peaks, doa_estimate *peaks,
                   size_t *num_found);

/** Initialize default MUSIC configuration. */
music_config music_config_default(size_t M, size_t N_snapshots,
                                  size_t num_sources);

/** Allocate array snapshot buffer. */
array_snapshot_buffer snapshot_buffer_alloc(size_t M, size_t N_snapshots);

/** Free snapshot buffer. */
void snapshot_buffer_free(array_snapshot_buffer *buf);

#endif /* BEAMFORMING_DOA_H */
