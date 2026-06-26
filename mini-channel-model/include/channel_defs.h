/**
 * @file channel_defs.h
 * @brief Core Definitions for Wireless Channel Modeling (L1)
 *
 * Defines fundamental data structures, enums, and constants for wireless
 * channel characterization. Covers all L1 definitions per SKILL.md:
 * path loss exponent, delay spread, Doppler shift, coherence bandwidth,
 * coherence time, angular spread, K-factor, shadow fading std-dev, MIMO rank.
 *
 * Reference: Molisch, "Wireless Communications", 2nd Ed, Ch. 6-7 (2011)
 * Reference: Proakis & Salehi, "Digital Communications", 5th Ed, Ch. 14 (2008)
 * Reference: Rappaport, "Wireless Communications: Principles and Practice", Ch. 4-5
 *
 * Course Mapping:
 *   MIT 6.450 - Digital Communications (wireless channel characterization)
 *   Stanford EE359 - Wireless Communications (fading, multipath)
 *   Berkeley EE123 - Digital Signal Processing (channel modeling)
 *   Georgia Tech ECE 6601 - Communications (wireless propagation)
 */

#ifndef CHANNEL_DEFS_H
#define CHANNEL_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * L1: Fundamental Constants for Wireless Channels
 *============================================================================*/

/** Speed of light in vacuum (m/s) - ITU-R P.525 */
#define CHANNEL_C0            299792458.0

/** Boltzmann constant (J/K) for thermal noise floor computation */
#define CHANNEL_K_BOLTZMANN   1.38064852e-23

/** Standard reference distance for far-field path loss (m) */
#define CHANNEL_D0_DEFAULT    1.0

/** Carrier frequencies for common wireless bands (Hz) */
#define CHANNEL_FC_700MHz      700e6
#define CHANNEL_FC_850MHz      850e6
#define CHANNEL_FC_900MHz      900e6
#define CHANNEL_FC_1800MHz    1800e6
#define CHANNEL_FC_1900MHz    1900e6
#define CHANNEL_FC_2100MHz    2100e6
#define CHANNEL_FC_2400MHz    2400e6
#define CHANNEL_FC_2600MHz    2600e6
#define CHANNEL_FC_3500MHz    3500e6      /**< 5G NR n78 */
#define CHANNEL_FC_5000MHz    5000e6
#define CHANNEL_FC_5800MHz    5800e6      /**< WiFi 5 GHz upper */
#define CHANNEL_FC_28GHz      28e9        /**< mmWave n257 */
#define CHANNEL_FC_39GHz      39e9        /**< mmWave n260 */
#define CHANNEL_FC_60GHz      60e9        /**< 802.11ad/ay */

/*============================================================================
 * L1: Fading Type Enumeration
 *
 * Small-scale fading types classified by:
 *   - Amplitude statistics (Rayleigh, Rician, Nakagami)
 *   - Frequency selectivity (flat vs frequency-selective)
 *   - Time variance (slow vs fast)
 *============================================================================*/

/** Amplitude fading distribution type */
typedef enum {
    FADING_NONE         = 0,  /**< No fading (AWGN only) */
    FADING_RAYLEIGH     = 1,  /**< NLOS, diffuse multipath, Rayleigh PDF */
    FADING_RICIAN       = 2,  /**< LOS + diffuse, Rician PDF (K-factor param) */
    FADING_NAKAGAMI_M   = 3,  /**< Generalized fading, Nakagami-m PDF (m param) */
    FADING_LOGNORMAL    = 4,  /**< Large-scale shadow fading */
    FADING_WEIBULL      = 5,  /**< Weibull fading for indoor/V2V channels */
    FADING_TWOWAVE_DIFFUSE = 6, /**< Two-wave with diffuse power (TWDP) */
    FADING_KAPPA_MU     = 7   /**< kappa-mu generalized fading */
} fading_type_t;

/** Frequency selectivity classification */
typedef enum {
    SELECTIVITY_FLAT          = 0,  /**< B_s << B_c: flat fading */
    SELECTIVITY_FREQ_SELECTIVE = 1,  /**< B_s > B_c: frequency-selective */
    SELECTIVITY_DOUBLY_SELECTIVE = 2 /**< Time + frequency selective */
} selectivity_type_t;

/** Time variance classification */
typedef enum {
    TIMEVAR_SLOW   = 0,  /**< T_c >> T_s: slow fading (coherence time >> symbol) */
    TIMEVAR_FAST   = 1   /**< T_c < T_s: fast fading */
} timevar_type_t;

/** Channel model family for 3GPP standardized models */
typedef enum {
    CHMODEL_TDL_A = 0,   /**< 3GPP Tapped Delay Line A (short delay) */
    CHMODEL_TDL_B = 1,   /**< 3GPP Tapped Delay Line B (medium delay) */
    CHMODEL_TDL_C = 2,   /**< 3GPP Tapped Delay Line C (long delay) */
    CHMODEL_TDL_D = 3,   /**< 3GPP TDL-D (very long delay) */
    CHMODEL_TDL_E = 4,   /**< 3GPP TDL-E (extreme delay) */
    CHMODEL_CDL_A = 5,   /**< 3GPP Clustered Delay Line A */
    CHMODEL_CDL_B = 6,   /**< 3GPP Clustered Delay Line B */
    CHMODEL_CDL_C = 7,   /**< 3GPP Clustered Delay Line C */
    CHMODEL_EPA    = 8,  /**< LTE Extended Pedestrian A */
    CHMODEL_EVA    = 9,  /**< LTE Extended Vehicular A */
    CHMODEL_ETU    = 10, /**< LTE Extended Typical Urban */
    CHMODEL_CUSTOM = 11  /**< User-defined channel model */
} channel_model_t;

/*============================================================================
 * L1: Path Loss Model Enumeration
 *
 * Each model characterized by validity range (frequency, distance, environment).
 * Reference: ITU-R P.1411, P.1238; Okumura-Hata (1968/1980); COST-231 (1999)
 *============================================================================*/

typedef enum {
    PATHLOSS_FREE_SPACE   = 0,  /**< Friis free-space equation (Friis, 1946) */
    PATHLOSS_TWO_RAY      = 1,  /**< Two-ray ground reflection model */
    PATHLOSS_LOG_DISTANCE = 2,  /**< Log-distance empirical model */
    PATHLOSS_OKUMURA_HATA = 3,  /**< Okumura-Hata for 150-1500 MHz (Hata 1980) */
    PATHLOSS_COST231_HATA = 4,  /**< COST-231 Hata for 1500-2000 MHz */
    PATHLOSS_WALFISCH_IKEGAMI = 5, /**< Walfisch-Ikegami (COST-231, microcell) */
    PATHLOSS_ITU_INDOOR   = 6,  /**< ITU-R P.1238 indoor propagation */
    PATHLOSS_3GPP_UMI     = 7,  /**< 3GPP TR 38.901 Urban Micro (UMi) */
    PATHLOSS_3GPP_UMA     = 8,  /**< 3GPP TR 38.901 Urban Macro (UMa) */
    PATHLOSS_3GPP_RMA     = 9,  /**< 3GPP TR 38.901 Rural Macro (RMa) */
    PATHLOSS_3GPP_INH     = 10, /**< 3GPP TR 38.901 Indoor Hotspot (InH) */
    PATHLOSS_WINNER_II    = 11, /**< WINNER II generic model */
    PATHLOSS_CUSTOM       = 12  /**< User-defined path loss model */
} pathloss_model_t;

/** Environment type for path loss model selection */
typedef enum {
    ENV_URBAN_MACRO       = 0,
    ENV_URBAN_MICRO       = 1,
    ENV_SUBURBAN          = 2,
    ENV_RURAL             = 3,
    ENV_INDOOR_OFFICE     = 4,
    ENV_INDOOR_FACTORY    = 5,
    ENV_INDOOR_HOTSPOT    = 6,
    ENV_URBAN_MACRO_5G    = 7,   /**< 5G NR with beamforming */
    ENV_VEHICULAR_HIGHWAY  = 8,
    ENV_VEHICULAR_URBAN   = 9
} environment_type_t;

/*============================================================================
 * L1: Core Data Structures
 *============================================================================*/

/**
 * @brief Complex tap weight for tapped delay line
 *
 * Each tap = attenuated, phase-shifted copy of transmitted signal.
 * The complex gain represents both magnitude fading and phase rotation.
 * Phase uniformly distributed over [0, 2*pi) for Rayleigh.
 */
typedef struct {
    double complex gain;        /**< Complex tap gain h = a*exp(j*theta) */
    double delay_ns;            /**< Excess delay (ns) relative to first arrival */
    double avg_power_db;        /**< Average tap power (dB), sum = 0 dB for normalized */
    double doppler_shift_hz;    /**< Doppler shift for this tap (Hz) */
} channel_tap_t;

/**
 * @brief Power Delay Profile (PDP)
 *
 * Describes the multipath intensity profile. Key metric:
 *   RMS delay spread = sqrt( (sum tau^2*P(tau))/(sum P(tau)) - (mean tau)^2 )
 *
 * Coherence bandwidth (50% correlation): B_c ~ 1/(5*delay_spread)
 * Coherence bandwidth (90% correlation): B_c ~ 1/(50*delay_spread)
 */
typedef struct {
    channel_tap_t *taps;         /**< Array of tap descriptors */
    size_t          num_taps;    /**< Number of multipath components */
    double          mean_delay_ns;     /**< Mean excess delay (1st moment) */
    double          rms_delay_ns;      /**< RMS delay spread (2nd central moment) */
    double          max_delay_ns;      /**< Maximum excess delay */
    double          coh_bw_50_hz;      /**< Coherence bandwidth (50% corr), Hz */
    double          coh_bw_90_hz;      /**< Coherence bandwidth (90% corr), Hz */
} power_delay_profile_t;

/**
 * @brief Channel impulse response (time-varying)
 *
 * h(tau, t) = sum_n a_n(t)*delta(tau - tau_n(t))*exp(j*theta_n(t))
 *
 * where a_n is amplitude, tau_n is delay, theta_n is phase of n-th multipath.
 * This is the fundamental LTV (linear time-varying) channel description.
 */
typedef struct {
    power_delay_profile_t pdp;           /**< Power delay profile */
    double                timestamp_s;   /**< Snapshot time (s) */
    double complex       *cir_samples;   /**< h(tau) sampled at uniform tau grid */
    size_t                num_samples;   /**< Number of CIR samples */
    double                sample_spacing_ns;  /**< tau grid spacing (ns) */
    double                carrier_freq_hz;    /**< Carrier frequency f_c (Hz) */
    double                bandwidth_hz;       /**< System bandwidth (Hz) */
} channel_impulse_response_t;

/**
 * @brief Fading channel parameters
 *
 * Encapsulates all parameters needed to characterize a fading channel.
 * These parameters map to the statistical models (L2-L4).
 */
typedef struct {
    fading_type_t       type;              /**< Amplitude distribution type */
    selectivity_type_t  selectivity;       /**< Frequency selectivity */
    timevar_type_t      time_variance;     /**< Time variance classification */
    double              k_factor_db;       /**< Rician K-factor = P_LOS/P_diffuse (dB) */
    double              m_parameter;       /**< Nakagami-m shape factor (m >= 0.5) */
    double              sigma_shadow_db;   /**< Shadow fading std dev (dB), typ 3-12 dB */
    double              doppler_spread_hz; /**< Maximum Doppler shift f_d = v*f_c/c */
    double              doppler_shift_hz;  /**< Mean Doppler shift (Hz) */
    double              coherence_time_s;  /**< Coherence time T_c ~ 0.423/f_d */
    double              num_paths;         /**< Number of independent multipath components */
    double              angular_spread_deg; /**< RMS angular spread (deg) */
} fading_params_t;

/**
 * @brief Path loss model parameters
 */
typedef struct {
    pathloss_model_t    model;              /**< Path loss model selection */
    environment_type_t  environment;        /**< Deployment environment */
    double              carrier_freq_hz;    /**< Carrier frequency f_c (Hz) */
    double              tx_height_m;        /**< Base station antenna height (m) */
    double              rx_height_m;        /**< Mobile station antenna height (m) */
    double              ref_distance_m;     /**< Reference distance d_0 for far-field (m) */
    double              ref_loss_db;        /**< Path loss at reference distance (dB) */
    double              path_loss_exponent; /**< Path loss exponent n (typ 2-5) */
    double              sigma_shadow_db;    /**< Shadow fading std dev (dB) */
    double              building_height_m;  /**< Average building height (m) */
    double              street_width_m;     /**< Street width for street canyon (m) */
    double              building_spacing_m; /**< Inter-building spacing (m) */
    double              street_angle_deg;   /**< Street orientation rel. to LOS (deg) */
} pathloss_params_t;

/**
 * @brief Spatial correlation matrix for MIMO
 *
 * For an N*M MIMO system, the full correlation matrix is N*M x N*M.
 * Under Kronecker separability assumption:
 *   R_full = kron(R_TX, R_RX)
 */
typedef struct {
    double complex *matrix;     /**< Correlation matrix, size rows*cols */
    size_t          rows;
    size_t          cols;
} spatial_corr_matrix_t;

/**
 * @brief MIMO channel matrix H (N_rx x N_tx)
 *
 * H = [h_11 h_12 ... h_1Ntx;
 *      h_21 h_22 ... h_2Ntx;
 *      ...
 *      h_Nrx1 ... h_NrxNtx]
 *
 * Each element h_ij is the complex channel gain from Tx antenna j to Rx antenna i.
 * Under Rayleigh i.i.d. model: h_ij ~ CN(0, 1)
 * Under Kronecker model: vec(H) = R^(1/2) * vec(H_iid)
 */
typedef struct {
    size_t        num_rx;    /**< Number of receive antennas */
    size_t        num_tx;    /**< Number of transmit antennas */
    double complex *h;       /**< Channel matrix elements, row-major, size N_rx * N_tx */
} mimo_channel_matrix_t;

/**
 * @brief Doppler spectrum parameters (Jakes/Clarke model)
 *
 * Jakes Doppler PSD for 2D isotropic scattering with vertical lambda/4 antenna:
 *   S(f) = 1 / (pi*f_d*sqrt(1 - (f/f_d)^2))    for |f| <= f_d
 *
 * where f_d = v*f_c/c is the maximum Doppler shift.
 * The autocorrelation is: R(tau) = sigma^2*J_0(2*pi*f_d*tau)  (Clarke model)
 */
typedef struct {
    double          max_doppler_hz;     /**< Maximum Doppler shift f_d (Hz) */
    double          velocity_ms;        /**< Mobile velocity (m/s) */
    double          carrier_freq_hz;    /**< Carrier frequency (Hz) */
    double          angle_of_arrival_deg; /**< Mean AoA for directional */
    size_t          num_sinusoids;      /**< Number of sinusoids for sum-of-sinusoids */
    double          psd_resolution_hz;  /**< PSD frequency resolution (Hz) */
} doppler_params_t;

/**
 * @brief Level crossing rate (LCR) and average fade duration (AFD) result
 *
 * LCR = rate at which envelope crosses a given threshold with positive slope.
 * For Rayleigh fading at threshold rho:
 *   N_rho = sqrt(2*pi)*f_d*rho*exp(-rho^2)      crossings per second
 *
 * AFD = average time below threshold:
 *   tau_rho = (exp(rho^2) - 1) / (sqrt(2*pi)*f_d*rho)
 */
typedef struct {
    double lcr_hz;              /**< Level crossing rate (crossings/s) */
    double afd_s;               /**< Average fade duration (seconds) */
    double threshold_linear;    /**< Normalized threshold amplitude rho */
    double threshold_db;        /**< Threshold in dB relative to RMS */
} lcr_afd_result_t;

/**
 * @brief Channel capacity result
 *
 * For SISO: C = B*log2(1 + SNR)
 * For MIMO: C = B*log2(det(I + (SNR/N_t)*HH^H))
 *
 * Units: bits per second (bps)
 */
typedef struct {
    double capacity_bps;          /**< Channel capacity (bps) */
    double spectral_efficiency;    /**< Spectral efficiency (bps/Hz) */
    size_t num_streams;           /**< Number of spatial streams (MIMO rank) */
    double *singular_values;       /**< Singular values of H (size min(N_rx,N_tx)) */
    size_t rank;                   /**< Effective rank of channel matrix */
} channel_capacity_t;

/**
 * @brief Channel state information (CSI) snapshot
 */
typedef struct {
    mimo_channel_matrix_t       mimo_h;           /**< MIMO channel matrix */
    channel_impulse_response_t  cir;              /**< Channel impulse response */
    double                      snr_db;           /**< Instantaneous SNR (dB) */
    double                      path_loss_db;     /**< Path loss (dB) */
    double                      rssi_dbm;         /**< Received signal strength (dBm) */
    lcr_afd_result_t            fading_stats;     /**< LCR and AFD */
    double                      eigenval_ratio;   /**< Condition number for spatial multiplexing */
} channel_state_info_t;

/*============================================================================
 * L4: Fundamental Theorem Data Structures
 *============================================================================*/

/**
 * @brief Shannon-Hartley capacity theorem parameters
 *
 * C = B*log2(1 + S/N)  [bps]
 *
 * @see channel_capacity_t for computed results
 */
typedef struct {
    double bandwidth_hz;   /**< Channel bandwidth B (Hz) */
    double signal_power_w; /**< Received signal power S (W) */
    double noise_power_w;  /**< Noise power N = k*T*B*F (W) */
    double snr_linear;     /**< Signal-to-noise ratio S/N (linear) */
    double snr_db;         /**< Signal-to-noise ratio (dB) */
} shannon_params_t;

/*============================================================================
 * API: Utility Functions
 *============================================================================*/

/**
 * @brief Compute free-space wavelength
 * @param freq_hz Carrier frequency (Hz)
 * @return Wavelength lambda = c/f (m)
 * Complexity: O(1)
 */
double channel_wavelength(double freq_hz);

/**
 * @brief Convert dB to linear
 * Complexity: O(1)
 */
double channel_db_to_linear(double db);

/**
 * @brief Convert linear to dB
 * Complexity: O(1)
 */
double channel_linear_to_db(double linear);

/**
 * @brief Compute receive power from transmit power and path loss
 * @param tx_power_dbm Transmit power (dBm)
 * @param path_loss_db Path loss (dB, positive)
 * @return Received power P_rx (dBm) = P_tx - PL
 * Complexity: O(1)
 */
double channel_rx_power_dbm(double tx_power_dbm, double path_loss_db);

/**
 * @brief Compute thermal noise power
 * @param bandwidth_hz Bandwidth (Hz)
 * @param temperature_k Temperature (K), typically 290 K room temp
 * @param noise_figure_db Receiver noise figure (dB)
 * @return Noise power N = k*T*B*F (dBm)
 * Complexity: O(1)
 */
double channel_noise_power_dbm(double bandwidth_hz, double temperature_k,
                               double noise_figure_db);

/**
 * @brief Compute maximum Doppler shift
 * @param velocity_ms Mobile velocity (m/s)
 * @param freq_hz Carrier frequency (Hz)
 * @return f_d = v*f_c / c (Hz)
 * Complexity: O(1)
 */
double channel_doppler_shift(double velocity_ms, double freq_hz);

/**
 * @brief Compute coherence time (50% correlation)
 * @param doppler_hz Maximum Doppler shift (Hz)
 * @return T_c ~ 0.423 / f_d (s)
 * Complexity: O(1)
 */
double channel_coherence_time(double doppler_hz);

/**
 * @brief Compute RMS delay spread from power delay profile
 * @param pdp Power delay profile
 * @return sigma_tau = sqrt( tau2_bar - (tau_bar)^2 ) (ns)
 * Complexity: O(N) where N = num_taps
 */
double channel_rms_delay_spread(const power_delay_profile_t *pdp);

/**
 * @brief Compute coherence bandwidth from RMS delay spread
 * @param rms_delay_ns RMS delay spread (ns)
 * @param correlation_level Correlation level (0.5 or 0.9)
 * @return B_c (Hz). For 0.5: B_c ~ 1/(5*sigma_tau), for 0.9: B_c ~ 1/(50*sigma_tau)
 * Complexity: O(1)
 */
double channel_coherence_bandwidth(double rms_delay_ns, double correlation_level);

/**
 * @brief Classify channel selectivity based on bandwidth and coherence BW
 * @param signal_bw_hz Signal bandwidth (Hz)
 * @param coherence_bw_hz Coherence bandwidth (Hz)
 * @return SELECTIVITY_FLAT if B_s << B_c, else SELECTIVITY_FREQ_SELECTIVE
 * Complexity: O(1)
 */
selectivity_type_t channel_classify_selectivity(double signal_bw_hz,
                                                 double coherence_bw_hz);

/**
 * @brief Classify time variance based on symbol duration and coherence time
 * @param symbol_duration_s Symbol period T_s (s)
 * @param coherence_time_s Coherence time T_c (s)
 * @return TIMEVAR_SLOW if T_c >> T_s, else TIMEVAR_FAST
 * Complexity: O(1)
 */
timevar_type_t channel_classify_timevar(double symbol_duration_s,
                                         double coherence_time_s);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_DEFS_H */
