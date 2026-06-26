/**
 * @file wifi_bt_core.c
 * @brief WiFi & Bluetooth Core — Type instantiation, helper functions, constants (L1)
 *
 * Provides initialization and utility functions for core data types:
 *   - WiFi PHY mode information queries
 *   - OFDM parameter instantiation
 *   - MCS rate table lookups
 *   - Bluetooth clock utility functions
 *   - Channel frequency conversions
 *
 * Each function implements an independent knowledge point from L1 Definitions.
 *
 * Reference: IEEE Std 802.11-2020, Bluetooth Core Spec v5.4
 * Reference: Molisch, A.F., "Wireless Communications", 2nd ed., Wiley 2011.
 */

#include "wifi_bt_types.h"
#include "wifi_phy.h"
#include <math.h>
#include <string.h>

/* Ensure M_PI is defined in C99 mode */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 * Internal Rate Tables
 * ========================================================================== */

/**
 * WiFi MCS rate table for 802.11a/g (OFDM, single stream, 20 MHz)
 *
 * Maps modulation type + coding rate to data rate.
 * Covers rates from BPSK 1/2 (6 Mbps) to 64-QAM 3/4 (54 Mbps).
 */
typedef struct {
    wifi_modulation_t mod;
    int    rate_num;
    int    rate_den;
    int    n_bpsc;          /**< Coded bits per subcarrier */
    int    n_cbps;          /**< Coded bits per OFDM symbol */
    int    n_dbps;          /**< Data bits per OFDM symbol */
    double data_rate;       /**< Data rate in Mbps */
} wifi_rate_entry_t;

/* 802.11a/g rate table (8 entries, mandatory rates bold) */
static const wifi_rate_entry_t rate_table_11ag[] = {
    /* index 0 */ { MOD_BPSK,  1, 2, 1,  48,  24,  6.0  },
    /* index 1 */ { MOD_BPSK,  3, 4, 1,  48,  36,  9.0  },
    /* index 2 */ { MOD_QPSK,  1, 2, 2,  96,  48, 12.0  },
    /* index 3 */ { MOD_QPSK,  3, 4, 2,  96,  72, 18.0  },
    /* index 4 */ { MOD_16QAM, 1, 2, 4, 192,  96, 24.0  },
    /* index 5 */ { MOD_16QAM, 3, 4, 4, 192, 144, 36.0  },
    /* index 6 */ { MOD_64QAM, 2, 3, 6, 288, 192, 48.0  },
    /* index 7 */ { MOD_64QAM, 3, 4, 6, 288, 216, 54.0  }
};

#define N_RATE_11AG (sizeof(rate_table_11ag) / sizeof(rate_table_11ag[0]))

/* ==========================================================================
 * OFDM Numerology Table (L1)
 * ========================================================================== */

/**
 * OFDM numerology per bandwidth (IEEE 802.11)
 *
 * Key relationship: subcarrier_spacing = BW / N_FFT
 * Symbol duration = N_FFT/BW + cp_duration
 * CP duration = N_CP / BW = N_FFT / (4*BW)  (for normal CP, GI = 1/4)
 */
typedef struct {
    double   bw_mhz;
    int      n_fft;
    int      n_data_sc;
    int      n_pilot_sc;
    int      n_guard_total;
    int      n_cp;
    double   delta_f_khz;
} ofdm_numerology_t;

static const ofdm_numerology_t ofdm_numerology_table[] = {
    /* BW=20  */ { 20.0,  64,  48,  4,  12, 16, 312.5 },
    /* BW=40  */ { 40.0, 128, 108,  6,  14, 32, 312.5 },
    /* BW=80  */ { 80.0, 256, 234,  8,  14, 64, 312.5 },
    /* BW=160 */ {160.0, 512, 468, 16,  28,128, 312.5 }
};

#define N_OFDM_NUMEROLOGY (sizeof(ofdm_numerology_table) / sizeof(ofdm_numerology_table[0]))

/* ==========================================================================
 * WiFi PHY Mode Queries (L1 Definition)
 * ========================================================================== */

/**
 * @brief Get PHY mode name string
 *
 * Maps WiFi PHY mode enum to human-readable name for debugging and display.
 *
 * @param mode   WiFi PHY mode
 * @return String name (e.g., "802.11ax")
 */
const char *wifi_phy_mode_name(wifi_phy_mode_t mode)
{
    static const char *names[] = {
        "802.11a", "802.11b", "802.11g",
        "802.11n", "802.11ac", "802.11ax", "802.11be"
    };
    if (mode < 0 || mode > WIFI_PHY_80211BE) return "Unknown";
    return names[mode];
}

/**
 * @brief Get maximum channel bandwidth supported by a PHY mode
 *
 * Bandwidth capability progression:
 *   802.11a/b/g → 20 MHz
 *   802.11n     → 40 MHz
 *   802.11ac    → 160 MHz
 *   802.11ax    → 160 MHz
 *   802.11be    → 320 MHz
 *
 * @param phy   WiFi PHY mode
 * @return Supported bandwidth type
 */
wifi_bandwidth_t wifi_phy_max_bandwidth(wifi_phy_mode_t phy)
{
    switch (phy) {
        case WIFI_PHY_80211A:
        case WIFI_PHY_80211B:
        case WIFI_PHY_80211G:
            return WIFI_BW_20_MHZ;
        case WIFI_PHY_80211N:
            return WIFI_BW_40_MHZ;
        case WIFI_PHY_80211AC:
        case WIFI_PHY_80211AX:
            return WIFI_BW_160_MHZ;
        case WIFI_PHY_80211BE:
            return WIFI_BW_320_MHZ;
        default:
            return WIFI_BW_20_MHZ;
    }
}

/**
 * @brief Get maximum spatial streams supported by PHY
 *
 * @param phy   WiFi PHY mode
 * @return Maximum number of spatial streams
 */
int wifi_phy_max_spatial_streams(wifi_phy_mode_t phy)
{
    switch (phy) {
        case WIFI_PHY_80211A:
        case WIFI_PHY_80211B:
        case WIFI_PHY_80211G:
            return 1;
        case WIFI_PHY_80211N:
            return 4;
        case WIFI_PHY_80211AC:
            return 8;
        case WIFI_PHY_80211AX:
        case WIFI_PHY_80211BE:
            return 8;
        default:
            return 1;
    }
}

/* ==========================================================================
 * MCS Rate Table Lookup (L1 Definition)
 * ========================================================================== */

/**
 * @brief Look up MCS parameters for 802.11a/g rates
 *
 * Returns modulation, coding rate, and data rate for a given rate index.
 * The 8 mandatory 802.11a/g OFDM rates cover 6 to 54 Mbps.
 *
 * Formula: data_rate = N_dbps * symbol_rate
 *   N_dbps = N_data * N_bpsc * coding_rate
 *   symbol_rate = 250,000 symbols/s (for 4 µs symbol)
 *
 * @param mcs           Output MCS struct
 * @param rate_index    Rate index (0-7 for 802.11a/g)
 * @return 0 if valid, -1 if out of range
 */
int wifi_rate_lookup_11ag(wifi_mcs_t *mcs, int rate_index)
{
    if (rate_index < 0 || rate_index >= (int)N_RATE_11AG) return -1;

    const wifi_rate_entry_t *entry = &rate_table_11ag[rate_index];
    mcs->mcs_index        = rate_index;
    mcs->spatial_streams  = 1;
    mcs->data_rate_mbps   = entry->data_rate;
    mcs->coding_rate_num  = entry->rate_num;
    mcs->coding_rate_den  = entry->rate_den;
    mcs->bits_per_symbol  = entry->n_dbps;
    return 0;
}

/**
 * @brief Get number of data bits per OFDM symbol for a given MCS
 *
 * Used to compute how many MAC payload bytes fit in one OFDM symbol.
 *
 * @param mcs           MCS parameters
 * @return Data bits per OFDM symbol
 */
int wifi_mcs_data_bits_per_symbol(const wifi_mcs_t *mcs)
{
    return mcs->bits_per_symbol;
}

/**
 * @brief Compute the number of OFDM symbols needed for a payload
 *
 * N_sym = ceil((8 * payload_bytes + service_bits + tail_bits) / N_dbps)
 *
 * Standard: service = 16 bits, tail = 6 bits (for BCC)
 *
 * @param payload_bytes  Payload in bytes
 * @param mcs            MCS parameters
 * @return Number of OFDM symbols required
 */
int wifi_payload_to_ofdm_symbols(int payload_bytes, const wifi_mcs_t *mcs)
{
    if (payload_bytes <= 0 || mcs->bits_per_symbol <= 0) return -1;

    int total_bits = payload_bytes * 8 + 16 + 6;  /* service(16) + tail(6) */
    int symbols = (total_bits + mcs->bits_per_symbol - 1) / mcs->bits_per_symbol;
    return symbols;
}

/* ==========================================================================
 * OFDM Parameters Initialization (L1 Definition)
 * ========================================================================== */

/**
 * @brief Initialize OFDM parameters for a given bandwidth
 *
 * Maps bandwidth to the correct IEEE 802.11 numerology.
 * The subcarrier spacing is always 312.5 kHz regardless of bandwidth,
 * because the FFT size scales proportionally with bandwidth.
 *
 * Mathematical verification:
 *   Δf = BW / N_FFT = 20 MHz / 64 = 312.5 kHz = 40 MHz / 128 = 80 MHz / 256 ✓
 *
 * @param params    OFDM parameters struct to fill
 * @param bw_mhz    Channel bandwidth in MHz (20/40/80/160)
 * @return 0 on success, -1 on invalid bandwidth
 */
int ofdm_params_init(ofdm_params_t *params, double bw_mhz)
{
    int found = 0;
    for (size_t i = 0; i < N_OFDM_NUMEROLOGY; i++) {
        /* Use approximate comparison to handle floating BW values */
        if (fabs(ofdm_numerology_table[i].bw_mhz - bw_mhz) < 1.0) {
            const ofdm_numerology_t *n = &ofdm_numerology_table[i];
            params->n_fft                = n->n_fft;
            params->n_data_sc            = n->n_data_sc;
            params->n_pilot_sc           = n->n_pilot_sc;
            params->n_guard_sc           = n->n_guard_total;
            params->n_cp                 = n->n_cp;
            params->subcarrier_spacing_khz = n->delta_f_khz;
            /* T_sym = T_fft + T_cp = N_FFT/BW + N_CP/BW = (N_FFT+N_CP)/BW µs */
            params->symbol_duration_us   = (double)(n->n_fft + n->n_cp) / n->bw_mhz;
            params->bandwidth_mhz        = n->bw_mhz;
            found = 1;
            break;
        }
    }
    if (!found) return -1;
    return 0;
}

/**
 * @brief Get OFDM useful symbol duration (without cyclic prefix)
 *
 * T_fft = N_FFT / BW = 1 / Δf
 * For 20 MHz with 64-point FFT: T_fft = 64/20e6 = 3.2 µs
 *
 * @param params    OFDM parameters
 * @return Useful symbol duration in microseconds
 */
double ofdm_useful_duration_us(const ofdm_params_t *params)
{
    return (double)params->n_fft / params->bandwidth_mhz;
}

/**
 * @brief Get the guard interval ratio (GI = CP/T_fft)
 *
 * Standard GI: 1/4 (800 ns for 20 MHz), Short GI: 1/8 (400 ns)
 *
 * @param params    OFDM parameters
 * @return Guard interval ratio (0.25 = normal GI in 802.11a/g)
 */
double ofdm_guard_interval_ratio(const ofdm_params_t *params)
{
    return (double)params->n_cp / (double)params->n_fft;
}

/* ==========================================================================
 * Bluetooth Clock Functions (L1 Definition)
 * ========================================================================== */

/**
 * @brief Initialize Bluetooth clock
 *
 * Sets native clock (28-bit, 312.5 µs tick resolution).
 * Slot number = CLK / 2.
 *
 * @param clk       Clock struct to initialize
 * @param init_val  Initial clock value (0 = cold start)
 */
void bt_clock_init(bt_clock_t *clk, uint32_t init_val)
{
    clk->clk_native  = init_val & 0x0FFFFFFF;  /* 28 bits */
    clk->clk_offset  = 0;
    clk->slot_number = init_val >> 1;           /* slot = CLK[27:1] */
}

/**
 * @brief Advance clock by one slot (625 µs = 2 ticks)
 */
void bt_clock_advance_slot(bt_clock_t *clk)
{
    clk->clk_native = (clk->clk_native + 2) & 0x0FFFFFFF;
    clk->slot_number++;
}

/**
 * @brief Get current slot number
 */
uint32_t bt_clock_slot(const bt_clock_t *clk)
{
    return clk->slot_number;
}

/**
 * @brief Compute clock phase for hop selection
 *
 * Phase = (CLK + offset) mod N (used by hop selection kernel)
 * The phase determines which entry in the hop sequence to use.
 *
 * @param clk           Clock struct
 * @param offset_slots  Offset in slots (e.g., piconet master offset)
 * @param n_slots       Modulus (number of hop channels)
 * @return Phase value
 */
uint32_t bt_clock_hop_phase(const bt_clock_t *clk, int offset_slots, int n_slots)
{
    return (clk->slot_number + (uint32_t)offset_slots) % (uint32_t)n_slots;
}

/* ==========================================================================
 * Bluetooth Device Address Utilities (L1 Definition)
 * ========================================================================== */

/**
 * @brief Compare two BD_ADDR values
 *
 * Used for identity checking in connection management and bonding.
 *
 * @param a     First address
 * @param b     Second address
 * @return 0 if equal, non-zero otherwise
 */
int bt_address_compare(const bt_address_t *a, const bt_address_t *b)
{
    return memcmp(a->addr, b->addr, 6);
}

/**
 * @brief Set BD_ADDR from byte array
 *
 * @param addr      Output address struct
 * @param bytes     6-byte address (LSB first = little-endian as on-air)
 */
void bt_address_set(bt_address_t *addr, const uint8_t bytes[6])
{
    memcpy(addr->addr, bytes, 6);
}

/**
 * @brief Get the LAP (Lower Address Part) — 24 bits
 *
 * LAP is the lower 24 bits of BD_ADDR, used for access code generation.
 *
 * @param addr      BD_ADDR
 * @return LAP value (24-bit, packed in uint32_t)
 */
uint32_t bt_address_get_lap(const bt_address_t *addr)
{
    return ((uint32_t)addr->addr[2] << 16) |
           ((uint32_t)addr->addr[1] << 8)  |
            (uint32_t)addr->addr[0];
}

/**
 * @brief Get the UAP (Upper Address Part) — 8 bits
 *
 * @param addr      BD_ADDR
 * @return UAP value (8-bit)
 */
uint8_t bt_address_get_uap(const bt_address_t *addr)
{
    return addr->addr[3];
}

/**
 * @brief Get the NAP (Non-significant Address Part) — 16 bits
 *
 * @param addr      BD_ADDR
 * @return NAP value (16-bit)
 */
uint16_t bt_address_get_nap(const bt_address_t *addr)
{
    return ((uint16_t)addr->addr[5] << 8) | addr->addr[4];
}

/* ==========================================================================
 * Channel Frequency Conversion (L1 Definition)
 * ========================================================================== */

/**
 * @brief Convert WiFi 2.4 GHz channel number to center frequency
 *
 * Channel 1-13: f_c = 2412 + 5*(ch-1) MHz
 * Channel 14:   f_c = 2484 MHz (Japan only)
 *
 * @param channel   Channel number (1-14)
 * @return Center frequency in MHz, or -1 for invalid channel
 */
double wifi_channel_to_freq_24ghz(int channel)
{
    if (channel >= 1 && channel <= 13) {
        return 2412.0 + 5.0 * (double)(channel - 1);
    } else if (channel == 14) {
        return 2484.0;
    }
    return -1.0;
}

/**
 * @brief Convert frequency to closest WiFi 2.4 GHz channel
 *
 * @param freq_mhz  Frequency in MHz
 * @return Channel number (1-14), or -1 if out of band
 */
int wifi_freq_to_channel_24ghz(double freq_mhz)
{
    if (freq_mhz < 2400.0 || freq_mhz > 2500.0) return -1;
    if (freq_mhz >= 2473.0) return 14;  /* Channel 14 */

    int ch = (int)((freq_mhz - 2412.0) / 5.0 + 1.5);
    if (ch < 1) ch = 1;
    if (ch > 13) ch = 13;
    return ch;
}

/**
 * @brief Convert Bluetooth BR/EDR channel to frequency
 *
 * f = 2402 + k MHz, where k = 0..78
 *
 * @param channel   Channel index (0-78)
 * @return Center frequency in MHz, or -1 if invalid
 */
double bt_channel_to_freq(int channel)
{
    if (channel < 0 || channel > 78) return -1.0;
    return 2402.0 + (double)channel;
}

/**
 * @brief Convert BLE channel to frequency
 *
 * f = 2402 + 2*k MHz for advertising channels (k = 0,12,39 → ch 37,38,39)
 * Actually: BLE channels 0-36 are data, 37-39 are advertising
 *
 * data: f = 2402 + 2*k MHz, k = 0..36
 * adv:  f = 2402 + 2*(k-37) MHz for ch 37, 2402+2*12=2426 for ch 38, 2402+2*39=2480 for ch 39
 *
 * @param channel   BLE channel index (0-39)
 * @return Center frequency in MHz, or -1 if invalid
 */
double ble_channel_to_freq(int channel)
{
    if (channel < 0 || channel > 39) return -1.0;

    /* Advertising channels 37, 38, 39 map to k = 0, 12, 39 */
    if (channel == 37) return 2402.0;
    if (channel == 38) return 2426.0;
    if (channel == 39) return 2480.0;

    /* Data channels 0-36 */
    return 2402.0 + 2.0 * (double)channel;
}

/* ==========================================================================
 * Link Budget and Metrics (L1 Definition)
 * ========================================================================== */

/**
 * @brief Compute thermal noise floor
 *
 * N₀ = k · T · BW
 * N₀(dBm) = -174 + 10·log₁₀(BW in Hz)  (at T = 290K)
 *
 * @param bandwidth_hz  Bandwidth in Hz
 * @return Noise floor in dBm
 */
double thermal_noise_floor_dbm(double bandwidth_hz)
{
    if (bandwidth_hz <= 0.0) return -999.0;
    /* k·T = -174 dBm/Hz at room temperature */
    return -174.0 + 10.0 * log10(bandwidth_hz);
}

/**
 * @brief Compute SNR from RSSI and noise floor
 *
 * SNR(dB) = RSSI(dBm) - N₀(dBm)
 *
 * @param rssi_dbm RSSI
 * @param nf_db    System noise figure (default ~7 dB for WiFi)
 * @param bw_hz    Bandwidth
 * @return SNR in dB
 */
double compute_snr_db(double rssi_dbm, double nf_db, double bw_hz)
{
    double noise_floor = thermal_noise_floor_dbm(bw_hz);
    return rssi_dbm - (noise_floor + nf_db);
}

/**
 * @brief Compute free space path loss (Friis equation)
 *
 * PL(d) = 20·log₁₀(4πd/λ)
 *       = 20·log₁₀(d) + 20·log₁₀(f) - 147.55  (d in m, f in Hz)
 *
 * At 2.45 GHz, 1 m: PL = 20·log₁₀(4π·1/0.1224) = 40.2 dB
 *
 * @param distance_m    Distance in meters
 * @param freq_hz       Carrier frequency in Hz
 * @return Path loss in dB
 */
double free_space_path_loss_db(double distance_m, double freq_hz)
{
    if (distance_m <= 0.0 || freq_hz <= 0.0) return 0.0;
    double wavelength = 299792458.0 / freq_hz;
    return 20.0 * log10(4.0 * M_PI * distance_m / wavelength);
}

/**
 * @brief Compute received power using log-distance path loss model
 *
 * P_rx = P_tx + G_tx + G_rx - PL(d₀) - 10·n·log₁₀(d/d₀)
 *
 * This is the most commonly used indoor path loss model.
 *
 * @param tx_power_dbm   Transmit power in dBm
 * @param tx_gain_dbi    Transmit antenna gain
 * @param rx_gain_dbi    Receive antenna gain
 * @param distance_m     Distance
 * @param freq_hz        Frequency
 * @param path_loss_exp  Path loss exponent (2.0 free space, 2.7-4.0 indoor)
 * @return Received power in dBm
 */
double received_power_dbm(double tx_power_dbm, double tx_gain_dbi,
                          double rx_gain_dbi, double distance_m,
                          double freq_hz, double path_loss_exp)
{
    double pl_1m = free_space_path_loss_db(1.0, freq_hz);
    double pl = pl_1m + 10.0 * path_loss_exp * log10(distance_m);
    return tx_power_dbm + tx_gain_dbi + rx_gain_dbi - pl;
}

/**
 * @brief Compute Shannon capacity bound for a given SNR and bandwidth
 *
 * C = B · log₂(1 + SNR)  (bps)
 *
 * This is the fundamental upper bound on channel capacity.
 * Real WiFi achieves 30-70% of Shannon bound.
 *
 * Theorem: Shannon-Hartley Theorem — no communication system can
 * exceed C bps in an AWGN channel of bandwidth B with SNR.
 *
 * @param bandwidth_hz   Bandwidth in Hz
 * @param snr_linear     SNR (linear, not dB)
 * @return Channel capacity in bps
 */
double shannon_capacity_bps(double bandwidth_hz, double snr_linear)
{
    if (bandwidth_hz <= 0.0 || snr_linear <= 0.0) return 0.0;
    return bandwidth_hz * log2(1.0 + snr_linear);
}

/**
 * @brief Convert SNR from dB to linear
 *
 * @param snr_db    SNR in dB
 * @return SNR linear
 */
double snr_db_to_linear(double snr_db)
{
    return pow(10.0, snr_db / 10.0);
}

/* ==========================================================================
 * WiFi WiFi-Direct / P2P Group Formation (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Compute the WiFi Direct Group Owner intent tiebreaker
 *
 * WiFi Direct uses a Group Owner (GO) negotiation where each device
 * sends its GO Intent. Higher intent wins; tie is broken by tie-breaker bit.
 *
 * Intent value = GO Intent attribute value (0-15)
 *
 * @param intent_self    Self intent
 * @param intent_peer    Peer intent
 * @param tiebreak_self  Self tie-breaker bit (0 or 1)
 * @param tiebreak_peer  Peer tie-breaker bit
 * @return 1 if self becomes GO, 0 if peer becomes GO
 */
int wifi_direct_go_decision(int intent_self, int intent_peer,
                            int tiebreak_self, int tiebreak_peer)
{
    if (intent_self > intent_peer) return 1;
    if (intent_self < intent_peer) return 0;
    /* Tie: higher tie-breaker bit wins */
    return (tiebreak_self > tiebreak_peer) ? 1 : 0;
}

/* ==========================================================================
 * Energy Detection Threshold (L2 Core Concept)
 * ========================================================================== */

/**
 * @brief Check if channel is busy via energy detection (CCA-ED)
 *
 * Clear Channel Assessment — Energy Detect threshold:
 *   - 802.11a/g: -62 dBm for 20 MHz
 *   - If RSSI > threshold → channel busy
 *
 * @param rssi_dbm          Current RSSI
 * @param cca_threshold_dbm CCA-ED threshold (typically -62 dBm)
 * @return 1 if busy, 0 if idle
 */
int wifi_cca_energy_detect(double rssi_dbm, double cca_threshold_dbm)
{
    return (rssi_dbm > cca_threshold_dbm) ? 1 : 0;
}

/**
 * @brief Compute minimum receiver sensitivity for a given data rate
 *
 * Sensitivity (dBm) = -174 + NF + 10·log₁₀(BW) + SNR_min(dB)
 *
 * IEEE 802.11a/g sensitivity requirements:
 *   6 Mbps:  -82 dBm
 *   54 Mbps: -65 dBm
 *
 * @param data_rate_mbps Data rate
 * @param nf_db          Noise figure (typical 7 dB)
 * @param bw_mhz         Bandwidth in MHz
 * @return Sensitivity in dBm
 */
double wifi_rx_sensitivity_dbm(double data_rate_mbps, double nf_db, double bw_mhz)
{
    double bw_hz = bw_mhz * 1e6;
    double noise = -174.0 + 10.0 * log10(bw_hz) + nf_db;
    /* SNR requirement increases ~3 dB per doubling of data rate */
    /* Baseline: BPSK 1/2 needs ~5 dB SNR for BER=1e-5 */
    double snr_min = 5.0 + 3.0 * log2(data_rate_mbps / 6.0);
    return noise + snr_min;
}
