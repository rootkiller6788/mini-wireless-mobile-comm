/**
 * @file bluetooth_ble.h
 * @brief Bluetooth Low Energy — BLE PHY, Link Layer, GATT, Advertising (L2,L5,L6)
 *
 * Implements Bluetooth Low Energy (BLE) protocol features:
 *   - BLE PHY (1M, 2M, Coded)
 *   - Link Layer state machine (Advertising, Scanning, Initiating, Connection)
 *   - GATT server/client (attribute database, service discovery)
 *   - Advertising data formatting
 *   - Connection parameter update
 *   - BLE security (LE Secure Connections)
 *   - BLE mesh networking basics
 *
 * Reference: Bluetooth Core Specification v5.4, Vol 6 "Low Energy Controller"
 * Reference: Townsend, K. et al., "Getting Started with Bluetooth Low Energy",
 *            O'Reilly, 2014.
 */
#ifndef BLUETOOTH_BLE_H
#define BLUETOOTH_BLE_H

#include "wifi_bt_types.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * BLE Link Layer State Machine (L2 Core Concept)
 * ========================================================================== */

/** BLE Link Layer states */
typedef enum {
    BLE_LL_STANDBY      = 0,  /**< Idle, no TX/RX */
    BLE_LL_ADVERTISING  = 1,  /**< Transmitting advertising packets */
    BLE_LL_SCANNING     = 2,  /**< Listening for advertising packets */
    BLE_LL_INITIATING   = 3,  /**< Initiating connection to an advertiser */
    BLE_LL_CONNECTION   = 4,  /**< In connection (master or slave role) */
    BLE_LL_ISOCHRONOUS  = 5   /**< Isochronous channel (LE Audio, BT 5.2) */
} ble_ll_state_t;

/** BLE device role in connection */
typedef enum {
    BLE_ROLE_MASTER     = 0,  /**< Central / Master — initiates connection */
    BLE_ROLE_SLAVE      = 1   /**< Peripheral / Slave — accepts connection */
} ble_role_t;

/**
 * @brief BLE connection parameters
 *
 * Connection interval = conn_interval × 1.25 ms (range 7.5 ms to 4.0 s)
 * Supervision timeout = timeout × 10 ms (range 100 ms to 32 s)
 * Slave latency = number of connection events slave may skip
 */
typedef struct {
    double   conn_interval_ms;    /**< Connection interval (7.5-4000 ms, multiples of 1.25) */
    int      slave_latency;       /**< Slave may skip this many events (0-499) */
    double   supervision_timeout_ms;/**< Link loss detection timeout (100-32000 ms) */
    int      conn_event_counter;  /**< Current connection event count */
    int      channel_map[40];     /**< Bitmap of used data channels (0-36) */
    int      hop_increment;       /**< Hop increment (5-16, pseudo-random) */
    int      current_data_ch;     /**< Current data channel index */
} ble_conn_params_t;

/**
 * @brief Initialize BLE connection parameters
 *
 * Default values:
 *   - Connection interval: 30 ms (good balance latency/power)
 *   - Slave latency: 0 (no skipped events)
 *   - Supervision timeout: 2000 ms (20× conn interval, minimum 10×)
 *
 * @param params             Connection parameters
 * @param interval_ms        Connection interval in ms
 * @param latency            Slave latency
 * @param timeout_ms         Supervision timeout
 * @return 0 on success, -1 on invalid parameters
 */
int ble_conn_params_init(ble_conn_params_t *params, double interval_ms,
                         int latency, double timeout_ms);

/**
 * @brief BLE link layer state transition
 *
 * State machine transitions:
 *   STANDBY → ADVERTISING (start advertising)
 *   STANDBY → SCANNING (start scan)
 *   STANDBY → INITIATING (connect to advertiser)
 *   ADVERTISING → CONNECTION (receive CONNECT_IND)
 *   INITIATING → CONNECTION (receive CONNECT_IND acknowledgment)
 *   SCANNING → STANDBY (stop scanning)
 *   CONNECTION → STANDBY (disconnect)
 *
 * @param current_state  Current LL state
 * @param new_state      Desired new state (output)
 * @param cmd            Command: 0=stop, 1=start_advertise, 2=start_scan,
 *                       3=initiate_connect, 4=disconnect
 * @return 0 if transition is valid, -1 if invalid
 */
int ble_ll_state_transition(ble_ll_state_t *new_state,
                            ble_ll_state_t current_state, int cmd);

/* ==========================================================================
 * BLE Advertising & Scanning (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief Format BLE advertising data
 *
 * Advertising data follows TLV format:
 *   Length(1) | AD Type(1) | AD Data(Length-1)
 *
 * Common AD Types:
 *   0x01: Flags
 *   0x02: Incomplete List of 16-bit Service UUIDs
 *   0x03: Complete List of 16-bit Service UUIDs
 *   0x08: Shortened Local Name
 *   0x09: Complete Local Name
 *   0x0A: TX Power Level
 *   0xFF: Manufacturer Specific Data
 *
 * @param adv_data       Output advertising data buffer (max 31 bytes)
 * @param max_len        Max advertising data length
 * @param flags          Advertising flags (0x02=General Discoverable, etc.)
 * @param name           Device name string
 * @param tx_power       TX power in dBm
 * @param mfg_id         Manufacturer ID (2 bytes, per Bluetooth SIG)
 * @param mfg_data       Manufacturer specific data
 * @param mfg_data_len   Manufacturer data length
 * @return Advertising data length, or -1 on error
 *
 * Complexity: O(adv_data_len)
 */
int ble_adv_data_format(uint8_t *adv_data, int max_len,
                        uint8_t flags, const char *name,
                        int8_t tx_power, uint16_t mfg_id,
                        const uint8_t *mfg_data, int mfg_data_len);

/**
 * @brief Parse received BLE advertising report
 *
 * Extracts fields from a received advertising report (ADV_IND, SCAN_RSP, etc.)
 *
 * @param adv_type       Output: advertising event type
 * @param rssi           Output: RSSI of received packet
 * @param adv_addr       Output: advertiser's BD_ADDR
 * @param name           Output: device name (if present, max 31 chars)
 * @param name_len       Output: length of device name
 * @param adv_data       Input: raw advertising data
 * @param adv_data_len   Input: advertising data length
 * @return 0 on success, -1 on malformed data
 */
int ble_adv_parse(ble_adv_type_t *adv_type, int8_t *rssi,
                  bt_address_t *adv_addr, char *name, int *name_len,
                  const uint8_t *adv_data, int adv_data_len);

/**
 * @brief Compute BLE advertising event timing
 *
 * Advertising interval: advInterval = N × 0.625 ms (N = 32 to 16384)
 *   → Interval range: 20 ms to 10.24 s
 *
 * Each advertising event: sequentially transmit on all 3 channels
 *   (37 → 38 → 39) with ≤10 ms between starts on successive channels.
 *
 * @param next_event_ms  Output: when next advertising event starts (ms)
 * @param interval_ms    Advertising interval
 * @param event_count    Current event number
 * @return 0 on success
 */
int ble_adv_timing(double *next_event_ms, double interval_ms, int event_count);

/* ==========================================================================
 * BLE Data Channel Hopping (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Compute BLE data channel index for connection event
 *
 * BLE uses an adaptive frequency hopping algorithm for data channels:
 *   unmappedChannel = (lastUnmappedChannel + hopIncrement) mod 37
 *   If unmappedChannel is in channel_map (remapped), use as-is
 *   Else: remap to a channel in the used set
 *
 * @param next_ch           Output: next data channel index (0-36)
 * @param last_ch           Previous data channel
 * @param hop_increment     Hop increment (5-16)
 * @param channel_map       Bitmap of used channels (1=good)
 * @param n_channels        Number of channels (should be 37)
 * @return 0 on success
 *
 * Complexity: O(1) amortized
 */
int ble_channel_hop(int *next_ch, int last_ch, int hop_increment,
                    const int *channel_map, int n_channels);

/* ==========================================================================
 * BLE GATT Server / Client Operations (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief Initialize a GATT service structure
 *
 * Creates a new GATT service with the given UUID and allocates
 * attribute storage.
 *
 * @param service       Service struct to initialize
 * @param uuid          Service UUID (16-bit or 128-bit)
 * @param num_attrs     Number of attributes to allocate
 * @return 0 on success, -1 on error
 */
int ble_gatt_service_init(ble_gatt_service_t *service,
                          ble_uuid_t uuid, int num_attrs);

/**
 * @brief Add a characteristic to a GATT service
 *
 * Characteristics are declared with two attributes:
 *   1. Characteristic Declaration (UUID 0x2803): Properties + Handle + UUID
 *   2. Characteristic Value: the actual data
 *
 * @param service       Service to add to
 * @param attr_index    Attribute index in service
 * @param uuid          Characteristic UUID
 * @param properties    Characteristic properties (read/write/notify/etc.)
 * @param permissions   Access permissions
 * @return 0 on success, -1 on error
 */
int ble_gatt_add_characteristic(ble_gatt_service_t *service, int attr_index,
                                ble_uuid_t uuid, uint8_t properties,
                                uint16_t permissions);

/**
 * @brief Read a GATT characteristic value
 *
 * @param value         Output value buffer
 * @param max_len       Max value length
 * @param service       GATT service
 * @param handle        Attribute handle to read
 * @return Number of bytes read, or -1 on error
 */
int ble_gatt_read(uint8_t *value, int max_len,
                  const ble_gatt_service_t *service, ble_handle_t handle);

/**
 * @brief Write a GATT characteristic value
 *
 * @param service       GATT service
 * @param handle        Attribute handle to write
 * @param value         Data to write
 * @param value_len     Length of data
 * @return 0 on success, -1 on error (handle not found, permission denied)
 */
int ble_gatt_write(ble_gatt_service_t *service, ble_handle_t handle,
                   const uint8_t *value, int value_len);

/**
 * @brief Discover services by UUID
 *
 * Searches the GATT attribute database for matching service UUIDs.
 *
 * @param handles       Output array of matching handle ranges (start_handle)
 * @param max_results   Max results
 * @param service       Root service to search
 * @param uuid          UUID to search for
 * @param n_attrs       Total attributes in database
 * @return Number of matches found
 *
 * Complexity: O(n_attrs)
 */
int ble_gatt_discover_service(ble_handle_t *handles, int max_results,
                              const ble_gatt_service_t *service,
                              ble_uuid_t uuid, int n_attrs);

/* ==========================================================================
 * BLE Pairing & Security (L5 Algorithm)
 * ========================================================================== */

/**
 * @brief BLE Secure Connections key generation (ECDH P-256)
 *
 * LE Secure Connections (BT 4.2+) uses Elliptic Curve Diffie-Hellman
 * over the NIST P-256 curve to establish a shared secret (DHKey).
 *
 * Curve: y² = x³ - 3x + b (mod p), p = 2^256 - 2^224 + 2^192 + 2^96 - 1
 *
 * Key derivation: LTK = f5(DHKey, N_master, N_slave, BD_ADDR_m, BD_ADDR_s, key_length)
 *
 * @param dhkey         Output: DHKey (256-bit shared secret)
 * @param private_key   Local private key (256-bit)
 * @param public_key_x  Peer public key X (256-bit)
 * @param public_key_y  Peer public key Y (256-bit)
 * @return 0 on success, -1 on error
 *
 * Complexity: O(1) for ECDH scalar multiplication on P-256
 */
int ble_le_sc_dhkey(uint8_t dhkey[32], const uint8_t private_key[32],
                    const uint8_t public_key_x[32], const uint8_t public_key_y[32]);

/**
 * @brief BLE f5 key derivation function (AES-CMAC based)
 *
 * f5 is the key derivation function used in LE Secure Connections
 * to derive the Long Term Key (LTK) and other keys from DHKey.
 *
 * f5(W, N1, N2, A1, A2) = AES-CMAC_{W}(counter=0 || keyID || N1 || N2 || A1 || A2 || Length=256)
 *
 * @param ltk           Output: Long Term Key (128-bit)
 * @param dhkey         DHKey (256-bit) from ECDH
 * @param nonce_m       Master nonce
 * @param nonce_s       Slave nonce
 * @param addr_m        Master BD_ADDR
 * @param addr_s        Slave BD_ADDR
 * @return 0 on success
 */
int ble_f5_ltk_derive(uint8_t ltk[16], const uint8_t dhkey[32],
                      const uint8_t nonce_m[16], const uint8_t nonce_s[16],
                      const bt_address_t *addr_m, const bt_address_t *addr_s);

/**
 * @brief BLE AES-CCM encryption (used for link-layer encryption)
 *
 * AES-CCM with 128-bit key, 13-byte nonce, and 4-byte MIC.
 *
 * Nonce = Packet_Counter(39) | Direction(1) | IV(24)  (total 64 bits, padded to 104)
 *
 * @param ciphertext    Output ciphertext (same length + 4 byte MIC)
 * @param plaintext     Input plaintext
 * @param plaintext_len Length of plaintext
 * @param key           AES-128 key (LTK or SK)
 * @param nonce         13-byte nonce
 * @param mic_len       MIC length (4 bytes for BLE)
 * @return Total output length (plaintext_len + mic_len), or -1 on error
 *
 * Complexity: O(plaintext_len)
 */
int ble_aes_ccm_encrypt(uint8_t *ciphertext, const uint8_t *plaintext,
                        int plaintext_len, const uint8_t key[16],
                        const uint8_t nonce[13], int mic_len);

/**
 * @brief BLE AES-CCM decryption with MIC verification
 *
 * @param plaintext     Output plaintext
 * @param ciphertext    Input ciphertext + MIC
 * @param ciphertext_len Total ciphertext length (including MIC)
 * @param key           AES-128 key
 * @param nonce         13-byte nonce
 * @param mic_len       MIC length
 * @return Plaintext length on success, -1 on MIC failure
 */
int ble_aes_ccm_decrypt(uint8_t *plaintext, const uint8_t *ciphertext,
                        int ciphertext_len, const uint8_t key[16],
                        const uint8_t nonce[13], int mic_len);

/* ==========================================================================
 * BLE Power / Range Estimation (L4 Fundamental Law)
 * ========================================================================== */

/**
 * @brief Estimate BLE communication range using link budget
 *
 * Based on the Friis transmission equation:
 *   P_rx = P_tx + G_tx + G_rx - 20·log₁₀(4πd/λ)
 *
 * For BLE with typical parameters:
 *   P_tx = 0 dBm (1 mW), G_tx = G_rx = 0 dBi (chip antenna)
 *   λ = 0.1224 m (2.45 GHz), Receiver sensitivity = -93 dBm (BLE 1M)
 *
 * Range d = (λ/(4π)) · 10^((P_tx - P_rx + G_tx + G_rx) / 20)
 *
 * @param tx_power_dbm       Transmit power
 * @param rx_sensitivity_dbm Receiver sensitivity (minimum SNR for target BER)
 * @param tx_gain_dbi        Transmit antenna gain
 * @param rx_gain_dbi        Receive antenna gain
 * @return Estimated maximum range in meters
 *
 * Complexity: O(1)
 */
double ble_range_estimate(double tx_power_dbm, double rx_sensitivity_dbm,
                          double tx_gain_dbi, double rx_gain_dbi);

/**
 * @brief Estimate BLE link budget closure
 *
 * Returns available margin above receiver sensitivity.
 *
 * @param rssi_dbm            Measured RSSI
 * @param rx_sensitivity_dbm  Required sensitivity
 * @return Link margin in dB (positive = link OK)
 */
double ble_link_margin(double rssi_dbm, double rx_sensitivity_dbm);

/* ==========================================================================
 * BLE Mesh Networking (L8 Advanced Topic)
 * ========================================================================== */

/**
 * @brief BLE Mesh message format
 *
 * BLE mesh uses managed flooding over advertising bearers.
 * Messages include TTL (Time To Live), source/destination addresses,
 * and a sequence number for replay protection.
 *
 * Mesh packet over advertising bearer:
 *   [ADV_IND header] [Network PDU: IVI|NID|CTL|TTL|SEQ|SRC|DST|TransportPDU|NetMIC]
 */
typedef struct {
    uint16_t src_addr;       /**< Source unicast address (16-bit) */
    uint16_t dst_addr;       /**< Destination address (unicast/group/virtual) */
    uint8_t  ttl;            /**< Time To Live (0-127, decremented each hop) */
    uint32_t seq_num;        /**< 24-bit sequence number (replay protection) */
    uint8_t *payload;        /**< Upper transport layer payload */
    int      payload_len;    /**< Payload length */
} ble_mesh_pdu_t;

/**
 * @brief Initialize a BLE mesh message
 *
 * @param msg           Mesh message struct
 * @param src           Source address
 * @param dst           Destination address
 * @param ttl           Initial TTL
 * @param payload       Payload data
 * @param payload_len   Payload length
 * @return 0 on success
 */
int ble_mesh_msg_init(ble_mesh_pdu_t *msg, uint16_t src, uint16_t dst,
                      uint8_t ttl, const uint8_t *payload, int payload_len);

/**
 * @brief BLE Mesh managed flooding relay decision
 *
 * A relay node decrements TTL and re-transmits if TTL > 1.
 * Uses a cache to suppress duplicate transmissions (network PDU cache).
 *
 * @param should_relay  Output: 1 if should relay, 0 otherwise
 * @param msg           Received mesh message
 * @param cache         Network PDU cache (store recently seen seq+src pairs)
 * @param cache_size    Cache size
 * @return 0 on success
 */
int ble_mesh_relay_decision(int *should_relay, const ble_mesh_pdu_t *msg,
                            uint32_t *cache, int cache_size);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_BLE_H */
