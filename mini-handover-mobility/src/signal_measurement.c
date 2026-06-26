/**
 * @file signal_measurement.c
 * @brief Signal measurement and radio propagation model implementations (L1, L3, L4)
 *
 * Each function implements an independent knowledge point in physical layer
 * measurements and propagation modeling for handover decisions.
 *
 * References:
 *   - 3GPP TS 36.214 (LTE Physical layer measurements)
 *   - 3GPP TS 38.215 (NR Physical layer measurements)
 *   - Rappaport (2002), "Wireless Communications: Principles and Practice"
 *   - Molisch (2011), "Wireless Communications"
 */

#include "signal_measurement.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Internal PRNG (same as mobility_model.c for consistency) */
static unsigned int _meas_rand_seed = 987654321u;

static double _meas_uniform(void) {
    _meas_rand_seed = 1664525u * _meas_rand_seed + 1013904223u;
    return (double)(_meas_rand_seed & 0x7FFFFFFF) / 2147483648.0;
}

static double _meas_gaussian(double mean, double std) {
    double u1 = _meas_uniform();
    double u2 = _meas_uniform();
    if (u1 < 1e-12) u1 = 1e-12;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + std * z;
}

/* ============================================================================
 * L1: RSSI Computation
 *
 * RSSI = P_tx + G_tx + G_rx - PL - L_body - L_penetration + X_shadow
 *
 * where:
 *   P_tx: transmitter power (dBm)
 *   G_tx, G_rx: antenna gains (dBi)
 *   PL: path loss (dB)
 *   L_body: body loss (dB), typically 3 dB for handheld UE
 *   L_penetration: building/vehicle penetration loss (dB)
 *   X_shadow: log-normal shadow fading (dB), X ~ N(0, σ²)
 *
 * All terms are in dB/dBm, so this is a simple linear sum in the log domain.
 */
double meas_compute_rssi(double tx_power_dbm,
                         double tx_antenna_gain_dbi,
                         double rx_antenna_gain_dbi,
                         double path_loss_db,
                         double shadow_fading_db,
                         double body_loss_db,
                         double penetration_loss_db)
{
    return tx_power_dbm
         + tx_antenna_gain_dbi
         + rx_antenna_gain_dbi
         - path_loss_db
         + shadow_fading_db
         - body_loss_db
         - penetration_loss_db;
}

/* ============================================================================
 * L1: RSRP Computation
 *
 * RSRP (Reference Signal Received Power) is the linear average over power
 * contributions of resource elements carrying reference signals.
 *
 * In LTE (TS 36.214 §5.1.1): RSRP is measured over CRS on antenna port 0
 * (and optionally port 1). It is the most important measurement for mobility.
 *
 * RSRP per RE (dBm) = RSSI_total (dBm) - 10·log10(N_RB · N_SC_per_RB)
 *                    + 10·log10(RS_power_ratio)
 *                    - NF
 *
 * where:
 *   N_RB: number of resource blocks in measurement BW
 *   N_SC_per_RB = 12 subcarriers per RB
 *   RS_power_ratio: fraction of total power allocated to reference signals
 *   NF: noise figure
 */
double meas_compute_rsrp(double rssi_dbm,
                         double measurement_bw_hz,
                         double reference_signal_power_ratio,
                         double noise_figure_db)
{
    /* Number of subcarriers in measurement BW (15 kHz SCS per LTE) */
    double n_subcarriers = measurement_bw_hz / 15000.0;

    if (n_subcarriers < 1.0) return -200.0; /* Invalid BW */

    /* RS power per subcarrier = total RSSI adjusted by RS ratio and BW */
    double rssi_per_sc_linear = pow(10.0, rssi_dbm / 10.0) / n_subcarriers;
    double rs_power_per_sc_linear = rssi_per_sc_linear * reference_signal_power_ratio;

    /* Add thermal noise contribution per subcarrier
     * kTB = -174 dBm/Hz, BW = 15 kHz → -174 + 10*log10(15000) ≈ -174 + 41.76 = -132.24 dBm */
    double thermal_noise_per_sc_dbm = -174.0 + 10.0 * log10(15000.0) + noise_figure_db;
    double thermal_noise_per_sc_linear = pow(10.0, thermal_noise_per_sc_dbm / 10.0);

    /* RSRP = RS power + noise floor (in linear, then back to dBm)
     * Note: In ideal measurement, noise is averaged out over many samples.
     * Here we return the signal+noise measurement typical of UE. */
    double rsrp_linear = rs_power_per_sc_linear + thermal_noise_per_sc_linear;

    if (rsrp_linear < 1e-30) return -200.0;

    return 10.0 * log10(rsrp_linear);
}

/* ============================================================================
 * L1: RSRQ Computation
 *
 * RSRQ (3GPP TS 36.214 §5.1.3):
 *   RSRQ = N_RB · RSRP / RSSI  (linear)
 *
 * Or in dB:
 *   RSRQ_dB = 10·log10(N_RB) + RSRP_dBm - RSSI_dBm
 *
 * RSRQ measures the quality of the received signal, incorporating both
 * signal strength and interference. An RSRQ of -3 dB indicates excellent
 * quality (strong signal, low load), while -19.5 dB indicates poor quality.
 *
 * Key insight: RSRQ degrades as cell load increases, even if RSRP is strong,
 * because RSSI includes interference from other cells' data REs.
 */
double meas_compute_rsrq(double rsrp_dbm,
                         double rssi_dbm,
                         int    num_resource_blocks)
{
    if (num_resource_blocks <= 0) return -30.0;

    /* RSRQ_dB = RSRP_dBm - (RSSI_dBm - 10*log10(N_RB))
     *         = RSRP_dBm - RSSI_dBm + 10*log10(N_RB) */
    double rsrq_db = rsrp_dbm - rssi_dbm + 10.0 * log10((double)num_resource_blocks);

    /* Clamp to 3GPP defined range [-19.5, -3] per TS 36.133 */
    if (rsrq_db > -3.0)  rsrq_db = -3.0;
    if (rsrq_db < -19.5) rsrq_db = -19.5;

    return rsrq_db;
}

/* ============================================================================
 * L1: SINR Computation
 *
 * SINR = S / (I + N)  (linear)
 * SINR_dB = 10·log10(S) - 10·log10(I + N)
 *
 * Thermal noise power in dBm (per bandwidth B):
 *   N_dBm = -174 + 10·log10(B_Hz) + NF_dB
 *
 * where:
 *   -174 dBm/Hz = kT₀ (Boltzmann constant × room temperature 290K)
 *   B_Hz = measurement bandwidth in Hz
 *   NF_dB = receiver noise figure (typical UE: 7-9 dB)
 *
 * References:
 *   - Shannon (1948), "A Mathematical Theory of Communication"
 *     C = B·log₂(1 + SINR)
 *   - 3GPP TS 36.214 §5.1.5 (SINR measurement definition)
 */
double meas_compute_sinr(double signal_power_dbm,
                         double interference_power_dbm,
                         double bandwidth_hz,
                         double noise_figure_db)
{
    /* Thermal noise power in dBm */
    double noise_power_dbm = -174.0 + 10.0 * log10(bandwidth_hz) + noise_figure_db;

    /* Convert to linear */
    double signal_linear       = pow(10.0, signal_power_dbm / 10.0);
    double interference_linear = pow(10.0, interference_power_dbm / 10.0);
    double noise_linear        = pow(10.0, noise_power_dbm / 10.0);

    double sinr_linear = signal_linear / (interference_linear + noise_linear);

    if (sinr_linear < 1e-20) return -40.0;

    return 10.0 * log10(sinr_linear);
}

/* ============================================================================
 * L4: Friis Free Space Path Loss
 *
 * Friis Transmission Equation (Harald T. Friis, 1946):
 *
 *   P_r = P_t · G_t · G_r · (λ / (4πd))²
 *
 * In dB:
 *   PL(dB) = 20·log10(4πd/λ)
 *          = 20·log10(4πd·f/c)
 *          = 20·log10(d) + 20·log10(f) + 20·log10(4π/c)
 *          = 20·log10(d_km) + 20·log10(f_MHz) + 32.45
 *
 * This is the fundamental law governing signal attenuation in free space.
 * It derives from conservation of energy over an expanding spherical wavefront
 * (Poynting vector ∝ 1/d² in the far field).
 *
 * Validity: Far-field region only (d > 2D²/λ, where D is the largest antenna dimension).
 */
double meas_friis_free_space_path_loss(double distance_m, double frequency_hz)
{
    if (distance_m <= 0.0 || frequency_hz <= 0.0) return 0.0;

    double wavelength = 299792458.0 / frequency_hz;
    double path_loss = 20.0 * log10(4.0 * M_PI * distance_m / wavelength);

    return path_loss;
}

/* ============================================================================
 * L3: Log-Distance Path Loss Model
 *
 * The log-distance model extends free-space loss to account for the
 * environment through the path loss exponent n:
 *
 *   PL(d) = PL(d₀) + 10·n·log10(d/d₀) + X_σ
 *
 * where:
 *   PL(d₀): path loss at reference distance d₀ (free space or measured)
 *   n: path loss exponent (environment-dependent)
 *   X_σ ~ N(0, σ²): shadow fading (zero-mean Gaussian in dB)
 *
 * Typical n values:
 *   Free space:    2.0
 *   Urban macro:   3.0–3.8
 *   Urban micro:   2.7–3.5
 *   Indoor office: 2.0–3.0 (same floor), 4.0–6.0 (multi-floor)
 *   Factory:       1.6–3.3
 *
 * The reference distance d₀ is typically 1 m for indoor, 100 m–1 km for outdoor.
 */
double meas_log_distance_path_loss(double distance_m,
                                   double reference_distance_m,
                                   double reference_loss_db,
                                   double path_loss_exponent)
{
    if (distance_m <= 0.0 || reference_distance_m <= 0.0) return 0.0;

    return reference_loss_db
         + 10.0 * path_loss_exponent * log10(distance_m / reference_distance_m);
}

/* ============================================================================
 * L3: Okumura-Hata Path Loss Model
 *
 * The Okumura-Hata model (Hata, 1980) is an empirical formulation based on
 * Okumura's extensive measurements in Tokyo (1968). It is the most widely
 * used macro-cell propagation model in cellular network planning.
 *
 * Urban area (the base formula):
 *   PL_urban = 69.55 + 26.16·log10(f) - 13.82·log10(h_b) - a(h_m)
 *            + [44.9 - 6.55·log10(h_b)]·log10(d)
 *
 * where:
 *   f: frequency [150, 1500] MHz
 *   h_b: BS height [30, 200] m
 *   h_m: MS height [1, 10] m
 *   d: distance [1, 20] km
 *
 * a(h_m): mobile antenna height correction factor
 *   Medium city: a(h_m) = (1.1·log10(f)-0.7)·h_m - (1.56·log10(f)-0.8)
 *   Large city (f≤200 MHz): a(h_m) = 8.29·[log10(1.54·h_m)]² - 1.1
 *   Large city (f≥400 MHz): a(h_m) = 3.2·[log10(11.75·h_m)]² - 4.97
 *
 * Suburban: PL_suburban = PL_urban - 2·[log10(f/28)]² - 5.4
 * Rural:    PL_rural    = PL_urban - 4.78·[log10(f)]² + 18.33·log10(f) - 40.94
 */
double meas_okumura_hata_path_loss(double frequency_mhz,
                                   double distance_km,
                                   double bs_height_m,
                                   double ue_height_m,
                                   int    area_type)
{
    /* Validate input ranges */
    if (frequency_mhz < 150.0)  frequency_mhz = 150.0;
    if (frequency_mhz > 1500.0) frequency_mhz = 1500.0;
    if (distance_km < 1.0)      distance_km = 1.0;
    if (distance_km > 20.0)     distance_km = 20.0;
    if (bs_height_m < 30.0)     bs_height_m = 30.0;
    if (bs_height_m > 200.0)    bs_height_m = 200.0;
    if (ue_height_m < 1.0)      ue_height_m = 1.0;
    if (ue_height_m > 10.0)     ue_height_m = 10.0;

    double log_f = log10(frequency_mhz);
    double log_hb = log10(bs_height_m);
    double log_d = log10(distance_km);

    /* Mobile antenna correction factor for medium city */
    double a_hm = (1.1 * log_f - 0.7) * ue_height_m
                - (1.56 * log_f - 0.8);

    /* Urban path loss */
    double pl_urban = 69.55 + 26.16 * log_f - 13.82 * log_hb - a_hm
                    + (44.9 - 6.55 * log_hb) * log_d;

    /* Area correction */
    switch (area_type) {
        case 0: /* Urban — base formula */
            return pl_urban;
        case 1: /* Suburban */
            return pl_urban - 2.0 * pow(log10(frequency_mhz / 28.0), 2.0) - 5.4;
        case 2: /* Rural */
            return pl_urban - 4.78 * pow(log_f, 2.0) + 18.33 * log_f - 40.94;
        default:
            return pl_urban;
    }
}

/* ============================================================================
 * L3: COST 231 Hata Model (Extended Hata for PCS bands)
 *
 * COST 231 (European COoperation in Science and Technology, Action 231)
 * extended the Okumura-Hata model to 1500–2000 MHz for GSM-1800/DCS and
 * early 3G (UMTS 2100) planning.
 *
 *   PL = 46.3 + 33.9·log10(f) - 13.82·log10(h_b) - a(h_m)
 *        + [44.9 - 6.55·log10(h_b)]·log10(d) + C_m
 *
 * C_m = 0 dB for medium-sized cities and suburban
 * C_m = 3 dB for metropolitan centers
 *
 * Limitations:
 *   - f: 1500–2000 MHz
 *   - d: 1–20 km
 *   - h_b: 30–200 m
 *   - h_m: 1–10 m
 */
double meas_cost231_hata_path_loss(double frequency_mhz,
                                   double distance_km,
                                   double bs_height_m,
                                   double ue_height_m,
                                   bool   is_metropolitan)
{
    if (frequency_mhz < 1500.0) frequency_mhz = 1500.0;
    if (frequency_mhz > 2000.0) frequency_mhz = 2000.0;
    if (distance_km < 1.0)      distance_km = 1.0;
    if (distance_km > 20.0)     distance_km = 20.0;

    double log_f = log10(frequency_mhz);
    double log_hb = log10(bs_height_m);
    double log_d = log10(distance_km);

    double a_hm = (1.1 * log_f - 0.7) * ue_height_m
                - (1.56 * log_f - 0.8);

    double cm = is_metropolitan ? 3.0 : 0.0;

    return 46.3 + 33.9 * log_f - 13.82 * log_hb - a_hm
         + (44.9 - 6.55 * log_hb) * log_d + cm;
}

/* ============================================================================
 * L3: 3GPP TR 38.901 UMa Path Loss (5G NR channel model)
 *
 * The 3GPP TR 38.901 model is the standard channel model for 5G NR
 * system-level simulations, covering 0.5–100 GHz.
 *
 * Urban Macro (UMa) — for base stations above rooftop:
 *
 * LOS (10 m < d_2D < 5 km):
 *   PL_UMa_LOS = 28.0 + 22·log10(d_3D) + 20·log10(f_c)  [d in m, f in GHz]
 *
 * NLOS (10 m < d_2D < 5 km):
 *   PL_UMa_NLOS = max(PL_UMa_LOS, PL_UMa_NLOS')
 *   PL_UMa_NLOS' = 13.54 + 39.08·log10(d_3D) + 20·log10(f_c)
 *                  - 0.6·(h_UT - 1.5)
 *
 * where:
 *   d_3D = sqrt(d_2D² + (h_BS - h_UT)²)
 *   h_BS = effective BS height (typically 25 m for UMa)
 *   h_UT = effective UT height (1.5–22.5 m)
 *   f_c = center frequency in GHz
 *
 * This model is crucial for 5G NR handover simulation.
 */
double meas_3gpp_38_901_uma_path_loss(double distance_3d_m,
                                      double frequency_ghz,
                                      double bs_height_m,
                                      double ue_height_m,
                                      bool   is_los)
{
    (void)bs_height_m; /* Used in 3D distance computation per TR 38.901 */
    if (distance_3d_m < 10.0) distance_3d_m = 10.0;
    if (frequency_ghz < 0.5)  frequency_ghz = 0.5;
    if (frequency_ghz > 100.0) frequency_ghz = 100.0;

    if (is_los) {
        /* LOS: PL = 28.0 + 22*log10(d_3D) + 20*log10(f_c) */
        return 28.0 + 22.0 * log10(distance_3d_m)
             + 20.0 * log10(frequency_ghz);
    } else {
        /* NLOS: PL' = 13.54 + 39.08*log10(d_3D) + 20*log10(f_c)
         *             - 0.6*(h_UT - 1.5) */
        double pl_nlos_prime = 13.54 + 39.08 * log10(distance_3d_m)
                             + 20.0 * log10(frequency_ghz)
                             - 0.6 * (ue_height_m - 1.5);

        /* LOS path loss at this distance for max() */
        double pl_los = 28.0 + 22.0 * log10(distance_3d_m)
                      + 20.0 * log10(frequency_ghz);

        /* NLOS = max(PL_LOS, PL_NLOS') */
        return (pl_nlos_prime > pl_los) ? pl_nlos_prime : pl_los;
    }
}

/* ============================================================================
 * L3: Log-Normal Shadow Fading with Spatial Correlation
 *
 * Shadow fading is caused by large-scale obstacles (buildings, hills) and
 * varies slowly with distance. It is modeled as a log-normal random process
 * with exponential spatial autocorrelation (Gudmundson, 1991):
 *
 *   ρ(Δx) = exp(-|Δx| / d_corr)
 *
 * where d_corr is the decorrelation distance (typically 20–100 m in urban,
 * 500–1000 m in rural environments).
 *
 * Generation using autoregressive model of order 1:
 *   S_n = ρ(Δx) · S_{n-1} + √(1-ρ²) · σ · w_n
 *
 * where w_n ~ N(0,1), σ is the shadow fading standard deviation.
 *
 * This ensures the generated sequence has the correct spatial correlation
 * properties for realistic handover simulation.
 */
double meas_shadow_fading_generate(double prev_shadow_db,
                                   double std_dev_db,
                                   double speed_mps,
                                   double dt_seconds,
                                   double correlation_distance_m)
{
    if (correlation_distance_m < 1e-6) return prev_shadow_db; /* Fully correlated */

    double distance_moved = speed_mps * dt_seconds;
    double rho = exp(-distance_moved / correlation_distance_m);

    /* Clamp to [0, 1] */
    if (rho < 0.0) rho = 0.0;
    if (rho > 1.0) rho = 1.0;

    double innovation_std = std_dev_db * sqrt(1.0 - rho * rho);
    double innovation = _meas_gaussian(0.0, innovation_std);

    return rho * prev_shadow_db + innovation;
}

/* ============================================================================
 * L3: Rayleigh Fading Generation (Jakes' Model)
 *
 * Rayleigh fading models multipath propagation without a dominant LOS path.
 * The complex channel coefficient is the sum of many independent scattered
 * components, leading to a Rayleigh-distributed envelope (by the Central
 * Limit Theorem).
 *
 * Jakes' sum-of-sinusoids model (Jakes, 1974; simplified by Dent et al. 1993):
 *
 *   h(t) = h_I(t) + j·h_Q(t)
 *
 *   h_I(t) = (2/√N) · Σ_{n=1}^{N} cos(β_n) · cos(2π·f_d·t·cos(α_n) + φ_n)
 *   h_Q(t) = (2/√N) · Σ_{n=1}^{N} sin(β_n) · cos(2π·f_d·t·cos(α_n) + φ_n)
 *
 * where:
 *   N = N₀/4 (N₀ is even, typically ≥ 8 oscillators)
 *   α_n = 2πn/N (angle of arrival)
 *   β_n = πn/N (phase offset)
 *   φ_n = random initial phase ~ U[0, 2π)
 *
 * The resulting envelope |h(t)| follows a Rayleigh distribution:
 *   f(r) = (r/σ²)·exp(-r²/(2σ²)), r ≥ 0
 */
double meas_rayleigh_fading_generate(int    num_oscillators,
                                     double doppler_freq_hz,
                                     double time_seconds)
{
    if (num_oscillators < 4) num_oscillators = 4;
    int N0 = (num_oscillators / 4) * 4; /* Ensure multiple of 4 */
    if (N0 < 4) N0 = 4;
    int N = N0 / 4;

    double h_i = 0.0;
    double h_q = 0.0;

    for (int n = 1; n <= N; n++) {
        double alpha_n = 2.0 * M_PI * n / N;
        /* Use deterministic phase based on n for repeatability without storing state */
        double phi_n = 2.0 * M_PI * ((double)n / (N + 1.0));
        double beta_n = M_PI * n / N;
        double cos_term = cos(2.0 * M_PI * doppler_freq_hz * time_seconds
                              * cos(alpha_n) + phi_n);

        h_i += cos(beta_n) * cos_term;
        h_q += sin(beta_n) * cos_term;
    }

    double scale = 2.0 / sqrt((double)N);
    h_i *= scale;
    h_q *= scale;

    double envelope = sqrt(h_i * h_i + h_q * h_q);

    /* Normalize to unit mean power */
    return envelope / sqrt(2.0);
}

/* ============================================================================
 * L3: Rician Fading Generation
 *
 * Rician fading occurs when there is a dominant LOS component in addition
 * to scattered multipath. The Rice K-factor quantifies the ratio of LOS
 * power to scattered power:
 *
 *   K = A² / (2σ²)  (linear)
 *   K_dB = 10·log10(K)
 *
 * The Rician PDF:
 *   f(r) = (r/σ²)·exp(-(r²+A²)/(2σ²))·I₀(r·A/σ²), r ≥ 0
 *
 * where I₀ is the modified Bessel function of the first kind, order zero.
 *
 *   K → 0:  Rician → Rayleigh (no LOS)
 *   K → ∞:  Rician → AWGN channel (pure LOS, no fading)
 *
 * Generation: h_rician = √(K/(K+1))·h_LOS + √(1/(K+1))·h_rayleigh
 */
double meas_rician_fading_generate(int    num_oscillators,
                                   double doppler_freq_hz,
                                   double time_seconds,
                                   double k_factor_db)
{
    double k_linear = pow(10.0, k_factor_db / 10.0);

    /* Rayleigh (scattered) component */
    double h_rayleigh = meas_rayleigh_fading_generate(num_oscillators,
                                                       doppler_freq_hz,
                                                       time_seconds);

    /* LOS component (deterministic, unit amplitude with Doppler shift) */
    double h_los = cos(2.0 * M_PI * doppler_freq_hz * time_seconds);

    /* Combine: h = sqrt(K/(K+1)) * h_LOS + sqrt(1/(K+1)) * h_rayleigh */
    double los_weight = sqrt(k_linear / (k_linear + 1.0));
    double scatter_weight = sqrt(1.0 / (k_linear + 1.0));

    return los_weight * h_los + scatter_weight * h_rayleigh;
}

/* ============================================================================
 * L5: Kalman Filter for RSRP Tracking
 *
 * The Kalman filter (Kalman, 1960) provides optimal recursive state estimation
 * for linear dynamic systems with Gaussian noise. It is widely used in UE
 * receivers for smoothing and predicting RSRP measurements.
 *
 * State vector: x = [RSRP, dRSRP/dt]ᵀ
 * Measurement: z = RSRP_measured
 *
 * Process model:
 *   x_k = F·x_{k-1} + w_k,  w_k ~ N(0, Q)
 *   where F = [[1, Δt], [0, 1]]
 *
 * Measurement model:
 *   z_k = H·x_k + v_k,  v_k ~ N(0, R)
 *   where H = [1, 0]
 *
 * Prediction step:
 *   x̂_k⁻ = F·x̂_{k-1}
 *   P_k⁻ = F·P_{k-1}·Fᵀ + Q
 *
 * Update step:
 *   K_k = P_k⁻·Hᵀ / (H·P_k⁻·Hᵀ + R)
 *   x̂_k = x̂_k⁻ + K_k·(z_k - H·x̂_k⁻)
 *   P_k = (I - K_k·H)·P_k⁻
 *
 * This implementation uses the scalar form of the 2×2 covariance matrix,
 * simplifying for embedded UE implementation.
 *
 * References:
 *   - Kalman (1960), "A New Approach to Linear Filtering and Prediction
 *     Problems", Trans. ASME–J. Basic Eng.
 *   - 3GPP TS 36.331 (L3 filtering is an IIR, but Kalman is often used
 *     as a pre-filter before L3 filtering)
 */
double meas_kalman_filter_rsrp(double  measured_rsrp_dbm,
                               double  dt_seconds,
                               double  process_noise_var,
                               double  measurement_noise_var,
                               double *state_rsrp,
                               double *state_rate,
                               double *cov_p11,
                               double *cov_p12,
                               double *cov_p22)
{
    if (!state_rsrp || !state_rate || !cov_p11 || !cov_p12 || !cov_p22) {
        return measured_rsrp_dbm; /* Fallback: return raw measurement */
    }

    /* --- Prediction step --- */
    double predicted_rsrp = *state_rsrp + *state_rate * dt_seconds;
    double predicted_rate = *state_rate;

    /* P⁻ = F·P·Fᵀ + Q
     * F = [[1, dt], [0, 1]]
     * P⁻[0][0] = P[0][0] + 2*dt*P[0][1] + dt²*P[1][1] + Q[0][0]
     * P⁻[0][1] = P[0][1] + dt*P[1][1]
     * P⁻[1][1] = P[1][1] + Q[1][1] */
    double p11_pred = *cov_p11 + 2.0 * dt_seconds * (*cov_p12)
                    + dt_seconds * dt_seconds * (*cov_p22) + process_noise_var;
    double p12_pred = *cov_p12 + dt_seconds * (*cov_p22);
    double p22_pred = *cov_p22 + process_noise_var;

    /* --- Update step --- */
    /* Innovation (measurement residual) */
    double y = measured_rsrp_dbm - predicted_rsrp;

    /* Innovation covariance: S = H·P⁻·Hᵀ + R = P⁻[0][0] + R */
    double s = p11_pred + measurement_noise_var;

    if (s < 1e-15) s = 1e-15;

    /* Kalman gain: K = P⁻·Hᵀ / S */
    double k1 = p11_pred / s; /* Gain for RSRP */
    double k2 = p12_pred / s; /* Gain for RSRP rate */

    /* Updated state */
    double filtered_rsrp = predicted_rsrp + k1 * y;
    double filtered_rate = predicted_rate + k2 * y;

    /* Updated covariance: P = (I - K·H)·P⁻
     * P[0][0] = (1 - K₁)·P⁻[0][0]
     * P[0][1] = (1 - K₁)·P⁻[0][1]
     * P[1][1] = P⁻[1][1] - K₂·P⁻[0][1] */
    *cov_p11 = (1.0 - k1) * p11_pred;
    *cov_p12 = (1.0 - k1) * p12_pred;
    *cov_p22 = p22_pred - k2 * p12_pred;

    /* Output updated state */
    *state_rsrp = filtered_rsrp;
    *state_rate = filtered_rate;

    return filtered_rsrp;
}

/* ============================================================================
 * L3: Layer-3 Filtering (3GPP TS 36.331 §5.5.3.2)
 *
 * 3GPP specifies a first-order IIR (Infinite Impulse Response) filter for
 * measurement smoothing at the RRC layer:
 *
 *   F_n = (1 - a)·F_{n-1} + a·M_n
 *
 * where:
 *   a = 1 / 2^(k/4)
 *   k ∈ {0, 1, 2, ..., 19}
 *
 * The filter coefficient k controls the degree of averaging:
 *   k=0:  a=1     → no filtering (instantaneous)
 *   k=4:  a=1/2   → equal weight to old and new
 *   k=8:  a=1/4   → more smoothing
 *   k=19: a≈1/27  → heavy smoothing (slow response)
 *
 * This is equivalent to an exponentially weighted moving average (EWMA).
 *
 * Note: The filter input M_n comes from the physical layer (L1 filtering
 * is specified in TS 36.214 §5.1). The L3 filter output F_n is used
 * for event evaluation.
 */
double meas_l3_filter(double prev_filtered,
                      double new_measurement,
                      int    filter_coeff_k)
{
    if (filter_coeff_k < 0)  filter_coeff_k = 0;
    if (filter_coeff_k > 19) filter_coeff_k = 19;

    double a = 1.0 / pow(2.0, (double)filter_coeff_k / 4.0);

    return (1.0 - a) * prev_filtered + a * new_measurement;
}
