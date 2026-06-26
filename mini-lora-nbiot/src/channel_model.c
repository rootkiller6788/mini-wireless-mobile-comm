/**
 * @file channel_model.c
 * @brief Wireless Channel Simulation -- AWGN, Rayleigh/Rician fading, multi-path
 *
 * Knowledge: L2 fading channel concepts, L3 Rayleigh/Rician statistics,
 *            L5 Jakes sum-of-sinusoids, AWGN generation, channel application
 *            L8 multi-SF interference analysis, capture effect
 *
 * @license MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "lora_channel.h"
#include "lora_phy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Pseudo-random number generator (simple LCG for reproducibility) */
static uint32_t lcg_state = 12345;

static void __attribute__((unused)) lcg_seed(uint32_t seed) { lcg_state = seed; }

static double lcg_uniform(void) {
    lcg_state = lcg_state * 1103515245 + 12345;
    return (double)(lcg_state & 0x7FFFFFFF) / 2147483648.0;
}

/* Box-Muller: two independent uniform → two independent Gaussian */
static double box_muller_gaussian(void) {
    double u1 = lcg_uniform();
    double u2 = lcg_uniform();
    /* Avoid log(0) */
    if (u1 < 1e-10) u1 = 1e-10;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ======================================================================
   L5: AWGN Generation
   ====================================================================== */

/*
 * Generate complex AWGN sample.
 *
 * Each I/Q component is independent Gaussian with variance sigma^2.
 * For desired SNR with unity signal power:
 *   sigma^2 = 10^(-SNR_dB / 10) / 2
 *
 * The factor of 2 accounts for noise power split between I and Q.
 *
 * Box-Muller transform: converts uniform random to Gaussian.
 *   Z0 = sqrt(-2*ln(U1)) * cos(2*pi*U2)  ~ N(0, 1)
 *   Z1 = sqrt(-2*ln(U1)) * sin(2*pi*U2)  ~ N(0, 1)
 *
 * @param sigma  Standard deviation per I/Q dimension
 * @return Complex noise sample
 */
double complex awgn_sample(double sigma)
{
    if (sigma <= 0.0) return CMPLX(0.0, 0.0);
    double ni = box_muller_gaussian() * sigma;
    double nq = box_muller_gaussian() * sigma;
    return CMPLX(ni, nq);
}

/* ======================================================================
   L3: Rayleigh Fading
   ====================================================================== */

/*
 * Rayleigh fading coefficient using Jakes sum-of-sinusoids method.
 *
 * h(t) = sum_{n=1}^{N_s} [cos(alpha_n) + j*sin(alpha_n)]
 *                       * cos(2*pi * f_d * cos(beta_n) * t + phi_n)
 *
 * where:
 *   N_s = number of sinusoids (typically 8+)
 *   alpha_n = random angle of arrival
 *   beta_n = pi * (n - 0.5) / (2 * N_s)  (equally spaced)
 *   phi_n = random initial phase
 *   f_d = maximum Doppler frequency
 *
 * The resulting complex envelope has:
 *   - Magnitude: Rayleigh distributed (no LOS component)
 *   - Phase: uniform [0, 2*pi)
 *   - Auto-correlation: J_0(2*pi*f_d*tau) (Bessel function)
 *
 * @param state     Channel state with Doppler configuration
 * @param tap_index Which multi-path tap (for multi-tap channels)
 * @param t         Current time in seconds
 * @return Complex fading coefficient
 */
double complex rayleigh_fading_coeff(const channel_state_t *state,
                                      uint16_t tap_index, double t)
{
    if (!state) return CMPLX(1.0, 0.0);

    double fd = state->doppler_hz;
    if (fd <= 0.0) return CMPLX(1.0, 0.0);  /* Static channel */

    uint16_t Ns = 16;  /* Number of sinusoids */
    double complex h = CMPLX(0.0, 0.0);

    for (uint16_t n = 0; n < Ns; n++) {
        /* Angle of arrival for this sinusoid */
        double alpha = 2.0 * M_PI * (double)(n + tap_index * 7) / (double)Ns;
        double beta  = M_PI * ((double)n + 0.5) / (2.0 * (double)Ns);
        /* Deterministic phase from seed (for reproducibility) */
        double phi = (double)(n * 131 + tap_index * 37) * M_PI / 180.0;

        double doppler_term = 2.0 * M_PI * fd * cos(beta) * t + phi;
        double oscillator = cos(doppler_term);

        h += CMPLX(cos(alpha), sin(alpha)) * oscillator;
    }

    /* Normalize to unit average power */
    double scale = 1.0 / sqrt((double)Ns);
    return h * scale;
}

/* ======================================================================
   L5: Channel Initialization and Application
   ====================================================================== */

void channel_init(channel_state_t *state, fading_channel_type_t type,
                   double snr_db, double sample_rate, double doppler_hz)
{
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->type = type;
    state->snr_db = snr_db;
    state->sample_rate_hz = sample_rate;
    state->doppler_hz = doppler_hz;
    state->path_loss_db = 0.0;
    state->rician_k_db = 10.0;    /* Default K=10 dB (strong LOS) */
    state->nakagami_m = 1.0;      /* Default m=1 (Rayleigh) */
    state->noise_figure_db = 3.0;
    state->num_taps = 1;
    state->tap_delays_s[0] = 0.0;
    state->tap_powers_db[0] = 0.0;

    /* Initialize tap states to unity */
    for (int i = 0; i < 12; i++)
        state->tap_states[i] = CMPLX(1.0, 0.0);
}

double complex channel_apply(channel_state_t *state, double complex tx_sample)
{
    if (!state) return tx_sample;

    double t = (double)state->sample_count / state->sample_rate_hz;
    state->sample_count++;

    /* Compute composite channel gain from all taps */
    double complex h_total = CMPLX(0.0, 0.0);

    for (uint16_t tap = 0; tap < state->num_taps; tap++) {
        double complex h_tap;

        switch (state->type) {
            case CHANNEL_AWGN:
                h_tap = CMPLX(1.0, 0.0);
                break;

            case CHANNEL_RAYLEIGH:
                h_tap = rayleigh_fading_coeff(state, tap, t);
                break;

            case CHANNEL_RICIAN: {
                /* Rician = LOS + Rayleigh scatter
                 * K = A^2 / (2*sigma^2), where A = LOS amplitude
                 * Normalize total power to 1:
                 *   A^2 = K / (K + 1)
                 *   sigma^2 = 1 / (2*(K + 1))
                 */
                double K_lin = pow(10.0, state->rician_k_db / 10.0);
                double A = sqrt(K_lin / (K_lin + 1.0));
                double sigma = sqrt(1.0 / (2.0 * (K_lin + 1.0)));
                double complex los = CMPLX(A, 0.0);
                double complex scatter = rayleigh_fading_coeff(state, tap, t) * sigma;
                h_tap = los + scatter;
                break;
            }

            case CHANNEL_NAKAGAMI:
                /* Simplified: Nakagami-m approximated as Rayleigh for m=1 */
                h_tap = rayleigh_fading_coeff(state, tap, t);
                break;

            default:
                h_tap = CMPLX(1.0, 0.0);
        }

        /* Apply tap power */
        double tap_gain = pow(10.0, state->tap_powers_db[tap] / 20.0);
        h_total += h_tap * tap_gain;
    }

    /* Apply path loss */
    double pl_gain = pow(10.0, -state->path_loss_db / 20.0);
    double complex rx = tx_sample * h_total * pl_gain;

    /* Add AWGN */
    double snr_lin = pow(10.0, state->snr_db / 10.0);
    double noise_sigma = sqrt(1.0 / (2.0 * snr_lin));
    double complex noise = awgn_sample(noise_sigma);
    rx = rx + noise;

    return rx;
}

void channel_apply_block(channel_state_t *state, const double complex *tx,
                          double complex *rx, size_t num_samples)
{
    if (!state || !tx || !rx) return;

    for (size_t i = 0; i < num_samples; i++)
        rx[i] = channel_apply(state, tx[i]);
}

/* ======================================================================
   L8: Multi-SF Interference Analysis
   ====================================================================== */

/*
 * Cross-SF interference isolation.
 *
 * Empirically measured isolation between spreading factors
 * (from Croce et al., "Impact of LoRa Imperfect Orthogonality",
 * IEEE Trans. Wireless Comm., 2018).
 *
 * The isolation matrix ISOL[SF_desired-7][SF_interf-7]:
 * Values represent how much the interferer is attenuated
 * when using a different SF than the desired signal.
 *
 * Closer SFs interfere more (e.g., SF7-SF8 only ~8 dB isolation),
 * while far SFs interfere less (e.g., SF7-SF12 ~16 dB isolation).
 */
double lora_cross_sf_isolation_db(uint8_t sf_desired, uint8_t sf_interf)
{
    if (sf_desired < 7 || sf_desired > 12) return 0.0;
    if (sf_interf < 7 || sf_interf > 12) return 20.0;  /* Out of band */
    if (sf_desired == sf_interf) return 0.0;  /* Same SF: co-channel */

    /* Isolation matrix [desired-7][interferer-7] */
    static const double iso[6][6] = {
        /*       SF7   SF8   SF9   SF10  SF11  SF12 (interferer) */
        /*SF7*/ { 0.0,  8.0, 10.5, 12.0, 14.0, 16.0 },
        /*SF8*/ { 8.0,  0.0,  7.0,  9.5, 11.0, 13.0 },
        /*SF9*/ {10.5,  7.0,  0.0,  6.5,  8.5, 10.5 },
        /*SF10*/{12.0,  9.5,  6.5,  0.0,  6.0,  8.0 },
        /*SF11*/{14.0, 11.0,  8.5,  6.0,  0.0,  5.0 },
        /*SF12*/{16.0, 13.0, 10.5,  8.0,  5.0,  0.0 },
    };

    return iso[(int)sf_desired - 7][(int)sf_interf - 7];
}

/*
 * Compute effective SINR accounting for multi-SF interference.
 *
 * SINR = P_desired / (sum I_k + N)
 *
 * where I_k = P_k * isolation_k (each interferer attenuated by
 * cross-SF isolation).
 *
 * In dB: SINR_dB = P_desired_dBm - 10*log10(sum(10^(I_k_dBm/10)) + N_dBm)
 *
 * @param desired_sf        Desired signal SF
 * @param desired_power_db  Desired signal power in dBm
 * @param interfering_sf    Array of interfering SFs
 * @param interfering_power_db Array of interfering powers in dBm
 * @param num_interferers   Number of interferers
 * @return Effective SINR in dB
 */
double lora_multi_sf_sinr(uint8_t desired_sf, double desired_power_db,
                           const uint8_t *interfering_sf,
                           const double *interfering_power_db,
                           uint16_t num_interferers)
{
    if (num_interferers == 0)
        return desired_power_db - (-174.0 + 10.0 * log10(125000.0) + 6.0);

    /* Noise floor for BW=125kHz, NF=6dB */
    double noise_dbm = -174.0 + 10.0 * log10(125000.0) + 6.0;

    /* Sum interference in linear domain */
    double I_total_linear = pow(10.0, noise_dbm / 10.0);

    for (uint16_t i = 0; i < num_interferers; i++) {
        double isolation = lora_cross_sf_isolation_db(desired_sf, interfering_sf[i]);
        double I_eff_dbm = interfering_power_db[i] - isolation;
        I_total_linear += pow(10.0, I_eff_dbm / 10.0);
    }

    double I_total_dbm = 10.0 * log10(I_total_linear);
    return desired_power_db - I_total_dbm;
}

/*
 * LoRa capture effect probability.
 *
 * In LoRa, if two packets overlap in time and frequency but
 * the stronger one exceeds the weaker by the "capture ratio"
 * (typically 6 dB), the stronger packet is successfully decoded
 * and the weaker is treated as noise.
 *
 * This model gives the probability that the desired packet
 * captures the receiver given N simultaneous interferers.
 *
 * Capture condition: P_desired > max(P_interferer_i) + capture_ratio
 *
 * Simplified model: if capture condition met, P_capture = 1, else 0.
 * More sophisticated models consider SINR-based probability.
 *
 * @param desired_power_db     Desired signal power in dBm
 * @param interferer_power_db  Array of interferer powers in dBm
 * @param num_interferers      Number of interferers
 * @param capture_ratio_db     Capture ratio threshold (typically 6 dB)
 * @return Capture probability [0, 1]
 */
double lora_capture_probability(double desired_power_db,
                                 const double *interferer_power_db,
                                 uint16_t num_interferers,
                                 double capture_ratio_db)
{
    if (num_interferers == 0) return 1.0;

    /* Find the strongest interferer */
    double max_interf = -999.0;
    for (uint16_t i = 0; i < num_interferers; i++) {
        if (interferer_power_db[i] > max_interf)
            max_interf = interferer_power_db[i];
    }

    /* Capture if desired exceeds strongest interferer by capture_ratio */
    if (desired_power_db >= max_interf + capture_ratio_db)
        return 1.0;

    /* Otherwise, probability decreases with power deficit */
    double deficit = (max_interf + capture_ratio_db) - desired_power_db;
    if (deficit <= 0.0) return 1.0;

    /* Logistic falloff: P ~ 1 / (1 + exp(deficit/3)) */
    return 1.0 / (1.0 + exp(deficit / 3.0));
}

/*
 * LoRaWAN gateway throughput estimation.
 *
 * Models the aggregate throughput a gateway can sustain given
 * N devices transmitting at a given packet rate.
 *
 * Throughput per SF:
 *   T_sf = N_devices_sf * packet_rate * payload_bits
 *        * (1 - P_collision_sf)
 *
 * Collision probability uses a simplified ALOHA model:
 *   P_collision = 1 - exp(-2 * G)
 * where G = offered load per SF channel.
 *
 * @param num_devices          Total number of devices
 * @param sf_distribution      Fraction of devices per SF [SF7..SF12]
 * @param packets_per_hour     Average packets per device per hour
 * @param packet_payload_bytes Average payload bytes per packet
 * @return Gateway throughput in bits/second
 */
double lora_gateway_throughput_bps(uint32_t num_devices,
                                    const double *sf_distribution,
                                    double packets_per_hour,
                                    uint16_t packet_payload_bytes)
{
    if (!sf_distribution || num_devices == 0) return 0.0;

    double total_throughput = 0.0;

    for (int sf = 7; sf <= 12; sf++) {
        double frac = sf_distribution[sf - 7];
        uint32_t n_devs_sf = (uint32_t)((double)num_devices * frac);

        /* Packet rate per SF channel (packets/second) */
        double lambda = (double)n_devs_sf * packets_per_hour / 3600.0;

        /* Average airtime for this payload at this SF (simplified) */
        double bits_per_packet __attribute__((unused)) = (double)(packet_payload_bytes + 13) * 8.0;  /* +overhead */
        lora_phy_params_t params;
        lora_phy_params_init_default(&params);
        params.sf = (lora_spreading_factor_t)sf;
        params.payload_len = packet_payload_bytes;
        params.num_chips = (uint32_t)1 << (uint32_t)sf;
        params.symbol_period = (double)params.num_chips / (double)params.bw;
        double airtime = lora_packet_airtime(&params);
        if (airtime <= 0.0) airtime = 0.1;

        /* Offered load: fraction of time channel is occupied */
        double G = lambda * airtime;

        /* ALOHA collision: P_success = exp(-2*G) */
        double p_success = exp(-2.0 * G);

        /* Throughput for this SF */
        double sf_throughput = (double)n_devs_sf * packets_per_hour
                               * (double)(packet_payload_bytes * 8)
                               * p_success / 3600.0;

        total_throughput += sf_throughput;
    }

    return total_throughput;
}

/*
 * NB-IoT inter-cell interference SINR.
 *
 * SINR_dB = RSRP_serving - 10*log10(sum_i 10^(RSRP_i/10) + 10^(N/10))
 *
 * where N = thermal noise + NF in the NB-IoT bandwidth.
 *
 * @param serving_rsrp_dbm   Serving cell RSRP in dBm
 * @param neighbor_rsrp_dbm  Array of neighbor cell RSRPs
 * @param num_neighbors      Number of neighbors
 * @param noise_figure_db    UE noise figure in dB
 * @return SINR in dB
 */
double nbiot_intercell_sinr(double serving_rsrp_dbm,
                             const double *neighbor_rsrp_dbm,
                             uint16_t num_neighbors,
                             double noise_figure_db)
{
    /* Thermal noise in 180 kHz */
    double N_thermal = -174.0 + 10.0 * log10(180000.0) + noise_figure_db;

    /* Total interference + noise in linear */
    double I_total_lin = pow(10.0, N_thermal / 10.0);

    for (uint16_t i = 0; i < num_neighbors; i++) {
        I_total_lin += pow(10.0, neighbor_rsrp_dbm[i] / 10.0);
    }

    double I_total_db = 10.0 * log10(I_total_lin);
    return serving_rsrp_dbm - I_total_db;
}
