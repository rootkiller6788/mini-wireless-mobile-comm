/**
 * nr_phy_channel.h — 5G NR Channel Models & Estimation
 *
 * Knowledge Coverage:
 *   L1 Definitions: TDL/CDL channel model parameters, delay spread, Doppler
 *   L2 Core Concepts: Multipath fading, channel estimation (DMRS-based)
 *   L3 Math Structures: Channel matrix H, correlation matrices
 *   L4 Fundamental Laws: Friis transmission, Doppler shift equation
 *   L5 Algorithms: LS/MMSE channel estimation, TDL-A/B/C tap generation
 *   L6 Canonical Problems: 5G channel model per 3GPP TR 38.901
 *   L8 Advanced: Time-varying channel, MIMO channel correlation
 *
 * Course: Stanford EE359, MIT 6.450, Berkeley EE117
 * Ref: 3GPP TR 38.901 v17.0.0, 3GPP TS 38.211 Section 7.4.1.2
 */

#ifndef NR_PHY_CHANNEL_H
#define NR_PHY_CHANNEL_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TDL channel model profiles (3GPP TR 38.901 Table 7.7.2) */
#define NR_TDL_MAX_TAPS         24
#define NR_CDL_MAX_TAPS         24
#define NR_CDL_MAX_RAYS         20

/** Single multipath tap */
typedef struct {
    double  delay_ns;            /* Relative delay */
    double  power_db;            /* Average power (dB) */
    double  fading_type;         /* 0 = Rayleigh, 1 = Rice, 2 = AWGN */
    double  k_factor_db;         /* Rice K factor (dB), only for Rice */
} nr_tap_t;

/** TDL channel model (Tapped Delay Line) */
typedef struct {
    int         num_taps;
    nr_tap_t    taps[NR_TDL_MAX_TAPS];
    double      delay_spread_ns;    /* RMS delay spread */
    double      doppler_shift_hz;   /* Max Doppler shift */
    double      carrier_freq_ghz;
    double      ue_speed_kmh;       /* UE velocity */
    char        profile;            /* 'A', 'B', 'C', 'D', 'E' */
    int         is_los;             /* 1 = LOS (Rician), 0 = NLOS (Rayleigh) */
} nr_tdl_channel_t;

/** One ray of a CDL cluster */
typedef struct {
    double  delay_ns;
    double  power_db;
    double  aoa_az_deg;         /* Angle of arrival azimuth */
    double  aoa_zen_deg;        /* Angle of arrival zenith */
    double  aod_az_deg;         /* Angle of departure azimuth */
    double  aod_zen_deg;        /* Angle of departure zenith */
    double  xpr_db;             /* Cross-polarization ratio */
} nr_cdl_ray_t;

/** CDL cluster */
typedef struct {
    int         num_rays;
    nr_cdl_ray_t rays[NR_CDL_MAX_RAYS];
    double      cluster_delay_ns;
    double      cluster_power_db;
    double      cluster_asa_deg;    /* Azimuth spread of arrival */
    double      cluster_asa_zen_deg;
    double      cluster_asd_deg;
    double      cluster_asd_zen_deg;
} nr_cdl_cluster_t;

/** CDL channel model (Clustered Delay Line) */
typedef struct {
    int             num_clusters;
    nr_cdl_cluster_t clusters[NR_CDL_MAX_TAPS];
    double          delay_spread_ns;
    double          doppler_shift_hz;
    double          carrier_freq_ghz;
    double          ue_speed_kmh;
    char            profile;    /* 'A', 'B', 'C', 'D', 'E' */
    int             is_los;
    int             num_tx_ant;
    int             num_rx_ant;
} nr_cdl_channel_t;

/** Channel estimate for one RE */
typedef struct {
    nr_complex_t h;             /* Channel coefficient */
    double       noise_var;     /* Noise variance estimate */
    double       sinr_db;       /* Estimated SINR */
} nr_chan_est_t;

/** Channel estimation context (DMRS-based) */
typedef struct {
    int             num_sc;             /* Number of subcarriers */
    int             num_sym;            /* Number of symbols */
    nr_chan_est_t  *estimates;          /* RE grid of estimates */
    int             dmrs_sc_spacing;    /* DMRS subcarrier spacing */
    int             dmrs_sym_spacing;   /* DMRS symbol spacing */
    double          noise_power;        /* Estimated noise power */
} nr_chan_est_ctx_t;

/** Path loss model type */
typedef enum {
    NR_PATHLOSS_FREE_SPACE = 0,
    NR_PATHLOSS_UMA        = 1,  /* Urban Macro (TR 38.901) */
    NR_PATHLOSS_UMI        = 2,  /* Urban Micro */
    NR_PATHLOSS_RMA        = 3,  /* Rural Macro */
    NR_PATHLOSS_INDOOR     = 4
} nr_pathloss_model_t;

/* ============================================================================
 * L5: TDL Channel Generation
 * ============================================================================ */

/**
 * nr_tdl_init — Initialize TDL channel model
 *
 * 3GPP TR 38.901 Section 7.7.2 defines five TDL profiles:
 *   A: short delay, low spread (30 ns)
 *   B: medium delay (100 ns)
 *   C: long delay (300 ns)
 *   D: very long delay (1000 ns)
 *   E: ultra long delay (3000 ns)
 *
 * L4 (Friis + Doppler): f_d = (v * f_c) / c
 * Carrier at 3.5 GHz with UE at 120 km/h → f_d ≈ 389 Hz.
 *
 * @param chan        Channel output
 * @param profile     'A' through 'E'
 * @param ds_ns       Desired delay spread (0 = default)
 * @param fc_ghz      Carrier frequency (GHz)
 * @param speed_kmh   UE speed (km/h)
 * @param is_los      LOS/NLOS
 */
void nr_tdl_init(nr_tdl_channel_t *chan, char profile,
                  double ds_ns, double fc_ghz,
                  double speed_kmh, int is_los);

/**
 * nr_tdl_generate — Generate time-domain channel coefficients
 *
 * Generates complex channel taps with Jakes' Doppler spectrum.
 * Each tap evolves independently as a Rayleigh (or Rician) process.
 *
 * L5 (Jakes 1974): The Doppler power spectrum for isotropic scattering
 * is S(f) = 1 / (pi * f_d * sqrt(1 - (f/f_d)^2)) for |f| < f_d.
 *
 * @param chan     Initialized TDL channel
 * @param t_sec    Time instant
 * @param h        Output: complex tap coefficients [num_taps]
 */
void nr_tdl_generate(const nr_tdl_channel_t *chan, double t_sec,
                      nr_complex_t *h);

/**
 * nr_tdl_apply — Apply channel to input signal (convolution)
 *
 * y[n] = sum_k h_k * x[n - d_k] + noise
 *
 * @param chan    TDL channel
 * @param x       Input samples
 * @param n_in    Input length
 * @param noise   Noise samples (or NULL for no noise)
 * @param y       Output samples (length n_in + max_delay)
 * @return Number of output samples
 */
int nr_tdl_apply(const nr_tdl_channel_t *chan,
                  const nr_complex_t *x, int n_in,
                  const nr_complex_t *noise,
                  nr_complex_t *y);

/**
 * nr_tdl_freq_response — Frequency-domain channel response
 *
 * Computes H(f) = sum_k h_k * exp(-j*2*pi*f*tau_k)
 * at each subcarrier frequency f.
 *
 * @param chan       TDL channel
 * @param t_sec      Time instant
 * @param sc_spacing Subcarrier spacing (Hz)
 * @param num_sc     Number of subcarriers
 * @param f_offset   Frequency offset of first subcarrier (Hz)
 * @param h_freq     Output: frequency response per subcarrier
 */
void nr_tdl_freq_response(const nr_tdl_channel_t *chan,
                           double t_sec,
                           double sc_spacing, int num_sc,
                           double f_offset,
                           nr_complex_t *h_freq);

/* ============================================================================
 * L5: DMRS-Based Channel Estimation
 * ============================================================================ */

/**
 * nr_chan_est_init — Initialize channel estimation context
 *
 * @param ctx          Estimation context
 * @param num_sc       Subcarriers in resource grid
 * @param num_sym      OFDM symbols per slot
 * @param dmrs_sc_sp   DMRS density in frequency (e.g., 1 for Type1 every SC)
 * @param dmrs_sym_sp  DMRS density in time
 */
void nr_chan_est_init(nr_chan_est_ctx_t *ctx,
                       int num_sc, int num_sym,
                       int dmrs_sc_sp, int dmrs_sym_sp);

/**
 * nr_chan_est_ls — Least-Squares channel estimation
 *
 * L5: H_LS[k] = Y_dmrs[k] / X_dmrs[k] at DMRS REs
 *
 * LS is simple but does not exploit channel correlation.
 * Suitable as a baseline when noise power is unknown.
 *
 * @param ctx         Estimation context
 * @param dmrs_tx     Transmitted DMRS sequence
 * @param dmrs_rx     Received DMRS at the same REs
 * @param num_dmrs    Number of DMRS REs
 */
void nr_chan_est_ls(nr_chan_est_ctx_t *ctx,
                     const nr_complex_t *dmrs_tx,
                     const nr_complex_t *dmrs_rx,
                     int num_dmrs);

/**
 * nr_chan_est_mmse — MMSE channel estimation
 *
 * L5: H_MMSE = R_HH * (R_HH + sigma^2 * I)^(-1) * H_LS
 *
 * MMSE exploits the channel's frequency correlation to
 * improve estimates, especially at low SNR. Uses the fact
 * that the channel is bandlimited by the delay spread.
 *
 * L4 (Wiener filter): The MMSE estimator is the optimal linear
 * estimator minimizing E[|H - H_hat|^2].
 *
 * @param ctx              Estimation context
 * @param dmrs_tx          Transmitted DMRS
 * @param dmrs_rx          Received DMRS
 * @param num_dmrs         Number of DMRS REs
 * @param delay_spread_ns  Channel RMS delay spread (ns)
 * @param noise_power      Noise variance sigma^2
 */
void nr_chan_est_mmse(nr_chan_est_ctx_t *ctx,
                       const nr_complex_t *dmrs_tx,
                       const nr_complex_t *dmrs_rx,
                       int num_dmrs,
                       double delay_spread_ns,
                       double noise_power);

/**
 * nr_chan_est_interpolate — Interpolate estimates to all REs
 *
 * After estimating at DMRS positions, interpolate in
 * frequency and time to fill the entire RE grid.
 * Uses linear (or optionally sinc) interpolation.
 */
void nr_chan_est_interpolate(nr_chan_est_ctx_t *ctx);

/**
 * nr_chan_est_get — Retrieve channel estimate at a specific RE
 */
nr_chan_est_t nr_chan_est_get(const nr_chan_est_ctx_t *ctx,
                                int sc_idx, int sym_idx);

/**
 * nr_chan_est_free — Free estimation context
 */
void nr_chan_est_free(nr_chan_est_ctx_t *ctx);

/* ============================================================================
 * L4: Path Loss Models (3GPP TR 38.901 Section 7.4)
 * ============================================================================ */

/**
 * nr_pathloss_db — Compute path loss for given distance and model
 *
 * L4 (Friis transmission equation):
 *   PL_free(d) = 20*log10(4*pi*d/lambda)
 *
 * 3GPP UMa LOS (TR 38.901 Table 7.4.1-1):
 *   PL = 28.0 + 22*log10(d_3D) + 20*log10(f_c)  (10m < d < 5km)
 *
 * @param model    Path loss model
 * @param d_m      3D distance (meters)
 * @param fc_ghz   Carrier frequency (GHz)
 * @param h_bs_m   Base station height (m) — for UMa/UMi
 * @param h_ue_m   UE height (m)
 * @param is_los   LOS / NLOS flag
 * @return Path loss in dB
 */
double nr_pathloss_db(nr_pathloss_model_t model, double d_m,
                       double fc_ghz, double h_bs_m, double h_ue_m,
                       int is_los);

/**
 * nr_pathloss_inverse — Estimate distance from path loss
 *
 * Inverts the path loss model. Useful for RSSI-based ranging.
 *
 * @return Estimated distance (m), negative on error
 */
double nr_pathloss_inverse(double pl_db, nr_pathloss_model_t model,
                            double fc_ghz, double h_bs_m, double h_ue_m,
                            int is_los);

/**
 * nr_shadow_fading_db — Generate log-normal shadow fading sample
 *
 * 3GPP TR 38.901 Section 7.4.5: Shadow fading is modeled as
 * a log-normal random variable with std dev sigma_SF dB.
 * Typical sigma_SF = 4 dB (LOS) to 6-8 dB (NLOS).
 *
 * Uses Box-Muller transform for Gaussian generation.
 *
 * @param sigma_db  Standard deviation (dB)
 * @param seed      Random seed (0 = use time)
 * @return Shadow fading value (dB), zero-mean
 */
double nr_shadow_fading_db(double sigma_db, unsigned int *seed);

/**
 * nr_snr_dbm — Compute SNR from TX power, path loss, and noise figure
 *
 * SNR_dB = P_tx_dBm - PL_dB + G_tx + G_rx - N_dBm
 * N_dBm = -174 + 10*log10(BW_Hz) + NF_dB
 *
 * L4 (Shannon-Hartley): C = B * log2(1 + SNR)
 * This gives the theoretical upper bound on spectral efficiency.
 *
 * @param p_tx_dbm   Transmit power (dBm)
 * @param pl_db      Path loss (dB)
 * @param g_tx_db    TX antenna gain (dBi)
 * @param g_rx_db    RX antenna gain (dBi)
 * @param bw_hz      Bandwidth (Hz)
 * @param nf_db      Receiver noise figure (dB)
 * @param snr_lin    Output: SNR (linear)
 * @return SNR (dB)
 */
double nr_snr_dbm(double p_tx_dbm, double pl_db,
                   double g_tx_db, double g_rx_db,
                   double bw_hz, double nf_db,
                   double *snr_lin);

/**
 * nr_channel_capacity — Shannon capacity for given bandwidth and SNR
 *
 * L4: C = B * log2(1 + SNR) bits/s
 */
double nr_channel_capacity(double bw_hz, double snr_linear);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_CHANNEL_H */
