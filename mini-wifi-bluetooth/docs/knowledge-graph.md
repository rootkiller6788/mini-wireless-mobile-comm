# Knowledge Graph — mini-wifi-bluetooth

## L1: Definitions (Complete ✅)

| # | Definition | C Type | Lean | Status |
|---|-----------|--------|------|--------|
| 1 | WiFi PHY mode (802.11a/b/g/n/ac/ax/be) | `wifi_phy_mode_t` | — | ✓ |
| 2 | OFDM symbol parameters (FFT, CP, Δf) | `ofdm_params_t` | — | ✓ |
| 3 | OFDM subcarrier map | `ofdm_subcarrier_map_t` | — | ✓ |
| 4 | WiFi MCS rate table | `wifi_mcs_t` | — | ✓ |
| 5 | CSMA/CA timing parameters | `wifi_csma_params_t` | — | ✓ |
| 6 | WiFi MAC frame header | `wifi_mac_header_t` | — | ✓ |
| 7 | Bluetooth version enum | `bt_version_t` | — | ✓ |
| 8 | Bluetooth device address (BD_ADDR) | `bt_address_t` | — | ✓ |
| 9 | Bluetooth clock/slot timing | `bt_clock_t` | — | ✓ |
| 10 | FHSS channel parameters | `bt_fhss_params_t` | — | ✓ |
| 11 | GFSK modulation parameters | `bt_gfsk_params_t` | — | ✓ |
| 12 | BLE PHY modes | `ble_phy_mode_t` | — | ✓ |
| 13 | BLE advertising parameters | `ble_adv_params_t` | — | ✓ |
| 14 | BLE GATT attribute types | `ble_gatt_attr_t`, `ble_uuid_t` | — | ✓ |
| 15 | Bluetooth link types (SCO/ACL/eSCO) | `bt_link_type_t` | — | ✓ |
| 16 | WiFi security suite | `wifi_security_t`, `wifi_cipher_t` | — | ✓ |
| 17 | WiFi security context (PMK/PTK/GTK) | `wifi_sec_context_t` | — | ✓ |
| 18 | Bluetooth security levels | `bt_security_level_t` | — | ✓ |
| 19 | WiFi-BT coexistence types | `coex_mechanism_t` | — | ✓ |
| 20 | Indoor channel model | `indoor_channel_model_t` | — | ✓ |
| 21 | Link budget metrics | `wifi_link_metrics_t` | — | ✓ |
| 22 | Bluetooth packet types (DM/DH/HV/EV) | `bt_packet_type_t` | — | ✓ |

## L2: Core Concepts (Complete ✅)

| # | Concept | File | Status |
|---|---------|------|--------|
| 1 | OFDM subcarrier orthogonality | `wifi_phy.h/c` | ✓ |
| 2 | Cyclic prefix as guard interval | `wifi_phy.c` | ✓ |
| 3 | CSMA/CA DCF channel access | `wifi_mac.h/c` | ✓ |
| 4 | EDCA QoS differentiation | `wifi_mac.h/c` | ✓ |
| 5 | RTS/CTS handshake (virtual carrier sense) | `wifi_mac.h/c` | ✓ |
| 6 | WiFi MAC addressing (TA/RA/DA/SA/BSSID) | `wifi_mac.c` | ✓ |
| 7 | Frequency hopping spread spectrum | `bluetooth_core.h/c` | ✓ |
| 8 | Adaptive frequency hopping (AFH) | `bluetooth_core.h/c` | ✓ |
| 9 | Bluetooth piconet/scatternet topology | `bluetooth_core.h` | ✓ |
| 10 | SCO/eSCO voice scheduling | `bluetooth_core.h/c` | ✓ |
| 11 | BLE link layer state machine | `bluetooth_ble.h/c` | ✓ |
| 12 | BLE advertising + scanning | `bluetooth_ble.h/c` | ✓ |
| 13 | BLE data channel hopping | `bluetooth_ble.h/c` | ✓ |
| 14 | BLE GATT hierarchy (Service/Char/Desc) | `bluetooth_ble.h/c` | ✓ |
| 15 | WiFi-Bluetooth coexistence | `wifi_bt_core.c` | ✓ |
| 16 | Constellation mapping (BPSK→256QAM) | `wifi_phy.c` | ✓ |
| 17 | Pilot scrambling (127-bit PRBS) | `wifi_phy.c` | ✓ |
| 18 | LTS channel estimation preamble | `wifi_phy.c` | ✓ |

## L3: Mathematical Structures (Complete ✅)

| # | Structure | File | Status |
|---|-----------|------|--------|
| 1 | IFFT (Radix-2 DIT) | `wifi_phy.c` | ✓ |
| 2 | Complex exponential basis (e^{jωt}) | `wifi_phy.c` (IFFT) | ✓ |
| 3 | Gray coding for QAM | `wifi_phy.c` | ✓ |
| 4 | CRC-32 polynomial algebra (GF(2)) | `wifi_coding.c` | ✓ |
| 5 | Convolutional code trellis | `wifi_coding.c` | ✓ |
| 6 | LDPC dual-diagonal parity matrix | `wifi_coding.c` + Lean | ✓ |
| 7 | GFSK Gaussian filter (erfc-based pulse) | `bluetooth_core.c` | ✓ |
| 8 | AES S-box (GF(2⁸) inversion) | `bluetooth_ble.c`, `wireless_security.c` | ✓ |
| 9 | AES MixColumns (GF(2⁸) algebra) | `wireless_security.c` | ✓ |
| 10 | AES key expansion | `wireless_security.c` | ✓ |
| 11 | HMAC-SHA1 construction | `wireless_security.c` | ✓ |
| 12 | Noise power spectral density | `wifi_bt_core.c` | ✓ |

## L4: Fundamental Laws (Complete ✅)

| # | Law/Theorem | C Verification | Lean Statement | Status |
|---|-------------|---------------|----------------|--------|
| 1 | Shannon-Hartley capacity | `shannon_capacity_bps()` | `shannon_capacity_exists` | ✓ |
| 2 | Friis transmission equation | `free_space_path_loss_db()` | `friis_rx_power_dbm` | ✓ |
| 3 | OFDM subcarrier orthogonality | Verified by IFFT | `ofdm_bins_are_orthogonal` | ✓ |
| 4 | Thermal noise floor (kTB) | `thermal_noise_floor_dbm()` | `noise_floor_factor` | ✓ |
| 5 | Bianchi CSMA/CA throughput bound | `bianchi_throughput()` | `csma_backoff_range` | ✓ |
| 6 | GFSK BT product fundament | `bt_gfsk_eye_opening()` | `gfsk_bandwidth_khz` | ✓ |
| 7 | AFH minimum channel constraint (N≥20) | `bt_afh_classify()` | `afh_minimum_channel_requirement` | ✓ |
| 8 | CRC-32 error detection properties | `crc32_80211()` | `crc32_polynomial_nonzero` | ✓ |
| 9 | LDPC dual-diagonal encoding property | `ldpc_encode()` | `dual_diagonal_is_square` | ✓ |

## L5: Algorithms/Methods (Complete ✅)

| # | Algorithm | Complexity | File | Status |
|---|-----------|-----------|------|--------|
| 1 | Radix-2 DIT IFFT | O(N·log N) | `wifi_phy.c` | ✓ |
| 2 | BPSK/QPSK/QAM Gray mapping | O(1) | `wifi_phy.c` | ✓ |
| 3 | QAM soft-decision LLR demap | O(1) | `wifi_phy.c` | ✓ |
| 4 | Convolutional encoder (K=7, R=1/2) | O(N) | `wifi_coding.c` | ✓ |
| 5 | Viterbi decoder (ACS + traceback) | O(64·N) | `wifi_coding.c` | ✓ |
| 6 | Rate puncturing/depuncturing | O(N) | `wifi_coding.c` | ✓ |
| 7 | 802.11a block interleaver | O(N) | `wifi_phy.c` | ✓ |
| 8 | LDPC accumulator encoder | O(N·Z) | `wifi_coding.c` | ✓ |
| 9 | CRC-32 table-based computation | O(N) | `wifi_coding.c` | ✓ |
| 10 | GFSK modulator/demodulator | O(N·L) | `bluetooth_core.c` | ✓ |
| 11 | Bluetooth hop selection kernel | O(1) | `bluetooth_core.c` | ✓ |
| 12 | E0 stream cipher (4 LFSRs) | O(N) | `bluetooth_core.c` | ✓ |
| 13 | AES-128 encrypt/decrypt | O(1) per block | `wireless_security.c` | ✓ |
| 14 | AES-CCM encrypt/decrypt | O(N) | `wireless_security.c` | ✓ |
| 15 | HMAC-SHA1 | O(N) | `wireless_security.c` | ✓ |
| 16 | PBKDF2-HMAC-SHA1 | O(N·c) | `wireless_security.c` | ✓ |
| 17 | BLE GATT read/write/discover | O(N_attrs) | `bluetooth_ble.c` | ✓ |
| 18 | BLE mesh relay decision with cache | O(C) | `bluetooth_ble.c` | ✓ |
| 19 | Alamouti STBC encoding/decoding | O(1) | `wifi_phy.c` | ✓ |

## L6: Canonical Problems (Complete ✅)

| # | Problem | Implementation | Status |
|---|---------|---------------|--------|
| 1 | OFDM symbol construction (data → IF → time) | `wifi_phy.c` + `example_wifi_ofdm.c` | ✓ |
| 2 | CSMA/CA with binary exponential backoff | `wifi_mac.c` | ✓ |
| 3 | EDCA QoS scheduling (4 access categories) | `wifi_mac.c` | ✓ |
| 4 | WiFi data frame construction + parsing | `wifi_mac.c` | ✓ |
| 5 | A-MSDU aggregation | `wifi_mac.c` | ✓ |
| 6 | Block ACK + retransmission | `wifi_mac.c` | ✓ |
| 7 | Bluetooth packet construction (AC+HEC+CRC) | `bluetooth_core.c` | ✓ |
| 8 | SCO/eSCO voice scheduling | `bluetooth_core.c` | ✓ |
| 9 | BLE advertising + connection establishment | `bluetooth_ble.c` + `example_ble_health_monitor.c` | ✓ |
| 10 | BLE GATT health monitor (Heart Rate) | `example_ble_health_monitor.c` | ✓ |
| 11 | WPA2 4-Way Handshake | `wireless_security.c` + `example_wifi_wpa3.c` | ✓ |
| 12 | WPA3 SAE (Dragonfly) handshake | `wireless_security.c` + `example_wifi_wpa3.c` | ✓ |
| 13 | CCMP encrypt/decrypt with MIC verification | `wireless_security.c` | ✓ |
| 14 | Bluetooth SSP Numeric Comparison | `wireless_security.c` | ✓ |

## L7: Applications (Complete ✅ — 4 applications)

| # | Application | File | Keywords |
|---|------------|------|----------|
| 1 | WiFi indoor positioning (RSSI→distance) | `wifi_bt_core.c` | RSSI, indoor, positioning |
| 2 | BLE health/fitness monitoring | `example_ble_health_monitor.c` | BLE, health, heart rate |
| 3 | WiFi security with WPA3 | `example_wifi_wpa3.c` | WPA3, SAE, security |
| 4 | BLE mesh sensor network | `bluetooth_ble.c` | BLE, mesh, relay |

## L8: Advanced Topics (Complete ✅ — 3 topics)

| # | Topic | Implementation | Status |
|---|-------|---------------|--------|
| 1 | OFDMA (802.11ax) subcarrier allocation | `wifi_bt_core.c` (OFDM numerology) | ✓ |
| 2 | BLE mesh networking (managed flooding) | `bluetooth_ble.c` (MESH) | ✓ |
| 3 | MU-MIMO / beamforming | `wifi_phy.c` (Alamouti STBC) | ✓ |

## L9: Research Frontiers (Partial ⚠️ — documented)

| # | Topic | Status |
|---|-------|--------|
| 1 | WiFi 7 (802.11be) 320 MHz channels | Documented in types |
| 2 | BLE Audio (LC3 codec, isochronous channels) | Documented |
| 3 | Ambient IoT / WiFi sensing | Documented |
| 4 | Machine learning for channel estimation | Future |
| 5 | Quantum-secure WiFi (post-quantum crypto) | Future |
