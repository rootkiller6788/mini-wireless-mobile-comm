/**
 * @file lora_mac.h
 * @brief LoRaWAN MAC Layer -- Frame formats, device classes, channel access
 *
 * Knowledge Coverage:
 *   L1 -- LoRaWAN frame types (Join Request/Accept, Data Up/Down),
 *        DevAddr, AppEUI, DevEUI, AppKey, NwkSKey, AppSKey,
 *        MIC (Message Integrity Code), Frame Counter, ADR
 *   L2 -- Class A/B/C device operation, confirmed/unconfirmed messages,
 *        Adaptive Data Rate (ADR), duty cycle restrictions
 *   L3 -- AES-128 CMAC for MIC calculation, key derivation
 *   L4 -- Duty cycle limits: ETSI EN 300 220 sub-band restrictions
 *   L5 -- Frame encoding/decoding, MIC verification, ADR algorithm
 *   L6 -- Join procedure, uplink/downlink scheduling
 *
 * References:
 *   - LoRaWAN Link Layer Specification 1.0.4 (LoRa Alliance)
 *   - LoRaWAN Regional Parameters RP2-1.0.4
 *   - ETSI EN 300 220-1/2 (European SRD regulations)
 *
 * Curriculum Mapping:
 *   - Stanford EE359: Wireless MAC protocols
 *   - MIT 6.450: Communication networks
 *   - CMU 14-740: Network protocol design
 *   - Georgia Tech ECE 6601: Communication network architecture
 *
 * @license MIT
 */

#ifndef LORA_MAC_H
#define LORA_MAC_H

#include <stdint.h>
#include <stddef.h>
#include "lora_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   L1: Core Definitions -- Device Identifiers and Keys
   ============================================================================ */

/** Size constants for LoRaWAN identifiers (in bytes) */
#define LORA_DEVEUI_SIZE   8    /**< Globally unique end-device ID (IEEE EUI-64) */
#define LORA_APPEUI_SIZE   8    /**< Application identifier (IEEE EUI-64) */
#define LORA_APPKEY_SIZE   16   /**< Application root key (AES-128) */
#define LORA_DEVADDR_SIZE  4    /**< 32-bit device network address */
#define LORA_NWKSKEY_SIZE  16   /**< Network session key (AES-128) */
#define LORA_APPSKEY_SIZE  16   /**< Application session key (AES-128) */
#define LORA_MIC_SIZE      4    /**< Message Integrity Code length */
#define LORA_FCTRL_SIZE    1    /**< Frame control octet */

/**
 * LoRaWAN device class
 *
 * Class A (All devices must support):
 *   - Uplink at any time (ALOHA)
 *   - Two short downlink receive windows follow each uplink
 *   - Lowest power consumption
 *
 * Class B (Beacon-synchronized):
 *   - Class A + scheduled periodic receive windows
 *   - Network sends beacons for time synchronization
 *   - Moderate power consumption, deterministic downlink latency
 *
 * Class C (Continuously listening):
 *   - Class A + receiver always on when not transmitting
 *   - Lowest downlink latency, highest power consumption
 *   - Suitable for mains-powered devices
 */
typedef enum {
    LORA_CLASS_A = 0,  /**< Bidirectional end-devices (ALOHA uplink, 2 RX windows) */
    LORA_CLASS_B = 1,  /**< Beacon-synchronized with scheduled RX slots */
    LORA_CLASS_C = 2,  /**< Continuous RX (always listening) */
} lora_device_class_t;

/**
 * LoRaWAN message type (MType) -- 3 bits in the frame header
 */
typedef enum {
    LORA_MTYPE_JOIN_REQUEST            = 0,  /**< Join request from device to network */
    LORA_MTYPE_JOIN_ACCEPT             = 1,  /**< Join accept from network to device */
    LORA_MTYPE_UNCONFIRMED_DATA_UP     = 2,  /**< Unconfirmed uplink data */
    LORA_MTYPE_UNCONFIRMED_DATA_DOWN   = 3,  /**< Unconfirmed downlink data */
    LORA_MTYPE_CONFIRMED_DATA_UP       = 4,  /**< Confirmed uplink data (needs ACK) */
    LORA_MTYPE_CONFIRMED_DATA_DOWN     = 5,  /**< Confirmed downlink data (needs ACK) */
    LORA_MTYPE_REJOIN_REQUEST          = 6,  /**< Rejoin (LoRaWAN 1.1+) */
    LORA_MTYPE_PROPRIETARY             = 7,  /**< Proprietary message */
} lora_mtype_t;

/**
 * LoRaWAN MAC commands (CID)
 *
 * These are piggybacked in the FOpts field of data messages.
 * Each CID is 1 byte followed by command-specific payload.
 */
typedef enum {
    LORA_CID_LINK_CHECK       = 0x02,  /**< Link check request/answer */
    LORA_CID_LINK_ADR         = 0x03,  /**< Adaptive Data Rate request/answer */
    LORA_CID_DUTY_CYCLE       = 0x04,  /**< Duty cycle answer */
    LORA_CID_RX_PARAM_SETUP   = 0x05,  /**< RX parameter setup */
    LORA_CID_DEV_STATUS       = 0x06,  /**< Device status request/answer */
    LORA_CID_NEW_CHANNEL      = 0x07,  /**< New channel definition */
    LORA_CID_RX_TIMING_SETUP  = 0x08,  /**< RX timing setup */
    LORA_CID_TX_PARAM_SETUP   = 0x09,  /**< TX parameter setup */
    LORA_CID_DL_CHANNEL       = 0x0A,  /**< Downlink channel frequency */
    LORA_CID_DEVICE_TIME      = 0x0D,  /**< Device time request/answer */
} lora_mac_cid_t;

/* ============================================================================
   L1: LoRaWAN Frame Structures
   ============================================================================ */

/**
 * LoRaWAN frame header (FHDR)
 *
 * Byte layout:
 *   [DevAddr 4B] [FCtrl 1B] [FCnt 2B] [FOpts 0-15B]
 */
typedef struct {
    uint32_t dev_addr;        /**< 32-bit device address (network assigned) */
    uint8_t  f_ctrl;          /**< Frame control byte */
    uint16_t f_cnt;           /**< Frame counter (prevents replay attacks) */
    uint8_t  f_opts[15];      /**< MAC command options */
    uint8_t  f_opts_len;      /**< Actual length of FOpts field */
} lora_fhdr_t;

/**
 * LoRaWAN MAC payload (full frame)
 *
 * Frame structure:
 *   [MHDR 1B] [FHDR 7-22B] [FPort 1B] [FRMPayload N B] [MIC 4B]
 *
 * MHDR (MAC Header):
 *   Bits 7-5: MType (message type)
 *   Bits 4-2: RFU (reserved)
 *   Bits 1-0: Major (LoRaWAN version, 0=LoRaWAN R1)
 */
typedef struct {
    uint8_t       mhdr;            /**< MAC header (MType + Major) */
    lora_mtype_t  mtype;           /**< Parsed message type */
    lora_fhdr_t   fhdr;            /**< Frame header */
    uint8_t       f_port;          /**< Application port (0=MAC only, 1-223=app) */
    uint8_t      *frm_payload;     /**< Application payload (NULL if FPort=0) */
    uint16_t      frm_payload_len; /**< Payload length in bytes */
    uint8_t       mic[LORA_MIC_SIZE]; /**< Message Integrity Code (4 bytes) */
} lora_mac_frame_t;

/**
 * Join Request frame
 *
 * Sent by device to network server to initiate OTAA (Over-The-Air Activation).
 * Contains device and application identifiers plus a random nonce.
 */
typedef struct {
    uint8_t  join_eui[LORA_APPEUI_SIZE];  /**< Application EUI (8 bytes, reversed) */
    uint8_t  dev_eui[LORA_DEVEUI_SIZE];   /**< Device EUI (8 bytes, reversed) */
    uint16_t dev_nonce;                   /**< Random nonce (prevents replay) */
} lora_join_request_t;

/**
 * Join Accept frame
 *
 * Response from network server after successful join request.
 * Delivered with a delay (JOIN_ACCEPT_DELAY1 or JOIN_ACCEPT_DELAY2).
 */
typedef struct {
    uint8_t  app_nonce[3];          /**< Application nonce */
    uint8_t  net_id[3];             /**< Network identifier */
    uint32_t dev_addr;              /**< Assigned device address */
    uint8_t  dl_settings;           /**< Downlink settings (RX1 offset + data rate) */
    uint8_t  rx_delay;              /**< Delay between TX and RX1 */
    uint8_t  cf_list[16];           /**< Optional channel frequency list */
    uint8_t  cf_list_len;           /**< Length of CFList */
} lora_join_accept_t;

/**
 * LoRaWAN session state
 *
 * Maintained after successful OTAA join or ABP activation.
 * Session keys are derived from AppKey (OTAA) or pre-provisioned (ABP).
 */
typedef struct {
    uint32_t dev_addr;                   /**< Device network address */
    uint8_t  nwk_s_key[LORA_NWKSKEY_SIZE]; /**< Network session key */
    uint8_t  app_s_key[LORA_APPSKEY_SIZE]; /**< Application session key */
    uint32_t f_cnt_up;                   /**< Uplink frame counter */
    uint32_t f_cnt_down;                 /**< Downlink frame counter */
    lora_device_class_t device_class;    /**< Device class */
    uint8_t  rx1_dr_offset;              /**< RX1 data rate offset from uplink */
    uint8_t  rx2_dr;                     /**< RX2 data rate */
    double   rx2_freq;                   /**< RX2 frequency in Hz */
    uint8_t  rx1_delay;                  /**< RX1 delay in seconds */
    uint8_t  adr_enabled;                /**< Adaptive Data Rate enabled */
} lora_session_t;

/* ============================================================================
   L2: Core Concepts -- Channel Access and Duty Cycle
   ============================================================================ */

/**
 * European SRD 868 MHz sub-band definitions (ETSI EN 300 220)
 *
 * g1: 868.1 MHz, duty cycle 1% (SF7-12, BW125/250)
 * g2: 868.3 MHz, duty cycle 1%
 * g3: 868.5 MHz, duty cycle 1%
 * g4: 869.525 MHz, duty cycle 10% (SF9-12, BW125 only)
 */
typedef struct {
    double   frequency;        /**< Center frequency in Hz */
    double   duty_cycle;       /**< Maximum duty cycle (0.0-1.0) */
    uint8_t  min_sf;           /**< Minimum allowed spreading factor */
    uint8_t  max_sf;           /**< Maximum allowed spreading factor */
    uint32_t max_payload_size; /**< Maximum MAC payload size (bytes) */
    uint32_t max_eirp_dbm;     /**< Maximum EIRP in dBm */
} lora_channel_params_t;

/**
 * Duty cycle tracker state
 *
 * Tracks transmission time accumulation to enforce regulatory
 * duty cycle limits (e.g., 1% = 36 seconds per hour).
 */
typedef struct {
    double   duty_cycle_limit;   /**< Maximum allowed duty cycle (0.01 = 1%) */
    double   observation_period; /**< Sliding window period in seconds (3600) */
    double   toa_accumulated;    /**< Accumulated time-on-air in window */
    double   window_start;       /**< Start time of current observation window */
    double   last_tx_time;       /**< Timestamp of last transmission */
} lora_duty_cycle_tracker_t;

/* ============================================================================
   L2: Adaptive Data Rate (ADR)
   ============================================================================ */

/**
 * ADR control state
 *
 * ADR algorithm adjusts SF and TX power to optimize:
 *   1. Data rate (higher SF = faster, less airtime)
 *   2. Range (lower SF = better sensitivity)
 *   3. Power consumption (optimized TX power)
 *
 * The network server monitors uplink SNR and frame count
 * to compute optimal SF and power settings.
 */
typedef struct {
    uint8_t  enabled;              /**< ADR enabled flag */
    uint8_t  current_sf;           /**< Current spreading factor */
    uint8_t  current_tx_power_idx; /**< Current TX power index */
    uint8_t  adr_ack_cnt;          /**< Downlink frames since last ADR request */
    uint8_t  adr_ack_limit;        /**< Limit before re-requesting ADR */
    uint8_t  adr_ack_delay;        /**< Delay before re-requesting */
    double   snr_margin_db;        /**< SNR margin above demod floor */
    uint8_t  nb_trans;             /**< Number of transmissions per uplink */
} lora_adr_state_t;

/* ============================================================================
   L3: Mathematical Structures -- Key Derivation and MIC
   ============================================================================ */

/**
 * LoRaWAN key derivation (LoRaWAN Spec 1.0.4, Section 6.2)
 *
 * Session keys are derived from AppKey using AES-128 ECB:
 *
 * NwkSKey = aes128_encrypt(AppKey, 0x01 | AppNonce | NetID | DevNonce | pad16)
 * AppSKey = aes128_encrypt(AppKey, 0x02 | AppNonce | NetID | DevNonce | pad16)
 *
 * @param app_key    16-byte application key
 * @param app_nonce  3-byte application nonce
 * @param net_id     3-byte network ID
 * @param dev_nonce  2-byte device nonce
 * @param nwk_s_key  Output: 16-byte network session key
 * @param app_s_key  Output: 16-byte application session key
 */
void lora_derive_session_keys(const uint8_t *app_key,
                               const uint8_t *app_nonce,
                               const uint8_t *net_id,
                               uint16_t dev_nonce,
                               uint8_t *nwk_s_key,
                               uint8_t *app_s_key);

/**
 * AES-128 CMAC (Cipher-based Message Authentication Code)
 *
 * Used for LoRaWAN MIC calculation (RFC 4493).
 * The MIC is computed over: MHDR | FHDR | FPort | FRMPayload
 * with B0 block prepended for directionality.
 *
 * CMAC = AES-CBC-MAC with subkey derivation for final block padding.
 *
 * @param key         16-byte AES key
 * @param msg         Message to authenticate
 * @param msg_len     Message length in bytes
 * @param mic         Output: 4-byte MIC (first 4 bytes of full CMAC)
 */
void lora_aes128_cmac(const uint8_t *key,
                       const uint8_t *msg,
                       size_t msg_len,
                       uint8_t *mic);

/**
 * Calculate LoRaWAN frame MIC
 *
 * The MIC authenticates the message and prevents tampering.
 * B0 block structure:
 *   [0x49] [4B zeros] [Dir] [DevAddr] [FCntUp/Down 4B] [0x00] [msg_len]
 *
 * MIC = cmac(NwkSKey, B0 | msg)[0:4]
 *
 * @param session   Current session with NwkSKey
 * @param frame     Frame to compute MIC for
 * @param direction 0=uplink, 1=downlink
 */
void lora_calculate_mic(const lora_session_t *session,
                         const lora_mac_frame_t *frame,
                         int direction);

/**
 * Verify LoRaWAN frame MIC
 *
 * Computes expected MIC and compares with received MIC.
 * Returns 0 if MIC matches, -1 if mismatch.
 *
 * @param session   Current session
 * @param frame     Frame with MIC to verify
 * @param direction 0=uplink, 1=downlink
 * @return 0 if verified, -1 if mismatch
 */
int lora_verify_mic(const lora_session_t *session,
                     const lora_mac_frame_t *frame,
                     int direction);

/* ============================================================================
   L4: Fundamental Limits -- Duty Cycle and Channel Occupancy
   ============================================================================ */

/**
 * Initialize duty cycle tracker
 *
 * @param tracker     Output tracker state
 * @param duty_cycle  Maximum duty cycle fraction (e.g., 0.01 for 1%)
 * @param window_sec  Observation window in seconds (typically 3600)
 */
void lora_duty_cycle_init(lora_duty_cycle_tracker_t *tracker,
                           double duty_cycle,
                           double window_sec);

/**
 * Check if transmission is allowed under duty cycle limit
 *
 * @param tracker  Duty cycle tracker state
 * @param toa_sec  Planned time-on-air in seconds
 * @param now_sec  Current time in seconds (Unix timestamp)
 * @return 1 if allowed, 0 if blocked by duty cycle
 */
int lora_duty_cycle_check(lora_duty_cycle_tracker_t *tracker,
                           double toa_sec,
                           double now_sec);

/**
 * Record a transmission for duty cycle tracking
 *
 * @param tracker  Duty cycle tracker (updated in place)
 * @param toa_sec  Transmitted time-on-air in seconds
 * @param now_sec  Current time in seconds
 */
void lora_duty_cycle_record(lora_duty_cycle_tracker_t *tracker,
                             double toa_sec,
                             double now_sec);

/**
 * Calculate time until next transmission is allowed
 *
 * @param tracker  Duty cycle tracker state
 * @param toa_sec  Planned time-on-air
 * @param now_sec  Current time
 * @return Seconds until transmission is allowed, 0 if already allowed
 */
double lora_duty_cycle_wait_sec(lora_duty_cycle_tracker_t *tracker,
                                 double toa_sec,
                                 double now_sec);

/* ============================================================================
   L5: Algorithms -- Frame Construction and ADR
   ============================================================================ */

/**
 * Build a LoRaWAN join request frame
 *
 * @param join_eui   8-byte application EUI
 * @param dev_eui    8-byte device EUI
 * @param dev_nonce  2-byte random nonce
 * @param buffer     Output buffer
 * @param buf_len    Buffer length
 * @return Frame length in bytes, or -1 on error
 */
int lora_build_join_request(const uint8_t *join_eui,
                             const uint8_t *dev_eui,
                             uint16_t dev_nonce,
                             uint8_t *buffer,
                             size_t buf_len);

/**
 * Build a LoRaWAN data frame (uplink)
 *
 * @param session    Current session
 * @param f_port     Application port (0=MAC only)
 * @param payload    Application data
 * @param payload_len Payload length
 * @param confirmed  Nonzero for confirmed (needs ACK)
 * @param buffer     Output buffer for complete frame
 * @param buf_len    Buffer length
 * @return Frame length in bytes, or -1 on error
 */
int lora_build_data_frame(const lora_session_t *session,
                           uint8_t f_port,
                           const uint8_t *payload,
                           uint16_t payload_len,
                           int confirmed,
                           uint8_t *buffer,
                           size_t buf_len);

/**
 * Parse a LoRaWAN frame from raw bytes
 *
 * @param frame  Output parsed frame structure
 * @param data   Raw frame bytes
 * @param len    Frame length
 * @return 0 on success, -1 on parse error
 */
int lora_parse_frame(lora_mac_frame_t *frame,
                      const uint8_t *data,
                      size_t len);

/**
 * ADR algorithm: compute optimal data rate and TX power
 *
 * Algorithm:
 *   1. Measure SNR_margin = SNR_measured - SNR_required
 *   2. If SNR_margin > threshold: increase data rate (decrease SF)
 *   3. If SNR_margin < 0: decrease data rate (increase SF)
 *   4. Optimize TX power: if SNR_margin large, reduce power
 *
 * @param adr_state      Current ADR state (updated in place)
 * @param snr_measured   Measured uplink SNR in dB
 * @param frame_count    Frames since last ADR update
 * @return New SF setting if changed, current SF if unchanged
 */
uint8_t lora_adr_update(lora_adr_state_t *adr_state,
                         double snr_measured,
                         uint32_t frame_count);

/* ============================================================================
   L6: Canonical Problems -- Join Procedure and Class A Scheduling
   ============================================================================ */

/**
 * LoRaWAN over-the-air activation (OTAA) join procedure
 *
 * State machine for the OTAA join flow:
 *   1. Device sends Join Request
 *   2. Network processes and sends Join Accept (RX1 or RX2 window)
 *   3. Device derives session keys
 *   4. Session established, device starts data transmission
 *
 * This is the canonical LoRaWAN network entry problem.
 */

typedef enum {
    JOIN_STATE_IDLE = 0,        /**< Not started */
    JOIN_STATE_REQ_SENT,        /**< Join request transmitted */
    JOIN_STATE_WAIT_RX1,        /**< Waiting in RX1 window */
    JOIN_STATE_WAIT_RX2,        /**< Waiting in RX2 window (RX1 missed) */
    JOIN_STATE_ACCEPTED,        /**< Join accept received, deriving keys */
    JOIN_STATE_ACTIVE,          /**< Session active */
    JOIN_STATE_FAILED,          /**< Join failed */
} lora_join_state_t;

/**
 * Join procedure state
 */
typedef struct {
    lora_join_state_t state;              /**< Current join state */
    uint8_t  join_eui[LORA_APPEUI_SIZE];  /**< Application EUI */
    uint8_t  dev_eui[LORA_DEVEUI_SIZE];   /**< Device EUI */
    uint8_t  app_key[LORA_APPKEY_SIZE];   /**< Application root key */
    uint16_t dev_nonce;                   /**< Last used device nonce */
    double   tx_timestamp;                /**< Timestamp of join request TX */
    double   rx1_start;                   /**< RX1 window start time */
    double   rx2_start;                   /**< RX2 window start time */
    uint8_t  retry_count;                 /**< Join retry counter */
    uint8_t  max_retries;                 /**< Maximum join attempts */
    lora_session_t session;               /**< Session (populated on success) */
} lora_join_procedure_t;

/**
 * Initialize join procedure state
 *
 * @param proc       Output join procedure state
 * @param join_eui   8-byte application EUI
 * @param dev_eui    8-byte device EUI
 * @param app_key    16-byte application key
 */
void lora_join_init(lora_join_procedure_t *proc,
                     const uint8_t *join_eui,
                     const uint8_t *dev_eui,
                     const uint8_t *app_key);

/**
 * Handle join accept frame and derive session keys
 *
 * @param proc         Join procedure state (updated in place)
 * @param join_accept  Raw join accept frame bytes
 * @param len          Frame length
 * @return 0 on success (session active), -1 on error
 */
int lora_join_handle_accept(lora_join_procedure_t *proc,
                             const uint8_t *join_accept,
                             size_t len);

/**
 * Calculate receive window start times for Class A
 *
 * RX1: opens RECEIVE_DELAY1 seconds after uplink end
 * RX2: opens RECEIVE_DELAY1 + 1 second after uplink end
 *
 * @param uplink_end_sec  Uplink transmission end time
 * @param rx_delay        RX1 delay in seconds (default 1)
 * @param rx1_start       Output: RX1 window start time
 * @param rx2_start       Output: RX2 window start time
 */
void lora_class_a_rx_windows(double uplink_end_sec,
                              uint8_t rx_delay,
                              double *rx1_start,
                              double *rx2_start);

#ifdef __cplusplus
}
#endif

#endif /* LORA_MAC_H */
