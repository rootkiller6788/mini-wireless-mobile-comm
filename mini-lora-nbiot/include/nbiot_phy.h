/**
 * @file nbiot_phy.h
 * @brief NB-IoT Physical Layer -- 3GPP Release 13/14 NB-IoT PHY definitions
 *
 * Knowledge Coverage:
 *   L1 -- NPSS, NSSS, NPBCH, NPDCCH, NPDSCH, NPUSCH, NPRACH,
 *        Resource Unit (RU), Subcarrier Spacing (15 kHz / 3.75 kHz),
 *        Coverage Enhancement (CE) Levels, MCL, TBS
 *   L2 -- OFDM/SC-FDMA subcarrier framework, frame structure,
 *        NPSS auto-correlation synchronization, CE level selection
 *   L3 -- Zadoff-Chu sequences for NPSS/NSSS, DFT-spread OFDM,
 *        Complex baseband OFDM symbol generation
 *   L4 -- Shannon-Hartley applied to NB-IoT, Maximum Coupling Loss (MCL)
 *   L5 -- NPSS detection via auto-correlation, NSSS cell ID decoding,
 *        Rate matching and Turbo coding for NPDSCH
 *   L6 -- NB-IoT cell search, PBCH decoding, PRACH procedure
 *
 * References:
 *   - 3GPP TS 36.211: Physical channels and modulation
 *   - 3GPP TS 36.213: Physical layer procedures
 *   - 3GPP TS 36.101: UE radio transmission and reception
 *   - 3GPP TR 45.820: Cellular IoT (NB-IoT feasibility study)
 *
 * Curriculum Mapping:
 *   - Stanford EE359: OFDM physical layer
 *   - MIT 6.450: Digital communications -- synchronization
 *   - Berkeley EE123: DSP for communication
 *   - TU Munich: Cellular network PHY
 *   - Georgia Tech ECE 6601: 3GPP physical layer
 *
 * @license MIT
 */

#ifndef NBIOT_PHY_H
#define NBIOT_PHY_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <complex.h>

#ifndef CMPLX
#define CMPLX(r, i) ((double complex)((r) + (i) * _Complex_I))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   L1: Core Definitions -- NB-IoT Physical Layer Parameters
   ============================================================================ */

/**
 * NB-IoT deployment mode
 *
 * Standalone:     Dedicated spectrum (e.g., GSM refarmed)
 * Guard-band:     Unused guard band within LTE carrier
 * In-band:        Resource blocks within LTE carrier
 */
typedef enum {
    NBIOT_DEPLOY_STANDALONE   = 0,  /**< Dedicated 200 kHz carrier */
    NBIOT_DEPLOY_GUARDBAND    = 1,  /**< LTE guard band (e.g., 10 MHz LTE) */
    NBIOT_DEPLOY_INBAND       = 2,  /**< Within LTE PRB (resource block) */
} nbiot_deployment_mode_t;

/**
 * NB-IoT subcarrier spacing
 *
 * DL always 15 kHz (OFDM). UL can be 15 kHz (SC-FDMA) or 3.75 kHz.
 * 3.75 kHz provides better coverage (4x symbol duration) at lower data rate.
 */
typedef enum {
    NBIOT_SCS_15_KHZ   = 15000,   /**< Standard 15 kHz subcarrier spacing */
    NBIOT_SCS_3_75_KHZ = 3750,    /**< Extended coverage 3.75 kHz (UL only) */
} nbiot_subcarrier_spacing_t;

/**
 * NB-IoT Coverage Enhancement (CE) Level
 *
 * Three CE levels for NPRACH:
 *   CE Level 0: Normal coverage  (MCL < 144 dB)
 *   CE Level 1: Robust coverage  (MCL < 154 dB)
 *   CE Level 2: Extreme coverage (MCL < 164 dB)
 *
 * Higher CE levels use more repetitions for reliability.
 */
typedef enum {
    NBIOT_CE_LEVEL_0 = 0,  /**< Normal coverage, min repetitions */
    NBIOT_CE_LEVEL_1 = 1,  /**< Robust coverage, moderate repetitions */
    NBIOT_CE_LEVEL_2 = 2,  /**< Extreme coverage, max repetitions */
} nbiot_ce_level_t;

/**
 * NB-IoT physical channel types
 */
typedef enum {
    NBIOT_CHAN_NPSS    = 0,  /**< Narrowband Primary Synchronization Signal */
    NBIOT_CHAN_NSSS    = 1,  /**< Narrowband Secondary Synchronization Signal */
    NBIOT_CHAN_NPBCH   = 2,  /**< Narrowband Physical Broadcast Channel */
    NBIOT_CHAN_NPDCCH  = 3,  /**< Narrowband Physical Downlink Control Channel */
    NBIOT_CHAN_NPDSCH  = 4,  /**< Narrowband Physical Downlink Shared Channel */
    NBIOT_CHAN_NPUSCH  = 5,  /**< Narrowband Physical Uplink Shared Channel */
    NBIOT_CHAN_NPRACH  = 6,  /**< Narrowband Physical Random Access Channel */
    NBIOT_CHAN_NRS     = 7,  /**< Narrowband Reference Signal */
} nbiot_channel_type_t;

/**
 * NB-IoT frame and slot structure
 *
 * One radio frame = 10 ms = 10 subframes = 20 slots (15 kHz SCS)
 * One slot = 7 OFDM symbols (normal CP) or 6 symbols (extended CP)
 * One subframe = 1 ms = 2 slots
 * One hyperframe = 1024 frames (for extended DRX)
 *
 * NB-IoT occupies 1 PRB = 180 kHz = 12 subcarriers * 15 kHz
 */
#define NBIOT_FRAME_DURATION_MS    10.0    /**< Radio frame = 10 ms */
#define NBIOT_SUBFRAME_DURATION_MS  1.0    /**< Subframe = 1 ms */
#define NBIOT_SLOT_DURATION_MS      0.5   /**< Slot = 0.5 ms at 15 kHz SCS */
#define NBIOT_NUM_SUBCARRIERS      12     /**< 1 PRB = 12 subcarriers */
#define NBIOT_TOTAL_BW_KHZ       180.0    /**< Total occupied BW = 180 kHz */
#define NBIOT_SYMBOLS_PER_SLOT      7     /**< Normal CP OFDM symbols */
#define NBIOT_FFT_SIZE            128     /**< 1.92 MHz sampling, 128-point FFT */

/**
 * NPSS parameters
 *
 * NPSS is transmitted in subframe 5 of every radio frame.
 * Uses a length-11 Zadoff-Chu sequence mapped to the last 11 OFDM symbols
 * of the subframe (symbols 3-13 in normal CP).
 *
 * Root sequence index u = 5 for Zadoff-Chu sequence generation.
 * Sequence occupies all 12 subcarriers x 11 symbols = 132 REs.
 */
#define NBIOT_NPSS_SUBFRAME        5    /**< NPSS in subframe 5 every frame */
#define NBIOT_NPSS_ZC_LEN         11    /**< Zadoff-Chu sequence length */
#define NBIOT_NPSS_ZC_ROOT         5    /**< ZC sequence root index */
#define NBIOT_NPSS_OCC_LEN         4    /**< Orthogonal cover code length */
#define NBIOT_NPSS_SYMBOL_START    3    /**< First symbol of NPSS in subframe */
#define NBIOT_NPSS_REPETITIONS     1    /**< Single transmission per frame */

/**
 * NSSS parameters
 *
 * NSSS is transmitted in subframe 9 of even-numbered radio frames.
 * Carries 504 physical cell identities (PCID).
 * PCID = 3*N1 + N2, where N1 in [0,167], N2 in [0,2].
 *
 * Uses a length-132 Zadoff-Chu sequence with cyclic shift and
 * binary scrambling based on N1 and N2.
 */
#define NBIOT_NSSS_SUBFRAME        9    /**< NSSS in subframe 9 */
#define NBIOT_NSSS_FRAME_PERIOD    20   /**< Transmitted every 20 ms */
#define NBIOT_NSSS_ZC_LEN        131    /**< Zadoff-Chu sequence length (prime) */
#define NBIOT_NSSS_RE_LEN        132    /**< Resource elements: 12 SC * 11 sym */
#define NBIOT_MAX_PCID           504    /**< 504 physical cell identities */

/**
 * NPBCH parameters
 *
 * Transmitted in subframe 0 of every frame.
 * Master Information Block (MIB) = 34 bits:
 *   - 4 bits: system frame number (SFN) MSB
 *   - 7 bits: hyper SFN (H-SFN) LSB
 *   - 4 bits: SIB1 scheduling info
 *   - 5 bits: system information value tag
 *   - 1 bit: access barring flag
 *   - 11 bits: spare/reserved
 *   - 2 bits: CRC (after encoding, 16-bit CRC attached pre-encoding)
 *
 * TTI = 640 ms (64 frames), self-decodable every 64 ms.
 */
#define NBIOT_NPBCH_SUBFRAME       0    /**< NPBCH in subframe 0 */
#define NBIOT_NPBCH_TTI_MS       640    /**< TTI = 640 ms */
#define NBIOT_NPBCH_MIB_BITS       34    /**< MIB payload bits */
#define NBIOT_NPBCH_CRC_BITS       16    /**< CRC bits before encoding */
#define NBIOT_NPBCH_TAIL_BITS      16    /**< TBCC tail bits */
#define NBIOT_NPBCH_CODED_BITS   1600    /**< After TBCC rate-1/3 encoding */

/**
 * Resource Unit (RU) — basic scheduling unit
 *
 * NPDSCH:
 *   1 RU = 1 subframe * 12 subcarriers (1 ms) for non-BL
 *   1 RU = 2 ms or 4 ms or 8 ms or 10 ms depending on repetition count
 *
 * NPUSCH:
 *   1 RU = N_sc * N_slots * N_symbols resource elements
 *   Where N_sc RU = 1, 3, 6, or 12 subcarriers
 */
typedef struct {
    uint8_t  num_subcarriers;     /**< 1, 3, 6, or 12 subcarriers */
    uint8_t  num_slots;           /**< 2, 4, 8, 16 slots per RU */
    uint16_t num_symbols;         /**< Total symbols in this RU */
    double   duration_ms;         /**< RU duration in milliseconds */
} nbiot_resource_unit_t;

/**
 * Transport Block Size (TBS) table entry
 *
 * TBS depends on MCS index and number of RUs allocated.
 * 3GPP TS 36.213 Table 16.5.1.2-1 (simplified subset).
 */
typedef struct {
    uint8_t  mcs_index;          /**< Modulation and Coding Scheme index (0-12) */
    uint8_t  modulation_order;   /**< QPSK=2 bits/symbol */
    double   code_rate;          /**< Effective code rate */
    uint16_t tbs_bits;           /**< Transport block size for 1 RU */
} nbiot_tbs_entry_t;

/**
 * NB-IoT PHY configuration for a cell
 */
typedef struct {
    nbiot_deployment_mode_t deployment;     /**< Standalone, guard-band, in-band */
    uint16_t pci;                           /**< Physical Cell Identity (0-503) */
    double   dl_freq_hz;                    /**< Downlink center frequency */
    double   ul_freq_hz;                    /**< Uplink center frequency */
    nbiot_subcarrier_spacing_t ul_scs;      /**< UL subcarrier spacing */
    uint32_t sfn;                           /**< System Frame Number */
    uint8_t  sib1_repetitions;              /**< SIB1 repetition pattern index */
    nbiot_ce_level_t ce_level;              /**< Coverage enhancement level */
    uint8_t  npdcch_max_repetitions;        /**< Max NPDCCH repetitions */
    uint8_t  npdsch_max_repetitions;        /**< Max NPDSCH repetitions */
    double   rsrp_dbm;                      /**< Reference Signal Received Power */
    double   rsrq_db;                       /**< Reference Signal Received Quality */
    double   sinr_db;                       /**< Estimated SINR */
} nbiot_cell_config_t;

/* ============================================================================
   L2: Core Concepts -- OFDM/SC-FDMA Frame Structure
   ============================================================================ */

/**
 * NB-IoT subframe structure (1 ms, 12 SC x 14 symbols)
 *
 * SF structure for normal CP:
 *   [symbol 0: CP + NBIOT_FFT_SIZE samples] x 14
 *
 * Resource element grid: [subcarrier 0..11] x [symbol 0..13]
 * Each RE carries one QPSK symbol (2 bits) before repetition coding.
 */
typedef struct {
    double complex re_grid[12][14];   /**< [subcarrier][symbol] resource grid */
    uint16_t       subframe_number;   /**< Subframe index within frame (0-9) */
    uint32_t       sfn;               /**< System Frame Number */
} nbiot_subframe_t;

/**
 * DFT-Spread OFDM (SC-FDMA) symbol generation for uplink
 *
 * SC-FDMA (used in NPUSCH) has lower PAPR than OFDM:
 *   1. M-point DFT of modulation symbols
 *   2. Subcarrier mapping (localized)
 *   3. N-point IFFT (N > M)
 *   4. CP insertion
 *
 * This is equivalent to OFDM with a DFT precoder,
 * which spreads each symbol across all allocated subcarriers.
 */
typedef struct {
    uint16_t m_dft;              /**< DFT size (1, 3, 6, or 12) */
    uint16_t n_ifft;             /**< IFFT size (typically 128) */
    uint16_t first_sc;           /**< First subcarrier index for mapping */
    double   sc_spacing_hz;      /**< Subcarrier spacing */
} nbiot_scfdma_config_t;

/* ============================================================================
   L3: Mathematical Structures -- Zadoff-Chu and OFDM
   ============================================================================ */

/**
 * Generate Zadoff-Chu sequence
 *
 * ZC sequence of length N_ZC (odd prime):
 *   z_u[n] = exp(-j * pi * u * n * (n+1) / N_ZC),  n = 0..N_ZC-1
 *
 * Properties:
 *   - Constant amplitude zero auto-correlation (CAZAC)
 *   - DFT of ZC is also ZC (Fourier invariant)
 *   - Used for NPSS (N=11, u=5) and NSSS (N=131, variable u)
 *
 * @param zc_len  Sequence length (must be odd prime for ZC properties)
 * @param u       Root sequence index (coprime with zc_len)
 * @param seq     Output complex sequence
 * @return 0 on success, -1 on error
 */
int nbiot_zadoff_chu(uint16_t zc_len, uint16_t u, double complex *seq);

/**
 * Generate NPSS signal for one subframe
 *
 * NPSS = ZC sequence (u=5, len=11) repeated per symbol
 *      * orthogonal cover code [+1 +1 +1 +1] or [+1 -1 +1 -1] per 4-symbol block
 *
 * Mapped to symbols 3-13 of subframe 5, all 12 subcarriers.
 *
 * @param subframe Output subframe grid (zeros out non-NPSS symbols)
 * @param pci      Physical cell identity (used for OCC selection)
 * @return 0 on success, -1 on error
 */
int nbiot_npss_generate(nbiot_subframe_t *subframe, uint16_t pci);

/**
 * Generate NSSS signal for one subframe
 *
 * NSSS encodes PCID using:
 *   - ZC root u = N1 mod 131, where N1 = PCID / 3
 *   - Cyclic shift theta_f applied to ZC sequence
 *   - Binary scrambling based on N2 = PCID % 3
 *
 * @param subframe Output subframe grid
 * @param pci      Physical cell identity (0-503)
 * @param sfn      System frame number for frame-dependent scrambling
 * @return 0 on success, -1 on error
 */
int nbiot_nsss_generate(nbiot_subframe_t *subframe, uint16_t pci, uint32_t sfn);

/**
 * OFDM modulation: frequency domain to time domain
 *
 * Takes subcarrier grid as input and produces time-domain baseband samples.
 *
 * x[n] = sum_{k=0}^{N_fft-1} X[k] * exp(j * 2pi * k * n / N_fft)
 *
 * where X[k] contains the mapped subcarriers with DC and guard zeros.
 *
 * @param freq_grid   Frequency domain symbols (N_fft elements)
 * @param n_fft       FFT size (128 for NB-IoT)
 * @param cp_len      Cyclic prefix length in samples
 * @param time_signal Output time-domain signal
 * @param time_len    Output signal length (n_fft + cp_len)
 * @return 0 on success, -1 on error
 */
int nbiot_ofdm_modulate(const double complex *freq_grid,
                         uint16_t n_fft,
                         uint16_t cp_len,
                         double complex *time_signal,
                         size_t time_len);

/**
 * OFDM demodulation: time domain to frequency domain
 *
 * Removes CP and performs FFT to recover subcarrier symbols.
 *
 * @param time_signal Input time-domain samples
 * @param n_fft       FFT size
 * @param cp_len      CP length in samples
 * @param freq_grid   Output frequency-domain symbols
 * @return 0 on success, -1 on error
 */
int nbiot_ofdm_demodulate(const double complex *time_signal,
                           uint16_t n_fft,
                           uint16_t cp_len,
                           double complex *freq_grid);

/* ============================================================================
   L4: Fundamental Laws -- MCL and Link Budget
   ============================================================================ */

/**
 * Maximum Coupling Loss (MCL)
 *
 * MCL = max allowable path loss between UE and eNB for reliable communication.
 * This is the key performance metric for NB-IoT coverage.
 *
 * MCL(dB) = P_TX - S_RX
 *
 * where:
 *   P_TX = transmitter power (23 dBm for UE, 43 dBm for eNB)
 *   S_RX = receiver sensitivity (see lora_receiver_sensitivity)
 *
 * NB-IoT target MCL = 164 dB, compared to:
 *   - LTE Cat-1: 140 dB
 *   - LTE-M: 156 dB
 *   - GPRS: 144 dB
 *
 * Achieving 164 dB MCL enables deep indoor / basement coverage.
 *
 * @param tx_power_dbm  Transmitter power in dBm
 * @param rx_sens_dbm   Receiver sensitivity in dBm
 * @return MCL in dB
 */
double nbiot_max_coupling_loss(double tx_power_dbm, double rx_sens_dbm);

/**
 * NB-IoT link budget analysis
 *
 * Computes the complete link budget for a given configuration.
 *
 * Parameters:
 *   - tx_power: Transmitter EIRP (dBm)
 *   - tx_antenna_gain: Transmitter antenna gain (dBi)
 *   - path_loss: Path loss from propagation model (dB)
 *   - rx_antenna_gain: Receiver antenna gain (dBi)
 *   - rx_noise_figure: Receiver noise figure (dB)
 *   - bw: Signal bandwidth (Hz)
 *
 * Returns received SNR and whether link is viable for given CE level.
 *
 * @param config  Cell configuration
 * @param tx_dbm  Transmitter power in dBm
 * @param path_loss_db  Estimated path loss in dB
 * @param snr_out Output SNR at receiver in dB
 * @return 0 if link viable, -1 if insufficient
 */
int nbiot_link_budget(const nbiot_cell_config_t *config,
                       double tx_dbm,
                       double path_loss_db,
                       double *snr_out);

/**
 * Shannon capacity for NB-IoT 180 kHz channel
 *
 * C = BW * log2(1 + SNR)
 *
 * where BW = 180 kHz for NB-IoT.
 * At MCL=164 dB (SNR ~ -14 dB after despreading), the theoretical
 * capacity is very low, requiring extensive repetition coding.
 *
 * @param snr_linear Linear SNR (not dB)
 * @return Channel capacity in bits/second
 */
double nbiot_shannon_capacity(double snr_linear);

/**
 * Effective data rate accounting for repetition coding
 *
 * R_eff = R_peak / N_rep
 *
 * where N_rep is the number of repetitions based on CE level.
 * CE0: N_rep up to 16, CE1: up to 128, CE2: up to 2048.
 *
 * @param peak_rate_bps  Peak data rate without repetition
 * @param num_reps       Number of repetitions
 * @return Effective data rate in bps
 */
double nbiot_effective_rate(double peak_rate_bps, uint16_t num_reps);

/* ============================================================================
   L5: Algorithms -- NPSS Detection and Cell Search
   ============================================================================ */

/**
 * NPSS auto-correlation detector
 *
 * NPSS detection algorithm (L5):
 *   1. Receive 10 ms (1 frame) of baseband samples
 *   2. Compute auto-correlation with lag = 1 frame (10 ms)
 *      Because NPSS is identical in every frame, correlation peaks at lag=10ms
 *   3. Accumulate over N_acc frames for robust detection
 *   4. Peak above threshold indicates NPSS presence
 *
 * This exploits the periodicity of NPSS to detect at very low SNR
 * without knowledge of the ZC sequence.
 *
 * @param samples     Complex baseband samples
 * @param num_samples Total samples (must be >= 2 frames)
 * @param fs          Sample rate in Hz
 * @param threshold   Detection threshold (0.0-1.0, normalized)
 * @param frame_start Output: sample index of detected frame start
 * @return 0 if NPSS detected, -1 if not found
 */
int nbiot_npss_detect(const double complex *samples,
                       size_t num_samples,
                       double fs,
                       double threshold,
                       size_t *frame_start);

/**
 * NSSS Physical Cell Identity decoder
 *
 * Algorithm (L5):
 *   1. Extract NSSS REs from subframe 9 (known after NPSS sync)
 *   2. Try all 504 PCID hypotheses:
 *      a. Generate local NSSS for PCID candidate
 *      b. Cross-correlate with received NSSS
 *   3. Select PCID with maximum correlation
 *
 * Complexity: O(504 * 132) complex multiply-adds per frame.
 *
 * @param rx_subframe Received NSSS subframe (subframe 9)
 * @param sfn         System frame number
 * @param pci         Output: detected physical cell identity
 * @return 0 on success, -1 on detection failure
 */
int nbiot_nsss_decode_pci(const nbiot_subframe_t *rx_subframe,
                           uint32_t sfn,
                           uint16_t *pci);

/**
 * Simplified tail-biting convolutional code (TBCC) encoder for NB-IoT
 *
 * NB-IoT uses a rate-1/3 TBCC with constraint length 7:
 *   G0 = 133 (octal) = [1 0 1 1 0 1 1]
 *   G1 = 171 (octal) = [1 1 1 1 0 0 1]
 *   G2 = 165 (octal) = [1 1 1 0 0 1 1]
 *
 * This is the same encoder as LTE PBCH (3GPP TS 36.212 Sec 5.1.3.1).
 * Input bits are encoded with tail biting (shift register initialized
 * with last 6 input bits) for efficient block encoding.
 *
 * @param input      Input bits
 * @param input_len  Number of input bits
 * @param output     Output coded bits (3 * input_len bits)
 * @return Number of output bits (3 * input_len), or -1 on error
 */
int nbiot_tbcc_encode(const uint8_t *input, size_t input_len, uint8_t *output);

/**
 * NPUSCH subcarrier group hopping pattern
 *
 * Generates the frequency hopping pattern for NPUSCH transmissions.
 * Uses a Gold sequence generator initialized with PCID.
 *
 * Hopping pattern (3GPP TS 36.211, Sec 10.1.3):
 *   f_hop(i) = (f_hop(i-1) + delta_f) mod N_sc
 *
 * @param pci       Physical cell identity
 * @param num_slots Number of slots to generate pattern for
 * @param pattern   Output: subcarrier offset per slot
 * @return 0 on success
 */
int nbiot_hopping_pattern(uint16_t pci, uint16_t num_slots, uint8_t *pattern);

/* ============================================================================
   L6: Canonical Problem -- NB-IoT Cell Search Procedure
   ============================================================================ */

/**
 * NB-IoT cell search procedure
 *
 * Canonical problem (L6): UE powers on and must find and synchronize
 * to an NB-IoT cell. The procedure:
 *
 * Step 1: Frequency scan — tune to raster frequencies
 * Step 2: NPSS detection — find subframe 5 timing
 * Step 3: Frequency offset correction — estimate CFO from NPSS
 * Step 4: NSSS detection — decode PCID (504 hypotheses)
 * Step 5: NPBCH decoding — extract MIB (SFN, SIB1 info)
 * Step 6: SIB1 acquisition — decode SIB1 for full cell config
 */

typedef enum {
    CELL_SEARCH_IDLE = 0,         /**< Not started */
    CELL_SEARCH_SCANNING,         /**< Scanning frequency raster */
    CELL_SEARCH_NPSS_DETECT,      /**< Searching for NPSS */
    CELL_SEARCH_NSSS_DECODE,      /**< Decoding NSSS for PCID */
    CELL_SEARCH_NPBCH_DECODE,     /**< Decoding MIB */
    CELL_SEARCH_SIB1_ACQUIRE,     /**< Acquiring SIB1 */
    CELL_SEARCH_COMPLETE,         /**< Cell found and configured */
    CELL_SEARCH_FAILED,           /**< No suitable cell found */
} nbiot_cell_search_state_t;

/**
 * Cell search state machine
 */
typedef struct {
    nbiot_cell_search_state_t state;       /**< Current search state */
    double   freq_raster_khz;              /**< Current frequency being scanned */
    uint16_t detected_pci;                  /**< Detected physical cell identity */
    double   freq_offset_hz;                /**< Estimated frequency offset */
    double   timing_offset_samples;         /**< Estimated timing offset */
    uint32_t sfn;                           /**< System frame number */
    uint64_t hsfn;                          /**< Hyper SFN */
    uint8_t  mib_bits[NBIOT_NPBCH_MIB_BITS]; /**< Decoded MIB bits */
    double   rsrp_dbm;                      /**< Measured RSRP */
    double   sinr_db;                       /**< Estimated SINR */
    nbiot_cell_config_t cell_config;        /**< Populated cell configuration */
    uint16_t scan_attempts;                 /**< Number of frequency scan attempts */
} nbiot_cell_search_t;

/**
 * Initialize cell search state
 */
void nbiot_cell_search_init(nbiot_cell_search_t *search);

/**
 * Process one frame of received samples through cell search state machine
 *
 * @param search   Cell search state (updated in place)
 * @param samples  One frame (10 ms) of complex baseband samples
 * @param fs       Sample rate in Hz
 * @return 0 on state advance, 1 if complete, -1 on failure
 */
int nbiot_cell_search_process(nbiot_cell_search_t *search,
                               const double complex *samples,
                               double fs);

/**
 * Initialize NB-IoT cell configuration with defaults
 */
void nbiot_cell_config_init_default(nbiot_cell_config_t *config);

#ifdef __cplusplus
}
#endif


/* ============================================================================
   NB-IoT Power Saving Types (from nbiot_power.c)
   ============================================================================ */

/** PSM state */
typedef enum {
    PSM_STATE_ACTIVE = 0,
    PSM_STATE_ACTIVE_TIMER,
    PSM_STATE_PSM,
    PSM_STATE_TAU,
} psm_state_t;

/** PSM timer configuration */
typedef struct {
    psm_state_t state;
    double   t3324_seconds;
    double   t3412_seconds;
    double   t3324_elapsed;
    double   t3412_elapsed;
    double   psm_current_ua;
    double   idle_current_ma;
    double   active_current_ma;
    double   tau_duration_sec;
    double   tau_current_ma;
    uint32_t tau_count;
} nbiot_psm_state_t;

/** eDRX configuration */
typedef struct {
    double   edrx_cycle_seconds;
    double   ptw_seconds;
    double   drx_cycle_seconds;
    double   rx_on_duration_ms;
    double   sleep_current_ua;
    double   rx_current_ma;
    uint32_t paging_count;
    uint32_t missed_page_count;
} nbiot_edrx_config_t;

/* PSM functions */
void nbiot_psm_init(nbiot_psm_state_t *psm);
psm_state_t nbiot_psm_advance(nbiot_psm_state_t *psm, double dt);
double nbiot_psm_current_ma(const nbiot_psm_state_t *psm);
double nbiot_psm_average_current_ma(const nbiot_psm_state_t *psm);

/* eDRX functions */
void nbiot_edrx_init(nbiot_edrx_config_t *edrx);
void nbiot_edrx_power_analysis(const nbiot_edrx_config_t *edrx,
                                double *avg_ma, double *duty_pct);

/* CE level and power functions */
int nbiot_select_ce_level(double rsrp_dbm, nbiot_ce_level_t *ce_level,
                           uint16_t *num_reps);
double nbiot_data_rate_with_ce(uint8_t mcs_index, uint16_t num_reps, int is_uplink);
double nbiot_battery_life_psm(const nbiot_psm_state_t *psm,
                               double battery_mah, double battery_v,
                               double tx_interval_s, uint16_t tx_data_bytes,
                               uint8_t mcs_index, uint16_t num_reps);
int nbiot_power_mode_select(double report_interval_s,
                             double *psm_avg_ma, double *edrx_avg_ma);

#endif /* NBIOT_PHY_H */
