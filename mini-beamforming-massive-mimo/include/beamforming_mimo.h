/**
 * beamforming_mimo.h ? MIMO Channel Models & Capacity Theory
 *
 * Nine-Level Knowledge Mapping:
 *   L1 Definitions: mimo_channel, capacity_result, waterfilling_result
 *   L2 Core Concepts: Spatial multiplexing, spatial diversity, channel hardening,
 *                     favorable propagation, degrees of freedom, channel rank
 *   L3 Mathematical Structures: SVD-based eigenmode decomposition,
 *                               Kronecker channel model, Weichselberger model
 *   L4 Fundamental Laws: Shannon capacity for MIMO, capacity scaling laws,
 *                        deterministic equivalent for large MIMO,
 *                        waterfilling power allocation optimality
 *   L6 Canonical Problems: MIMO capacity vs SNR, SU-MIMO vs MU-MIMO,
 *                          massive MIMO asymptotic sum-rate
 *   L8 Advanced Topics: Channel hardening, favorable propagation condition
 *
 * Reference: Telatar (1999) "MIMO Capacity," European Trans. Telecom
 *            Foschini & Gans (1998) "MIMO Capacity," Wireless Personal Comm
 *            Goldsmith et al. (2003) "MIMO Capacity," IEEE TIT
 *            Marzetta (2010) "Massive MIMO," IEEE TWC
 *            Bjornson, Hoydis, Sanguinetti (2017) Massive MIMO Networks
 */

#ifndef BEAMFORMING_MIMO_H
#define BEAMFORMING_MIMO_H

#include "beamforming_types.h"
#include "beamforming_array.h"

/* ================================================================
 * L1: MIMO Channel & Capacity Types
 * ================================================================ */

/** MIMO channel model type.
 *  L2 Concept: Different channel models for different propagation scenarios. */
typedef enum {
    CHANNEL_RAYLEIGH_IID = 0,   /* i.i.d. Rayleigh fading (rich scattering) */
    CHANNEL_RAYLEIGH_CORR,      /* Correlated Rayleigh (Kronecker model)    */
    CHANNEL_RICIAN,             /* Rician fading (LOS + scattering)          */
    CHANNEL_MIMILLIMETER,       /* mmWave clustered channel                  */
    CHANNEL_LOS                 /* Pure line-of-sight (rank-1)               */
} mimo_channel_type;

/** MIMO channel realization.
 *  H: M_r x M_t complex channel matrix.
 *  channel_type: statistical model used.
 *  is_normalized: whether E[||H||_F^2] = M_r * M_t. */
typedef struct {
    complex_matrix H;           /* Channel matrix (M_r x M_t)      */
    size_t num_rx;              /* Number of receive antennas      */
    size_t num_tx;              /* Number of transmit antennas      */
    mimo_channel_type type;
    int is_normalized;          /* 1 = normalized to unit gain     */
} mimo_channel;

/** MIMO capacity result ? ergodic (average) capacity.
 *  L4 Fundamental Law: Shannon MIMO capacity. */
typedef struct {
    double capacity_bps_hz;     /* C = log2 det(I + SNR/M_t H H^H) */
    double *per_stream_rate;    /* Waterfilled per-stream rates    */
    double *water_level;        /* Waterfilling solution           */
    size_t num_active_streams;  /* Streams with non-zero power     */
    double *singular_values;    /* Channel singular values sigma_i */
    size_t rank;                /* Channel matrix rank             */
} mimo_capacity;

/** Waterfilling power allocation result.
 *  L4 Fundamental Law: Optimal power allocation = max(0, mu - 1/SNR_i).
 *  Reference: Cover & Thomas (2006) Elements of Information Theory, ?9.4. */
typedef struct {
    double *power_allocation;   /* P_i allocated to stream i      */
    double water_level;         /* mu ? water level               */
    size_t num_active;          /* Number of active streams       */
    double total_power;         /* Sum of allocated powers        */
} waterfilling_result;

/** Deterministic equivalent for massive MIMO capacity.
 *  L8 Advanced Topic: Asymptotic analysis M,N -> infinity, M/N fixed.
 *  Maps to: capacity with reduced pilot overhead analysis. */
typedef struct {
    double asymptotic_rate;     /* Large-system limit capacity    */
    double hardening_metric;    /* var(||h_k||^2/M) ? channel hardening */
    double favorable_prop_metric; /* Inner product ratio ? orthogonal channels */
} massive_mimo_metrics;

/** Realistic massive MIMO channel configuration.
 *  L7 Application: 3GPP-style channel setup. */
typedef struct {
    size_t num_bs_antennas;     /* Base station antennas M        */
    size_t num_users;           /* Single-antenna users K         */
    size_t num_pilots;          /* Uplink pilot symbols           */
    double cell_radius;         /* Cell radius (meters)           */
    double coherence_bandwidth; /* B_c (Hz)                       */
    double coherence_time;      /* T_c (seconds)                  */
    double carrier_freq_hz;     /* f_c (Hz)                       */
    double path_loss_exponent;  /* Path loss exponent alpha       */
    double shadow_fading_db;    /* Lognormal shadowing std (dB)   */
} massive_mimo_config;

/* ================================================================
 * L4: MIMO Capacity Computation
 * ================================================================ */

/** MIMO ergodic capacity: C = E[log2 det(I + (SNR/N_t) H H^H)].
 *  Uses SVD: C = sum_i log2(1 + SNR*sigma_i^2/N_t).
 *  L4 Fundamental Law: Shannon capacity extended to MIMO.
 *  This is the capacity with equal power allocation (no CSIT).
 *  Reference: Telatar (1999), Foschini & Gans (1998).
 *  Complexity: O(M_t * M_r * min(M_t, M_r)) ? SVD dominates. */
int mimo_capacity_openloop(const mimo_channel *channel, double snr_linear,
                           mimo_capacity *result);

/** MIMO capacity with waterfilling (perfect CSIT).
 *  C = max_{P_i} sum_i log2(1 + P_i * SNR/N_t * sigma_i^2)
 *  subject to sum P_i = 1, P_i >= 0.
 *  L4 Fundamental Law: Waterfilling is capacity-achieving with CSIT.
 *  The solution uses KKT conditions: P_i = (mu - N_t/(SNR*sigma_i^2))^+.
 *  Reference: Goldsmith (2005) Wireless Communications, ?7.1.
 *  Complexity: O(min(M_t, M_r) * log(min(M_t, M_r))) ? sorting + bisection. */
int mimo_capacity_waterfilling(const mimo_channel *channel, double snr_linear,
                               mimo_capacity *result);

/** Waterfilling power allocation algorithm.
 *  Algorithm: Find mu (water level) such that sum (mu - 1/gamma_i)^+ = 1.
 *  Uses bisection on mu.
 *  L5 Algorithm: Classic waterfilling.
 *  Complexity: O(N log N + N log(1/eps)) for N streams with tolerance eps. */
int waterfilling_compute(const double *snr_per_stream, size_t num_streams,
                         double total_power, waterfilling_result *result);

/** Single-user MIMO capacity with finite block length.
 *  C = sum log2(1 + SNR*sigma_i^2/N_t) - sqrt(V/(n)) Q^{-1}(epsilon).
 *  L8 Advanced Topic: Finite blocklength capacity (Polyanskiy 2010).
 *  Complexity: O(min(M,N)). */
double mimo_capacity_finite_blocklen(const mimo_channel *channel,
                                     double snr_linear,
                                     size_t block_length,
                                     double error_prob);

/* ================================================================
 * L2: Channel Generation
 * ================================================================ */

/** Generate i.i.d. Rayleigh fading MIMO channel.
 *  H_ij ~ CN(0, 1) (complex Gaussian, unit variance).
 *  Most common channel model for rich scattering environments.
 *  L2 Concept: Uncorrelated Rayleigh fading = worst-case spatial correlation.
 *  Complexity: O(M_r * M_t). */
int channel_rayleigh_iid(size_t M_r, size_t M_t, mimo_channel *channel);

/** Generate correlated Rayleigh MIMO channel (Kronecker model).
 *  H = R_r^{1/2} H_iid R_t^{1/2}, where R_r/R_t are correlation matrices.
 *  L2 Concept: Spatial correlation degrades capacity but realistic.
 *  Used by: 3GPP SCM, WINNER II channel models.
 *  Reference: Kermoal et al. (2002) "Kronecker Model," IEEE JSAC. */
int channel_rayleigh_corr(const complex_matrix *R_rx,
                          const complex_matrix *R_tx,
                          mimo_channel *channel);

/** Generate Rician fading MIMO channel.
 *  H = sqrt(K/(K+1)) H_LOS + sqrt(1/(K+1)) H_Rayleigh.
 *  K = Rice factor (ratio of LOS to scattered power).
 *  L2 Concept: LOS path + diffuse multipath.
 *  Used for: drone-to-BS, fixed wireless access, satellite.
 *  Complexity: O(M_r * M_t). */
int channel_rician(const complex_matrix *H_los, double rice_factor,
                   mimo_channel *channel);

/** Generate mmWave clustered channel (Saleh-Valenzuela model).
 *  H = sum_{cl=1}^{N_cl} sum_{ray=1}^{N_ray} alpha_{cl,ray}
 *      a_r(phi_r,theta_r) a_t^H(phi_t,theta_t).
 *  L8 Advanced Topic: Sparse angular-domain MIMO channel.
 *  Each cluster has angular spread; limited number of dominant paths.
 *  Complexity: O(N_cl * N_ray * M_r * M_t). */
int channel_mmwave_clustered(size_t M_r, size_t M_t,
                             size_t num_clusters, size_t num_rays_per_cluster,
                             mimo_channel *channel);

/** Normalize channel matrix: E[||H||_F^2] = M_r * M_t.
 *  Ensures consistent SNR definition across different channel types. */
void channel_normalize(mimo_channel *channel);

/** Free MIMO channel internal allocations. */
void channel_free(mimo_channel *channel);

/* ================================================================
 * L8: Massive MIMO Specific Concepts
 * ================================================================ */

/** Channel hardening metric: measures how deterministic the effective
 *  channel gain becomes as M grows.
 *  hardening = var(||h_k||^2) / E[||h_k||^2]^2.
 *  L8 Advanced Topic: As M -> infinity, ||h_k||^2/M -> 1 almost surely.
 *  Enables: simple power control, low-overhead CSI acquisition.
 *  Reference: Ngo et al. (2014) "Massive MIMO Fundamentals," IEEE JSAC. */
double channel_hardening_metric(const complex_matrix *H);

/** Favorable propagation condition check.
 *  (h_i^H h_j)/(M) -> 0 as M -> infinity for i != j (i.i.d. Rayleigh).
 *  L8 Advanced Topic: Orthogonal channels enable simple linear processing.
 *  Used by: MRT/ZF asymptotic optimality in massive MIMO. */
int check_favorable_propagation(const complex_matrix *H, double threshold);

/** Massive MIMO uplink achievable rate with MRC.
 *  R_k^{MRC} = log2(1 + M*SNR_k / (1 + sum_{j!=k} SNR_j)).
 *  L8 Advanced Topic: Asymptotic rate with simple linear receiver.
 *  Shows: interference vanishes as M -> infinity with MRC. */
double massive_mimo_ul_rate_mrc(const complex_matrix *H,
                                const double *snr_per_user,
                                size_t user_idx, size_t M);

/** Massive MIMO downlink sum-rate with ZF precoding.
 *  L8 Advanced Topic: ZF achieves near-optimal performance with massive arrays. */
double massive_mimo_dl_sum_rate_zf(const complex_matrix *H,
                                   const double *snr_per_user,
                                   size_t K, size_t M);

/** Pilot contamination analysis.
 *  L8 Advanced Topic: Reuse of pilots across cells limits capacity with infinite M.
 *  The asymptotic SINR with pilot contamination is limited by inter-cell interference. */
double pilot_contamination_limit(double tau_p, double tau_c,
                                 double *large_scale_fading,
                                 size_t num_cells, size_t num_users);

/** Allocate MIMO capacity result. */
mimo_capacity mimo_capacity_alloc(size_t rank);

/** Free MIMO capacity result. */
void mimo_capacity_free(mimo_capacity *cap);

/** Allocate waterfilling result. */
waterfilling_result waterfilling_alloc(size_t num_streams);

/** Free waterfilling result. */
void waterfilling_free(waterfilling_result *wf);

#endif /* BEAMFORMING_MIMO_H */
