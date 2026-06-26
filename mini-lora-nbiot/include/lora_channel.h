/**
 * @file lora_channel.h
 * @brief LoRa/NB-IoT Channel Models -- Fading, Interference, Multi-path
 *
 * Knowledge Coverage:
 *   L2 -- Multi-path fading, Doppler shift, coherence bandwidth/time
 *   L3 -- Rayleigh/Rician fading models, Jakes Doppler spectrum
 *   L5 -- AWGN generation, channel simulation
 *   L8 -- Multi-SF interference, capture effect
 *
 * References:
 *   - Molisch, "Wireless Communications" (2011), Ch. 5-8
 *   - Jakes, "Microwave Mobile Communications" (1974)
 *   - 3GPP TR 36.873: 3D channel model
 *
 * Curriculum Mapping:
 *   - Stanford EE359: Fading and diversity
 *   - MIT 6.450: Channel models and capacity
 *   - Berkeley EE117: EM wave propagation
 *
 * @license MIT
 */

#ifndef LORA_CHANNEL_H
#define LORA_CHANNEL_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   L2: Core Concepts -- Fading Channel Types
   ============================================================================ */

/**
 * Fading channel model types
 *
 * AWGN:      Additive White Gaussian Noise only
 * Rayleigh:  No line-of-sight, magnitude follows Rayleigh distribution
 * Rician:    Line-of-sight + scattered components, K-factor determines ratio
 * Nakagami:  Generalized fading with m-parameter (more flexible)
 */
typedef enum {
    CHANNEL_AWGN     = 0,  /**< AWGN only (no fading) */
    CHANNEL_RAYLEIGH = 1,  /**< Rayleigh fading (no LOS) */
    CHANNEL_RICIAN   = 2,  /**< Rician fading (LOS + scatter) */
    CHANNEL_NAKAGAMI = 3,  /**< Nakagami-m fading */
} fading_channel_type_t;

/**
 * Channel state parameters
 *
 * L2 — Coherence concepts:
 *   Coherence bandwidth B_c ≈ 1/(2π*τ_rms): frequencies within B_c
 *     experience correlated fading. For τ_rms = 1us, B_c ≈ 160 kHz.
 *     LoRa BW125 (125 kHz) < B_c → flat fading (all chips correlated).
 *
 *   Coherence time T_c ≈ 0.423/f_d: time within which channel is ~constant.
 *     For v = 30 m/s (108 km/h), f_c = 868 MHz:
 *       f_d = v*f_c/c = 30*868e6/3e8 = 86.8 Hz
 *       T_c ≈ 0.423/86.8 = 4.9 ms
 *     LoRa symbol at SF12/BW125: T_sym = 32.8 ms >> T_c → fast fading!
 *     But SF7/BW125: T_sym = 1.0 ms < T_c → slow fading.
 *
 * Doppler spread f_d = v * f_c / c
 *
 *   where:
 *     v = relative velocity (m/s)
 *     f_c = carrier frequency (Hz)
 *     c = speed of light (3e8 m/s)
 */
typedef struct {
    fading_channel_type_t type;       /**< Fading type */
    double   rician_k_db;             /**< Rician K factor in dB (LOS/scatter power ratio) */
    double   nakagami_m;              /**< Nakagami shape parameter (m>=0.5) */
    double   path_loss_db;            /**< Path loss to apply */
    double   snr_db;                  /**< Target SNR at receiver */
    double   doppler_hz;              /**< Maximum Doppler frequency */
    double   carrier_freq_hz;         /**< Carrier frequency */
    double   sample_rate_hz;          /**< System sample rate */
    double   noise_figure_db;         /**< Receiver noise figure */

    /* L3 — Multi-path parameters */
    uint16_t num_taps;                /**< Number of multi-path taps */
    double   tap_delays_s[12];        /**< Tap delays in seconds */
    double   tap_powers_db[12];       /**< Tap average powers in dB */
    double   tap_k_factors_db[12];    /**< Per-tap Rician K-factors */

    /* Internal state */
    double   complex tap_states[12];  /**< Current tap gains */
    uint64_t sample_count;            /**< Total samples processed */
} channel_state_t;

/* ============================================================================
   L3: Mathematical Structures -- Fading Statistics
   ============================================================================ */

/**
 * Rayleigh fading PDF:
 *   f_R(r) = (r/σ^2) * exp(-r^2 / 2σ^2),  r >= 0
 *
 * where σ^2 = average power of scattered component.
 * Mean = σ * sqrt(pi/2), Variance = (2 - pi/2)*σ^2
 */
typedef struct {
    double sigma_sq;        /**< Average power of scattered component */
    double mean;            /**< Expected magnitude */
    double variance;        /**< Magnitude variance */
} rayleigh_fading_stats_t;

/**
 * Rician fading PDF:
 *   f_R(r) = (r/σ^2) * exp(-(r^2 + A^2) / 2σ^2) * I_0(r*A / σ^2)
 *
 * where A = LOS amplitude, σ^2 = scattered power.
 * K = A^2 / (2σ^2) = K-factor (ratio of LOS to scattered power).
 *
 * K = 0 → Rayleigh, K → ∞ → no fading (pure LOS).
 */
typedef struct {
    double k_linear;        /**< K-factor (linear, A^2 / 2σ^2) */
    double k_db;            /**< K-factor in dB */
    double sigma_sq;        /**< Scattered component power */
    double a_sq;            /**< LOS power = 2K*sigma^2 */
} rician_fading_stats_t;

/**
 * Jakes Doppler spectrum model
 *
 * The classic U-shaped spectrum for isotropic scattering with
 * uniformly distributed angles of arrival:
 *
 *   S(f) = 1 / (pi * f_d * sqrt(1 - (f/f_d)^2)),  |f| < f_d
 *
 * This models the frequency spreading due to motion,
 * where f_d is the maximum Doppler shift.
 */

/* ============================================================================
   L5: Algorithms -- Channel Simulation
   ============================================================================ */

/**
 * Initialize channel state
 *
 * @param state  Channel state to initialize
 * @param type   Fading type
 * @param snr_db Target SNR in dB
 * @param sample_rate Sample rate in Hz
 * @param doppler_hz Maximum Doppler shift (0 = static/no fading)
 */
void channel_init(channel_state_t *state,
                   fading_channel_type_t type,
                   double snr_db,
                   double sample_rate,
                   double doppler_hz);

/**
 * Generate complex AWGN sample
 *
 * Noise variance sigma^2 = N_0/2 per dimension (I/Q).
 * For unity signal power at given SNR:
 *   sigma^2 = 10^(-SNR_dB / 10) / 2
 *
 * Uses Box-Muller transform on uniform random values.
 *
 * @param sigma  Standard deviation per dimension
 * @return Complex noise sample
 */
double complex awgn_sample(double sigma);

/**
 * Apply channel to a single complex sample
 *
 * Applies path loss, fading, and AWGN to an input sample.
 * The channel state is updated for time-varying fading simulation.
 *
 * r[n] = h[n] * s[n] + w[n]
 *
 * where h[n] is the fading gain and w[n] is AWGN.
 *
 * @param state    Channel state (updated)
 * @param tx_sample Input (transmitted) sample
 * @return Received sample after channel effects
 */
double complex channel_apply(channel_state_t *state, double complex tx_sample);

/**
 * Apply channel to a block of complex samples
 *
 * @param state      Channel state (updated)
 * @param tx         Input samples
 * @param rx         Output received samples
 * @param num_samples Number of samples
 */
void channel_apply_block(channel_state_t *state,
                          const double complex *tx,
                          double complex *rx,
                          size_t num_samples);

/**
 * Generate Rayleigh fading tap using Jakes method
 *
 * Sum-of-sinusoids method for generating correlated Rayleigh fading:
 *
 * h(t) = sum_{n=1}^{N0} [cos(beta_n) + j*sin(beta_n)] * cos(2*pi*f_n*t + theta_n)
 *
 * where:
 *   N0 = number of oscillators (typically 8-34)
 *   beta_n = pi * n / N0
 *   f_n = f_d * cos(2*pi*n / N)
 *   theta_n = random initial phase
 *
 * @param state       Channel state
 * @param tap_index   Which tap to update
 * @param t           Current time in seconds
 * @return Complex fading coefficient for this tap
 */
double complex rayleigh_fading_coeff(const channel_state_t *state,
                                      uint16_t tap_index,
                                      double t);

/* ============================================================================
   L8: Advanced Topics -- Multi-SF Interference and Capture Effect
   ============================================================================ */

/**
 * Multi-SF interference analysis for LoRa
 *
 * In LoRaWAN, multiple devices can transmit simultaneously using
 * different spreading factors. The orthogonality is not perfect
 * due to imperfect time-frequency alignment.
 *
 * Inter-SF interference power:
 *   I_k = sum_{j!=k} P_j * rho(SF_j, SF_k)
 *
 * where rho(SF_i, SF_j) is the cross-correlation between SF_i and SF_j.
 *
 * Empirical cross-SF isolation (from Croce et al., IEEE TWC 2018):
 *   SF7-SF8:  ~8 dB isolation
 *   SF7-SF12: ~16 dB isolation
 *   SF11-SF12: ~5 dB isolation (close SFs interfere more)
 *
 * Capture effect: the strongest signal that meets the SNR threshold
 * is successfully decoded, even with weaker interfering signals.
 *
 * @param desired_sf       Desired signal spreading factor
 * @param desired_power_db Desired signal power in dBm
 * @param interfering_sf   Array of interfering signal SFs
 * @param interfering_power_db Array of interfering signal powers
 * @param num_interferers  Number of interfering signals
 * @return Effective SINR in dB after accounting for inter-SF interference
 */
double lora_multi_sf_sinr(uint8_t desired_sf,
                           double desired_power_db,
                           const uint8_t *interfering_sf,
                           const double *interfering_power_db,
                           uint16_t num_interferers);

/**
 * Cross-SF isolation in dB
 *
 * Returns the attenuation of an interfering signal with SF_interf
 * relative to the desired spreading factor SF_desired.
 *
 * Based on measured data from multiple research papers.
 *
 * @param sf_desired  Desired signal SF
 * @param sf_interf   Interfering signal SF
 * @return Interference isolation in dB
 */
double lora_cross_sf_isolation_db(uint8_t sf_desired, uint8_t sf_interf);

/**
 * LoRa capture effect probability
 *
 * Probability that the desired packet is successfully received
 * given N interfering packets arriving simultaneously.
 *
 * Simplified model: capture occurs if desired signal power >
 * sum of interference power multiplied by capture ratio.
 *
 * @param desired_power_db  Desired signal power in dBm
 * @param interferer_power_db Array of interferer powers
 * @param num_interferers   Number of interferers
 * @param capture_ratio_db  Capture ratio threshold (typically 6 dB)
 * @return Capture probability [0, 1]
 */
double lora_capture_probability(double desired_power_db,
                                 const double *interferer_power_db,
                                 uint16_t num_interferers,
                                 double capture_ratio_db);

/**
 * Compute interference-limited capacity for LoRaWAN gateway
 *
 * Given N active devices with known SF distribution, compute
 * the maximum throughput the gateway can sustain.
 *
 * Gateway throughput model:
 *   T = sum_{sf} T_single(sf) * N_sf * (1 - P_collision(N, duty_cycle))
 *
 * @param num_devices        Total number of devices
 * @param sf_distribution    Array of length 6 (SF7-SF12), fraction of devices per SF
 * @param packets_per_hour   Average packet rate per device
 * @param packet_payload_bytes Average payload size
 * @return Gateway throughput in bits/second (average)
 */
double lora_gateway_throughput_bps(uint32_t num_devices,
                                    const double *sf_distribution,
                                    double packets_per_hour,
                                    uint16_t packet_payload_bytes);

/**
 * NB-IoT inter-cell interference model
 *
 * SINR = P_serving / (I_intercell + N)
 *
 * where I_intercell = sum of received powers from neighboring cells.
 * Inter-cell interference dominates in dense NB-IoT deployments.
 *
 * @param serving_rsrp_dbm   Serving cell RSRP in dBm
 * @param neighbor_rsrp_dbm  Array of neighbor cell RSRPs
 * @param num_neighbors      Number of neighboring cells
 * @param noise_figure_db    UE noise figure
 * @return Estimated SINR in dB
 */
double nbiot_intercell_sinr(double serving_rsrp_dbm,
                             const double *neighbor_rsrp_dbm,
                             uint16_t num_neighbors,
                             double noise_figure_db);

#ifdef __cplusplus
}
#endif

#endif /* LORA_CHANNEL_H */
