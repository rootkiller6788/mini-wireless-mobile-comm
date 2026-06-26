/**
 * wireless_phy_sec.h — Physical Layer Security for Wireless Communications
 *
 * Covers: Wiretap channel model, secrecy capacity, channel-based key
 *         generation, artificial noise, LDPC codes for physical security
 * Knowledge Levels: L1 (channel type defs), L2 (PLS concept),
 *                   L3 (mutual information/secrecy math),
 *                   L4 (Wyner's secrecy theorem),
 *                   L8 (MIMO wiretap, RIS-assisted PLS)
 *
 * Course Mapping:
 *   Stanford EE359 — Wireless (physical layer security)
 *   MIT 6.450 — Digital Communications (wiretap channel)
 *   Berkeley EE117 — EM Waves (channel reciprocity for key gen)
 *   TU Munich — High-Frequency Engineering (PLS)
 *
 * References:
 *   Wyner, A.D. (1975): "The Wire-Tap Channel" — Bell System Tech Journal
 *   Csiszar & Korner (1978): "Broadcast Channels with Confidential Messages"
 *   Bloch & Barros (2011): "Physical-Layer Security: From Information Theory
 *        to Security Engineering"
 *   Maurer (1993): "Secret Key Agreement by Public Discussion" —
 *        IEEE Trans. Information Theory
 */

#ifndef WIRELESS_PHY_SEC_H
#define WIRELESS_PHY_SEC_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Physical Layer Security Type Definitions
 * ============================================================================ */

/** Channel state information (CSI) for key generation */
typedef struct {
    double magnitude;           /* |h| — channel gain */
    double phase;               /* arg(h) — channel phase in radians */
    double timestamp;           /* Measurement time */
} csi_sample_t;

/** Wiretap channel model parameters */
typedef struct {
    double snr_main;            /* SNR on main channel (dB) */
    double snr_eve;             /* SNR on eavesdropper channel (dB) */
    double channel_correlation; /* Correlation between main and eavesdropper (0-1) */
    int    is_fading;           /* 1 = Rayleigh fading, 0 = AWGN */
    double doppler_spread;      /* Hz (for fading channel) */
} wiretap_channel_t;

/** Secrecy metrics */
typedef struct {
    double secrecy_capacity;    /* bits/s/Hz — C_s = max{ I(X;Y) - I(X;Z), 0 } */
    double secrecy_rate;        /* Achievable secrecy rate (bits/s/Hz) */
    double secrecy_outage_prob; /* Outage probability at given rate */
    double equivocation;        /* H(M|Z) — Eve's uncertainty about message */
    double key_generation_rate; /* bits per channel sample */
} secrecy_metrics_t;

/** Channel-based key generation context */
typedef struct {
    csi_sample_t *samples;      /* CSI measurement samples */
    size_t        num_samples;
    size_t        capacity;

    /* Quantization parameters */
    double        mean_level;   /* Mean of RSS/phase */
    double        std_level;    /* Standard deviation */
    int           bits_per_sample;  /* Quantizer resolution */

    /* Extracted bits */
    uint8_t      *raw_bits;     /* Raw extracted bits (not yet reconciled) */
    size_t        raw_bit_len;

    /* Reconciled key (after information reconciliation) */
    uint8_t      *reconciled_key;
    size_t        reconciled_key_len;

    /* Privacy amplification output */
    uint8_t      *final_key;
    size_t        final_key_len;
} channel_key_gen_ctx_t;

/** Artificial noise precoding context (MIMO) */
typedef struct {
    int     num_tx_antennas;    /* N_t */
    int     num_rx_antennas;    /* N_r (Bob) */
    int     num_eve_antennas;   /* N_e (Eve, estimated) */

    /* Beamforming vector for Bob */
    double *beamforming_vec;    /* Length: N_t (complex interleaved re/im) */
    double *an_precoder;        /* Artificial noise precoder: N_t × (N_t-1) */

    double power_allocation;    /* Fraction of power to signal (0-1) */
} an_precoding_ctx_t;

/* ============================================================================
 * L4: Wyner's Secrecy Capacity Theorem
 * ============================================================================ */

/**
 * wyner_secrecy_capacity — Compute secrecy capacity of the wiretap channel
 *
 * L4 Theorem (Wyner 1975, generalized by Csiszar-Korner 1978):
 *
 *   C_s = max_{P_{UX}} { I(U;Y) - I(U;Z) }
 *
 * For the Gaussian wiretap channel with main SNR γ_m and eavesdropper SNR γ_e:
 *
 *   C_s = [log₂(1 + γ_m) - log₂(1 + γ_e)]⁺
 *       = [log₂((1 + γ_m)/(1 + γ_e))]⁺
 *
 * where [x]⁺ = max(x, 0). Positive secrecy capacity requires γ_m > γ_e.
 *
 * This is the fundamental limit: no amount of coding can achieve a secrecy
 * rate above C_s while maintaining both reliability and confidentiality.
 *
 * @param gamma_m  Main channel SNR (linear, not dB)
 * @param gamma_e  Eavesdropper channel SNR (linear, not dB)
 * @return Secrecy capacity in bits/s/Hz (always ≥ 0)
 */
double wyner_secrecy_capacity(double gamma_m, double gamma_e);

/**
 * wyner_secrecy_capacity_db — Same but inputs in dB
 */
double wyner_secrecy_capacity_db(double snr_main_db, double snr_eve_db);

/**
 * fading_secrecy_capacity — Ergodic secrecy capacity for Rayleigh fading
 *
 * For Rayleigh fading: C_s = E_h[ {log₂(1+|h_m|²γ_m) - log₂(1+|h_e|²γ_e)}⁺ ]
 *
 * This Monte Carlo estimate samples the distribution.
 *
 * @param avg_snr_main_db Average SNR on main channel (dB)
 * @param avg_snr_eve_db  Average SNR on eavesdropper channel (dB)
 * @param num_samples     Monte Carlo samples
 * @return Ergodic secrecy capacity estimate (bits/s/Hz)
 */
double fading_secrecy_capacity(double avg_snr_main_db, double avg_snr_eve_db,
                                 int num_samples);

/* ============================================================================
 * L5: Channel-Based Secret Key Generation
 * ============================================================================ */

/**
 * channel_key_gen_init — Initialize channel-based key generation context
 *
 * Uses the principle of channel reciprocity: in TDD systems, the
 * forward and reverse channels are identical within the coherence time.
 * Both parties measure the same channel → extract same bits.
 *
 * L2 Concept: RSS-based key generation works because the physical
 * channel is reciprocal and decorrelates over ~λ/2 distance,
 * making it hard for an eavesdropper in a different location
 * to infer the same key.
 */
void channel_key_gen_init(channel_key_gen_ctx_t *ctx);

/**
 * channel_key_gen_add_sample — Add one CSI measurement
 */
int channel_key_gen_add_sample(channel_key_gen_ctx_t *ctx,
                                double magnitude, double phase);

/**
 * channel_key_gen_quantize — Quantize CSI samples into bits
 *
 * L5 Algorithm:
 *   1. Normalize samples to zero mean, unit variance
 *   2. Apply guard-band quantization: skip samples near the mean
 *      to reduce bit mismatch between parties
 *   3. Output: raw bit string
 *
 * @param bits_per_sample Quantizer resolution (1-4 bits)
 * @return Number of bits extracted
 */
int channel_key_gen_quantize(channel_key_gen_ctx_t *ctx, int bits_per_sample);

/**
 * channel_key_gen_reconcile — Information reconciliation via Cascade protocol
 *
 * The Cascade protocol (Brassard & Salvail 1993) corrects bit
 * mismatches between Alice and Bob's raw keys using iterative
 * parity-check exchange over a public channel.
 *
 * @param local_bits  Local raw bit string
 * @param bit_len     Length
 * @param max_passes  Maximum Cascade passes
 * @param error_rate  Estimated bit error rate
 * @return 0 on success
 */
int channel_key_gen_reconcile(channel_key_gen_ctx_t *ctx,
                               const uint8_t *local_bits, size_t bit_len,
                               int max_passes, double error_rate);

/**
 * channel_key_gen_amplify — Privacy amplification via universal hashing
 *
 * Removes any partial information Eve may have gleaned during
 * reconciliation. Uses a random universal₂ hash function.
 *
 * @param output_bits Desired final key length (bits)
 * @return 0 on success
 */
int channel_key_gen_amplify(channel_key_gen_ctx_t *ctx, int output_bits);

/**
 * channel_key_gen_get_key — Get the final derived key
 */
const uint8_t* channel_key_gen_get_key(const channel_key_gen_ctx_t *ctx,
                                        size_t *key_len);

/**
 * channel_key_gen_free — Free all allocated memory
 */
void channel_key_gen_free(channel_key_gen_ctx_t *ctx);

/* ============================================================================
 * L5: Artificial Noise Precoding (MIMO Wiretap)
 * ============================================================================ */

/**
 * an_precoding_init — Initialize artificial noise precoding
 *
 * L8 Concept (Goel & Negi 2008): The transmitter uses some antennas
 * to transmit artificial noise in the null space of Bob's channel.
 * Eve, whose channel is different, cannot cancel the AN.
 *
 * For MISO (N_t antennas → Bob 1 antenna):
 *   x = √(α*P) ⋅ w ⋅ s + √((1-α)*P/(N_t-1)) ⋅ V_null ⋅ n
 * where:
 *   w = h_bob^H / ||h_bob||  (beamforming toward Bob)
 *   V_null = null(h_bob)      (AN in Bob's nullspace)
 *   α ∈ (0,1]: power allocation factor
 *
 * Eve sees: y_e = h_eve ⋅ x + z_e
 * Bob sees: y_b = h_bob ⋅ x + z_b = √(α*P) ⋅ s + z_b  (AN perfectly nulled)
 */
void an_precoding_init(an_precoding_ctx_t *ctx,
                        int num_tx_antennas,
                        int num_rx_antennas,
                        int num_eve_antennas,
                        double power_allocation);

/**
 * an_precoding_design — Design the beamforming vector and AN precoder
 *
 * @param channel_bob Channel from TX to Bob (length: 2*N_t*N_r, interleaved)
 * @return 0 on success
 */
int an_precoding_design(an_precoding_ctx_t *ctx,
                         const double *channel_bob);

/**
 * an_precoding_secrecy_rate — Compute the achievable secrecy rate
 *
 * Secrecy rate for MIMO wiretap with AN:
 *
 *   R_s = {log₂ det(I + H_b^H (σ²I + H_b Σ_an H_b^H)⁻¹ H_b Σ_s) -
 *          log₂ det(I + H_e^H (σ²I + H_e Σ_an H_e^H)⁻¹ H_e Σ_s)}⁺
 *
 * @param channel_eve Channel from TX to Eve (length: 2*N_t*N_e, interleaved)
 * @param noise_power Noise variance σ²
 * @return Secrecy rate (bits/s/Hz)
 */
double an_precoding_secrecy_rate(const an_precoding_ctx_t *ctx,
                                   const double *channel_eve,
                                   double noise_power);

/**
 * an_precoding_free — Free allocated memory
 */
void an_precoding_free(an_precoding_ctx_t *ctx);

/* ============================================================================
 * L5: Secrecy Metrics Computation
 * ============================================================================ */

/**
 * compute_secrecy_metrics — Compute full secrecy metrics for a wiretap channel
 *
 * @param channel  Wiretap channel parameters
 * @param rate     Target secrecy rate (bits/s/Hz)
 * @param metrics  Output metrics struct
 */
void compute_secrecy_metrics(const wiretap_channel_t *channel,
                               double rate,
                               secrecy_metrics_t *metrics);

/**
 * compute_equivocation — Compute equivocation H(M|Z)
 *
 * For Gaussian wiretap: H(M|Z) = H(M) - I(M;Z)
 * This measures how uncertain Eve is about the message after
 * observing her channel output Z.
 *
 * @param message_entropy  H(M) — entropy of the message (bits)
 * @param leakage_rate     I(M;Z) — information leaked to Eve (bits/s/Hz)
 * @param block_length     Number of channel uses
 * @return Equivocation in bits
 */
double compute_equivocation(double message_entropy,
                              double leakage_rate,
                              int block_length);

/* ============================================================================
 * L8: RIS-Assisted Physical Layer Security
 * ============================================================================ */

/**
 * Reconfigurable Intelligent Surface (RIS) assisted secrecy:
 *
 * An RIS with N passive elements can be configured to:
 *   1. Beamform toward Bob (improve main channel SNR)
 *   2. Create destructive interference at Eve (reduce leakage)
 *
 * L9 Concept: Joint beamforming + RIS phase optimization for
 * maximizing secrecy rate is an emerging 6G research topic.
 */

typedef struct {
    int      num_elements;      /* Number of RIS elements */
    double  *phases;            /* Phase shifts (radians) */
    double   element_spacing;   /* In wavelengths */

    /* Locations (3D coordinates in meters) */
    double   tx_pos[3];         /* Transmitter */
    double   rx_pos[3];         /* Bob (legitimate receiver) */
    double   eve_pos[3];        /* Eve */
    double   ris_pos[3];        /* RIS center */
} ris_config_t;

/**
 * ris_optimize_secrecy — Greedy phase optimization for max secrecy rate
 *
 * Iteratively optimizes each RIS element's phase to maximize
 * the secrecy rate R_s = [log₂(1+SNR_bob) - log₂(1+SNR_eve)]⁺
 *
 * @param freq_hz  Carrier frequency (Hz)
 * @param max_iters Maximum iterations
 * @return Achieved secrecy rate (bits/s/Hz)
 */
double ris_optimize_secrecy(ris_config_t *ris, double freq_hz, int max_iters);

/**
 * ris_channel_gain — Compute end-to-end channel gain via RIS
 *
 * h_end_to_end = h_direct + h_via_ris
 * h_via_ris = Σ_i h_tx_ris[i] * e^(jφ_i) * h_ris_rx[i]
 *
 * @param ris    RIS configuration
 * @param target Target position (Bob or Eve)
 * @param freq_hz Carrier frequency
 * @return Channel gain magnitude squared |h|² (linear)
 */
double ris_channel_gain(const ris_config_t *ris,
                         const double *target_pos,
                         double freq_hz);

#ifdef __cplusplus
}
#endif

#endif /* WIRELESS_PHY_SEC_H */
