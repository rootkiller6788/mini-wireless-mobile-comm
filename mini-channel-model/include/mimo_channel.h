/**
 * @file mimo_channel.h
 * @brief MIMO Channel Models — Spatial Multiplexing & Diversity (L6, L8)
 *
 * Models the Multiple-Input Multiple-Output channel matrix H characterizing
 * the propagation between N_tx transmit and N_rx receive antennas.
 *
 * L3 Mathematical Structures:
 *   - MIMO channel matrix H (N_rx x N_tx)
 *   - Singular Value Decomposition: H = U*Sigma*V^H
 *   - Eigenvalue decomposition of HH^H for capacity computation
 *   - Kronecker spatial correlation model: R = kron(R_TX, R_RX)
 *
 * L4 Fundamental Laws:
 *   - MIMO Capacity Theorem (Telatar 1999, Foschini 1998):
 *     C = E[log2(det(I + (rho/N_t)*HH^H))]  bps/Hz
 *   - Water-filling power allocation across eigenmodes
 *   - Degrees of freedom = min(N_tx, N_rx) for full-rank channel
 *
 * L5 Algorithms:
 *   - Cholesky-based correlated MIMO channel generation
 *   - Singular value computation for channel capacity
 *   - Water-filling algorithm
 *   - Condition number for MIMO stream quality assessment
 *
 * L6 Canonical Problems:
 *   - Spatial multiplexing vs diversity trade-off
 *   - MIMO-OFDM channel estimation
 *   - Precoding and beamforming matrix computation
 *
 * L8 Advanced Topics:
 *   - Kronecker correlated channel model (3GPP spatial channel model)
 *   - Massive MIMO channel hardening
 *   - mmWave hybrid beamforming channel model
 *
 * Reference: Telatar, "Capacity of Multi-antenna Gaussian Channels", 1999
 * Reference: Paulraj, Nabar, Gore, "Introduction to Space-Time Wireless
 *            Communications", 2003
 * Reference: 3GPP TR 38.901 — 5G NR channel model
 *
 * Course Mapping:
 *   MIT 6.450 - Digital Communications (MIMO capacity)
 *   Stanford EE359 - Wireless (MIMO, spatial multiplexing)
 *   Georgia Tech ECE 6601 - Communications (MIMO systems)
 */

#ifndef MIMO_CHANNEL_H
#define MIMO_CHANNEL_H

#include "channel_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L3: MIMO Channel Matrix Operations
 *============================================================================*/

/**
 * @brief Allocate a MIMO channel matrix
 * @param num_rx Number of receive antennas
 * @param num_tx Number of transmit antennas
 * @return Pointer to allocated channel matrix, or NULL on failure.
 *         The H matrix elements are zero-initialized.
 * Complexity: O(N_rx * N_tx)
 */
mimo_channel_matrix_t *mimo_channel_alloc(size_t num_rx, size_t num_tx);

/**
 * @brief Free MIMO channel matrix
 * @param mimo MIMO channel matrix (NULL safe)
 */
void mimo_channel_free(mimo_channel_matrix_t *mimo);

/**
 * @brief Copy MIMO channel matrix (deep copy)
 * @param dst Destination (pre-allocated with matching dimensions)
 * @param src Source
 * @return 0 on success, -1 if dimensions mismatch
 * Complexity: O(N*M)
 */
int mimo_channel_copy(mimo_channel_matrix_t *dst, const mimo_channel_matrix_t *src);

/**
 * @brief Get element H(i,j) from MIMO channel matrix
 * @param mimo MIMO channel matrix
 * @param rx_idx Receive antenna index (0-based)
 * @param tx_idx Transmit antenna index (0-based)
 * @return h_{ij} complex channel gain
 * Complexity: O(1)
 */
double complex mimo_get_element(const mimo_channel_matrix_t *mimo,
                                 size_t rx_idx, size_t tx_idx);

/**
 * @brief Set element H(i,j) in MIMO channel matrix
 * @param mimo MIMO channel matrix
 * @param rx_idx Receive antenna index
 * @param tx_idx Transmit antenna index
 * @param value Complex channel gain to set
 * Complexity: O(1)
 */
void mimo_set_element(mimo_channel_matrix_t *mimo,
                       size_t rx_idx, size_t tx_idx, double complex value);

/**
 * @brief Compute Frobenius norm of MIMO channel matrix
 * ||H||_F = sqrt(sum_{i,j} |h_{ij}|^2)
 * @param mimo MIMO channel matrix
 * @return Frobenius norm
 * Complexity: O(N*M)
 */
double mimo_frobenius_norm(const mimo_channel_matrix_t *mimo);

/*============================================================================
 * L5: MIMO Channel Matrix Generation
 *============================================================================*/

/**
 * @brief Generate i.i.d. Rayleigh MIMO channel (uncorrelated)
 * Each element h_{ij} ~ CN(0, 1) i.i.d.
 * This is the baseline model: each antenna pair sees independent fading.
 * @param mimo Pre-allocated MIMO channel matrix (filled with samples)
 * @return 0 on success
 * Complexity: O(N_rx * N_tx)
 */
int mimo_generate_iid_rayleigh(mimo_channel_matrix_t *mimo);

/**
 * @brief Generate i.i.d. Rician MIMO channel
 * Each element has a deterministic LOS component + random diffuse component.
 * @param mimo Pre-allocated MIMO channel matrix
 * @param k_factor_db Rician K-factor (dB), same for all links
 * @return 0 on success
 * Complexity: O(N_rx * N_tx)
 */
int mimo_generate_iid_rician(mimo_channel_matrix_t *mimo, double k_factor_db);

/**
 * @brief Generate Kronecker correlated MIMO channel
 * vec(H) = R_TX^(1/2) * kron(R_RX^(1/2), I) * vec(H_iid)
 *
 * Simplified: H = R_RX^(1/2) * H_iid * R_TX^(T/2)
 * where R_RX is N_rx x N_rx Rx correlation, R_TX is N_tx x N_tx Tx correlation.
 *
 * @param mimo Pre-allocated MIMO channel matrix
 * @param corr_rx Rx correlation matrix, size N_rx x N_rx (row-major)
 * @param corr_tx Tx correlation matrix, size N_tx x N_tx (row-major)
 * @return 0 on success, -1 on Cholesky failure
 * Complexity: O(N_rx^3 + N_tx^3 + N_rx*N_tx*...)
 */
int mimo_generate_kronecker(mimo_channel_matrix_t *mimo,
                             const double *corr_rx,
                             const double *corr_tx);

/**
 * @brief Build exponential correlation model
 * R(i,j) = rho^(|i-j|) where rho is the correlation coefficient.
 * Common model for uniform linear arrays (ULA).
 * @param corr_matrix Pre-allocated n x n matrix (row-major, filled on output)
 * @param n Array size (number of antennas)
 * @param rho Correlation coefficient (0 <= rho < 1)
 * Complexity: O(n^2)
 */
void mimo_exponential_correlation(double *corr_matrix, size_t n, double rho);

/*============================================================================
 * L4: MIMO Channel Capacity (Telatar 1999)
 *
 * For a deterministic MIMO channel H with equal power allocation:
 *   C = log2(det(I_Nrx + (rho/N_tx)*HH^H))   bps/Hz
 *
 * where rho = P_total / noise_power is the average SNR.
 *
 * For ergodic capacity (fading):
 *   C_erg = E_H[C(H)]   averaged over channel realizations
 *
 * When CSI is available at transmitter, optimal power allocation uses
 * water-filling across eigenmodes.
 *============================================================================*/

/**
 * @brief Compute MIMO channel capacity with equal power allocation
 *
 * C = B*log2(det(I + (SNR_linear/N_tx)*HH^H)) bps
 *
 * Uses numerical eigenvalue decomposition via power iteration.
 *
 * @param mimo MIMO channel matrix H
 * @param snr_db Average SNR (dB) at each receive antenna = P_tx/Noise/B
 * @param bandwidth_hz Channel bandwidth (Hz)
 * @param capacity Output capacity structure (capacity_bps, spectral efficiency,
 *                 singular values, rank)
 * @return 0 on success, -1 if memory allocation fails
 * Complexity: O(N^3) for eigenvalue decomposition
 */
int mimo_capacity_equal_power(const mimo_channel_matrix_t *mimo,
                               double snr_db, double bandwidth_hz,
                               channel_capacity_t *capacity);

/**
 * @brief Compute MIMO capacity with water-filling power allocation
 *
 * P_i = max(0, mu - 1/lambda_i) where mu is water level chosen to satisfy
 * total power constraint. lambda_i are eigenvalues of HH^H scaled by SNR/N_tx.
 *
 * @param mimo MIMO channel matrix H
 * @param snr_db Average SNR (dB)
 * @param bandwidth_hz Channel bandwidth (Hz)
 * @param capacity Output capacity structure
 * @return 0 on success
 * Complexity: O(N^3 + N log N) for SVD + sorting + water-filling
 */
int mimo_capacity_waterfilling(const mimo_channel_matrix_t *mimo,
                                double snr_db, double bandwidth_hz,
                                channel_capacity_t *capacity);

/**
 * @brief Free channel capacity structure (frees singular_values array)
 * @param capacity Capacity structure to free
 */
void mimo_capacity_free(channel_capacity_t *capacity);

/*============================================================================
 * L5: MIMO Channel Metrics
 *============================================================================*/

/**
 * @brief Compute condition number of MIMO channel
 * kappa = sigma_max / sigma_min (ratio of largest to smallest singular value)
 * High condition number (> 20 dB) indicates ill-conditioned channel,
 * spatial multiplexing gain is limited.
 * @param mimo MIMO channel matrix
 * @return Condition number (linear), >= 1.0. INFINITY if rank-deficient.
 * Complexity: O(N^3)
 */
double mimo_condition_number(const mimo_channel_matrix_t *mimo);

/**
 * @brief Compute channel rank (number of non-negligible singular values)
 * @param mimo MIMO channel matrix
 * @param threshold_db Threshold below which singular values are considered zero
 *                      (e.g., -30 dB relative to strongest)
 * @return Effective rank (1 to min(N_rx, N_tx))
 * Complexity: O(N^3)
 */
size_t mimo_rank(const mimo_channel_matrix_t *mimo, double threshold_db);

/**
 * @brief Compute spatial correlation matrix from channel realizations
 * R_H = (1/K)*sum_k vec(H_k)*vec(H_k)^H (sample covariance)
 *
 * @param realizations Array of K channel realizations
 * @param num_realizations Number of channel snapshots K
 * @param corr_matrix Output correlation matrix, size (N_rx*N_tx)x(N_rx*N_tx)
 * @return 0 on success
 * Complexity: O(K*N^2)
 */
int mimo_spatial_correlation(const mimo_channel_matrix_t *realizations,
                              size_t num_realizations,
                              spatial_corr_matrix_t *corr_matrix);

/**
 * @brief Compute diversity order from channel matrix
 * For a Rayleigh MIMO channel, diversity order = N_rx * N_tx (full diversity
 * with space-time coding). For correlated channels, diversity is reduced.
 *
 * @param mimo MIMO channel matrix
 * @return Estimated diversity order (based on eigenvalue spread)
 * Complexity: O(N^3)
 */
double mimo_diversity_order(const mimo_channel_matrix_t *mimo);

/*============================================================================
 * L8: Massive MIMO and Advanced Channel Models
 *============================================================================*/

/**
 * @brief Generate massive MIMO channel with asymptotic orthogonality
 *
 * As N_tx grows large (>> N_rx), the channel columns become asymptotically
 * orthogonal: (1/N_tx)*H*H^H -> I_Nrx (channel hardening).
 *
 * @param mimo Pre-allocated MIMO channel matrix (N_rx << N_tx expected)
 * @return 0 on success
 * Complexity: O(N_rx * N_tx)
 */
int mimo_generate_massive_iid(mimo_channel_matrix_t *mimo);

/**
 * @brief Compute channel hardening metric
 * Measures how close (1/N_tx)*HH^H is to identity.
 * Metric = ||(1/N_tx)*HH^H - I||_F / ||I||_F
 * Smaller values indicate stronger channel hardening.
 *
 * @param mimo MIMO channel matrix
 * @return Hardening metric (0 = perfect hardening)
 * Complexity: O(N_rx^3)
 */
double mimo_channel_hardening_metric(const mimo_channel_matrix_t *mimo);

/**
 * @brief Generate 3GPP spatial channel model (3D)
 *
 * Models 3D MIMO channel with azimuth and elevation angular spreads,
 * per-cluster delays and powers following 3GPP TR 38.901 CDL model.
 *
 * @param mimo Pre-allocated MIMO channel matrix
 * @param num_clusters Number of scattering clusters
 * @param angular_spread_deg Azimuth angular spread (deg)
 * @param elevation_spread_deg Elevation angular spread (deg)
 * @param carrier_freq_hz Carrier frequency
 * @param ant_spacing_rx Rx antenna spacing in wavelengths
 * @param ant_spacing_tx Tx antenna spacing in wavelengths
 * @return 0 on success
 * Complexity: O(N_rx*N_tx*clusters)
 */
int mimo_generate_3gpp_3d(mimo_channel_matrix_t *mimo,
                           size_t num_clusters,
                           double angular_spread_deg,
                           double elevation_spread_deg,
                           double carrier_freq_hz,
                           double ant_spacing_rx,
                           double ant_spacing_tx);

#ifdef __cplusplus
}
#endif

#endif /* MIMO_CHANNEL_H */
