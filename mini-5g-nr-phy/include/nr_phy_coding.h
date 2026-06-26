/**
 * nr_phy_coding.h — 5G NR LDPC & Polar Channel Coding
 *
 * Knowledge Coverage:
 *   L1 Definitions: LDPC base graphs (BG1/BG2), lifting size, Polar
 *                   reliability sequence, rate matching
 *   L2 Core Concepts: Channel coding, belief propagation, SC decoding
 *   L3 Math Structures: GF(2) matrices, parity check, Tanner graph
 *   L5 Algorithms: LDPC encode/decode (min-sum BP), Polar encode/SC decode
 *   L6 Canonical Problems: LDPC rate matching, Polar interleaving
 *   L8 Advanced: Layered decoding, list decoding for Polar
 *
 * Course: MIT 6.450, Stanford EE359, ETH 227-0436
 * Ref: 3GPP TS 38.212 v17.0.0
 */

#ifndef NR_PHY_CODING_H
#define NR_PHY_CODING_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LDPC BG matrix dimensions (3GPP TS 38.212 Section 5.3.2) */
#define NR_LDPC_BG1_MAX_K  8448  /* Max info bits for BG1 */
#define NR_LDPC_BG2_MAX_K  3840  /* Max info bits for BG2 */
#define NR_LDPC_BG1_NCOL   68    /* BG1: 68 columns base */
#define NR_LDPC_BG1_NROW   46    /* BG1: 46 rows base */
#define NR_LDPC_BG2_NCOL   52    /* BG2: 52 columns */
#define NR_LDPC_BG2_NROW   42    /* BG2: 42 rows */
#define NR_LDPC_MAX_Z      384   /* Max lifting size */
#define NR_LDPC_LIFT_SET_SIZE 8  /* Z_c values in each lifting set */
#define NR_POLAR_NMAX      1024  /* Max Polar code length */

/** LDPC base graph selection */
typedef enum {
    NR_LDPC_BG1 = 1,
    NR_LDPC_BG2 = 2
} nr_ldpc_bg_type_t;

/** LDPC encoder context */
typedef struct {
    nr_ldpc_bg_type_t bg;       /* BG1 or BG2 */
    int     Z_c;                /* Lifting size */
    int     K;                  /* Info bits after segmentation */
    int     N;                  /* Codeword bits after lifting */
    int     num_sys_cols;       /* Systematic columns */
    int     num_info_cols;      /* Info columns */
    int     num_parity_cols;    /* Parity columns */
    int     num_core_parity;    /* Core parity check columns */
    int     num_ext_parity;     /* Extension parity columns */
    /* Punctured systematic bits count */
    int     num_punctured;
} nr_ldpc_enc_ctx_t;

/** LDPC decoder context (min-sum BP) */
typedef struct {
    nr_ldpc_enc_ctx_t enc_ctx;  /* Encoder parameters */
    int     max_iterations;     /* Max BP iterations */
    double  *llr_in;            /* Input LLRs (N bits) */
    double  *llr_out;           /* Output LLRs (K bits) */
    int     *hard_decision;     /* Decoded bits */
    int     converged;          /* Parity check satisfied */
    int     iterations_used;    /* Actual iterations run */
    /* Internal: variable-to-check / check-to-variable messages */
    double  *v2c;
    double  *c2v;
} nr_ldpc_dec_ctx_t;

/** Polar encoder context */
typedef struct {
    int     N;                  /* Code length (power of 2, <= 1024) */
    int     K;                  /* Info bits (+ CRC if used) */
    int     n;                  /* n = log2(N) */
    int     *info_set;          /* Indices of information bit positions */
    int     *frozen_set;        /* Indices of frozen bit positions (set to 0) */
    int     *frozen_values;     /* Values at frozen positions */
    int     crc_length;         /* CRC bits (6 or 11 for NR) */
    int     crc_polynomial;     /* CRC generator polynomial */
    int     rate_match_mode;    /* 0=block, 1=puncturing, 2=shortening */
    int     E;                  /* Rate-matched output length */
} nr_polar_enc_ctx_t;

/** Polar decoder context (SC / SCL) */
typedef struct {
    nr_polar_enc_ctx_t enc_ctx; /* Corresponding encoder */
    int     list_size;          /* L for SCL (1 = SC) */
    double  *llr_in;            /* Input LLRs */
    int     *decoded_bits;      /* Output bits */
    int     crc_pass;           /* CRC check result */
    int     path_metrics_valid; /* Path metric validity */
} nr_polar_dec_ctx_t;

/** Rate matching context for LDPC (3GPP TS 38.212 5.4.2) */
typedef struct {
    int     E;                  /* Rate-matched output length */
    int     rv;                 /* Redundancy version (0..3) */
    int     N_cb;               /* Circular buffer length */
    int     k0;                 /* Starting position in circular buffer */
    int     ilv_mode;           /* Interleaving mode */
} nr_rate_match_ctx_t;

/* ============================================================================
 * L5: LDPC Encoding / Decoding
 * ============================================================================ */

/**
 * nr_ldpc_init — Initialize LDPC encoder for given transport block
 *
 * 3GPP TS 38.212 Section 5.3.2: Select BG1 or BG2 based on
 * TBS and code rate. BG1 for large blocks/high rates,
 * BG2 for small blocks/low rates.
 *
 * BG1: 46x68, max K=8448. BG2: 42x52, max K=3840.
 *
 * Lifting: Z_c = a * 2^j where a in {2,3,5,7,9,11,13,15}.
 * Select the smallest Z_c in the set s.t. Z_c * K_b >= K_input.
 *
 * @param ctx     Output encoder context
 * @param bg      BG1 or BG2
 * @param K       Number of information bits (including CRC)
 * @param rate    Target code rate (as double, e.g., 0.5)
 * @return 0 on success
 */
int nr_ldpc_init(nr_ldpc_enc_ctx_t *ctx, nr_ldpc_bg_type_t bg,
                  int K, double rate);

/**
 * nr_ldpc_encode — LDPC encoding per 3GPP TS 38.212
 *
 * L5: For systematic LDPC, the codeword c = [u | p] where
 * u = information bits, p = parity bits.
 *
 * Parity bits are computed such that H * c^T = 0 over GF(2).
 * The NR LDPC base graph has a special structure facilitating
 * efficient encoding using double-diagonal core parity.
 *
 * Complexity: O(N * Z_c^2) for dense, O(N * d_v * Z_c) for sparse.
 *
 * @param ctx       Encoder context
 * @param info_bits Information bits (K bits, packed as bytes)
 * @param codeword  Output codeword (N bits, packed as bytes)
 */
void nr_ldpc_encode(const nr_ldpc_enc_ctx_t *ctx,
                      const uint8_t *info_bits,
                      uint8_t *codeword);

/**
 * nr_ldpc_rate_match — LDPC rate matching
 *
 * 3GPP TS 38.212 Section 5.4.2: Circular buffer with RV-based
 * starting position. Bit interleaving is applied after selection.
 *
 * Four redundancy versions (RV 0..3) enable incremental redundancy HARQ.
 * RV=0 starts near the beginning for first transmission.
 *
 * @param rm_ctx    Rate matching context
 * @param codeword  Encoded codeword (N bits)
 * @param output    Rate-matched output (E bits)
 * @return Number of bits output (E)
 */
int nr_ldpc_rate_match(nr_rate_match_ctx_t *rm_ctx,
                        const uint8_t *codeword,
                        uint8_t *output);

/**
 * nr_ldpc_rate_recover — Reverse rate matching (fill LLRs)
 *
 * Maps received LLRs back to the circular buffer for decoding.
 * Missing positions (punctured/shortened) get LLR=0 (no information).
 */
void nr_ldpc_rate_recover(const nr_rate_match_ctx_t *rm_ctx,
                           const double *llr_in,
                           double *llr_buffer, int buffer_len);

/**
 * nr_ldpc_decode_init — Initialize LDPC decoder
 *
 * Allocates message storage for min-sum belief propagation.
 *
 * @param dec       Output decoder context
 * @param enc_ctx   Encoder context (shared parameters)
 * @param max_iter  Maximum BP iterations
 * @return 0 on success
 */
int nr_ldpc_decode_init(nr_ldpc_dec_ctx_t *dec,
                          const nr_ldpc_enc_ctx_t *enc_ctx,
                          int max_iter);

/**
 * nr_ldpc_decode_min_sum — Min-sum LDPC belief propagation decoding
 *
 * L5 (Fossorier et al. 1999): Min-sum approximation to BP decoding.
 *
 * Variable→Check: V2C = LLR_in + sum(C2V from other checks)
 * Check→Variable: C2V = product(sign(V2C)) * min(abs(V2C))
 *                     * alpha (scaling factor ~ 0.75)
 *
 * Termination: Parity check H * x_hat^T = 0 or max_iter reached.
 *
 * Complexity: O(N * d_v * I_max) where d_v is average variable degree.
 *
 * @param ctx       Decoder context (with input LLRs filled in)
 * @param llr_in    Channel LLRs (N bits)
 * @param output    Hard decision bits (K info bits)
 * @return 0 = converged, 1 = max iterations, -1 = error
 */
int nr_ldpc_decode_min_sum(nr_ldpc_dec_ctx_t *ctx,
                             const double *llr_in,
                             uint8_t *output);

/**
 * nr_ldpc_decode_free — Free decoder resources
 */
void nr_ldpc_decode_free(nr_ldpc_dec_ctx_t *ctx);

/**
 * nr_ldpc_llr_to_hard — Convert LLRs to hard decisions
 *
 * bit = 0 if LLR >= 0 else 1
 */
void nr_ldpc_llr_to_hard(const double *llr, int len, uint8_t *bits);

/* ============================================================================
 * L5: Polar Encoding / Decoding
 * ============================================================================ */

/**
 * nr_polar_init — Initialize Polar encoder
 *
 * 3GPP TS 38.212 Section 5.3.1: Polar codes for control channels.
 *
 * N is the smallest power of 2 >= K + n_pc + CRC.
 * N_max = 1024 for DL, 512 for UL.
 *
 * L5 (Arikan 2009): Polar codes achieve symmetric capacity of any
 * B-DMC as N → ∞ with SC decoding. The key insight is channel
 * polarization: repeated applications of W → (W^-, W^+) create
 * "good" and "bad" bit-channels.
 *
 * The NR reliability sequence Q_N^max determines which positions
 * carry information vs. frozen bits.
 *
 * @param ctx       Output encoder context
 * @param K         Number of information bits
 * @param E         Rate-matched output length
 * @param crc_len   CRC length (6 for K<20, 11 for K>=20 per NR)
 * @return 0 on success
 */
int nr_polar_init(nr_polar_enc_ctx_t *ctx, int K, int E, int crc_len);

/**
 * nr_polar_encode — Polar encoding
 *
 * c = u * G_N where G_N = F^{⊗n} (n-th Kronecker power of F = [1 0; 1 1]).
 *
 * This is implemented efficiently via the recursive factor graph
 * using in-place butterfly operations: O(N log N).
 *
 * @param ctx        Encoder context
 * @param info_bits  Information bits (packed)
 * @param codeword   Output codeword (N bits, packed)
 */
void nr_polar_encode(const nr_polar_enc_ctx_t *ctx,
                       const uint8_t *info_bits,
                       uint8_t *codeword);

/**
 * nr_polar_subblock_interleave — NR Polar sub-block interleaver
 *
 * 3GPP TS 38.212 Section 5.4.1.1: 32-subblock interleaver
 * applied before rate matching to improve robustness.
 */
void nr_polar_subblock_interleave(const uint8_t *in, int N,
                                   uint8_t *out);

/**
 * nr_polar_rate_match — Polar rate matching
 *
 * 3GPP TS 38.212 Section 5.4.1:
 *   - Puncturing: remove first N-E bits
 *   - Shortening: remove last N-E bits
 *   - Repetition: repeat bits if E > N
 */
int nr_polar_rate_match(const uint8_t *codeword, int N, int E,
                         uint8_t *output);

/**
 * nr_polar_rate_recover — Reverse polar rate matching
 *
 * Fills LLRs back to length N for decoding.
 */
void nr_polar_rate_recover(const double *llr_in, int E,
                            int N, int rate_match_mode,
                            double *llr_out);

/**
 * nr_polar_decode_sc — Successive Cancellation (SC) decoding
 *
 * L5 (Arikan 2009): SC decoding processes bits sequentially.
 * For each information bit i:
 *   LLR_i = f(L_child, R_child)  (f = node operation)
 *   u_i = 0 if LLR_i >= 0 else 1
 *
 * SC is optimal for N → ∞ but suboptimal at finite N.
 *
 * Complexity: O(N log N) with O(N) memory.
 *
 * @param ctx        Decoder context
 * @param llr_in     Input channel LLRs (N values)
 * @param output     Decoded bits (K info bits, packed)
 * @return 0 on success, -1 on CRC fail
 */
int nr_polar_decode_sc(nr_polar_dec_ctx_t *ctx,
                         const double *llr_in,
                         uint8_t *output);

/**
 * nr_polar_decode_scl — Successive Cancellation List (SCL) decoding
 *
 * L8 (Tal & Vardy 2015): Maintains L parallel decoding paths.
 * At each information bit, each path splits into 2 (for u_i=0 and 1).
 * The L best paths are kept based on path metric.
 *
 * CRC-aided SCL (CA-SCL) selects the CRC-passing path as output.
 * CA-SCL with L=8 approaches ML performance.
 *
 * Complexity: O(L * N log N).
 *
 * @param ctx        Decoder (with list_size set)
 * @param llr_in     Input LLRs
 * @param output     Decoded bits
 * @return 0 on success
 */
int nr_polar_decode_scl(nr_polar_dec_ctx_t *ctx,
                          const double *llr_in,
                          uint8_t *output);

/**
 * nr_polar_dec_free — Free decoder resources
 */
void nr_polar_dec_free(nr_polar_dec_ctx_t *ctx);

/* ============================================================================
 * L3: Utility — CRC Calculation (3GPP TS 38.212 Section 5.1)
 * ============================================================================ */

/**
 * nr_crc24c — CRC-24C calculation
 *
 * g_CRC24C(D) = D^24 + D^23 + D^21 + D^20 + D^17 + D^15 + D^13 + D^12 +
 *               D^8 + D^4 + D^2 + D + 1
 *
 * Used for LDPC-coded transport blocks.
 *
 * @param data  Input bits (packed)
 * @param len   Number of bits
 * @return 24-bit CRC
 */
uint32_t nr_crc24c(const uint8_t *data, int len);

/**
 * nr_crc24a — CRC-24A
 *
 * g_CRC24A(D) = D^24 + D^23 + D^18 + D^17 + D^14 + D^11 + D^10 + D^7 +
 *               D^6 + D^5 + D^4 + D^3 + D + 1
 *
 * Used for HARQ-ACK, SR, and other control.
 */
uint32_t nr_crc24a(const uint8_t *data, int len);

/**
 * nr_crc16 — CRC-16
 *
 * g_CRC16(D) = D^16 + D^12 + D^5 + 1
 */
uint16_t nr_crc16(const uint8_t *data, int len);

/**
 * nr_crc6 — CRC-6 for Polar codes
 *
 * g_CRC6(D) = D^6 + D^5 + 1
 */
uint8_t nr_crc6(const uint8_t *data, int len);

/**
 * nr_crc11 — CRC-11 for Polar codes (larger blocks)
 *
 * g_CRC11(D) = D^11 + D^10 + D^9 + D^5 + 1
 */
uint16_t nr_crc11(const uint8_t *data, int len);

/* ============================================================================
 * L3: Bit operations utility
 * ============================================================================ */

/**
 * nr_bits_pack — Pack unpacked bits (1 per byte) into bytes (8 per byte)
 */
void nr_bits_pack(const uint8_t *bits, int nbits, uint8_t *packed);

/**
 * nr_bits_unpack — Unpack bytes into individual bits
 */
void nr_bits_unpack(const uint8_t *packed, int nbytes, uint8_t *bits, int nbits);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_CODING_H */
