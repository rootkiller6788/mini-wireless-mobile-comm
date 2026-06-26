/**
 * beamforming_precoder.h ? MIMO Precoding & Beamforming Algorithms
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: precoder_config, precoder_result, combine_type
 *   L2 Core Concepts: Transmit precoding, receive combining,
 *                     spatial multiplexing, power allocation
 *   L4 Fundamental Laws: Shannon capacity with precoding,
 *                        capacity scaling with antenna number
 *   L5 Algorithms: MRT, ZF, MMSE, SLNR, Block Diagonalization
 *   L6 Canonical Problems: Multi-user MIMO sum-rate maximization
 *   L8 Advanced Topics: Hybrid beamforming for mmWave
 *
 * Reference: Tse & Viswanath (2005) Fundamentals of Wireless Comm, Ch.10
 *            Bjornson et al. (2017) Massive MIMO Networks, Ch.4
 *            Heath et al. (2016) Foundations of MIMO Communication, Ch.7
 *            Sadek et al. (2007) "SLNR Preceding" IEEE TWC
 */

#ifndef BEAMFORMING_PRECODER_H
#define BEAMFORMING_PRECODER_H

#include "beamforming_types.h"

/* ================================================================
 * L1: Precoder Type Definitions
 * ================================================================ */

/** Precoder type enumeration.
 *  Maps to: different precoding strategies at the transmitter. */
typedef enum {
    PRECODER_MRT = 0,           /* Maximum Ratio Transmission       */
    PRECODER_ZF,                /* Zero-Forcing                      */
    PRECODER_MMSE,              /* Minimum Mean Square Error         */
    PRECODER_SLNR,              /* Signal-to-Leakage-plus-Noise Ratio*/
    PRECODER_BLOCK_DIAG,        /* Block Diagonalization (MU-MIMO)   */
    PRECODER_SVD,               /* SVD-based (optimal single-user)   */
    PRECODER_HYBRID             /* Hybrid analog-digital (mmWave)    */
} precoder_type;

/** Spatial multiplexing configuration.
 *  L2 Concept: Multiple data streams transmitted simultaneously
 *              in the same time-frequency resource.
 *  num_streams: number of independent data streams (<= min(N_tx, N_rx)).
 *  num_users: number of scheduled users (for MU-MIMO). */
typedef struct {
    size_t num_tx_antennas;     /* N_t: transmit antennas           */
    size_t num_rx_antennas;     /* N_r: receive antennas            */
    size_t num_streams;         /* N_s: spatial streams             */
    size_t num_users;           /* K: users (1 = SU-MIMO)           */
    double tx_power;            /* Total transmit power P_t (linear) */
    double noise_variance;      /* Noise power sigma^2              */
    precoder_type type;
} precoder_config;

/** Precoding result ? contains the precoding matrix W and metrics.
 *  W is N_tx x N_streams for SU-MIMO or N_tx x (K*N_s) for MU-MIMO. */
typedef struct {
    complex_matrix W;           /* Precoding matrix                 */
    double *stream_powers;      /* Power allocated per stream       */
    double total_power;         /* Total power used (<= P_t)        */
    double sum_rate_bps_hz;     /* Achievable sum-rate (bps/Hz)     */
    double *user_rates;         /* Per-user rate (bps/Hz)           */
    size_t num_streams;
    size_t num_users;
    precoder_type type_used;
} precoder_result;

/** Combining type for receiver.
 *  L2 Concept: Combining at the receiver to extract desired signals. */
typedef enum {
    COMBINE_MRC = 0,            /* Maximum Ratio Combining          */
    COMBINE_ZF,                 /* Zero-Forcing Combining           */
    COMBINE_MMSE,               /* MMSE Combining                   */
    COMBINE_ML                  /* Maximum Likelihood (optimum)     */
} combine_type;

/** Channel state information type.
 *  L2 Concept: What the transmitter knows about the channel. */
typedef enum {
    CSI_PERFECT = 0,            /* Perfect instantaneous CSI        */
    CSI_STATISTICAL,            /* Statistical CSI (covariance only) */
    CSI_PARTIAL,                /* Partial/quantized CSI            */
    CSI_NONE                    /* No CSI ? open-loop              */
} csi_type;

/* ================================================================
 * L5: Precoding Algorithms
 * ================================================================ */

/** MRT (Maximum Ratio Transmission) precoding.
 *  W_MRT = sqrt(P_t) * H^H / ||H||_F.
 *  L5 Algorithm: Matched filter in spatial domain.
 *  Maximizes received SNR for single-stream transmission.
 *  Optimal when: K=1, N_s=1 (single-user, single-stream).
 *  Complexity: O(N_t * N_r) ? just Hermitian + normalization.
 *  Reference: Lo (1999) "Maximum Ratio Transmission," IEEE TCOM. */
int precoder_mrt(const complex_matrix *H, precoder_result *result);

/** ZF (Zero-Forcing) precoding.
 *  W_ZF = sqrt(P_t/beta) * H^H (H H^H)^{-1}, beta = trace((H H^H)^{-1}).
 *  L5 Algorithm: Channel inversion.
 *  Completely eliminates inter-stream interference.
 *  Suboptimal at low SNR (noise enhancement).
 *  Complexity: O(N_t * N_r * min(N_t,N_r)) ? includes matrix inversion.
 *  Reference: Wiesel et al. (2008) "ZF Precoding," IEEE TSP. */
int precoder_zf(const complex_matrix *H, precoder_result *result);

/** MMSE (Minimum Mean Square Error) precoding.
 *  W_MMSE = sqrt(P_t/beta) * H^H (H H^H + (sigma^2/P_t) K I)^{-1}.
 *  L5 Algorithm: Regularized channel inversion.
 *  Balances interference suppression and noise enhancement.
 *  Tends to MRT at low SNR, tends to ZF at high SNR.
 *  Complexity: O(N_t * N_r * min(N_t,N_r)).
 *  Reference: Joham et al. (2005) "MMSE Precoding," IEEE TSP. */
int precoder_mmse(const complex_matrix *H, precoder_result *result);

/** SLNR (Signal-to-Leakage-plus-Noise Ratio) precoding.
 *  Maximizes SLNR_k = ||H_k w_k||^2 / (sum_{j!=k} ||H_j w_k||^2 + sigma^2).
 *  L5 Algorithm: Decouples MU-MIMO precoding into per-user maximizations.
 *  Each user's precoder is the dominant eigenvector of a generalized eigenproblem.
 *  Complexity: O(K * N_t^3) for K users.
 *  Reference: Sadek et al. (2007) IEEE TWC, vol.6, no.3. */
int precoder_slnr(const complex_matrix *H, precoder_result *result);

/** Block Diagonalization (BD) precoding for MU-MIMO.
 *  Forces W_j^H H_i = 0 for all i != j (zero inter-user interference).
 *  L5 Algorithm: Nullspace projection for each user.
 *  Each user's precoder lies in the nullspace of all other users' channels.
 *  Requires: N_tx >= sum of N_rx_i for all users.
 *  Complexity: O(K * N_t^3) ? SVD per user.
 *  Reference: Spencer et al. (2004) "BD for MU-MIMO," IEEE TSP. */
int precoder_block_diag(const complex_matrix *H, precoder_result *result);

/** SVD-based optimal single-user precoding.
 *  W_opt = V * sqrt(P_alloc), where H = U Sigma V^H and P_alloc = waterfilling.
 *  L5 Algorithm: Optimal SU-MIMO precoding (achieves capacity).
 *  Decouples MIMO channel into parallel SISO channels via SVD.
 *  Complexity: O(N_t * N_r * min(N_t,N_r)) dominated by SVD.
 *  Reference: Telatar (1999) "MIMO Capacity," ETT. */
int precoder_svd(const complex_matrix *H, precoder_result *result);

/** Hybrid beamforming (analog + digital) for mmWave.
 *  W_hybrid = W_RF * W_BB, where W_RF has constant-modulus constraint.
 *  L8 Advanced Topic: mmWave hybrid architecture with fewer RF chains than antennas.
 *  Uses OMP-based sparse precoding for RF beamformer design.
 *  Complexity: O(N_t * N_RF^2) per iteration.
 *  Reference: El Ayach et al. (2014) "Hybrid Precoding," IEEE TWC. */
int precoder_hybrid(const complex_matrix *H, size_t N_RF,
                    precoder_result *result);

/* ================================================================
 * L2: Receive Combining
 * ================================================================ */

/** MRC receiver: g_k = H_k h_k / ||H_k||_F (matched filter).
 *  L5 Algorithm: Spatial matched filter at receiver.
 *  Optimal for: single-user, noise-limited case. */
int combine_mrc(const complex_matrix *H, complex_vector *rx_weights);

/** MMSE receiver: g_mmse = (H H^H + sigma^2 I)^{-1} h_target.
 *  L5 Algorithm: MMSE combining ? optimal linear receiver.
 *  Balances interference suppression and noise. */
int combine_mmse(const complex_matrix *H, size_t target_stream,
                 double noise_var, complex_vector *rx_weights);

/** ZF receiver: g_zf = (H H^H)^{-1} h_target.
 *  L5 Algorithm: Interference nulling at receiver. */
int combine_zf(const complex_matrix *H, size_t target_stream,
               complex_vector *rx_weights);

/* ================================================================
 * L6: End-to-End MU-MIMO System
 * ================================================================ */

/** Multi-user MIMO sum-rate computation with given precoder.
 *  Computes rate of user k: R_k = log2 det(I + SINR_k).
 *  L6 Canonical Problem: MU-MIMO downlink sum-rate.
 *  Reference: Viswanath & Tse (2003) "Sum Rate of MU-MIMO," IEEE TIT.
 *  Complexity: O(K * min(N_r,N_t)^3). */
double mu_mimo_sum_rate(const complex_matrix *H, const precoder_result *prec,
                        double noise_variance);

/** SINR computation for stream s of user k.
 *  SINR_{k,s} = |g_k^H H_k w_{k,s}|^2 / (I_inter + I_intra + sigma^2).
 *  L2 Concept: Signal-to-Interference-plus-Noise Ratio.
 *  Reference: Bjornson et al. (2017) Massive MIMO Networks, Eq.4.12. */
double compute_sinr(const complex_matrix *H_channel,
                    const complex_vector *rx_combiner,
                    const complex_vector *tx_precoder,
                    size_t user_idx, double noise_var);

/** Initialize precoder configuration with typical values.
 *  Maps to: practical MIMO system setup. */
precoder_config precoder_config_init(size_t N_tx, size_t N_rx,
                                     size_t N_streams, size_t K,
                                     double P_t, double sigma2,
                                     precoder_type type);

/** Allocate precoder result for given dimensions. */
precoder_result precoder_result_alloc(size_t N_tx, size_t N_streams,
                                      size_t K);

/** Free precoder result internal allocations. */
void precoder_result_free(precoder_result *result);

#endif /* BEAMFORMING_PRECODER_H */
