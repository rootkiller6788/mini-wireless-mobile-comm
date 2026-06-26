/**
 * @file lora_nbiot_common.h
 * @brief Common LPWAN definitions -- Shared types for LoRa and NB-IoT
 *
 * Knowledge Coverage:
 *   L1 -- ISM band definitions, EIRP, RSSI, SNR, link margin,
 *        path loss exponent, shadow fading
 *   L2 -- LPWAN taxonomy: UNB vs CSS vs OFDMA
 *   L3 -- Log-distance path loss model, Okumura-Hata model
 *   L4 -- Friis transmission equation, link budget equation
 *   L7 -- ETSI/FCC regulatory parameters, smart metering, asset tracking
 *
 * References:
 *   - Molisch, "Wireless Communications" (2011), Ch. 4-7
 *   - Rappaport, "Wireless Communications: Principles and Practice"
 *   - ETSI EN 300 220, FCC Part 15.247
 *
 * Curriculum Mapping:
 *   - Stanford EE359: Path loss and shadowing
 *   - MIT 6.450: Channel modeling
 *   - Berkeley EE117: Radio propagation
 *   - TU Munich: RF system design
 *
 * @license MIT
 */

#ifndef LORA_NBIOT_COMMON_H
#define LORA_NBIOT_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   L1: Core Definitions -- Frequency Bands and Power
   ============================================================================ */

/**
 * LPWAN frequency band definitions
 *
 * LoRa operates in ISM bands:
 *   - EU 863-870 MHz (g1-g4 sub-bands)
 *   - US 902-928 MHz (64+8 uplink + 8 downlink channels)
 *   - CN 470-510 MHz, 779-787 MHz
 *   - AU 915-928 MHz
 *   - AS 923 MHz
 *   - IN 865-867 MHz
 *   - KR 920-923 MHz
 *
 * NB-IoT operates in licensed cellular bands:
 *   - B1  (2100), B2 (1900), B3 (1800), B5 (850), B8 (900)
 *   - B12 (700), B13 (780), B17 (700), B19 (850), B20 (800)
 *   - B26 (850), B28 (700), B66 (1700)
 */
typedef enum {
    BAND_EU868 = 0,    /**< EU 863-870 MHz ISM (LoRa) */
    BAND_US915,        /**< US 902-928 MHz ISM (LoRa) */
    BAND_AS923,        /**< AS 923 MHz ISM (LoRa) */
    BAND_CN470,        /**< CN 470-510 MHz ISM (LoRa) */
    BAND_LTE_B8,       /**< LTE Band 8: 880-960 MHz (NB-IoT) */
    BAND_LTE_B20,      /**< LTE Band 20: 791-862 MHz (NB-IoT) */
    BAND_LTE_B28,      /**< LTE Band 28: 703-748 MHz (NB-IoT) */
    BAND_UNKNOWN = 255,
} lpwan_frequency_band_t;

/**
 * LPWAN technology type
 */
typedef enum {
    LPWAN_LORA    = 0,  /**< LoRa CSS modulation + LoRaWAN protocol */
    LPWAN_NBIOT   = 1,  /**< NB-IoT 3GPP cellular LPWAN */
    LPWAN_LTEM    = 2,  /**< LTE Cat-M1/M2 (eMTC) */
    LPWAN_SIGFOX  = 3,  /**< Sigfox UNB (Ultra Narrow Band) */
    LPWAN_UNKNOWN = 255,
} lpwan_technology_t;

/**
 * LPWAN technology comparison key metrics
 *
 * Range:
 *   LoRa:    2-5 km urban, 15+ km rural (SF12, BW125)
 *   NB-IoT:  1-10 km (depends on cell deployment, 164 dB MCL)
 *   Sigfox:  3-10 km urban, 40+ km rural
 *
 * Data rate:
 *   LoRa:    250 bps - 50 kbps
 *   NB-IoT:  160 bps - 250 kbps (DL), 160 bps - 204 kbps (UL)
 *   Sigfox:  100 bps (UL), 600 bps (DL, limited)
 *
 * Power consumption:
 *   LoRa:    ~30 mA TX, ~10 mA RX (SX1276)
 *   NB-IoT:  ~200 mA TX (23 dBm), ~50 mA RX
 *   Sigfox:  ~50 mA TX (14 dBm typical)
 */
typedef struct {
    lpwan_technology_t tech;          /**< Technology type */
    double   min_datarate_bps;        /**< Minimum data rate */
    double   max_datarate_bps;        /**< Maximum data rate */
    double   max_range_km_urban;      /**< Max range in urban environment */
    double   max_range_km_rural;      /**< Max range in rural environment */
    double   tx_current_ma;           /**< Typical TX current draw */
    double   rx_current_ma;           /**< Typical RX current draw */
    double   sleep_current_ua;        /**< Deep sleep current */
    double   max_tx_power_dbm;        /**< Maximum TX power */
    double   link_budget_db;          /**< Maximum link budget */
    double   battery_life_years;      /**< Estimated battery life (AA Li-SOCl2) */
} lpwan_tech_profile_t;

/**
 * Get technology profile for a given LPWAN technology
 *
 * @param tech Technology type
 * @return Pointer to static profile structure (do not free)
 */
const lpwan_tech_profile_t *lpwan_get_profile(lpwan_technology_t tech);

/* ============================================================================
   L2: Core Concepts -- Radio Propagation
   ============================================================================ */

/**
 * Radio propagation environment type
 *
 * Different environments have different path loss characteristics
 * due to clutter, building density, and terrain.
 */
typedef enum {
    PROP_ENV_FREE_SPACE  = 0,  /**< Free space (satellite, no obstructions) */
    PROP_ENV_URBAN       = 1,  /**< Dense urban (high-rise buildings) */
    PROP_ENV_SUBURBAN    = 2,  /**< Suburban (residential, low buildings) */
    PROP_ENV_RURAL       = 3,  /**< Rural (open terrain, few obstructions) */
    PROP_ENV_INDOOR      = 4,  /**< Indoor (walls, floors, furniture) */
    PROP_ENV_DEEP_INDOOR = 5,  /**< Deep indoor (basement, underground) */
} propagation_environment_t;

/**
 * Path loss model parameters
 *
 * L3 — Log-distance path loss model:
 *   PL(d) = PL(d0) + 10*n*log10(d/d0) + X_sigma
 *
 * where:
 *   PL(d0) = free-space path loss at reference distance
 *   n = path loss exponent (2.0 free space, 2.7-3.5 urban, 4-6 indoor)
 *   X_sigma = zero-mean Gaussian shadow fading with std dev sigma (dB)
 *
 * Okumura-Hata model (urban macro-cell, 150-1500 MHz):
 *   PL = 69.55 + 26.16*log10(f) - 13.82*log10(h_b) - a(h_m)
 *        + (44.9 - 6.55*log10(h_b))*log10(d)
 *
 * where f in MHz, h_b = base station height (30-200m),
 * h_m = mobile height (1-10m), d in km.
 */
typedef struct {
    propagation_environment_t env;  /**< Environment type */
    double   path_loss_exponent;    /**< Path loss exponent n */
    double   shadow_fading_std_db;  /**< Shadow fading standard deviation */
    double   reference_distance_m;  /**< Reference distance d0 */
    double   reference_loss_db;     /**< Path loss at d0  */
    double   frequency_mhz;         /**< Carrier frequency in MHz */
    double   tx_height_m;           /**< Transmitter antenna height */
    double   rx_height_m;           /**< Receiver antenna height */
} path_loss_model_t;

/**
 * Measured radio metrics
 *
 * RSSI (Received Signal Strength Indicator):
 *   - LoRa: RSSI is measured during preamble detection
 *   - NB-IoT: RSRP is measured from NRS (reference signals)
 *
 * SNR:
 *   - LoRa: Estimated from de-spread signal
 *   - NB-IoT: SINR from NRS subcarriers
 */
typedef struct {
    double   rssi_dbm;      /**< Received Signal Strength in dBm */
    double   snr_db;        /**< Signal-to-Noise Ratio in dB */
    double   noise_floor_dbm; /**< Estimated noise floor */
    uint32_t packet_count;  /**< Total packets received */
    uint32_t crc_error_count; /**< Packets with CRC errors */
    double   per;           /**< Packet Error Rate */
} radio_metrics_t;

/* ============================================================================
   L3: Mathematical Structures -- Path Loss Models
   ============================================================================ */

/**
 * Friis free-space path loss (L4 — Fundamental Law)
 *
 * Friis transmission equation:
 *   P_r = P_t * G_t * G_r * (lambda / (4*pi*d))^2
 *
 * Path loss in dB:
 *   PL(dB) = 20*log10(4*pi*d/lambda)
 *          = 20*log10(d) + 20*log10(f) + 32.45
 *
 * where:
 *   d = distance in meters
 *   f = frequency in MHz
 *   lambda = c/f, c = 3e8 m/s
 *
 * @param distance_m   Distance in meters
 * @param freq_mhz     Frequency in MHz
 * @return Free-space path loss in dB
 */
double friis_path_loss_db(double distance_m, double freq_mhz);

/**
 * Log-distance path loss model (L3)
 *
 * PL(d) = PL(d0) + 10*n*log10(d/d0)
 *
 * This is the standard empirical model for terrestrial propagation.
 *
 * @param distance_m   Distance in meters
 * @param model        Path loss model parameters
 * @return Path loss in dB
 */
double log_distance_path_loss_db(double distance_m,
                                  const path_loss_model_t *model);

/**
 * Okumura-Hata urban path loss model (L3)
 *
 * Classic empirical model for macro-cellular propagation (150-1500 MHz).
 *
 * Correction factor a(h_m) for mobile antenna height:
 *   Medium-small city: a = (1.1*log10(f) - 0.7)*h_m - (1.56*log10(f) - 0.8)
 *   Large city (f <= 200): a = 8.29*(log10(1.54*h_m))^2 - 1.1
 *   Large city (f >= 400): a = 3.2*(log10(11.75*h_m))^2 - 4.97
 *
 * @param distance_km  Distance in km (1-20 km valid range)
 * @param freq_mhz     Frequency in MHz (150-1500 MHz)
 * @param tx_height_m  Base station height (30-200m)
 * @param rx_height_m  Mobile height (1-10m)
 * @param large_city   1 for large city, 0 for medium-small city
 * @return Path loss in dB
 */
double okumura_hata_path_loss_db(double distance_km,
                                  double freq_mhz,
                                  double tx_height_m,
                                  double rx_height_m,
                                  int large_city);

/* ============================================================================
   L4: Fundamental Laws -- Link Budget
   ============================================================================ */

/**
 * Complete link budget calculation
 *
 * Link Budget (dB) = P_tx + G_tx + G_rx - PL - L_misc
 * Received Power (dBm) = P_tx + G_tx + G_rx - PL - L_misc
 *
 * Link Margin = Received Power - Receiver Sensitivity
 *
 * If Link Margin > 0: link is viable
 * If Link Margin < 0: link is not viable
 *
 * @param tx_power_dbm      Transmitter output power in dBm
 * @param tx_antenna_gain_db Transmitter antenna gain in dBi
 * @param rx_antenna_gain_db Receiver antenna gain in dBi
 * @param path_loss_db      Path loss in dB
 * @param misc_loss_db      Miscellaneous losses (cable, connector, fading margin)
 * @param rx_sensitivity_dbm Receiver sensitivity in dBm
 * @param rx_power_dbm      Output: received power in dBm
 * @return Link margin in dB (positive = viable)
 */
double link_budget_compute(double tx_power_dbm,
                            double tx_antenna_gain_db,
                            double rx_antenna_gain_db,
                            double path_loss_db,
                            double misc_loss_db,
                            double rx_sensitivity_dbm,
                            double *rx_power_dbm);

/**
 * Maximum range estimation from link budget
 *
 * Given all other parameters, solve for the distance at which
 * received power equals receiver sensitivity (link margin = 0).
 *
 * Using log-distance model:
 *   d_max = d0 * 10^((P_tx + G_tx + G_rx - L_misc - S_rx - PL(d0)) / (10*n))
 *
 * @param model             Path loss model
 * @param tx_power_dbm      Transmitter power in dBm
 * @param tx_antenna_gain_db Transmitter antenna gain in dBi
 * @param rx_antenna_gain_db Receiver antenna gain in dBi
 * @param rx_sensitivity_dbm Receiver sensitivity in dBm
 * @param misc_loss_db      Miscellaneous losses
 * @return Maximum range in meters
 */
double max_range_from_link_budget(const path_loss_model_t *model,
                                   double tx_power_dbm,
                                   double tx_antenna_gain_db,
                                   double rx_antenna_gain_db,
                                   double rx_sensitivity_dbm,
                                   double misc_loss_db);

/* ============================================================================
   L5: Methods -- Energy and Battery Life Modeling
   ============================================================================ */

/**
 * Transmission energy per bit
 *
 * E_b = P_tx * T_bit  (Joules per bit)
 *
 * where:
 *   P_tx = transmitter power in Watts
 *   T_bit = 1 / R_b (bit duration)
 *
 * @param tx_power_dbm  Transmitter power in dBm
 * @param bit_rate_bps  Bit rate in bps
 * @return Energy per bit in Joules
 */
double energy_per_bit_joules(double tx_power_dbm, double bit_rate_bps);

/**
 * Battery life estimation for LPWAN device
 *
 * Battery capacity: typical AA Li-SOCl2: 2400 mAh at 3.6V = 8.64 Wh
 *
 * Average current:
 *   I_avg = (T_tx * I_tx + T_rx * I_rx + T_sleep * I_sleep) / T_cycle
 *
 * Battery life = Capacity / I_avg
 *
 * @param battery_capacity_mah  Battery capacity in mAh
 * @param battery_voltage_v     Battery voltage
 * @param tx_interval_sec       Interval between transmissions
 * @param tx_duration_sec       Transmission duration
 * @param tx_current_ma         TX current in mA
 * @param rx_duration_sec       Receive window duration
 * @param rx_current_ma         RX current in mA
 * @param sleep_current_ua      Sleep current in uA
 * @return Estimated battery life in years
 */
double battery_life_estimate_years(double battery_capacity_mah,
                                    double battery_voltage_v,
                                    double tx_interval_sec,
                                    double tx_duration_sec,
                                    double tx_current_ma,
                                    double rx_duration_sec,
                                    double rx_current_ma,
                                    double sleep_current_ua);

/**
 * Energy-optimized transmission scheduling
 *
 * Given a delay tolerance, compute the optimal spreading factor
 * that minimizes total energy consumption per bit.
 *
 * Higher SF = longer airtime = more energy per packet
 *             but may avoid retransmissions (fewer total packets).
 *
 * E_total = N_tx * P_tx * T_packet
 *
 * @param distance_m    Link distance
 * @param model         Path loss model
 * @param data_bytes    Payload size
 * @param delay_tol_sec Maximum allowable delay
 * @param optimal_sf    Output: optimal spreading factor
 * @return Estimated energy per successful delivery in Joules
 */
double energy_optimal_sf(double distance_m,
                          const path_loss_model_t *model,
                          uint16_t data_bytes,
                          double delay_tol_sec,
                          uint8_t *optimal_sf);

/* ============================================================================
   L7: Applications -- Smart Metering, Asset Tracking, Agriculture
   ============================================================================ */

/**
 * Smart meter application profile
 *
 * Water/gas/electricity metering via LPWAN:
 *   - Typical reporting interval: 1 hour (water/gas), 15 min (electricity)
 *   - Payload: 10-50 bytes (meter reading + timestamp + status)
 *   - Battery life target: 10-20 years
 *   - Deep indoor installation (basement, underground pit)
 *
 * This is the canonical NB-IoT use case (3GPP TR 45.820).
 */
typedef struct {
    uint32_t meter_id;              /**< Unique meter identifier */
    double   reading_value;         /**< Current meter reading */
    uint32_t reading_timestamp;     /**< Unix timestamp of reading */
    uint8_t  battery_level_pct;     /**< Remaining battery percentage */
    uint8_t  valve_status;          /**< Valve open/closed status (gas/water) */
    uint8_t  tamper_flag;           /**< Tamper detection flag */
    uint8_t  alarm_flag;            /**< Leak/overcurrent alarm */
    uint32_t interval_s;            /**< Reporting interval in seconds */
} smart_meter_report_t;

/**
 * Asset tracking application profile
 *
 * GPS-free location using LoRaWAN TDOA/RSSI geolocation:
 *   - Typical reporting interval: 30-300 seconds (moving), hours (static)
 *   - Payload: 10-20 bytes (device ID + GPS coordinates or network metrics)
 *   - Battery life target: 1-5 years (rechargeable optional)
 *   - Mobility: walking speed to vehicle speed
 *
 * Geolocation accuracy:
 *   - TDOA (Time Difference of Arrival): 20-200m (requires 3+ gateways)
 *   - RSSI: 500-2000m (coarse, low infrastructure requirement)
 *   - GNSS + LPWAN: 2-5m (higher power, but with LPWAN assistance)
 */
typedef struct {
    uint32_t device_id;             /**< Asset tracker identifier */
    double   latitude;              /**< WGS84 latitude (valid if has_gnss) */
    double   longitude;             /**< WGS84 longitude (valid if has_gnss) */
    int      has_gnss;              /**< 1 if GNSS position valid */
    uint8_t  gateway_count;         /**< Number of gateways in range */
    double   rssi_dbm[8];           /**< RSSI from nearby gateways */
    uint32_t gateway_ids[8];        /**< Gateway MAC addresses */
    double   battery_v;             /**< Battery voltage */
    uint8_t  motion_state;          /**< 0=static, 1=walking, 2=vehicle */
} asset_tracker_report_t;

/**
 * Agriculture sensor application profile
 *
 * Soil moisture, temperature, humidity monitoring:
 *   - Reporting interval: 15-60 minutes
 *   - Payload: 10-30 bytes
 *   - Battery + solar: indefinite operation
 *   - Range: 2-15 km (rural, line-of-sight)
 *
 * LoRa advantage: direct sensor-to-gateway, no cellular subscription.
 */
typedef struct {
    uint32_t sensor_id;             /**< Sensor node identifier */
    double   soil_moisture_pct;     /**< Volumetric water content (%) */
    double   soil_temperature_c;    /**< Soil temperature (Celsius) */
    double   air_temperature_c;     /**< Air temperature */
    double   air_humidity_pct;      /**< Relative humidity (%) */
    double   leaf_wetness;          /**< Leaf wetness (0-1) */
    uint16_t solar_radiation_wm2;   /**< Solar radiation (W/m^2) */
    double   battery_v;             /**< Battery voltage */
    double   solar_panel_v;         /**< Solar panel voltage */
} agriculture_sensor_report_t;

/**
 * Encode smart meter report into a compact binary format for LPWAN
 *
 * @param report     Smart meter report
 * @param buffer     Output buffer
 * @param buf_size   Buffer size
 * @return Encoded length in bytes, or -1 on error
 */
int smart_meter_encode(const smart_meter_report_t *report,
                        uint8_t *buffer, size_t buf_size);

/**
 * Decode smart meter report from compact binary format
 *
 * @param buffer     Input buffer
 * @param len        Buffer length
 * @param report     Output: decoded report
 * @return 0 on success, -1 on error
 */
int smart_meter_decode(const uint8_t *buffer, size_t len,
                        smart_meter_report_t *report);

/**
 * Encode asset tracker report
 */
int asset_tracker_encode(const asset_tracker_report_t *report,
                          uint8_t *buffer, size_t buf_size);

/**
 * Decode asset tracker report
 */
int asset_tracker_decode(const uint8_t *buffer, size_t len,
                          asset_tracker_report_t *report);

/**
 * Encode agriculture sensor report
 */
int agriculture_sensor_encode(const agriculture_sensor_report_t *report,
                               uint8_t *buffer, size_t buf_size);

/**
 * Decode agriculture sensor report
 */
int agriculture_sensor_decode(const uint8_t *buffer, size_t len,
                               agriculture_sensor_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* LORA_NBIOT_COMMON_H */
