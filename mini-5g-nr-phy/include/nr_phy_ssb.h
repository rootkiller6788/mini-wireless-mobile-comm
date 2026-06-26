/**
 * nr_phy_ssb.h — 5G NR SSB: Synchronization Signal Block & Initial Access
 *
 * Knowledge Coverage:
 *   L1 Definitions: PSS, SSS, PBCH, SSB burst set, SSB index
 *   L2 Core Concepts: Cell search, time/freq synchronization, beam sweeping
 *   L3 Math Structures: m-sequences (PSS), Gold sequences (SSS), correlation
 *   L5 Algorithms: PSS detection (3 candidates), SSS detection (336 cell IDs)
 *   L6 Canonical Problems: Full cell search: PSS → SSS → PBCH DMRS → MIB
 *   L7 Applications: 5G initial access, FR2 beam management
 *
 * Course: Stanford EE359, MIT 6.450
 * Ref: 3GPP TS 38.211 7.4.2 (PSS), 7.4.3 (SSS), 7.3.3 (PBCH), 7.4.1.4 (SSB)
 */

#ifndef NR_PHY_SSB_H
#define NR_PHY_SSB_H

#include "nr_phy_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NR_PSS_LEN          127     /* PSS sequence length */
#define NR_SSS_LEN          127     /* SSS sequence length */
#define NR_SSB_BW_RB        20      /* SSB bandwidth in RBs (240 SCs) */
#define NR_SSB_SYMBOLS      4       /* SSB occupies 4 OFDM symbols */
#define NR_PSS_SYMBOL       0       /* PSS at symbol 0 */
#define NR_SSS_SYMBOL       2       /* SSS at symbol 2 */
#define NR_PBCH_SYMBOLS     3       /* PBCH at symbols 1,2,3 (#2 shared with SSS) */
#define NR_NID2_MAX         3       /* Number of PSS sequences (N_ID^(2)) */
#define NR_NID1_MAX         336     /* Number of SSS sequences (N_ID^(1)) */
#define NR_NID_MAX          1008    /* Total cell IDs (3 * 336 = 1008) */
#define NR_SSB_MAX_BEAMS    64      /* L_max: max SSB beams per burst */
#define NR_SSB_BURST_PERIOD_MS 20   /* Default SSB periodicity */
#define NR_MIB_PAYLOAD_BITS  24     /* MIB payload (before encoding) */
#define NR_PBCH_PAYLOAD_BITS 56     /* PBCH payload incl MIB + timing */
#define NR_PBCH_DMRS_SC      60     /* PBCH DMRS REs per SSB */

/** Physical cell identity */
typedef struct {
    int nid2;   /* N_ID^(2) in {0,1,2} from PSS */
    int nid1;   /* N_ID^(1) in {0..335} from SSS */
    int nid;    /* N_ID = 3*N_ID^(1) + N_ID^(2) in {0..1007} */
} nr_phy_cell_id_t;

/** PSS detector output */
typedef struct {
    int         nid2;           /* Detected N_ID^(2) */
    double      corr_peak[NR_NID2_MAX]; /* Correlation peak per hypothesis */
    int         best_timing;    /* Sample offset of best correlation */
    double      freq_offset_hz; /* Estimated frequency offset */
    int         detected;       /* 1 = detected */
} nr_pss_result_t;

/** SSS detector output */
typedef struct {
    int         nid1;           /* Detected N_ID^(1) */
    double      corr_peak;      /* Correlation peak */
    nr_phy_cell_id_t cell_id;   /* Full cell identity */
    int         detected;
} nr_sss_result_t;

/** SSB index and beam info */
typedef struct {
    int         ssb_index;      /* SSB index within burst (0..L_max-1) */
    int         ssb_subcarrier_offset; /* k_SSB (0..23) */
    double      rsrp_dbm;       /* SS-RSRP in dBm */
    double      sinr_db;        /* SS-SINR in dB */
    int         beam_id;        /* Logical beam ID */
    double      timing_offset;  /* Timing offset estimate */
} nr_ssb_measurement_t;

/** MIB contents (3GPP TS 38.331) */
typedef struct {
    int         sfn_6msb;       /* 6 MSBs of SFN */
    int         subcarrier_spacing_common; /* scs for SIB1/other SI */
    int         ssb_subcarrier_offset;     /* k_SSB */
    int         dmrs_type_a_position;     /* PDSCH DMRS position */
    int         pdcch_config_sib1;        /* 8 bits for CORESET#0 */
    int         cell_barred;              /* Barred = 1 */
    int         intra_freq_reselection;   /* Allowed = 0 */
    uint8_t     raw_bits[NR_MIB_PAYLOAD_BITS / 8 + 1];
} nr_mib_t;

/** Full cell search result */
typedef struct {
    nr_phy_cell_id_t cell_id;
    nr_mib_t         mib;
    double           pss_corr;
    double           sss_corr;
    double           rsrp_dbm;
    double           freq_offset_hz;
    double           timing_offset_samples;
    int              ssb_index;
    int              detected;   /* 0 = not found, 1 = found */
} nr_cell_search_result_t;

/* ============================================================================
 * L5: PSS Sequence Generation and Detection
 * ============================================================================ */

/**
 * nr_pss_sequence — Generate PSS sequence for N_ID^(2)
 *
 * 3GPP TS 38.211 Table 7.4.2.2.1-1:
 * PSS is an m-sequence (length-127) cyclically shifted by N_ID^(2):
 *
 * d_PSS(n) = 1 - 2 * x(m)
 * m = (n + 43*N_ID^(2)) mod 127
 * x(i+7) = (x(i+4) + x(i)) mod 2
 * Initial: [x(6)..x(0)] = [1 1 1 0 1 1 0]
 *
 * Three PSS sequences map to N_ID^(2) ∈ {0,1,2}.
 *
 * @param nid2  PSS index 0, 1, or 2
 * @param seq   Output 127 BPSK symbols (+1/-1)
 */
void nr_pss_sequence(int nid2, double *seq);

/**
 * nr_pss_detect — Detect PSS via time-domain correlation
 *
 * L5: Cross-correlate received signal with all 3 PSS candidates.
 * The peak of the correlation magnitude identifies N_ID^(2)
 * and provides coarse timing + frequency offset estimate.
 *
 * Algorithm:
 *   1. Correlate rx with each PSS candidate (127 samples)
 *   2. Find max |corr| → N_ID^(2) and sample offset
 *   3. Frequency offset from phase rotation: Delta_f = phase(corr_peak) / (2*pi*T_sym)
 *
 * Complexity: O(3 * N_rx * 127).
 *
 * @param rx_signal      Received baseband samples
 * @param rx_len         Number of samples
 * @param fft_size       FFT size (from numerology)
 * @param scs_hz         Subcarrier spacing
 * @param result         Detection result output
 */
void nr_pss_detect(const nr_complex_t *rx_signal, int rx_len,
                    int fft_size, double scs_hz,
                    nr_pss_result_t *result);

/* ============================================================================
 * L5: SSS Sequence Generation and Detection
 * ============================================================================ */

/**
 * nr_sss_sequence — Generate SSS sequence for N_ID^(1), N_ID^(2)
 *
 * 3GPP TS 38.211 7.4.3.1:
 * SSS is a Gold sequence (XOR of two m-sequences):
 *
 * d_SSS(n) = [1 - 2*x0((n+m0) mod 127)] * [1 - 2*x1((n+m1) mod 127)]
 *
 * m0 = 15 * floor(N_ID^(1)/112) + 5 * N_ID^(2)
 * m1 = N_ID^(1) mod 112
 *
 * Produces 336 unique sequences, one per N_ID^(1) ∈ {0..335}.
 *
 * @param nid1  SSS index 0..335
 * @param nid2  PSS index 0..2
 * @param seq   Output 127 BPSK symbols
 */
void nr_sss_sequence(int nid1, int nid2, double *seq);

/**
 * nr_sss_detect — Detect SSS given known N_ID^(2)
 *
 * Cross-correlates frequency-domain received SSS with all
 * 336 candidate sequences. Maximum peak identifies N_ID^(1).
 *
 * Complexity: O(336 * 127).
 *
 * @param rx_sss_f  Frequency-domain received SSS (127 complex)
 * @param nid2      Known PSS N_ID^(2)
 * @param result    Detection result output
 */
void nr_sss_detect(const nr_complex_t *rx_sss_f, int nid2,
                    nr_sss_result_t *result);

/* ============================================================================
 * L5: PBCH DMRS Sequence
 * ============================================================================ */

/**
 * nr_pbch_dmrs_sequence — Generate PBCH DMRS sequence
 *
 * 3GPP TS 38.211 Section 7.4.1.4:
 * Gold sequence initialized with c_init = 2^11*(i_SSB+1)*(floor(N_ID/4)+1)
 * + 2^6*(i_SSB+1) + (N_ID mod 4).
 *
 * The 3 LSBs of SSB index (for L_max=4) or 3 MSBs (for L_max=8/64)
 * are carried in the DMRS sequence, allowing SSB index detection.
 *
 * @param nid        Physical cell ID (0..1007)
 * @param i_ssb      SSB index
 * @param n_hf       Half-frame indicator
 * @param seq        Output: BPSK symbols (NR_PBCH_DMRS_SC)
 */
void nr_pbch_dmrs_sequence(int nid, int i_ssb, int n_hf, double *seq);

/**
 * nr_pbch_dmrs_detect_ssb_index — Detect SSB index from PBCH DMRS
 *
 * For L_max=8: 3 LSBs of SSB index in DMRS. Correlates against
 * 8 hypotheses to find the best match.
 *
 * @param rx_dmrs    Received PBCH DMRS symbols
 * @param nid        Known cell ID
 * @param l_max      Max SSB beams (4, 8, or 64)
 * @param n_hf       Half-frame indicator (known from PSS/SSS timing)
 * @return Detected SSB index (0..L_max-1)
 */
int nr_pbch_dmrs_detect_ssb_index(const nr_complex_t *rx_dmrs,
                                    int nid, int l_max, int n_hf);

/* ============================================================================
 * L5: PBCH Processing
 * ============================================================================ */

/**
 * nr_pbch_payload_encode — Encode PBCH payload
 *
 * PBCH carries the MIB (Master Information Block) plus 8 additional
 * timing bits (SFN LSB, half-frame, SSB index MSBs).
 *
 * 3GPP TS 38.212 Section 7.1:
 *   1. Attach CRC
 *   2. Polar encoding
 *   3. Rate matching to 864 bits
 *   4. QPSK modulation → 432 symbols
 *
 * @param payload  56 payload bits (24 MIB + 8 timing + 24 CRC-24C)
 * @param symbols  Output: 432 QPSK symbols
 */
void nr_pbch_payload_encode(const uint8_t *payload, nr_complex_t *symbols);

/**
 * nr_pbch_scramble_dmrs — Scramble PBCH payload with cell-specific sequence
 *
 * Before modulation, PBCH bits are scrambled with a Gold sequence
 * initialized by the physical cell ID N_ID.
 *
 * @param bits     PBCH encoded bits
 * @param nbits    Number of bits
 * @param nid      Physical cell ID
 */
void nr_pbch_scramble_bits(uint8_t *bits, int nbits, int nid);

/**
 * nr_mib_decode — Decode MIB from PBCH payload bits
 *
 * 3GPP TS 38.331 Section 6.2.2:
 * Parses the 24-bit MIB payload.
 *
 * @param pbch_payload  Decoded PBCH payload (32 bits after removing timing)
 * @param mib           Output MIB structure
 * @return 0 on success
 */
int nr_mib_decode(const uint8_t *pbch_payload, nr_mib_t *mib);

/* ============================================================================
 * L6: Full Cell Search
 * ============================================================================ */

/**
 * nr_cell_search — Full 5G NR cell search
 *
 * L6 Canonical Problem: Starting from raw baseband samples, perform:
 *   1. PSS detection → N_ID^(2), coarse timing, frequency offset
 *   2. Correct frequency offset, extract SSS from known position
 *   3. SSS detection → N_ID^(1), full cell ID
 *   4. Extract PBCH, detect SSB index from DMRS
 *   5. Decode PBCH payload → MIB (SFN, CORESET#0, etc.)
 *
 * This is the complete initial access procedure that every 5G UE
 * must perform when powering on or searching for cells.
 *
 * @param rx_signal      Wideband received samples (>= 20 ms worth)
 * @param rx_len         Total samples
 * @param scs_khz        Subcarrier spacing assumption (15 or 30 kHz)
 * @param fft_size       FFT size
 * @param l_max          Max SSB beams (L_max=4 for FR1-sub3, 8 for FR1-3to6)
 * @param result         Full cell search result
 * @return 0 = cell found, 1 = no cell found
 */
int nr_cell_search(const nr_complex_t *rx_signal, int rx_len,
                    int scs_khz, int fft_size, int l_max,
                    nr_cell_search_result_t *result);

/**
 * nr_ssb_rsrp — Measure SS-RSRP (SS Reference Signal Received Power)
 *
 * 3GPP TS 38.215 Section 5.1.1:
 * SS-RSRP is the linear average of the power contributions (in Watts)
 * of the resource elements that carry SSS.
 *
 * @param rx_sss  Received frequency-domain SSS (127 complex)
 * @return RSRP in dBm (assuming correct scaling)
 */
double nr_ssb_rsrp(const nr_complex_t *rx_sss);

/**
 * nr_ssb_sinr — Measure SS-SINR
 *
 * SS-SINR = SSS signal power / (noise + interference power)
 * measured at the SSS REs.
 */
double nr_ssb_sinr(const nr_complex_t *rx_sss, const nr_complex_t *rx_noise);

#ifdef __cplusplus
}
#endif

#endif /* NR_PHY_SSB_H */