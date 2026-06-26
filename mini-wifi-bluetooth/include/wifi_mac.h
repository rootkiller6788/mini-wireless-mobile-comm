/**
 * @file wifi_mac.h
 * @brief WiFi MAC Layer — CSMA/CA, Frame Format, QoS, Aggregation (L2,L5)
 *
 * Implements IEEE 802.11 MAC sublayer functionality:
 *   - Distributed Coordination Function (DCF) — CSMA/CA
 *   - Enhanced Distributed Channel Access (EDCA) — QoS
 *   - Frame construction and parsing
 *   - A-MPDU / A-MSDU aggregation
 *   - Block ACK mechanism
 *   - RTS/CTS virtual carrier sense
 *
 * Reference: IEEE Std 802.11-2020, Clause 10 "MAC sublayer"
 * Reference: Gast, M.S., "802.11 Wireless Networks: The Definitive
 *            Guide", 2nd ed., O'Reilly, 2005.
 */
#ifndef WIFI_MAC_H
#define WIFI_MAC_H

#include "wifi_bt_types.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * CSMA/CA — Distributed Coordination Function (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Initialize CSMA/CA parameters for a given PHY
 *
 * Default slot times and IFS values:
 *   - OFDM (20 MHz): Slot = 9 µs, SIFS = 16 µs
 *   - DSSS (b): Slot = 20 µs, SIFS = 10 µs
 *   - HT (n): Slot = 9 µs, SIFS = 16 µs
 *   - VHT (ac): Slot = 9 µs, SIFS = 16 µs
 *
 * DIFS = SIFS + 2×SlotTime
 * CWmin = 15 (2^4 - 1) for OFDM, 31 (2^5 - 1) for DSSS
 * CWmax = 1023 (2^10 - 1)
 *
 * @param params   CSMA/CA parameters to fill
 * @param phy      Physical layer mode
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int csma_params_init(wifi_csma_params_t *params, wifi_phy_mode_t phy);

/**
 * @brief Compute backoff duration for CSMA/CA
 *
 * Backoff = random[0, CW] × SlotTime
 *
 * The contention window doubles after each failed transmission:
 *   CW_new = min(2×(CW+1) - 1, CWmax)
 * On successful transmission: CW = CWmin
 *
 * @param cw             Current contention window
 * @param backoff_slots  Randomly selected backoff slot count
 * @param slot_time_us   Slot time in microseconds
 * @return Backoff duration in microseconds
 *
 * Complexity: O(1)
 * Reference: IEEE 802.11-2020 §10.3.3
 */
double csma_backoff_duration(int cw, int *backoff_slots, double slot_time_us);

/**
 * @brief CSMA/CA channel access decision
 *
 * Implements the DCF channel access procedure:
 *   1. Sense channel (CS — Carrier Sense)
 *   2. If idle for DIFS, immediately transmit
 *   3. If busy, wait until idle + DIFS, then random backoff
 *   4. Decrement backoff counter while channel idle
 *   5. Freeze counter when channel busy (virtual + physical CS)
 *   6. Transmit when counter reaches 0
 *
 * Returns whether transmission is permitted and required wait time.
 *
 * @param can_transmit    Output: 1 if ready to transmit
 * @param wait_us         Output: wait time in microseconds before TX
 * @param channel_busy    Input: current channel state (1=busy, 0=idle)
 * @param backoff_remaining Input/output: remaining backoff counter
 * @param params          CSMA/CA parameters
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int csma_channel_access(int *can_transmit, double *wait_us,
                        int channel_busy, int *backoff_remaining,
                        const wifi_csma_params_t *params);

/**
 * @brief Calculate NAV (Network Allocation Vector) duration
 *
 * NAV is the virtual carrier-sense mechanism. Each frame carries a
 * Duration/ID field that reserves the medium for the transmission
 * sequence duration (including ACK and IFS).
 *
 * NAV_duration = TX_time(frame) + SIFS + TX_time(ACK) + ... (µs)
 *
 * @param frame_len_bytes  Length of the data frame
 * @param data_rate_mbps   Transmission data rate
 * @param params           CSMA/CA parameters
 * @return NAV duration in microseconds
 *
 * Complexity: O(1)
 */
double nav_compute_duration(int frame_len_bytes, double data_rate_mbps,
                            const wifi_csma_params_t *params);

/**
 * @brief Contention window doubling after collision
 *
 * CW_new = min(2*(CW_old + 1) - 1, CWmax)
 *
 * Also known as Binary Exponential Backoff (BEB).
 * Standard defines up to 6 doublings (retries 0-6 = 7 attempts total).
 *
 * @param cw     Current contention window
 * @param cw_min Minimum contention window
 * @param cw_max Maximum contention window
 * @return New contention window
 *
 * Complexity: O(1)
 */
int csma_cw_double(int cw, int cw_min, int cw_max);

/* ==========================================================================
 * EDCA — Enhanced Distributed Channel Access (L5 Algorithm)
 * ========================================================================== */

/** EDCA Access Categories (AC) per 802.11e/802.11-2020 */
typedef enum {
    EDCA_AC_BK  = 0,        /**< Background (lowest priority) */
    EDCA_AC_BE  = 1,        /**< Best Effort (default) */
    EDCA_AC_VI  = 2,        /**< Video (high priority) */
    EDCA_AC_VO  = 3         /**< Voice (highest priority) */
} edca_access_category_t;

/**
 * @brief EDCA parameter set for a given access category
 *
 * EDCA parameters differ per AC to provide service differentiation:
 *   - AIFSN: Arbitration IFS Number (lower = higher priority)
 *   - CWmin/CWmax: Smaller for higher priority
 *   - TXOP Limit: Maximum transmission opportunity duration
 */
typedef struct {
    int      aifsn;          /**< AIFS Number (AIFS = SIFS + AIFSN*SlotTime) */
    int      cw_min;         /**< Minimum contention window */
    int      cw_max;         /**< Maximum contention window */
    double   txop_limit_ms;  /**< TXOP limit in ms (0 = single frame) */
} edca_params_t;

/**
 * @brief Initialize EDCA parameters per access category
 *
 * Default EDCA parameters (OFDM PHY):
 *   | AC  | AIFSN | CWmin | CWmax  | TXOP Limit |
 *   |-----|-------|-------|--------|-----------|
 *   | BK  |   7   |  15   |  1023  |   0       |
 *   | BE  |   3   |  15   |  1023  |   0       |
 *   | VI  |   2   |   7   |   15   |   3.008 ms|
 *   | VO  |   2   |   3   |    7   |   1.504 ms|
 *
 * @param edca  Output EDCA parameters
 * @param ac    Access category
 * @param phy   PHY mode (affects slot time)
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int edca_params_init(edca_params_t *edca, edca_access_category_t ac,
                     wifi_phy_mode_t phy);

/**
 * @brief EDCA backoff procedure
 *
 * Unlike DCF, EDCA uses AIFS instead of DIFS and per-AC queue
 * with internal collision resolution (higher priority AC wins).
 *
 * AIFS[AC] = SIFS + AIFSN[AC] × SlotTime
 *
 * @param backoff_us     Output: computed backoff (microseconds)
 * @param ac             Access category
 * @param cw             Current contention window for this AC
 * @param params         CSMA/CA base parameters
 * @param edca           EDCA parameters for this AC
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int edca_backoff(double *backoff_us, edca_access_category_t ac, int cw,
                 const wifi_csma_params_t *params, const edca_params_t *edca);

/* ==========================================================================
 * MAC Frame Construction (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Build a WiFi data frame
 *
 * Constructs a complete 802.11 data frame including MAC header,
 * optional QoS control, and FCS (Frame Check Sequence — CRC-32).
 *
 * Frame layout:
 *   [MAC Header (30-36 bytes)] [LLC/SNAP (8 bytes)] [Payload] [FCS (4 bytes)]
 *
 * @param frame        Output frame buffer
 * @param max_len      Maximum frame length
 * @param da           Destination MAC address (6 bytes)
 * @param sa           Source MAC address (6 bytes)
 * @param bssid        BSSID (6 bytes)
 * @param payload      Data payload
 * @param payload_len  Payload length
 * @param tid          Traffic Identifier (0-7) for QoS
 * @return Total frame length, or -1 on error
 *
 * Complexity: O(payload_len)
 */
int wifi_frame_build_data(uint8_t *frame, int max_len,
                          const uint8_t da[6], const uint8_t sa[6],
                          const uint8_t bssid[6],
                          const uint8_t *payload, int payload_len,
                          int tid);

/**
 * @brief Parse a received WiFi frame
 *
 * Extracts key fields from an 802.11 MAC header.
 *
 * @param header        Output MAC header struct to fill
 * @param frame_type    Output frame type
 * @param subtype       Output subtype
 * @param to_ds         Output: 1 if To-DS bit set
 * @param from_ds       Output: 1 if From-DS bit set
 * @param frame         Input raw frame bytes
 * @param frame_len     Length of received frame
 * @return 0 on success, -1 on invalid frame
 *
 * Complexity: O(1)
 */
int wifi_frame_parse(wifi_mac_header_t *header,
                     wifi_frame_type_t *frame_type,
                     int *subtype, int *to_ds, int *from_ds,
                     const uint8_t *frame, int frame_len);

/**
 * @brief Build an RTS (Request To Send) control frame
 *
 * RTS frame format (20 bytes):
 *   - Frame Control (2): Type=01, Subtype=1011
 *   - Duration (2): NAV duration for CTS + Data + ACK + 3×SIFS
 *   - RA (6): Receiver address
 *   - TA (6): Transmitter address
 *   - FCS (4): CRC-32
 *
 * @param rts_frame    Output RTS frame (20 bytes)
 * @param max_len      Max buffer length
 * @param ra           Receiver address
 * @param ta           Transmitter address
 * @param nav_duration_us NAV duration in microseconds
 * @return Frame length (20), or -1 on error
 */
int wifi_frame_build_rts(uint8_t *rts_frame, int max_len,
                         const uint8_t ra[6], const uint8_t ta[6],
                         uint16_t nav_duration_us);

/**
 * @brief Build a CTS (Clear To Send) control frame
 *
 * CTS frame format (14 bytes):
 *   - Frame Control (2): Type=01, Subtype=1100
 *   - Duration (2): NAV duration for Data + ACK + 2×SIFS
 *   - RA (6): Receiver address (copied from TA of RTS)
 *   - FCS (4): CRC-32
 *
 * @param cts_frame    Output CTS frame
 * @param max_len      Max buffer length
 * @param ra           Receiver address
 * @param nav_duration_us NAV duration
 * @return Frame length (14), or -1 on error
 */
int wifi_frame_build_cts(uint8_t *cts_frame, int max_len,
                         const uint8_t ra[6], uint16_t nav_duration_us);

/**
 * @brief Build an ACK frame
 *
 * ACK frame (14 bytes): same format as CTS but subtype=1101
 *
 * @param ack_frame    Output ACK frame
 * @param max_len      Max buffer length
 * @param ra           Receiver address
 * @return Frame length (14), or -1 on error
 */
int wifi_frame_build_ack(uint8_t *ack_frame, int max_len, const uint8_t ra[6]);

/* ==========================================================================
 * Block ACK Mechanism (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief Block ACK bitmap (64 bits = up to 64 MSDUs)
 *
 * 802.11n introduced Block ACK to acknowledge multiple frames with
 * a single response. Block ACK bitmap: 1 = received, 0 = missing.
 */
typedef struct {
    uint64_t bitmap;         /**< 64-bit ACK bitmap */
    int      starting_seq;   /**< Starting sequence number */
    int      n_frames;       /**< Number of frames in block */
} wifi_block_ack_t;

/**
 * @brief Initialize a block ACK session
 *
 * @param ba           Block ACK struct
 * @param start_seq    Starting sequence number
 * @param n_frames     Number of frames in block (max 64)
 * @return 0 on success
 */
int block_ack_init(wifi_block_ack_t *ba, int start_seq, int n_frames);

/**
 * @brief Mark a frame as received in the block ACK bitmap
 *
 * @param ba           Block ACK struct
 * @param seq_num      Sequence number to mark as received
 * @return 0 if recorded, -1 if out of range
 */
int block_ack_record(wifi_block_ack_t *ba, int seq_num);

/**
 * @brief Check if a frame was received per block ACK bitmap
 *
 * @param ba           Block ACK struct
 * @param seq_num      Sequence number to query
 * @return 1 if received, 0 if missing, -1 if out of range
 */
int block_ack_is_received(const wifi_block_ack_t *ba, int seq_num);

/**
 * @brief Build retransmission list from block ACK (missing frames only)
 *
 * @param retx_seqs    Output array of missing sequence numbers
 * @param max_retx     Max size of retx_seqs array
 * @param ba           Block ACK struct
 * @return Number of missing frames
 */
int block_ack_get_missing(int *retx_seqs, int max_retx,
                          const wifi_block_ack_t *ba);

/* ==========================================================================
 * A-MSDU / A-MPDU Aggregation (L5 Algorithm — 802.11n+)
 * ========================================================================== */

/**
 * @brief Aggregate multiple MSDUs into one A-MSDU
 *
 * A-MSDU aggregates multiple MSDUs with the same TID, SA, and DA
 * into a single MPDU. Each subframe has a 14-byte header:
 *   - DA (6), SA (6), Length (2)
 * Padding ensures each subframe aligns to 4 bytes.
 *
 * Max A-MSDU size: 3839/7935 bytes (n) or 11454 bytes (ac)
 *
 * @param amsdu        Output A-MSDU buffer
 * @param max_len      Max buffer length
 * @param da           Common destination address
 * @param sa           Common source address
 * @param msdus        Array of MSDU payloads
 * @param msdu_lens    Array of MSDU lengths
 * @param n_msdus      Number of MSDUs to aggregate
 * @return Total A-MSDU length, or -1 on error
 *
 * Complexity: O(total_payload_bytes)
 */
int amsdu_aggregate(uint8_t *amsdu, int max_len,
                    const uint8_t da[6], const uint8_t sa[6],
                    const uint8_t **msdus, const int *msdu_lens, int n_msdus);

/**
 * @brief Disassemble an A-MSDU into individual MSDUs
 *
 * @param msdus        Output array of pointers into amsdu buffer
 * @param msdu_lens    Output MSDU lengths
 * @param max_msdus    Max number of MSDUs to extract
 * @param amsdu        Input A-MSDU buffer
 * @param amsdu_len    A-MSDU length
 * @return Number of MSDUs extracted, or -1 on error
 */
int amsdu_disassemble(const uint8_t **msdus, int *msdu_lens, int max_msdus,
                      const uint8_t *amsdu, int amsdu_len);

/* ==========================================================================
 * Throughput Estimation (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Compute effective MAC-layer throughput
 *
 * Effective throughput accounts for:
 *   - PHY preamble and header overhead
 *   - MAC header overhead
 *   - IFS (DIFS/SIFS) spacing
 *   - Backoff time
 *   - ACK overhead
 *   - Packet error rate retransmissions
 *
 * T_eff = (payload_bits * (1 - PER)) / (T_preamble + T_signal + T_data + SIFS + T_ACK + DIFS + T_backoff_avg)
 *
 * @param payload_bytes       Payload size per frame
 * @param phy_rate_mbps       PHY data rate
 * @param per                 Packet error rate (0.0 to 1.0)
 * @param params              CSMA/CA parameters
 * @param phy                 PHY mode (for preamble duration)
 * @return Effective throughput in Mbps
 *
 * Complexity: O(1)
 * Reference: Bianchi, G., "Performance Analysis of the IEEE 802.11
 *            DCF", IEEE JSAC, 2000.
 */
double wifi_throughput_estimate(int payload_bytes, double phy_rate_mbps,
                                double per, const wifi_csma_params_t *params,
                                wifi_phy_mode_t phy);

/**
 * @brief Bianchi model: saturated throughput for DCF
 *
 * The Bianchi model computes the maximum achievable throughput for
 * a saturated network with n contending stations using a 2-D Markov
 * chain model of the binary exponential backoff.
 *
 * τ = 2 / (CWmin + 1)   (1st order approximation, large n)
 * S = (P_s·P_tr·E[P]) / ((1-P_tr)·σ + P_tr·P_s·T_s + P_tr·(1-P_s)·T_c)
 *
 * where:
 *   P_tr = 1 - (1-τ)^n       (probability at least one transmits)
 *   P_s = n·τ·(1-τ)^(n-1) / P_tr  (probability of successful transmission)
 *
 * @param n_stations     Number of contending stations
 * @param cw_min         Minimum contention window
 * @param payload_bytes  Average payload bytes
 * @param phy_rate_mbps  PHY data rate
 * @param params         CSMA/CA parameters
 * @return Saturated throughput (fraction of channel capacity)
 *
 * Complexity: O(1)
 */
double bianchi_throughput(int n_stations, int cw_min,
                          int payload_bytes, double phy_rate_mbps,
                          const wifi_csma_params_t *params);

/* ==========================================================================
 * CRC-32 / FCS (L3 Mathematical Structure)
 * ========================================================================== */

/**
 * @brief Compute IEEE 802.11 CRC-32 (Frame Check Sequence)
 *
 * Generator polynomial:
 *   G(x) = x³² + x²⁶ + x²³ + x²² + x¹⁶ + x¹² + x¹¹ + x¹⁰ + x⁸ + x⁷ + x⁵ + x⁴ + x² + x + 1
 *   (Reversed: 0xEDB88320, complement output)
 *
 * Standard Ethernet/802.11 MAC CRC. FCS is transmitted MSB first.
 *
 * @param data    Input data
 * @param n_bytes Number of bytes
 * @return 32-bit CRC (canonical: bits complement-inverted)
 *
 * Complexity: O(N)
 */
uint32_t crc32_80211(const uint8_t *data, int n_bytes);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MAC_H */
