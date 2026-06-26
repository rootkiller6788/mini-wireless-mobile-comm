/**
 * nr_phy_pdcch.h — 5G NR PDCCH & PDSCH Processing Chain
 *
 * Knowledge Coverage:
 *   L1 Definitions: DCI, CCE, REG, CORESET, search space, aggregation level
 *   L2 Core Concepts: Control channel design, blind decoding, HARQ
 *   L3 Math Structures: CCE indexing, REG bundle hashing
 *   L5 Algorithms: PDCCH candidate hashing, DCI format detection
 *   L6 Canonical Problems: PDCCH blind decoding for SIB1, PDSCH processing
 *   L7 Applications: gNB scheduler emulation, UE control processing
 *
 * Course: Stanford EE359, MIT 6.450
 * Ref: 3GPP TS 38.211 7.3, TS 38.213 10.1, TS 38.214 5.1
 */

#ifndef NR_PHY_PDCCH_H
#define NR_PHY_PDCCH_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NR_PDCCH_MAX_CANDIDATES  8
#define NR_DCI_MAX_BITS         128
#define NR_DCI_1_0_BITS         48   /* Typical DCI 1_0 length */
#define NR_DCI_0_0_BITS         45   /* Typical DCI 0_0 length */
#define NR_PDSCH_MAX_CODEWORDS  2
#define NR_PDSCH_MAX_LAYERS     8

/** DCI format types */
typedef enum {
    NR_DCI_0_0 = 0,    /* UL grant fallback */
    NR_DCI_0_1 = 1,    /* UL grant non-fallback */
    NR_DCI_1_0 = 2,    /* DL assignment fallback */
    NR_DCI_1_1 = 3,    /* DL assignment non-fallback */
    NR_DCI_2_0 = 4,    /* SFI */
    NR_DCI_2_1 = 5     /* Preemption indication */
} nr_dci_format_t;

/** Aggregation levels (number of CCEs per PDCCH candidate) */
typedef enum {
    NR_AL_1  = 1,
    NR_AL_2  = 2,
    NR_AL_4  = 4,
    NR_AL_8  = 8,
    NR_AL_16 = 16
} nr_aggregation_level_t;

/** Control Channel Element (CCE) = 6 REGs = 72 REs */
typedef struct {
    int     cce_index;          /* CCE index within CORESET */
    int     start_reg;          /* Starting REG index */
    int     num_reg_bundles;    /* Number of REG bundles = 6 / reg_bundle_size */
} nr_cce_t;

/** PDCCH candidate */
typedef struct {
    int                     candidate_index;
    nr_aggregation_level_t  aggr_level;
    int                     start_cce;     /* First CCE index */
    int                     num_cce;       /* Number of CCEs = AL */
    int                     search_space_id;
} nr_pdcch_candidate_t;

/** DCI 1_0 contents (DL assignment) */
typedef struct {
    nr_dci_format_t format_id;     /* Always 1 for DCI 1_0 */
    int     freq_domain_assign;    /* Frequency domain resource assignment */
    int     time_domain_assign;    /* Time domain resource assignment */
    int     vrb_to_prb_mapping;    /* VRB-to-PRB mapping */
    int     mcs;                   /* Modulation and coding scheme (5 bits) */
    int     new_data_indicator;    /* NDI */
    int     redundancy_version;    /* RV (2 bits) */
    int     harq_process_number;   /* HARQ process ID (4 bits) */
    int     downlink_assignment_index; /* DAI (2 bits) */
    int     tpc_command;           /* TPC for PUCCH (2 bits) */
    int     pucch_resource_indicator; /* PUCCH resource (3 bits) */
    int     pdsch_to_harq_feedback; /* Timing indicator (3 bits) */
    uint8_t raw_bits[NR_DCI_1_0_BITS / 8 + 1];
} nr_dci_1_0_t;

/** PDSCH resource allocation */
typedef struct {
    int     start_prb;          /* Starting PRB */
    int     num_prb;            /* Number of allocated PRBs */
    int     start_symbol;       /* Start symbol in slot */
    int     num_symbols;        /* Number of symbols */
    int     dmrs_type_a_pos;    /* DMRS position (2 or 3) */
    int     mapping_type;       /* Type A or B */
    nr_mod_scheme_t modulation;
    double  code_rate;
    int     num_layers;
    int     tbs_bytes;
    int     rv;
    int     harq_process_id;
    int     ndi;
} nr_pdsch_alloc_t;

/** PDSCH processing context */
typedef struct {
    nr_pdsch_alloc_t    allocation;
    int                 codeword_len_bits;
    uint8_t            *codeword;      /* Encoded + rate-matched bits */
    int                 num_symbols_qam;
    nr_complex_t       *qam_symbols;   /* Modulated symbols */
    int                 scrambler_init; /* c_init = n_rnti*2^15 + n_id */
    int                 rnti;           /* RNTI used for scrambling */
} nr_pdsch_ctx_t;

/* ============================================================================
 * L5: PDCCH Candidate Computation
 * ============================================================================ */

/**
 * nr_pdcch_cce_index — Compute CCE starting index for PDCCH candidate
 *
 * 3GPP TS 38.213 Section 10.1, hashing function:
 *
 * For CSS (common search space):
 *   Y_{p,n_s,f^mu} = 0
 *
 * For USS (UE-specific search space):
 *   Y_{p,n_s,f^mu} = (A_p * Y_{p,n_s,f^mu - 1}) mod D
 *   A_p = 39827 for p mod 3 = 0, 39829 for p=1, 39839 for p=2
 *
 * CCE indices for candidate m_s,n_CI with aggregation level L:
 *   L * { (Y_{p,n_s,f^mu} + floor(m_s,n_CI * N_CCE / (L * M_s^max)) + n_CI) mod floor(N_CCE / L) }
 *   + i   for i = 0..L-1
 *
 * This hashing randomizes CCE positions across UEs and slots to
 * minimize blocking probability (two UEs needing the same CCE).
 *
 * @param coreset       CORESET config
 * @param search_space  Search space config
 * @param slot_num      Slot number n_s,f^mu
 * @param rnti          UE RNTI (for USS)
 * @param is_uss        1 = USS, 0 = CSS
 * @param candidates    Output: array of candidates
 * @param max_cand      Max candidates to fill
 * @return Number of candidates found
 */
int nr_pdcch_get_candidates(const nr_coreset_config_t *coreset,
                              const nr_search_space_t *search_space,
                              int slot_num, int rnti, int is_uss,
                              nr_pdcch_candidate_t *candidates,
                              int max_cand);

/**
 * nr_pdcch_reg_mapping — Map encoded DCI bits to REGs within CCEs
 *
 * 3GPP TS 38.211 Section 7.3.2:
 *   1. Scramble DCI bits with RNTI
 *   2. QPSK modulate
 *   3. Map to REGs (each REG = 1 RB x 1 symbol = 12 REs)
 *   4. REG bundle interleaving if configured
 *
 * @param coreset        CORESET config
 * @param candidate      PDCCH candidate
 * @param dci_bits       Scrambled + QPSK-modulated DCI symbols
 * @param re_grid        CORESET RE grid (complex, row-major)
 * @return 0 on success
 */
int nr_pdcch_reg_mapping(const nr_coreset_config_t *coreset,
                          const nr_pdcch_candidate_t *candidate,
                          const nr_complex_t *dci_symbols,
                          nr_complex_t *re_grid);

/* ============================================================================
 * L6: DCI Encoding/Decoding
 * ============================================================================ */

/**
 * nr_dci_1_0_pack — Pack DCI format 1_0 fields into bit array
 *
 * 3GPP TS 38.212 Section 7.3.1.2:
 * DCI 1_0 is the fallback DL assignment used for
 * SIB1, paging, and basic PDSCH scheduling.
 */
void nr_dci_1_0_pack(const nr_dci_1_0_t *dci, uint8_t *bits, int *nbits);

/**
 * nr_dci_1_0_unpack — Unpack DCI 1_0 from bit array
 */
void nr_dci_1_0_unpack(const uint8_t *bits, int nbits, nr_dci_1_0_t *dci);

/**
 * nr_dci_crc_attach — Attach CRC scrambled with RNTI
 *
 * 3GPP TS 38.212 Section 7.3.2:
 * CRC-24C is computed over DCI payload, then XOR'ed with
 * the 16-bit RNTI (upper 8 bits of CRC unaffected by RNTI).
 *
 * This allows the UE to check CRC with its own RNTI during
 * blind decoding — only the correct RNTI yields CRC pass.
 *
 * @param bits   DCI payload bits
 * @param nbits  Payload length
 * @param rnti   16-bit RNTI
 * @param output Output: payload + CRC, length nbits + 24
 */
void nr_dci_crc_attach(const uint8_t *bits, int nbits, int rnti,
                        uint8_t *output);

/**
 * nr_dci_crc_check — Verify CRC with RNTI after decoding
 *
 * @param bits   Decoded bits (payload + CRC)
 * @param nbits  Total bits including CRC
 * @param rnti   Expected RNTI
 * @return 1 = CRC OK, 0 = CRC fail
 */
int nr_dci_crc_check(const uint8_t *bits, int nbits, int rnti);

/* ============================================================================
 * L6: PDSCH Processing Chain
 * ============================================================================ */

/**
 * nr_pdsch_alloc_from_dci — Derive PDSCH allocation from DCI 1_0
 *
 * Translates DCI field values into physical resource allocation
 * using the configured resource allocation tables.
 *
 * @param dci       Decoded DCI 1_0
 * @param bwp       Active BWP config
 * @param alloc     Output: PDSCH allocation
 */
void nr_pdsch_alloc_from_dci(const nr_dci_1_0_t *dci,
                               const nr_bwp_config_t *bwp,
                               nr_pdsch_alloc_t *alloc);

/**
 * nr_pdsch_scramble — PDSCH scrambling
 *
 * 3GPP TS 38.211 Section 7.3.1.1:
 * The scrambled bit sequence b'(i) = (b(i) + c(i)) mod 2,
 * where c(i) is a Gold sequence initialized with
 * c_init = n_RNTI * 2^15 + n_ID.
 *
 * Scrambling decorrelates signals between cells and UEs.
 */
void nr_pdsch_scramble(uint8_t *bits, int nbits,
                        int rnti, int n_id);

/**
 * nr_pdsch_modulate — QAM modulation
 *
 * Maps scrambled bits to QPSK/16QAM/64QAM/256QAM symbols
 * per 3GPP TS 38.211 Table 5.1-1 through 5.1-4.
 *
 * Normalized to unit average energy.
 *
 * @param bits       Scrambled bits
 * @param nbits      Number of bits (must be multiple of bits_per_sym)
 * @param mod        Modulation scheme
 * @param symbols    Output QAM symbols
 * @return Number of QAM symbols produced
 */
int nr_pdsch_modulate(const uint8_t *bits, int nbits,
                       nr_mod_scheme_t mod,
                       nr_complex_t *symbols);

/**
 * nr_pdsch_demodulate_soft — Soft QAM demodulation (LLR computation)
 *
 * Computes Log-Likelihood Ratios for each bit position.
 *
 * For QPSK (Gray mapping):
 *   LLR(b_0) = sqrt(2) * Re(z) / sigma^2  (simplified)
 *   LLR(b_1) = sqrt(2) * Im(z) / sigma^2
 *
 * For 16QAM: uses max-log approximation
 *   LLR(b_i) = 1/sigma^2 * (min_{x:b_i=0}|z-x|^2 - min_{x:b_i=1}|z-x|^2)
 *
 * @param symbols     Received QAM symbols
 * @param nsym        Number of symbols
 * @param mod         Modulation scheme
 * @param noise_var   Noise variance sigma^2
 * @param llr         Output LLRs (nsym * bits_per_sym values)
 */
void nr_pdsch_demodulate_soft(const nr_complex_t *symbols, int nsym,
                               nr_mod_scheme_t mod, double noise_var,
                               double *llr);

/**
 * nr_pdsch_full_chain_tx — Complete PDSCH TX chain
 *
 * TB → CRC attach → LDPC encode → Rate match → Scramble → Modulate → Layer map → Precoding
 *
 * @param tb_bits       Transport block bits
 * @param tb_len        TB length (bits)
 * @param alloc         PDSCH resource allocation
 * @param ctx           PDSCH context (output)
 * @return 0 on success
 */
int nr_pdsch_full_chain_tx(const uint8_t *tb_bits, int tb_len,
                            const nr_pdsch_alloc_t *alloc,
                            nr_pdsch_ctx_t *ctx);

/**
 * nr_pdsch_full_chain_rx — Complete PDSCH RX chain
 *
 * RX symbols → Equalization → Layer demap → Soft demod → Rate recover → LDPC decode → CRC check
 *
 * @param rx_syms       Received QAM symbols at RE positions
 * @param chan_est      Channel estimates at same REs
 * @param ctx           PDSCH context (with allocation)
 * @param tb_bits       Output: decoded TB bits
 * @return 0 = CRC pass, -1 = fail
 */
int nr_pdsch_full_chain_rx(const nr_complex_t *rx_syms,
                            const nr_complex_t *chan_est,
                            const nr_pdsch_ctx_t *ctx,
                            uint8_t *tb_bits);

/**
 * nr_pdsch_ctx_free — Free PDSCH context memory
 */
void nr_pdsch_ctx_free(nr_pdsch_ctx_t *ctx);

/**
 * nr_layer_mapping — Map codewords to MIMO layers
 *
 * 3GPP TS 38.211 Section 7.3.1.3:
 * 1 CW → up to 4 layers
 * 2 CWs → 5 to 8 layers
 *
 * @param cw_syms      Codeword symbols
 * @param cw_len       CW length in symbols
 * @param n_layers     Number of layers
 * @param n_cw         Number of codewords (1 or 2)
 * @param layer_syms   Output: symbols per layer (n_layers * symbols_per_layer)
 */
void nr_layer_mapping(const nr_complex_t *cw_syms, int cw_len,
                       int n_layers, int n_cw,
                       nr_complex_t *layer_syms);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_PDCCH_H */