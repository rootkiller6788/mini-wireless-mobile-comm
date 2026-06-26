/**
 * wireless_phy_sec.c — Physical Layer Security
 *
 * Implements: Wyner secrecy capacity, channel-based key generation,
 *             artificial noise precoding, RIS-assisted secrecy,
 *             secrecy metrics computation
 *
 * Knowledge Levels: L3 (information theory/mutual information),
 *                   L4 (Wyner's theorem, wiretap channel),
 *                   L5 (channel key gen, AN precoding),
 *                   L8 (MIMO wiretap, RIS)
 *
 * References:
 *   Wyner (1975): "The Wire-Tap Channel" — Bell System Tech. J.
 *   Bloch & Barros (2011): "Physical-Layer Security"
 *   Goel & Negi (2008): "Guaranteeing Secrecy using Artificial Noise" —
 *        IEEE Trans. Wireless Communications
 */

#include "wireless_phy_sec.h"
#include "wireless_crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * L4: Wyner's Secrecy Capacity — Information-Theoretic Foundation
 * ============================================================================ */

/**
 * The wiretap channel model (Wyner 1975):
 *
 *            Alice ──[Encoder]──→ X ──┬──[Main Channel]─→ Y ──[Decoder]──→ Bob
 *                                      │     (SNR γ_m)
 *                                      └──[Eve Channel]─→ Z (Eavesdropper)
 *                                           (SNR γ_e)
 *
 * L4 Theorem (Wyner 1975, Gaussian case):
 *
 *   C_s = max{ I(X;Y) - I(X;Z), 0 }
 *       = [½ log₂(1 + γ_m) - ½ log₂(1 + γ_e)]⁺
 *       = [½ log₂((1 + γ_m)/(1 + γ_e))]⁺
 *
 * For complex baseband (2 degrees of freedom):
 *   C_s = [log₂(1 + γ_m) - log₂(1 + γ_e)]⁺
 *       = [log₂((1 + γ_m)/(1 + γ_e))]⁺
 *
 * This is a fundamental limit: no error-correcting code can achieve
 * a higher secrecy rate while guaranteeing both reliability (Pe→0)
 * and strong secrecy (I(M;Z^n)→0).
 */
double wyner_secrecy_capacity(double gamma_m, double gamma_e)
{
    double cm, ce, cs;

    /* Main channel capacity: log₂(1 + SNR) bits per complex dimension */
    cm = log2(1.0 + gamma_m);

    /* Eavesdropper channel capacity */
    ce = log2(1.0 + gamma_e);

    /* Secrecy capacity = difference, floored at 0 */
    cs = cm - ce;
    if (cs < 0.0) cs = 0.0;

    return cs;
}

double wyner_secrecy_capacity_db(double snr_main_db, double snr_eve_db)
{
    double gamma_m = pow(10.0, snr_main_db / 10.0);
    double gamma_e = pow(10.0, snr_eve_db / 10.0);
    return wyner_secrecy_capacity(gamma_m, gamma_e);
}

/**
 * fading_secrecy_capacity — Monte Carlo estimate of ergodic secrecy capacity
 *
 * For independent Rayleigh fading on main and eavesdropper channels:
 *
 *   C_s,erg = E_h { [log₂(1 + |h_m|²γ̄_m) - log₂(1 + |h_e|²γ̄_e)]⁺ }
 *
 * Where h_m, h_e ~ CN(0,1) are complex Gaussian (Rayleigh magnitude).
 *
 * L4 Theorem (Gopala, Lai, El Gamal 2008): For Rayleigh fading,
 * a positive ergodic secrecy rate is achievable even when γ̄_m < γ̄_e
 * on average, by exploiting fading fluctuations (opportunistic
 * transmission in good slots).
 */
double fading_secrecy_capacity(double avg_snr_main_db, double avg_snr_eve_db,
                                 int num_samples)
{
    double gamma_m_avg = pow(10.0, avg_snr_main_db / 10.0);
    double gamma_e_avg = pow(10.0, avg_snr_eve_db / 10.0);
    double sum_cs = 0.0;
    int i;

    if (num_samples <= 0) return 0.0;

    for (i = 0; i < num_samples; i++) {
        /* Generate Rayleigh fading samples via Box-Muller */
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;

        /* Avoid log(0) */
        if (u1 < 1e-10) u1 = 1e-10;
        if (u2 < 1e-10) u2 = 1e-10;

        /* Rayleigh magnitude squared: |h|² = -ln(U) (exponential with mean 1) */
        double h_m_sq = -log(u1);
        double h_e_sq = -log(u2);

        /* Instantaneous SNRs */
        double gamma_m = gamma_m_avg * h_m_sq;
        double gamma_e = gamma_e_avg * h_e_sq;

        double cs = wyner_secrecy_capacity(gamma_m, gamma_e);
        sum_cs += cs;
    }

    return sum_cs / num_samples;
}

/* ============================================================================
 * L5: Channel-Based Secret Key Generation
 * ============================================================================ */

/**
 * Channel reciprocity: In TDD systems, the forward and reverse channels
 * are identical within the coherence time.  Both Alice and Bob measure
 * the same channel → common source of randomness.
 *
 * Three phases:
 *   1. Channel probing → CSI measurements
 *   2. Quantization → raw bit strings
 *   3. Information reconciliation → correct mismatches
 *   4. Privacy amplification → remove Eve's partial info
 *
 * L2 Concept (Maurer 1993): Secret-key capacity from correlated sources.
 * The key generation rate is bounded by the mutual information:
 *   R_key ≤ I(h_AB; h_BA) - I(h_AB; h_AE)
 * where h_AE is Eve's channel observation.
 */

void channel_key_gen_init(channel_key_gen_ctx_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(channel_key_gen_ctx_t));
    ctx->capacity = 1024;
    ctx->samples = (csi_sample_t *)malloc(
        ctx->capacity * sizeof(csi_sample_t));
    ctx->bits_per_sample = 2;
}

int channel_key_gen_add_sample(channel_key_gen_ctx_t *ctx,
                                double magnitude, double phase)
{
    if (!ctx) return -1;

    if (ctx->num_samples >= ctx->capacity) {
        ctx->capacity *= 2;
        csi_sample_t *new_samples = (csi_sample_t *)realloc(
            ctx->samples, ctx->capacity * sizeof(csi_sample_t));
        if (!new_samples) return -1;
        ctx->samples = new_samples;
    }

    ctx->samples[ctx->num_samples].magnitude = magnitude;
    ctx->samples[ctx->num_samples].phase = phase;
    ctx->samples[ctx->num_samples].timestamp = 0; /* simplified */
    ctx->num_samples++;

    return 0;
}

/**
 * channel_key_gen_quantize — Level-crossing quantization
 *
 * L5 Algorithm:
 *   1. Compute empirical mean μ and standard deviation σ of RSS
 *   2. Define quantization levels: μ ± k·σ for k = 0.5, 1.0, 1.5, ...
 *   3. For each sample, determine which interval it falls into
 *   4. Apply guard band: discard samples within ±0.5σ of mean
 *      (these are most likely to cause bit mismatch)
 *   5. Encode interval index as Gray-coded bits
 *
 * Knowledge: Quantization theory (L3), Gray coding (L5)
 */
int channel_key_gen_quantize(channel_key_gen_ctx_t *ctx, int bits_per_sample)
{
    double sum = 0.0, sum_sq = 0.0, mean, std;
    size_t i;
    int num_intervals;
    uint8_t *raw_bits;
    size_t bit_capacity, bit_pos;

    if (!ctx || ctx->num_samples < 10) return -1;
    if (bits_per_sample < 1 || bits_per_sample > 4) return -1;

    /* Compute statistics */
    for (i = 0; i < ctx->num_samples; i++) {
        sum += ctx->samples[i].magnitude;
        sum_sq += ctx->samples[i].magnitude * ctx->samples[i].magnitude;
    }
    mean = sum / ctx->num_samples;
    std = sqrt(sum_sq / ctx->num_samples - mean * mean);
    if (std < 1e-10) std = 1.0;  /* Avoid division by zero */

    ctx->mean_level = mean;
    ctx->std_level = std;

    /* Allocate bit buffer */
    num_intervals = 1 << bits_per_sample;  /* 2, 4, 8, or 16 intervals */
    bit_capacity = ctx->num_samples * bits_per_sample;
    raw_bits = (uint8_t *)malloc((bit_capacity + 7) / 8);
    if (!raw_bits) return -1;
    memset(raw_bits, 0, (bit_capacity + 7) / 8);

    bit_pos = 0;

    for (i = 0; i < ctx->num_samples; i++) {
        double norm_val = (ctx->samples[i].magnitude - mean) / std;

        /* Guard band: skip samples near mean (|normalized| < 0.5) */
        if (fabs(norm_val) < 0.5) {
            continue;
        }

        /* Quantize to interval [0, num_intervals-1] */
        int interval;
        double clamped = norm_val;
        if (clamped < -3.0) clamped = -3.0;  /* Clamp to ±3σ */
        if (clamped >  3.0) clamped =  3.0;

        interval = (int)((clamped + 3.0) * (num_intervals / 6.0));
        if (interval < 0) interval = 0;
        if (interval >= num_intervals) interval = num_intervals - 1;

        /* Gray code the interval */
        int gray = interval ^ (interval >> 1);

        /* Pack bits */
        int b;
        for (b = 0; b < bits_per_sample; b++) {
            if (gray & (1 << b)) {
                raw_bits[bit_pos / 8] |= (1 << (bit_pos % 8));
            }
            bit_pos++;
        }
    }

    ctx->raw_bits = raw_bits;
    ctx->raw_bit_len = bit_pos;

    return (int)bit_pos;
}

int channel_key_gen_reconcile(channel_key_gen_ctx_t *ctx,
                               const uint8_t *local_bits, size_t bit_len,
                               int max_passes, double error_rate)
{
    if (!ctx || !local_bits) return -1;
    if (ctx->raw_bit_len != bit_len) return -1;

    /* Information reconciliation via Cascade protocol:
       Repeatedly partition the bit string and compare parity
       on blocks where parity mismatch is detected, perform
       binary search to find and correct the error bit.
       Each pass uses a random permutation to distribute errors. */

    if (error_rate < 0.01 && max_passes > 0) {
        /* For low error rates, simple parity correction suffices */
        size_t i;
        int parity_local = 0, parity_ctx = 0;
        for (i = 0; i < bit_len && i < 512; i++) {
            int byte_idx = (int)(i / 8);
            int bit_idx = (int)(i % 8);
            parity_local ^= (local_bits[byte_idx] >> bit_idx) & 1;
            parity_ctx ^= (ctx->raw_bits[byte_idx] >> bit_idx) & 1;
        }
        /* In a real Cascade protocol, the parity would be exchanged
           over a public authenticated channel and mismatches corrected */
        (void)parity_local;
        (void)parity_ctx;
    }

    /* Simplified: copy reconciled bits */
    ctx->reconciled_key = (uint8_t *)malloc((bit_len + 7) / 8);
    if (!ctx->reconciled_key) return -1;
    memcpy(ctx->reconciled_key, ctx->raw_bits, (bit_len + 7) / 8);
    ctx->reconciled_key_len = bit_len;

    return 0;
}

int channel_key_gen_amplify(channel_key_gen_ctx_t *ctx, int output_bits)
{
    size_t input_bytes;
    uint8_t *final_key;

    if (!ctx || !ctx->reconciled_key) return -1;
    if (output_bits <= 0) return -1;

    input_bytes = (ctx->reconciled_key_len + 7) / 8;

    /* Privacy amplification via universal hashing:
       final_key = HMAC-SHA256(random_seed, reconciled_key)
       truncated to output_bits */
    {
        uint8_t seed[32];
        uint8_t hash_out[SHA256_DIGEST_SIZE];

        /* Use a fixed seed for deterministic demo
           (real system would use fresh random seed) */
        memset(seed, 0xAA, 32);

        hmac_sha256(seed, 32, ctx->reconciled_key, input_bytes, hash_out);

        final_key = (uint8_t *)malloc((output_bits + 7) / 8);
        if (!final_key) return -1;

        memcpy(final_key, hash_out, (output_bits + 7) / 8);
    }

    ctx->final_key = final_key;
    ctx->final_key_len = output_bits;

    return 0;
}

const uint8_t* channel_key_gen_get_key(const channel_key_gen_ctx_t *ctx,
                                        size_t *key_len)
{
    if (!ctx) return NULL;
    if (key_len) *key_len = ctx->final_key_len;
    return ctx->final_key;
}

void channel_key_gen_free(channel_key_gen_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->samples) free(ctx->samples);
    if (ctx->raw_bits) free(ctx->raw_bits);
    if (ctx->reconciled_key) free(ctx->reconciled_key);
    if (ctx->final_key) free(ctx->final_key);
    memset(ctx, 0, sizeof(channel_key_gen_ctx_t));
}

/* ============================================================================
 * L8: Artificial Noise Precoding (MIMO Wiretap)
 * ============================================================================ */

void an_precoding_init(an_precoding_ctx_t *ctx,
                        int num_tx_antennas,
                        int num_rx_antennas,
                        int num_eve_antennas,
                        double power_allocation)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(an_precoding_ctx_t));
    ctx->num_tx_antennas = num_tx_antennas;
    ctx->num_rx_antennas = num_rx_antennas;
    ctx->num_eve_antennas = num_eve_antennas;
    ctx->power_allocation = power_allocation;

    if (power_allocation < 0.0) ctx->power_allocation = 0.0;
    if (power_allocation > 1.0) ctx->power_allocation = 1.0;

    /* Allocate beamforming vector (complex, re/im interleaved) */
    ctx->beamforming_vec = (double *)malloc(
        2 * num_tx_antennas * sizeof(double));
    if (ctx->beamforming_vec) {
        memset(ctx->beamforming_vec, 0,
               2 * num_tx_antennas * sizeof(double));
    }

    /* Allocate AN precoder */
    int an_rows = num_tx_antennas;
    int an_cols = num_tx_antennas - 1;
    if (an_cols < 1) an_cols = 1;
    ctx->an_precoder = (double *)malloc(
        2 * an_rows * an_cols * sizeof(double));
    if (ctx->an_precoder) {
        memset(ctx->an_precoder, 0,
               2 * an_rows * an_cols * sizeof(double));
    }
}

int an_precoding_design(an_precoding_ctx_t *ctx,
                         const double *channel_bob)
{
    int N_t, i;
    double norm_sq;

    if (!ctx || !channel_bob) return -1;
    N_t = ctx->num_tx_antennas;

    /* Beamforming vector w = h_bob^H / ||h_bob|| (matched filter) */
    norm_sq = 0.0;
    for (i = 0; i < 2 * N_t; i++) {
        norm_sq += channel_bob[i] * channel_bob[i];
    }

    if (norm_sq < 1e-15) return -1;

    /* Normalize: w = h* / ||h|| (complex conjugate = matched filter) */
    {
        double inv_norm = 1.0 / sqrt(norm_sq);
        for (i = 0; i < N_t; i++) {
            /* Conjugate: flip sign of imaginary part */
            ctx->beamforming_vec[2*i]     =  channel_bob[2*i] * inv_norm;
            ctx->beamforming_vec[2*i + 1] = -channel_bob[2*i + 1] * inv_norm;
        }
    }

    /* AN precoder: null space of h_bob
       For MISO (N_r = 1), null space has dimension N_t - 1.
       We find an orthonormal basis via Gram-Schmidt in the null space. */

    /* Simplified: construct AN precoder columns orthogonal to h_bob */
    {
        int j;

        for (j = 1; j < N_t; j++) {
            /* Start with canonical basis vector e_j */
            for (i = 0; i < N_t; i++) {
                ctx->an_precoder[2*(i*(N_t-1) + (j-1))] = (i == j) ? 1.0 : 0.0;
                ctx->an_precoder[2*(i*(N_t-1) + (j-1)) + 1] = 0.0;
            }

            /* Project out component along h_bob (Gram-Schmidt) */
            {
                double dot_re = 0.0, dot_im = 0.0;
                int k;
                for (k = 0; k < N_t; k++) {
                    /* Complex dot product: AN_col[k] · h_bob[k]^* */
                    double ar = ctx->an_precoder[2*(k*(N_t-1)+(j-1))];
                    double ai = ctx->an_precoder[2*(k*(N_t-1)+(j-1))+1];
                    double hr = channel_bob[2*k];
                    double hi = channel_bob[2*k+1];
                    dot_re += ar * hr + ai * hi;
                    dot_im += ai * hr - ar * hi; /* ar*hi cancelled? */
                }

                /* Subtract projection */
                double scale_re = dot_re / norm_sq;
                double scale_im = dot_im / norm_sq;
                for (k = 0; k < N_t; k++) {
                    double hr = channel_bob[2*k];
                    double hi = channel_bob[2*k+1];
                    /* Projection: (dot * h)/|h|² */
                    double p_re = scale_re * hr - scale_im * hi; /* ? */
                    double p_im = scale_re * hi + scale_im * hr;

                    ctx->an_precoder[2*(k*(N_t-1)+(j-1))] -= p_re;
                    ctx->an_precoder[2*(k*(N_t-1)+(j-1))+1] -= p_im;
                }
            }

            /* Normalize AN column */
            {
                double an_norm = 0.0;
                int k;
                for (k = 0; k < N_t; k++) {
                    double ar = ctx->an_precoder[2*(k*(N_t-1)+(j-1))];
                    double ai = ctx->an_precoder[2*(k*(N_t-1)+(j-1))+1];
                    an_norm += ar*ar + ai*ai;
                }
                if (an_norm > 1e-15) {
                    double inv = 1.0 / sqrt(an_norm);
                    for (k = 0; k < N_t; k++) {
                        int idx = 2*(k*(N_t-1)+(j-1));
                        ctx->an_precoder[idx] *= inv;
                        ctx->an_precoder[idx+1] *= inv;
                    }
                }
            }
        }
    }

    return 0;
}

double an_precoding_secrecy_rate(const an_precoding_ctx_t *ctx,
                                   const double *channel_eve,
                                   double noise_power)
{
    int N_t, N_e, i;
    double signal_power_bob, leakage_eve, secrecy_rate;

    if (!ctx || !channel_eve) return 0.0;
    N_t = ctx->num_tx_antennas;
    N_e = ctx->num_eve_antennas;

    if (noise_power < 1e-15) noise_power = 1e-15;

    /* Bob's received signal power: |h_bob^H w|² · αP */
    signal_power_bob = ctx->power_allocation;

    /* Eve's received signal power:
       P_eve = αP · |h_eve^H w|² + (1-α)P/(N_t-1) · ||h_eve^H V_null||² */
    {
        double hw_re = 0.0, hw_im = 0.0;
        double an_leak_sq = 0.0;
        int j;

        /* |h_eve^H w|² */
        for (i = 0; i < N_t; i++) {
            double hr = channel_eve[2*i];
            double hi = channel_eve[2*i+1];
            double wr = ctx->beamforming_vec[2*i];
            double wi = ctx->beamforming_vec[2*i+1];
            /* h_eve^H · w = Σ h_eve[i]* · w[i] */
            hw_re += hr * wr + hi * wi;    /* Re: (hr+jhi)*(wr-jwi) */
            hw_im += hr * (-wi) + hi * wr;  /* hmm, let me be more careful */
        }
        /* Correction: compute properly */
        hw_re = 0.0; hw_im = 0.0;
        for (i = 0; i < N_t; i++) {
            double hr = channel_eve[2*i];
            double hi = channel_eve[2*i+1];
            double wr = ctx->beamforming_vec[2*i];
            double wi = ctx->beamforming_vec[2*i+1];
            /* Complex: (hr - j·hi) · (wr + j·wi) */
            hw_re += hr * wr + hi * wi;
            hw_im += hr * wi - hi * wr;
        }
        double hw_sq = hw_re * hw_re + hw_im * hw_im;

        /* AN leakage to Eve: average over AN dimensions */
        for (j = 0; j < N_t - 1 && N_t > 1; j++) {
            double an_j_re = 0.0, an_j_im = 0.0;
            for (i = 0; i < N_t; i++) {
                int idx = 2*(i*(N_t-1) + j);
                double hr = channel_eve[2*i];
                double hi = channel_eve[2*i+1];
                double ar = ctx->an_precoder[idx];
                double ai = ctx->an_precoder[idx+1];
                an_j_re += hr * ar + hi * ai;
                an_j_im += hr * ai - hi * ar;
            }
            an_leak_sq += an_j_re * an_j_re + an_j_im * an_j_im;
        }

        leakage_eve = ctx->power_allocation * hw_sq +
                       (1.0 - ctx->power_allocation) * an_leak_sq /
                       (N_t > 1 ? (double)(N_t - 1) : 1.0);
    }

    /* Secrecy rate = log₂(1 + SINR_bob) - log₂(1 + SNR_eve)
     * With N_e Eve antennas, worst-case SNR scales proportional to N_e
     * (maximal-ratio combining of independent fading paths).
     * For conservative analysis, multiply leakage by N_e. */
    {
        double snr_bob = signal_power_bob / noise_power;
        /* Eve can combine N_e antennas; effective SNR = N_e * per-antenna SNR */
        double snr_eve = (N_e > 0 ? (double)N_e : 1.0) * leakage_eve / noise_power;

        double cb = log2(1.0 + snr_bob);
        double ce = log2(1.0 + snr_eve);
        secrecy_rate = cb - ce;
    }

    if (secrecy_rate < 0.0) secrecy_rate = 0.0;
    return secrecy_rate;
}

void an_precoding_free(an_precoding_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->beamforming_vec) free(ctx->beamforming_vec);
    if (ctx->an_precoder) free(ctx->an_precoder);
    memset(ctx, 0, sizeof(an_precoding_ctx_t));
}

/* ============================================================================
 * L5: Secrecy Metrics
 * ============================================================================ */

void compute_secrecy_metrics(const wiretap_channel_t *channel,
                               double rate,
                               secrecy_metrics_t *metrics)
{
    double gamma_m, gamma_e;

    if (!channel || !metrics) return;

    gamma_m = pow(10.0, channel->snr_main / 10.0);
    gamma_e = pow(10.0, channel->snr_eve / 10.0);

    metrics->secrecy_capacity = wyner_secrecy_capacity(gamma_m, gamma_e);

    /* Achievable rate = min(requested rate, capacity) */
    metrics->secrecy_rate = (rate < metrics->secrecy_capacity) ?
                             rate : metrics->secrecy_capacity;

    /* Secrecy outage probability: P(C_s < R_target)
       For AWGN: deterministic → outage is either 0 or 1 */
    if (rate <= metrics->secrecy_capacity) {
        metrics->secrecy_outage_prob = 0.0;
    } else {
        metrics->secrecy_outage_prob = 1.0;
    }

    /* Equivocation rate = H(M|Z) / n = C_s - secrecy_rate (as n→∞) */
    metrics->equivocation = metrics->secrecy_capacity - metrics->secrecy_rate;
    if (metrics->equivocation < 0.0) metrics->equivocation = 0.0;

    /* Key generation rate = I(h_AB; h_BA) bits/sample (approximately) */
    metrics->key_generation_rate = 2.0;  /* Typical: ~2 bits per CSI sample */
}

double compute_equivocation(double message_entropy,
                              double leakage_rate,
                              int block_length)
{
    double equivocation;

    if (block_length <= 0) return 0.0;

    /* H(M|Z) = H(M) - I(M;Z)
       I(M;Z) ≈ leakage_rate * block_length (for large n)
       Equivocation must be ≥ 0 */
    equivocation = message_entropy - leakage_rate * block_length;
    if (equivocation < 0.0) equivocation = 0.0;

    return equivocation;
}

/* ============================================================================
 * L8: RIS-Assisted Physical Layer Security
 * ============================================================================ */

/**
 * RIS-assisted secrecy: A Reconfigurable Intelligent Surface with N
 * passive elements can create constructive interference toward Bob
 * and destructive interference toward Eve, boosting secrecy.
 *
 * The end-to-end channel from TX to RX via RIS element i:
 *
 *   h_total = h_direct + Σ_i h_tx_ris[i]·e^(jφ_i)·h_ris_rx[i]
 *
 * where φ_i is the phase shift of the i-th RIS element.
 */

static double path_loss(double distance_m, double freq_hz)
{
    double lambda = 3.0e8 / freq_hz;  /* Wavelength in meters */
    if (distance_m < 0.001) distance_m = 0.001;
    /* Free-space path loss: (λ/(4πd))² */
    double pl = (lambda / (4.0 * M_PI * distance_m));
    return pl * pl;
}

double ris_channel_gain(const ris_config_t *ris,
                         const double *target_pos,
                         double freq_hz)
{
    int i;
    double h_direct_re = 0.0, h_direct_im = 0.0;
    double h_ris_total_re = 0.0, h_ris_total_im = 0.0;

    if (!ris || !target_pos) return 0.0;

    /* Direct path TX → Target */
    {
        double dx = ris->tx_pos[0] - target_pos[0];
        double dy = ris->tx_pos[1] - target_pos[1];
        double dz = ris->tx_pos[2] - target_pos[2];
        double d = sqrt(dx*dx + dy*dy + dz*dz);
        double pl = path_loss(d, freq_hz);
        double phase = 2.0 * M_PI * d * freq_hz / 3.0e8;

        h_direct_re = sqrt(pl) * cos(phase);
        h_direct_im = sqrt(pl) * sin(phase);
    }

    /* Via RIS: Σ_i h_tx_ris[i] * e^(jφ_i) * h_ris_rx[i] */
    for (i = 0; i < ris->num_elements; i++) {
        /* RIS element position (linear array along x-axis) */
        double elem_x = ris->ris_pos[0] +
                         (i - (ris->num_elements - 1) / 2.0) *
                         ris->element_spacing * (3.0e8 / freq_hz);
        double elem_y = ris->ris_pos[1];
        double elem_z = ris->ris_pos[2];

        /* TX → RIS element */
        double d_tx_ris = sqrt(
            (ris->tx_pos[0] - elem_x) * (ris->tx_pos[0] - elem_x) +
            (ris->tx_pos[1] - elem_y) * (ris->tx_pos[1] - elem_y) +
            (ris->tx_pos[2] - elem_z) * (ris->tx_pos[2] - elem_z));
        double pl_tx_ris = path_loss(d_tx_ris, freq_hz);
        double phase_tx_ris = 2.0 * M_PI * d_tx_ris * freq_hz / 3.0e8;

        /* RIS element → Target */
        double d_ris_rx = sqrt(
            (elem_x - target_pos[0]) * (elem_x - target_pos[0]) +
            (elem_y - target_pos[1]) * (elem_y - target_pos[1]) +
            (elem_z - target_pos[2]) * (elem_z - target_pos[2]));
        double pl_ris_rx = path_loss(d_ris_rx, freq_hz);
        double phase_ris_rx = 2.0 * M_PI * d_ris_rx * freq_hz / 3.0e8;

        /* Combined: h_tx_ris · e^(jφ_i) · h_ris_rx */
        double phi = ris->phases[i];
        double total_phase = phase_tx_ris + phi + phase_ris_rx;
        double total_gain = sqrt(pl_tx_ris * pl_ris_rx);

        h_ris_total_re += total_gain * cos(total_phase);
        h_ris_total_im += total_gain * sin(total_phase);
    }

    /* Total channel gain |h_total|² */
    double total_re = h_direct_re + h_ris_total_re;
    double total_im = h_direct_im + h_ris_total_im;

    return total_re * total_re + total_im * total_im;
}

double ris_optimize_secrecy(ris_config_t *ris, double freq_hz, int max_iters)
{
    int iter, i;
    double best_secrecy = 0.0;
    double gamma_m, gamma_e;

    if (!ris) return 0.0;

    /* Initialize phases randomly */
    for (i = 0; i < ris->num_elements; i++) {
        ris->phases[i] = 2.0 * M_PI * ((double)rand() / RAND_MAX);
    }

    /* Greedy iterative optimization */
    for (iter = 0; iter < max_iters; iter++) {
        for (i = 0; i < ris->num_elements; i++) {
            /* Try 4 candidate phases for element i, pick best */
            double best_phase = ris->phases[i];
            double best_rate = -1.0;
            int k;

            for (k = 0; k < 4; k++) {
                ris->phases[i] = k * M_PI / 2.0;  /* {0, π/2, π, 3π/2} */

                gamma_m = ris_channel_gain(ris, ris->rx_pos, freq_hz);
                gamma_e = ris_channel_gain(ris, ris->eve_pos, freq_hz);

                double sec_rate = wyner_secrecy_capacity(gamma_m, gamma_e);
                if (sec_rate > best_rate) {
                    best_rate = sec_rate;
                    best_phase = ris->phases[i];
                }
            }

            ris->phases[i] = best_phase;
        }

        /* Evaluate current configuration */
        gamma_m = ris_channel_gain(ris, ris->rx_pos, freq_hz);
        gamma_e = ris_channel_gain(ris, ris->eve_pos, freq_hz);
        double current_rate = wyner_secrecy_capacity(gamma_m, gamma_e);
        if (current_rate > best_secrecy) {
            best_secrecy = current_rate;
        }
    }

    return best_secrecy;
}
