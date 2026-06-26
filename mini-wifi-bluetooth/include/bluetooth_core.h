/**
 * @file bluetooth_core.h
 * @brief Bluetooth Core — FHSS, GFSK, BR/EDR, Baseband, Link Manager (L2,L3,L5)
 *
 * Implements Bluetooth BR/EDR core functionality:
 *   - Frequency Hopping Spread Spectrum (FHSS)
 *   - Gaussian Frequency Shift Keying (GFSK)
 *   - Bluetooth clock and slot timing
 *   - Packet types and construction
 *   - Link management and SCO/eSCO scheduling
 *   - Adaptive Frequency Hopping (AFH)
 *
 * Reference: Bluetooth Core Specification v5.4, Vol 2 "BR/EDR Controller"
 * Reference: Morrow, R., "Bluetooth: Operation and Use", McGraw-Hill 2002.
 */
#ifndef BLUETOOTH_CORE_H
#define BLUETOOTH_CORE_H

#include "wifi_bt_types.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Frequency Hopping Spread Spectrum — Core (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Initialize FHSS parameters for Bluetooth BR/EDR
 *
 * Bluetooth BR/EDR uses 79 channels from 2402 MHz to 2480 MHz
 * with 1 MHz spacing. France has a reduced hop set (23 channels).
 *
 * Hopping rate: 1600 hops/second (1 hop per 625 µs slot).
 * Each slot time = 625 µs.
 *
 * @param fhss    FHSS parameters to fill
 * @param region  0=worldwide (79 ch), 1=france (23 ch reduced)
 * @return 0 on success
 *
 * Complexity: O(1)
 */
int bt_fhss_init(bt_fhss_params_t *fhss, int region);

/**
 * @brief Compute Bluetooth hop frequency for a given slot
 *
 * The Bluetooth hop selection kernel uses a 28-bit clock (CLK),
 * the master's BD_ADDR, and the selected hop scheme.
 *
 * Basic hop selection:
 *   phase = (CLK[27:1] + offset) mod N_channels
 *   channel = hop_sequence[phase]
 *
 * @param channel       Output: RF channel index (0-78)
 * @param fhss          FHSS parameters
 * @param clk           Bluetooth clock context
 * @return 0 on success
 *
 * Complexity: O(1)
 * Reference: Bluetooth Core Spec v5.4, Vol 2, Part B, §2.6
 */
int bt_hop_select(int *channel, const bt_fhss_params_t *fhss,
                  const bt_clock_t *clk);

/**
 * @brief Generate the Bluetooth hop sequence from BD_ADDR
 *
 * The hop selection kernel uses the 28 LSBs of the master's BD_ADDR
 * to permute a pseudo-random hop sequence. The sequence covers all
 * N available channels exactly once per cycle.
 *
 * Algorithm uses the BD_ADDR-based permutation:
 *   perm = [5,1,3,7,2,4,6,0] (example for small N, actual is BD_ADDR-based)
 *
 * @param hop_sequence  Output hop sequence array
 * @param max_channels  Max channels in sequence
 * @param bd_addr       Master BD_ADDR
 * @return Number of channels in sequence (N_channels)
 *
 * Complexity: O(N) where N = number of channels
 */
int bt_hop_sequence_generate(int *hop_sequence, int max_channels,
                             const bt_address_t *bd_addr);

/**
 * @brief Adaptive Frequency Hopping — classify channels
 *
 * AFH classifies channels as "good" or "bad" based on packet error
 * statistics. Bad channels are avoided. Bluetooth requires at least
 * 20 good channels; minimum hop set size N_min = 20.
 *
 * Classification: if PER > threshold → mark as bad
 *
 * @param fhss           FHSS parameters (channel_map updated)
 * @param per_channel    Packet error rate per channel
 * @param n_channels     Number of channels
 * @param per_threshold  PER threshold for classifying as "bad"
 * @return Number of good channels remaining
 *
 * Complexity: O(N_channels)
 * Reference: Bluetooth Core Spec v5.4, Vol 2, Part B, §4.1.3
 */
int bt_afh_classify(bt_fhss_params_t *fhss, const double *per_channel,
                    int n_channels, double per_threshold);

/* ==========================================================================
 * GFSK Modulation / Demodulation (L3 Mathematical Structure)
 * ========================================================================== */

/**
 * @brief Initialize GFSK modulation parameters
 *
 * GFSK shapes each bit with a Gaussian filter before FM modulation.
 * The Gaussian filter impulse response:
 *   h(t) = (√(2π)/ln 2)·B·exp(-2π²·B²·t²/ln 2)
 *
 * BT = 0.5 gives 3-dB bandwidth of 500 kHz for 1 Mbps symbol rate.
 * The GFSK pulse is the convolution of the Gaussian filter with a
 * rectangular pulse of width T (the bit period).
 *
 * GFSK pulse formula:
 *   g(t) = ½[erfc(-π√(2/ln 2)·BT·(t/T+½)) - erfc(-π√(2/ln 2)·BT·(t/T-½))]
 *
 * @param gfsk         GFSK parameters to fill
 * @param bt_product   Bandwidth-time product (typically 0.5)
 * @param mod_index    Modulation index h
 * @param symbol_rate  Symbol rate in Mbps
 * @return 0 on success
 *
 * Complexity: O(gaussian_len)
 */
int bt_gfsk_init(bt_gfsk_params_t *gfsk, double bt_product,
                 double mod_index, double symbol_rate);

/**
 * @brief Modulate data bits using GFSK
 *
 * GFSK modulates the carrier frequency as:
 *   f(t) = f_c + h/(2T) * Σ b[n]·g(t - nT)
 *
 * where b[n] ∈ {+1, -1}, h = modulation index, g(t) = GFSK pulse.
 *
 * For Bluetooth BR (h=0.32): Δf_peak = h/(2T) ≈ 160 kHz
 * For BLE 1M (h=0.5): Δf_peak = 250 kHz
 *
 * @param mod_samples   Output baseband IQ samples
 * @param bits          Input data bits (1/0 → +1/-1)
 * @param n_bits        Number of bits
 * @param samples_per_symbol Oversampling ratio
 * @param gfsk          GFSK parameters
 * @return Number of output samples
 *
 * Complexity: O(n_bits × samples_per_symbol × gaussian_len)
 */
int bt_gfsk_modulate(double *mod_samples, const uint8_t *bits, int n_bits,
                     int samples_per_symbol, const bt_gfsk_params_t *gfsk);

/**
 * @brief GFSK demodulation via frequency discriminator
 *
 * Simple non-coherent GFSK demodulation:
 *   1. Compute phase from IQ samples: φ[n] = atan2(Q[n], I[n])
 *   2. Differentiate: Δφ[n] = φ[n] - φ[n-1]
 *   3. Decide: if Δφ > 0 → '1', else → '0'
 *
 * Assumes GFSK uses the standard FSK mapping where f > f_c = '1'.
 *
 * @param decoded_bits  Output decoded bits
 * @param iq_samples    Input IQ samples (interleaved I/Q)
 * @param n_iq          Number of IQ pairs
 * @param samples_per_symbol Oversampling ratio
 * @param gfsk          GFSK parameters
 * @return Number of decoded bits
 *
 * Complexity: O(n_iq)
 */
int bt_gfsk_demodulate(uint8_t *decoded_bits, const double *iq_samples,
                       int n_iq, int samples_per_symbol,
                       const bt_gfsk_params_t *gfsk);

/**
 * @brief Compute GFSK eye diagram opening metric
 *
 * Evaluates the inter-symbol interference (ISI) of the GFSK pulse
 * at the decision instant. The eye opening at t=0 with no ISI from
 * adjacent symbols gives a metric for signal quality.
 *
 * @param gfsk          GFSK parameters
 * @return Eye opening ratio (1.0 = ideal, <1.0 = ISI present)
 *
 * Complexity: O(gaussian_len)
 */
double bt_gfsk_eye_opening(const bt_gfsk_params_t *gfsk);

/* ==========================================================================
 * Bluetooth Packet Construction (L2 Core Concept)
 * ========================================================================== */

/** Bluetooth BR/EDR packet types */
typedef enum {
    BT_PKT_NULL      = 0x00,  /**< NULL — no payload, link supervision */
    BT_PKT_POLL      = 0x01,  /**< POLL — master polls slave */
    BT_PKT_FHS       = 0x02,  /**< FHS — Frequency Hop Synchronization */
    BT_PKT_DM1       = 0x03,  /**< DM1 — Data Medium rate, 1 slot */
    BT_PKT_DH1       = 0x04,  /**< DH1 — Data High rate, 1 slot */
    BT_PKT_DM3       = 0x0A,  /**< DM3 — Data Medium rate, 3 slots */
    BT_PKT_DH3       = 0x0B,  /**< DH3 — Data High rate, 3 slots */
    BT_PKT_DM5       = 0x0E,  /**< DM5 — Data Medium rate, 5 slots */
    BT_PKT_DH5       = 0x0F,  /**< DH5 — Data High rate, 5 slots */
    BT_PKT_HV1       = 0x05,  /**< HV1 — High-quality Voice, 1 slot, 1.25ms */
    BT_PKT_HV2       = 0x06,  /**< HV2 — Voice, 1 slot, FEC 2/3 */
    BT_PKT_HV3       = 0x07,  /**< HV3 — Voice, 1 slot, no FEC */
    BT_PKT_EV3       = 0x08,  /**< EV3 — Enhanced Voice 3-slot */
    BT_PKT_EV4       = 0x0C,  /**< EV4 — Enhanced Voice 3-slot, 2x capacity */
    BT_PKT_EV5       = 0x0D,  /**< EV5 — Enhanced Voice 5-slot */
    BT_PKT_2EV3      = 0x10,  /**< 2-EV3 — EDR voice */
    BT_PKT_2EV5      = 0x11   /**< 2-EV5 — EDR voice */
} bt_packet_type_t;

/**
 * @brief Bluetooth baseband packet format
 *
 * Access Code (68/72 bits) | Header (54 bits) | Payload (0-2744 bits)
 *   - Access Code: Preamble(4) + Sync Word(64) + Trailer(4)
 *   - Header: LT_ADDR(3) + Type(4) + Flow(1) + ARQN(1) + SEQN(1) + HEC(8)
 *   - Payload: Data + CRC(16) (DM packets have FEC 2/3)
 */
typedef struct {
    bt_packet_type_t type;
    uint8_t  lt_addr;        /**< Logical Transport Address (3-bit) */
    uint8_t  flow;           /**< Flow control bit */
    uint8_t  arqn;           /**< Automatic Repeat reQuest Number */
    uint8_t  seqn;           /**< Sequence Number */
    uint8_t *payload;        /**< Payload data */
    int      payload_len;     /**< Payload length in bytes */
    int      slot_count;     /**< Number of slots (1/3/5) */
    int      has_fec;        /**< 1 if FEC 1/3 or 2/3 rate applied */
    int      has_crc;        /**< 1 if CRC-16 appended */
} bt_packet_t;

/**
 * @brief Compute Bluetooth Header Error Check (HEC)
 *
 * HEC is an 8-bit CRC computed over the 10 header bits before HEC
 * with generator polynomial: G(x) = x⁸ + x⁷ + x⁵ + x² + x + 1 (0x1A7)
 *
 * The HEC is initialized with the slave's upper address part (UAP).
 *
 * @param header_word   First 10 bits of header (packed in uint16_t LSB-aligned)
 * @param uap           Upper Address Part (8 bits) for initialization
 * @return 8-bit HEC
 *
 * Complexity: O(1)
 */
uint8_t bt_hec_compute(uint16_t header_word, uint8_t uap);

/**
 * @brief Construct a Bluetooth baseband packet
 *
 * Builds a complete Bluetooth packet including access code, header,
 * payload, and CRC. Optionally applies FEC encoding.
 *
 * @param packet_buf    Output packet buffer
 * @param max_len       Max buffer size
 * @param pkt           Packet descriptor
 * @param bd_addr       Master BD_ADDR (for access code sync word)
 * @return Packet length in bytes, or -1 on error
 *
 * Complexity: O(payload_len)
 */
int bt_packet_build(uint8_t *packet_buf, int max_len,
                    const bt_packet_t *pkt, const bt_address_t *bd_addr);

/**
 * @brief Parse a received Bluetooth baseband packet
 *
 * Extracts header fields and payload from a raw baseband packet.
 *
 * @param pkt           Output packet struct to fill
 * @param packet_buf    Raw received packet
 * @param packet_len    Received packet length
 * @return 0 on valid packet, -1 on error
 */
int bt_packet_parse(bt_packet_t *pkt, const uint8_t *packet_buf, int packet_len);

/* ==========================================================================
 * Bluetooth Clock & Slot Management (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Initialize Bluetooth clock
 *
 * The native clock is a free-running 28-bit counter with 312.5 µs
 * resolution (derived from a 3.2 kHz reference). Two ticks = one
 * 625 µs slot.
 *
 * @param clk       Clock struct to initialize
 * @param init_val  Initial clock value (0 for cold start)
 */
void bt_clock_init(bt_clock_t *clk, uint32_t init_val);

/**
 * @brief Advance Bluetooth clock by one slot (625 µs)
 *
 * Increments CLK by 2 (each tick = 312.5 µs, slot = 2 ticks).
 *
 * @param clk       Clock to advance
 */
void bt_clock_advance_slot(bt_clock_t *clk);

/**
 * @brief Get current slot number from clock
 *
 * Slot = CLK[27:1] (divide by 2, ignoring LSB).
 *
 * @param clk       Clock value
 * @return Slot number
 */
uint32_t bt_clock_slot(const bt_clock_t *clk);

/* ==========================================================================
 * SCO / eSCO Scheduling (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief SCO link scheduling parameters
 *
 * SCO reserves slots at fixed intervals:
 *   - T_sco = 2*Tsco (Tsco = 2/4/6 slots for HV1/HV2/HV3)
 *   - D_sco = T_sco - Tsco reserved slots
 *
 * HV1: T_sco=2, uses every other slot (64 kbps each direction)
 * HV2: T_sco=4, uses every 4th slot (64 kbps)
 * HV3: T_sco=6, uses every 6th slot (64 kbps)
 */
typedef struct {
    int      tsco;            /**< SCO interval in slots (2/4/6) */
    int      dsco;            /**< SCO offset from master-to-slave start */
    int      next_sco_slot;   /**< Next reserved SCO slot number */
    int      sco_count;       /**< Number of SCO links active */
} bt_sco_schedule_t;

/**
 * @brief Initialize SCO scheduling
 *
 * @param sched     SCO schedule
 * @param tsco      SCO interval (2=HV1, 4=HV2, 6=HV3)
 * @return 0 on success
 */
int bt_sco_schedule_init(bt_sco_schedule_t *sched, int tsco);

/**
 * @brief Check if current slot is reserved for SCO
 *
 * @param sched     SCO schedule
 * @param slot_num  Current slot number
 * @return 1 if reserved for SCO, 0 otherwise
 */
int bt_sco_is_reserved(const bt_sco_schedule_t *sched, uint32_t slot_num);

/* ==========================================================================
 * Bluetooth Baseband Cipher (E0 stream cipher, L5 Algorithm)
 * ========================================================================== */

/**
 * @brief Bluetooth E0 stream cipher initialization
 *
 * E0 is a synchronous stream cipher using 4 LFSRs (25+31+33+39 = 128 bits)
 * with a summation combiner. Used for BR/EDR encryption.
 *
 * LFSR polynomials:
 *   LFSR1: x²⁵ + x²⁰ + x¹² + x⁸ + 1  (25 bits)
 *   LFSR2: x³¹ + x²⁴ + x¹⁶ + x¹² + 1  (31 bits)
 *   LFSR3: x³³ + x²⁸ + x²⁴ + x⁴ + 1   (33 bits)
 *   LFSR4: x³⁹ + x³⁶ + x²⁸ + x⁴ + 1   (39 bits)
 *
 * @param key       128-bit encryption key (KC)
 * @param bd_addr   Master BD_ADDR
 * @param clk       26 bits of master clock
 * @return 0 on success
 *
 * Complexity: O(1) for initialization
 */
int bt_e0_init(const uint8_t key[16], const bt_address_t *bd_addr,
               uint32_t clk);

/**
 * @brief Generate E0 keystream bits
 *
 * @param keystream Output keystream byte (8 bits)
 * @param n_bits    Number of bits to generate (1-8)
 * @return 0 on success
 *
 * Complexity: O(n_bits × 1)
 */
int bt_e0_keystream(uint8_t *keystream, int n_bits);

/**
 * @brief Encrypt/decrypt data using E0 stream cipher
 *
 * BT encryption: ciphertext = plaintext XOR keystream
 *
 * @param output    Output buffer (same length as input)
 * @param input     Input data
 * @param n_bytes   Number of bytes
 * @return 0 on success
 *
 * Complexity: O(n_bytes)
 */
int bt_e0_crypt(uint8_t *output, const uint8_t *input, int n_bytes);

/* ==========================================================================
 * Link Manager Protocol Concepts (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Bluetooth device discovery — Inquiry procedure
 *
 * The inquiring device transmits ID packets on inquiry hop frequencies.
 * Discoverable devices respond with FHS packets containing their BD_ADDR,
 * clock offset, and device class.
 *
 * @param found_addr    Output: discovered device BD_ADDR
 * @param found_class   Output: device class
 * @param inquiry_time_ms   Inquiry duration in ms (≥10.24 s for full inquiry)
 * @return 0 if a device was found, -1 on timeout
 *
 * Complexity: O(inquiry_time) externally
 */
int bt_inquiry_discover(bt_address_t *found_addr, bt_device_class_t *found_class,
                        int inquiry_time_ms);

/**
 * @brief Bluetooth paging — connect to a known device
 *
 * The paging procedure synchronizes with a known device's clock and
 * hop sequence. After paging, the pager becomes master of the piconet.
 *
 * @param target_addr  Target BD_ADDR
 * @param target_clk   Estimated target clock (from prior inquiry)
 * @param page_timeout_ms Paging timeout
 * @return 0 on connection established, -1 on timeout
 */
int bt_page_connect(const bt_address_t *target_addr, uint32_t target_clk,
                    int page_timeout_ms);

/* ==========================================================================
 * RSSI / Link Quality (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Convert RSSI to distance estimate (log-distance path loss)
 *
 * Using simplified log-distance path loss model:
 *   PL(d) = PL(d₀) + 10·n·log₁₀(d/d₀)
 *   RSSI = P_tx - PL(d)
 *   d = d₀ · 10^((P_tx - RSSI - PL(d₀)) / (10·n))
 *
 * @param rssi_dbm       RSSI in dBm
 * @param tx_power_dbm   Transmit power in dBm
 * @param path_loss_exp  Path loss exponent (2.0 free space, 2.7-3.5 indoor)
 * @param ref_loss_db    Reference loss PL(d₀) at 1 meter
 * @return Estimated distance in meters
 *
 * Complexity: O(1)
 */
double bt_rssi_to_distance(double rssi_dbm, double tx_power_dbm,
                           double path_loss_exp, double ref_loss_db);

/**
 * @brief Bluetooth Link Quality estimation
 *
 * Link quality is estimated from received signal strength, packet
 * error statistics, and the link supervision timeout status.
 *
 * @param rssi_dbm       Current RSSI
 * @param per            Recent packet error rate
 * @param noise_floor    Noise floor in dBm
 * @return Link quality metric (0-255, higher = better, per BT HCI spec)
 */
int bt_link_quality(double rssi_dbm, double per, double noise_floor);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_CORE_H */
