/**
 * @file wifi_bt_types.h
 * @brief WiFi & Bluetooth — Core Type Definitions (L1 Definitions)
 *
 * Implements foundational data structures for IEEE 802.11 WiFi and
 * Bluetooth (BR/EDR + BLE) wireless communication systems.
 *
 * Reference: Molisch, A.F., "Wireless Communications", 2nd ed., Wiley 2011.
 * Reference: IEEE Std 802.11-2020, IEEE Std 802.15.1-2005.
 *
 * L1 Knowledge Coverage:
 *   - WiFi PHY modes (802.11a/b/g/n/ac/ax)
 *   - WiFi channel bandwidths
 *   - Bluetooth BR/EDR vs BLE
 *   - GFSK modulation parameters
 *   - Frequency hopping parameters
 *   - OFDM subcarrier structure
 *   - CSMA/CA timing parameters
 *   - WPA/WPA2/WPA3 security suite types
 *   - BLE GATT service/profile types
 */
#ifndef WIFI_BT_TYPES_H
#define WIFI_BT_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * WiFi PHY Mode Enumeration (IEEE 802.11)
 * ========================================================================== */

/** WiFi PHY standard identifier (L1 Definition) */
typedef enum {
    WIFI_PHY_80211A   = 0,  /**< 802.11a — OFDM, 5 GHz, up to 54 Mbps */
    WIFI_PHY_80211B   = 1,  /**< 802.11b — DSSS/CCK, 2.4 GHz, up to 11 Mbps */
    WIFI_PHY_80211G   = 2,  /**< 802.11g — OFDM/DSSS, 2.4 GHz, up to 54 Mbps */
    WIFI_PHY_80211N   = 3,  /**< 802.11n — HT-OFDM, MIMO, 2.4/5 GHz */
    WIFI_PHY_80211AC  = 4,  /**< 802.11ac — VHT-OFDM, MU-MIMO, 5 GHz */
    WIFI_PHY_80211AX  = 5,  /**< 802.11ax — HE-OFDM, OFDMA, 2.4/5/6 GHz */
    WIFI_PHY_80211BE  = 6   /**< 802.11be — EHT, 320 MHz, 16x16 MIMO */
} wifi_phy_mode_t;

/** WiFi channel bandwidth enumeration */
typedef enum {
    WIFI_BW_20_MHZ   = 0,   /**< 20 MHz — legacy baseline */
    WIFI_BW_40_MHZ   = 1,   /**< 40 MHz — introduced in 802.11n */
    WIFI_BW_80_MHZ   = 2,   /**< 80 MHz — introduced in 802.11ac */
    WIFI_BW_160_MHZ  = 3,   /**< 160 MHz — 802.11ac Wave 2 / 802.11ax */
    WIFI_BW_320_MHZ  = 4    /**< 320 MHz — 802.11be */
} wifi_bandwidth_t;

/** WiFi modulation and coding scheme (MCS) index */
typedef struct {
    int      mcs_index;      /**< MCS 0-11 (HT/VHT/HE) */
    int      spatial_streams;/**< Number of spatial streams (1-8) */
    double   data_rate_mbps; /**< Nominal PHY data rate in Mbps */
    int      coding_rate_num;/**< Coding rate numerator (e.g., 1 for 1/2) */
    int      coding_rate_den;/**< Coding rate denominator (e.g., 2 for 1/2) */
    int      bits_per_symbol;/**< Coded bits per OFDM symbol */
} wifi_mcs_t;

/* ==========================================================================
 * OFDM Symbol Structure (L1 Definition)
 * ========================================================================== */

/**
 * @brief OFDM symbol parameters (IEEE 802.11)
 *
 * The OFDM symbol consists of data subcarriers, pilot subcarriers,
 * guard subcarriers, and a cyclic prefix (CP). The FFT size determines
 * the total number of subcarriers.
 *
 * Key formula: Symbol duration T_sym = T_fft + T_cp
 *   - T_fft = N_fft / BW   (useful symbol duration)
 *   - T_cp  = N_fft / (4*BW) typically (1/4 guard interval)
 *
 * Reference: Heiskala, J. & Terry, J., "OFDM Wireless LANs", Sams 2001.
 */
typedef struct {
    int      n_fft;          /**< FFT/IFFT size (64 for 20 MHz) */
    int      n_data_sc;      /**< Number of data subcarriers (48 for 20 MHz) */
    int      n_pilot_sc;     /**< Number of pilot subcarriers (4 for 20 MHz) */
    int      n_guard_sc;     /**< Number of guard/DC subcarriers */
    int      n_cp;           /**< Cyclic prefix length in samples */
    double   subcarrier_spacing_khz;  /**< Δf = BW / N_fft in kHz (312.5) */
    double   symbol_duration_us;      /**< T_sym = T_fft + T_cp in microseconds */
    double   bandwidth_mhz;           /**< Channel bandwidth in MHz */
} ofdm_params_t;

/**
 * @brief OFDM subcarrier assignment table
 *
 * Maps logical subcarrier indices to physical FFT bins for a given
 * bandwidth configuration. IEEE 802.11 uses a specific pilot pattern
 * and data subcarrier mapping.
 */
typedef struct {
    int     *data_indices;   /**< Array of data subcarrier logical indices */
    int     *pilot_indices;  /**< Array of pilot subcarrier indices */
    int      n_data;         /**< Total data subcarriers */
    int      n_pilots;       /**< Total pilot subcarriers */
    int      fft_size;       /**< Corresponding FFT/IFFT size */
    double   pilot_values[4];/**< BPSK pilot sequence {1,1,1,-1} for 802.11a/g */
} ofdm_subcarrier_map_t;

/* ==========================================================================
 * CSMA/CA Parameters (L1 Definition — WiFi MAC)
 * ========================================================================== */

/**
 * @brief CSMA/CA channel access parameters
 *
 * The Distributed Coordination Function (DCF) uses:
 *   - DIFS: Distributed Inter-Frame Space
 *   - SIFS: Short Inter-Frame Space
 *   - Slot time
 *   - Contention Window (CWmin, CWmax)
 *   - Backoff = random[0, CW] * slot_time
 *
 * Reference: IEEE 802.11-2020, Clause 10 "MAC sublayer"
 */
typedef struct {
    double   slot_time_us;   /**< Slot time (9 µs for OFDM PHY in 2.4 GHz) */
    double   sifs_us;        /**< SIFS duration (16 µs for OFDM PHY) */
    double   difs_us;        /**< DIFS = SIFS + 2*SlotTime (34 µs for OFDM) */
    double   eifs_us;        /**< Extended IFS after erroneous reception */
    int      cw_min;         /**< CWmin = 15 for OFDM PHY (2^4 - 1) */
    int      cw_max;         /**< CWmax = 1023 for OFDM PHY (2^10 - 1) */
    int      retry_limit;    /**< Short retry limit (7) */
    int      long_retry_limit;/**< Long retry limit (4) */
} wifi_csma_params_t;

/**
 * @brief WiFi MAC frame header (simplified 802.11 format)
 *
 * The 802.11 MAC header carries frame control, duration, addresses,
 * sequence control, and QoS control fields.
 */
typedef struct {
    uint16_t frame_control;   /**< Protocol version + type + subtype + flags */
    uint16_t duration_id;     /**< Duration/ID for NAV setting */
    uint8_t  addr1[6];        /**< Receiver Address (RA) */
    uint8_t  addr2[6];        /**< Transmitter Address (TA) */
    uint8_t  addr3[6];        /**< BSSID / SA / DA depending on frame type */
    uint16_t seq_ctrl;        /**< Sequence number + fragment number */
    uint8_t  addr4[6];        /**< Fourth address (WDS frames only) */
    uint16_t qos_ctrl;        /**< QoS control field (QoS data frames) */
    uint16_t ht_ctrl;         /**< HT control field (802.11n+) */
} wifi_mac_header_t;

/** WiFi frame type enumeration */
typedef enum {
    WIFI_FRAME_MGMT     = 0x00,  /**< Management frame */
    WIFI_FRAME_CTRL     = 0x01,  /**< Control frame */
    WIFI_FRAME_DATA     = 0x02,  /**< Data frame */
    WIFI_FRAME_EXT      = 0x03   /**< Extension frame */
} wifi_frame_type_t;

/** WiFi management frame subtype */
typedef enum {
    WIFI_SUBTYPE_ASSOC_REQ    = 0x00,
    WIFI_SUBTYPE_ASSOC_RESP   = 0x01,
    WIFI_SUBTYPE_REASSOC_REQ  = 0x02,
    WIFI_SUBTYPE_REASSOC_RESP = 0x03,
    WIFI_SUBTYPE_PROBE_REQ    = 0x04,
    WIFI_SUBTYPE_PROBE_RESP   = 0x05,
    WIFI_SUBTYPE_BEACON       = 0x08,
    WIFI_SUBTYPE_ATIM         = 0x09,
    WIFI_SUBTYPE_DISASSOC     = 0x0A,
    WIFI_SUBTYPE_AUTH         = 0x0B,
    WIFI_SUBTYPE_DEAUTH       = 0x0C,
    WIFI_SUBTYPE_ACTION       = 0x0D
} wifi_mgmt_subtype_t;

/* ==========================================================================
 * Bluetooth Core Types (L1 Definitions)
 * ========================================================================== */

/** Bluetooth core specification version */
typedef enum {
    BT_VERSION_1_0B  = 0,    /**< Bluetooth 1.0B (1999) */
    BT_VERSION_1_1   = 1,    /**< Bluetooth 1.1 (2001) */
    BT_VERSION_1_2   = 2,    /**< Bluetooth 1.2 — AFH introduced */
    BT_VERSION_2_0   = 3,    /**< Bluetooth 2.0 + EDR (2004) */
    BT_VERSION_2_1   = 4,    /**< Bluetooth 2.1 + EDR — SSP */
    BT_VERSION_3_0   = 5,    /**< Bluetooth 3.0 + HS (2009) */
    BT_VERSION_4_0   = 6,    /**< Bluetooth 4.0 — BLE introduced */
    BT_VERSION_4_1   = 7,    /**< Bluetooth 4.1 */
    BT_VERSION_4_2   = 8,    /**< Bluetooth 4.2 — LE Secure Connections */
    BT_VERSION_5_0   = 9,    /**< Bluetooth 5.0 — 2M PHY, LE Long Range */
    BT_VERSION_5_1   = 10,   /**< Bluetooth 5.1 — Direction Finding */
    BT_VERSION_5_2   = 11,   /**< Bluetooth 5.2 — LE Audio, Isochronous */
    BT_VERSION_5_3   = 12,   /**< Bluetooth 5.3 — Channel Classification */
    BT_VERSION_5_4   = 13    /**< Bluetooth 5.4 — PAwR, EAD */
} bt_version_t;

/** Bluetooth device class (major service classes) */
typedef enum {
    BT_CLASS_COMPUTER      = 0x100,  /**< Desktop, laptop, server */
    BT_CLASS_PHONE         = 0x200,  /**< Cellular, cordless, smartphone */
    BT_CLASS_AUDIO_VIDEO   = 0x400,  /**< Headset, speaker, display */
    BT_CLASS_WEARABLE      = 0x700,  /**< Watch, glasses, fitness tracker */
    BT_CLASS_HEALTH        = 0x900,  /**< Blood pressure, thermometer, glucose */
    BT_CLASS_TOY           = 0x800,  /**< Robot, vehicle, game controller */
    BT_CLASS_UNCATEGORIZED = 0x000   /**< Miscellaneous device */
} bt_device_class_t;

/**
 * @brief Bluetooth device address (BD_ADDR)
 *
 * 48-bit IEEE 802 address, same format as MAC address.
 * Structure: NAP(16) + UAP(8) + LAP(24)
 */
typedef struct {
    uint8_t addr[6];         /**< 48-bit BD_ADDR (little-endian over air) */
} bt_address_t;

/**
 * @brief Bluetooth clock and timing
 *
 * The Bluetooth piconet is synchronized to the master's native clock.
 * Clock resolution = 312.5 µs (half slot = 1600 hops/s).
 */
typedef struct {
    uint32_t clk_native;     /**< Master native clock (28-bit, 312.5 µs tick) */
    uint16_t clk_offset;     /**< Slave clock offset from master */
    uint32_t slot_number;    /**< Current slot number (625 µs per slot) */
} bt_clock_t;

/* ==========================================================================
 * Frequency Hopping Spread Spectrum (L1 Definition — Bluetooth)
 * ========================================================================== */

/**
 * @brief FHSS channel and hopping parameters
 *
 * Bluetooth BR/EDR uses 79 channels (1 MHz spacing) in the 2.4 GHz ISM band.
 * Hopping rate = 1600 hops/s, dwell time = 625 µs per slot.
 *
 * BLE uses 40 channels (2 MHz spacing) with adaptive frequency hopping.
 *
 * Channel frequency: f(k) = 2402 + k MHz (k = 0..78 for BR, 0..39 for BLE)
 */
typedef struct {
    int      n_channels;          /**< 79 (BR/EDR) or 40 (BLE) */
    int      channel_map[79];     /**< Bitmap of usable channels (1=good) */
    double   channel_spacing_mhz; /**< 1.0 MHz (BR) or 2.0 MHz (BLE) */
    double   start_freq_mhz;      /**< 2402.0 MHz */
    int      hop_rate_hz;         /**< 1600 hops/s (BR) or variable (BLE) */
    int      current_channel;     /**< Current RF channel index */
    int      hop_sequence[79];    /**< Pre-computed hop sequence */
    int      hop_seq_length;      /**< Length of hop sequence */
    int      hop_index;           /**< Current position in hop sequence */
} bt_fhss_params_t;

/* ==========================================================================
 * GFSK Modulation Parameters (L1 Definition — Bluetooth)
 * ========================================================================== */

/**
 * @brief Gaussian Frequency Shift Keying parameters
 *
 * Bluetooth uses GFSK with BT = 0.5 (bandwidth-bit period product).
 *   - BR: h = 0.28..0.35 (modulation index)
 *   - BLE 1M: h = 0.45..0.55
 *   - BLE 2M: h = 0.45..0.55 (symbol rate doubled)
 *
 * GFSK pulse shape: g(t) = Q(2πB(t-T/2)/√(ln 2)) - Q(2πB(t+T/2)/√(ln 2))
 * where Q(x) = ½·erfc(x/√2)
 */
typedef struct {
    double   bt_product;      /**< Bandwidth-bit period product (typically 0.5) */
    double   modulation_index;/**< h = 2·Δf·T (0.28-0.35 BR, 0.45-0.55 BLE) */
    double   symbol_rate_mbps;/**< 1.0 (BR/BLE 1M) or 2.0 (BLE 2M) */
    double   frequency_dev_khz;/**< Peak frequency deviation in kHz */
    int      gaussian_len;    /**< Gaussian filter impulse response length */
    double  *gfsk_pulse;      /**< Pre-computed GFSK pulse shape */
} bt_gfsk_params_t;

/* ==========================================================================
 * BLE-Specific Types (L1 Definition)
 * ========================================================================== */

/** BLE PHY modes */
typedef enum {
    BLE_PHY_1M    = 0,      /**< LE 1M — 1 Mbps, mandatory */
    BLE_PHY_2M    = 1,      /**< LE 2M — 2 Mbps (BT 5.0) */
    BLE_PHY_CODED = 2       /**< LE Coded — 125/500 kbps long range (BT 5.0) */
} ble_phy_mode_t;

/** BLE advertising event types */
typedef enum {
    BLE_ADV_IND         = 0x00, /**< Connectable undirected advertising */
    BLE_ADV_DIRECT_IND  = 0x01, /**< Connectable directed advertising */
    BLE_ADV_NONCONN_IND = 0x02, /**< Non-connectable undirected advertising */
    BLE_ADV_SCAN_IND    = 0x06  /**< Scannable undirected advertising */
} ble_adv_type_t;

/**
 * @brief BLE advertising channel indices
 *
 * BLE uses 3 dedicated advertising channels:
 *   - Channel 37: 2402 MHz
 *   - Channel 38: 2426 MHz
 *   - Channel 39: 2480 MHz
 */
#define BLE_ADV_CH_37    37
#define BLE_ADV_CH_38    38
#define BLE_ADV_CH_39    39

/** BLE advertising interval (N * 0.625 ms, range 20 ms to 10.24 s) */
typedef struct {
    double   interval_ms;   /**< Advertising interval in ms (min 20, max 10240) */
    int      scan_window_ms;/**< Scan window duration */
    int      scan_interval_ms;/**< Scan interval */
    uint8_t  adv_data[31];  /**< Advertising data payload (max 31 bytes) */
    int      adv_data_len;  /**< Actual advertising data length */
    uint8_t  scan_rsp_data[31];/**< Scan response data (max 31 bytes) */
    int      scan_rsp_len;  /**< Actual scan response data length */
} ble_adv_params_t;

/* ==========================================================================
 * BLE GATT Profile Types (L1 Definition)
 * ========================================================================== */

/**
 * @brief BLE GATT (Generic Attribute Profile) UUID
 *
 * GATT uses 16-bit or 128-bit UUIDs. 16-bit UUIDs are defined by
 * Bluetooth SIG and expanded to 128-bit using the Bluetooth Base UUID:
 *   0000xxxx-0000-1000-8000-00805F9B34FB
 */
typedef struct {
    int      is_16bit;      /**< 1 = 16-bit SIG UUID, 0 = 128-bit custom */
    uint16_t uuid16;        /**< 16-bit SIG-assigned UUID */
    uint8_t  uuid128[16];   /**< 128-bit custom UUID (when is_16bit=0) */
} ble_uuid_t;

/** BLE GATT handle (attribute handle, 16-bit) */
typedef uint16_t ble_handle_t;

/** BLE GATT characteristic properties */
typedef enum {
    BLE_GATT_PROP_BROADCAST     = 0x01,
    BLE_GATT_PROP_READ          = 0x02,
    BLE_GATT_PROP_WRITE_NO_RESP = 0x04,
    BLE_GATT_PROP_WRITE         = 0x08,
    BLE_GATT_PROP_NOTIFY        = 0x10,
    BLE_GATT_PROP_INDICATE      = 0x20,
    BLE_GATT_PROP_AUTH_WRITE    = 0x40,
    BLE_GATT_PROP_EXTENDED      = 0x80
} ble_gatt_prop_t;

/**
 * @brief BLE GATT attribute
 *
 * Each attribute in the GATT database has a handle, type (UUID),
 * permissions, and value. Attributes form a hierarchy: Service →
 * Characteristic → Descriptor.
 */
typedef struct {
    ble_handle_t handle;     /**< Attribute handle (unique) */
    ble_uuid_t   type;       /**< Attribute type UUID */
    uint16_t     permissions;/**< Access permissions (read/write/encrypted) */
    uint8_t     *value;      /**< Attribute value buffer */
    int          value_len;  /**< Length of value in bytes */
} ble_gatt_attr_t;

/**
 * @brief BLE GATT service
 *
 * A GATT service is a collection of characteristics and included
 * services. Standard services include:
 *   - 0x1800: Generic Access
 *   - 0x1801: Generic Attribute
 *   - 0x180A: Device Information
 *   - 0x180D: Heart Rate
 *   - 0x180F: Battery Service
 */
typedef struct {
    ble_handle_t start_handle;   /**< First handle in this service */
    ble_handle_t end_handle;     /**< Last handle in this service */
    ble_uuid_t   uuid;           /**< Service UUID */
    int          n_attrs;        /**< Number of attributes in service */
    ble_gatt_attr_t *attrs;      /**< Array of attributes */
} ble_gatt_service_t;

/* ==========================================================================
 * Bluetooth Link Types (L1 Definition)
 * ========================================================================== */

/** Bluetooth link type */
typedef enum {
    BT_LINK_SCO  = 0,       /**< Synchronous Connection-Oriented (voice, 64 kbps) */
    BT_LINK_ACL  = 1,       /**< Asynchronous Connection-Less (data) */
    BT_LINK_ESCO = 2,       /**< Enhanced SCO (retransmission support) */
    BT_LINK_LE   = 3        /**< Low Energy connection */
} bt_link_type_t;

/**
 * @brief Bluetooth link parameters
 *
 * SCO: Fixed 64 kbps, reserved slots every 1/2/3 slot intervals.
 * eSCO: Retransmission window after reserved slots.
 * ACL: Packet-switched, best-effort, master polls slave.
 */
typedef struct {
    bt_link_type_t link_type;    /**< SCO / ACL / eSCO / LE */
    int      tx_interval_slots;  /**< Transmit interval in slots (625 µs) */
    int      retx_window_slots;  /**< Retransmission window (eSCO only) */
    int      packet_type_mask;   /**< Supported packet type bitmask */
    int      max_latency_slots;  /**< Maximum latency in slots */
    int      voice_setting;      /**< Voice encoding settings (SCO only) */
} bt_link_params_t;

/* ==========================================================================
 * WiFi Security Types (L1 Definition)
 * ========================================================================== */

/** WiFi security protocol suite */
typedef enum {
    WIFI_SEC_NONE   = 0,     /**< Open system — no security */
    WIFI_SEC_WEP    = 1,     /**< Wired Equivalent Privacy (deprecated, broken) */
    WIFI_SEC_WPA    = 2,     /**< WPA — TKIP + 802.1X */
    WIFI_SEC_WPA2   = 3,     /**< WPA2 — CCMP (AES-CCM) + 802.1X */
    WIFI_SEC_WPA3   = 4      /**< WPA3 — SAE (Dragonfly) + GCMP-256 */
} wifi_security_t;

/** WiFi encryption cipher type */
typedef enum {
    WIFI_CIPHER_NONE    = 0, /**< No encryption */
    WIFI_CIPHER_WEP40   = 1, /**< WEP 40-bit key */
    WIFI_CIPHER_TKIP    = 2, /**< Temporal Key Integrity Protocol */
    WIFI_CIPHER_CCMP    = 3, /**< AES-CCMP (128-bit) */
    WIFI_CIPHER_WEP104  = 4, /**< WEP 104-bit key */
    WIFI_CIPHER_GCMP128 = 5, /**< AES-GCMP 128-bit */
    WIFI_CIPHER_GCMP256 = 6  /**< AES-GCMP 256-bit (WPA3) */
} wifi_cipher_t;

/**
 * @brief WiFi security context
 *
 * Holds the pairwise transient key (PTK) and group temporal key (GTK)
 * derived during the 4-way handshake (WPA2) or SAE handshake (WPA3).
 */
typedef struct {
    wifi_security_t security_type;
    wifi_cipher_t   pairwise_cipher;
    wifi_cipher_t   group_cipher;
    uint8_t         pmk[32];   /**< Pairwise Master Key (256-bit) */
    uint8_t         ptk[64];   /**< Pairwise Transient Key */
    uint8_t         gtk[32];   /**< Group Temporal Key */
    uint8_t         anonce[32];/**< Authenticator nonce */
    uint8_t         snonce[32];/**< Supplicant nonce */
    int             ptk_len;   /**< Actual PTK length in bytes */
    int             gtk_len;   /**< Actual GTK length in bytes */
    int             key_id;    /**< Key ID for GTK (0-3) */
} wifi_sec_context_t;

/* ==========================================================================
 * Bluetooth Security Types (L1 Definition)
 * ========================================================================== */

/** Bluetooth pairing method */
typedef enum {
    BT_PAIR_LEGACY        = 0, /**< Legacy PIN-based pairing (≤ BT 2.0) */
    BT_PAIR_SECURE_SIMPLE = 1, /**< SSP — ECDH key exchange (BT 2.1+) */
    BT_PAIR_LE_SECURE     = 2  /**< LE Secure Connections (BT 4.2+) */
} bt_pairing_method_t;

/** Bluetooth security level */
typedef enum {
    BT_SEC_LEVEL_1 = 1,      /**< No security (open) */
    BT_SEC_LEVEL_2 = 2,      /**< Encryption only, no MITM protection */
    BT_SEC_LEVEL_3 = 3,      /**< Encryption + MITM protection */
    BT_SEC_LEVEL_4 = 4       /**< LE Secure Connections (ECDH P-256) */
} bt_security_level_t;

/**
 * @brief Bluetooth security/pairing context
 */
typedef struct {
    bt_pairing_method_t method;
    bt_security_level_t level;
    uint8_t  link_key[16];   /**< 128-bit link key (legacy) or LTK (BLE) */
    uint8_t  tk[16];         /**< Temporary Key (SSP pairing) */
    uint8_t  irk[16];        /**< Identity Resolving Key (BLE privacy) */
    uint8_t  csrk[16];       /**< Connection Signature Resolving Key */
    uint32_t ediv;           /**< Encrypted Diversifier (BLE) */
    uint64_t rand;           /**< Random number (BLE pairing) */
    int      is_authenticated;/**< 1 if MITM-protected pairing */
    int      is_bonded;       /**< 1 if devices are bonded */
} bt_sec_context_t;

/* ==========================================================================
 * Coexistence Types (L2 Core Concept)
 * ========================================================================== */

/** WiFi-Bluetooth coexistence mechanism */
typedef enum {
    COEX_NONE     = 0,      /**< No coexistence (collisions allowed) */
    COEX_PTA      = 1,      /**< Packet Traffic Arbitration (1/2/3-wire) */
    COEX_AFH      = 2,      /**< Adaptive Frequency Hopping (BT avoids WiFi) */
    COEX_TDM      = 3       /**< Time Division Multiplexing between radios */
} coex_mechanism_t;

/**
 * @brief Coexistence interface configuration
 *
 * When WiFi and Bluetooth share the 2.4 GHz ISM band, coexistence
 * mechanisms are required to avoid mutual interference. The 3-wire
 * PTA (Packet Traffic Arbitration) is the most common approach:
 *   - REQUEST: BT requests medium access
 *   - GRANT: WiFi grants/denies access
 *   - PRIORITY: BT indicates high-priority (SCO/eSCO) traffic
 */
typedef struct {
    coex_mechanism_t mechanism;
    int      pta_request;    /**< PTA REQUEST pin value */
    int      pta_grant;      /**< PTA GRANT pin value */
    int      pta_priority;   /**< PTA PRIORITY pin value */
    double   wifi_duty_cycle;/**< WiFi channel utilization (0.0-1.0) */
    double   bt_duty_cycle;  /**< Bluetooth channel utilization (0.0-1.0) */
    int      afh_channel_map[79];/**< Channels BT can use (after AFH) */
    int      afh_n_channels; /**< Number of usable channels after AFH */
} coex_config_t;

/* ==========================================================================
 * Channel Model Parameters (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Indoor wireless channel model parameters
 *
 * IEEE 802.11 TGn channel models A-F define typical indoor multipath
 * profiles. Models include path loss, delay spread, Doppler spread,
 * and Ricean K-factor.
 *
 * Path loss (simplified log-distance):
 *   PL(d) = PL(d₀) + 10·n·log₁₀(d/d₀) + Xσ
 *   where n = path loss exponent (2-4 indoor), Xσ ~ N(0,σ²)
 */
typedef struct {
    double   path_loss_exp;      /**< Path loss exponent n (2.0 free space) */
    double   reference_loss_db;  /**< PL(d₀) at reference distance (1 m) */
    double   shadow_std_db;      /**< Log-normal shadowing σ (dB) */
    double   rms_delay_spread_ns;/**< RMS delay spread (15-150 ns indoor) */
    double   coherence_bw_mhz;   /**< Bc ≈ 1/(5*τ_rms) */
    double   doppler_spread_hz;  /**< Maximum Doppler spread (walking ~5 Hz) */
    double   k_factor_db;        /**< Ricean K-factor (0 dB Rayleigh, >0 Rice) */
} indoor_channel_model_t;

/* ==========================================================================
 * PHY Performance Metrics (L1 Definition)
 * ========================================================================== */

/**
 * @brief Link budget and performance metrics
 *
 * Core metrics for wireless link analysis:
 *   - RSSI: Received Signal Strength Indicator
 *   - SNR: Signal-to-Noise Ratio
 *   - BER: Bit Error Rate
 *   - PER: Packet Error Rate
 *   - EVM: Error Vector Magnitude (OFDM modulation accuracy)
 *
 * Friis transmission equation (free space):
 *   P_r = P_t + G_t + G_r - 20·log₁₀(4πd/λ)
 *
 * Reference: Molisch, "Wireless Communications", Ch. 4
 */
typedef struct {
    double   rssi_dbm;           /**< Received Signal Strength (dBm) */
    double   snr_db;             /**< Signal-to-Noise Ratio (dB) */
    double   sinr_db;            /**< Signal-to-Interference-plus-Noise Ratio */
    double   noise_floor_dbm;    /**< Thermal noise floor = -174 + 10*log10(BW) */
    double   ber;                /**< Bit Error Rate */
    double   per;                /**< Packet Error Rate */
    double   evm_percent;        /**< Error Vector Magnitude (RMS, in percent) */
    double   throughput_mbps;    /**< Effective MAC throughput */
    double   spectral_efficiency;/**< Throughput / Bandwidth (bps/Hz) */
} wifi_link_metrics_t;

/* ==========================================================================
 * Helper Macros and Constants
 * ========================================================================== */

/** WiFi ISM band channel frequencies (2.4 GHz, channels 1-14) */
#define WIFI_CHAN_1_FREQ_MHZ    2412.0
#define WIFI_CHAN_6_FREQ_MHZ    2437.0
#define WIFI_CHAN_11_FREQ_MHZ   2462.0
#define WIFI_CHAN_14_FREQ_MHZ   2484.0

/** Standard WiFi channel frequency: f_c = 2412 + 5*(ch-1) MHz for ch 1-13 */
#define WIFI_CH2FREQ_24GHZ(ch)  (2412.0 + 5.0 * ((ch) - 1))

/** Bluetooth BR/EDR channel frequency: f = 2402 + k MHz, k=0..78 */
#define BT_CH2FREQ(k)           (2402.0 + (double)(k))

/** BLE channel frequency: f = 2402 + 2*k MHz, k=0..39 */
#define BLE_CH2FREQ(k)          (2402.0 + 2.0 * (double)(k))

/** Thermal noise power: N = k·T·B = -174 dBm/Hz + 10·log10(BW) */
#define THERMAL_NOISE_DBM_HZ    (-174.0)

/** Speed of light (m/s) */
#define SPEED_OF_LIGHT_MPS      299792458.0

/** ISM band wavelength at 2.45 GHz: λ = c/f ≈ 0.1224 m */
#define ISM_WAVELENGTH_M        0.1224

/* ==========================================================================
 * Forward Declarations — Common Functions (implemented in wifi_bt_core.c)
 * ========================================================================== */

/* WiFi channel utilities */
double wifi_channel_to_freq_24ghz(int channel);
int    wifi_freq_to_channel_24ghz(double freq_mhz);

/* Bluetooth/BLE channel utilities */
double bt_channel_to_freq(int channel);
double ble_channel_to_freq(int channel);

/* Link budget and metrics */
double thermal_noise_floor_dbm(double bandwidth_hz);
double compute_snr_db(double rssi_dbm, double nf_db, double bw_hz);
double free_space_path_loss_db(double distance_m, double freq_hz);
double received_power_dbm(double tx_power_dbm, double tx_gain_dbi,
                          double rx_gain_dbi, double distance_m,
                          double freq_hz, double path_loss_exp);
double shannon_capacity_bps(double bandwidth_hz, double snr_linear);
double snr_db_to_linear(double snr_db);

/* OFDM utility functions */
double ofdm_useful_duration_us(const ofdm_params_t *params);
double ofdm_guard_interval_ratio(const ofdm_params_t *params);

/* WiFi MCS utilities */
int wifi_rate_lookup_11ag(wifi_mcs_t *mcs, int rate_index);
int wifi_payload_to_ofdm_symbols(int payload_bytes, const wifi_mcs_t *mcs);

/* WiFi receiver sensitivity estimation */
double wifi_rx_sensitivity_dbm(double data_rate_mbps, double nf_db, double bw_mhz);

/* Bluetooth clock utilities */
void bt_clock_init(bt_clock_t *clk, uint32_t init_val);
void bt_clock_advance_slot(bt_clock_t *clk);
uint32_t bt_clock_slot(const bt_clock_t *clk);

/* Bluetooth address utilities */
int      bt_address_compare(const bt_address_t *a, const bt_address_t *b);
void     bt_address_set(bt_address_t *addr, const uint8_t bytes[6]);
uint32_t bt_address_get_lap(const bt_address_t *addr);
uint8_t  bt_address_get_uap(const bt_address_t *addr);
uint16_t bt_address_get_nap(const bt_address_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_BT_TYPES_H */
