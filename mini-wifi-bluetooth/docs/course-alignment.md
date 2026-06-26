# Nine-School Course Alignment — mini-wifi-bluetooth

## MIT — 6.450 Digital Communications / 6.829 Wireless Networks

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| OFDM modulation and IFFT | Complete implementation | `wifi_phy.c` |
| Shannon capacity theorem | Verified in C + Lean | `wifi_bt_core.c` + Lean |
| Channel coding (convolutional, LDPC) | Encoder + Viterbi + LDPC | `wifi_coding.c` |
| MIMO and spatial diversity | Alamouti STBC (2×1 MISO) | `wifi_phy.c` |
| Wireless medium access (CSMA/CA) | DCF + EDCA implementation | `wifi_mac.c` |

## Stanford — EE359 Wireless Communications / EE360 Digital Comm

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| WiFi 802.11 PHY/MAC design | Full PHY + MAC layers | `wifi_phy.c`, `wifi_mac.c` |
| MIMO-OFDM | STBC encoding/decoding | `wifi_phy.c` |
| Adaptive modulation and coding | Rate tables + constellation mapping | `wifi_phy.c`, `wifi_bt_core.c` |
| Wireless security (WPA2/WPA3) | 4-way handshake + SAE | `wireless_security.c` |
| Bluetooth/BLE coexistence | Coexistence + AFH | `wifi_bt_core.c`, `bluetooth_core.c` |

## Berkeley — EE123 Digital Signal Processing / EE225D Audio Signal Processing

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| FFT and spectral analysis | Radix-2 DIT IFFT | `wifi_phy.c` |
| Filter design (Gaussian for GFSK) | GFSK pulse generation | `bluetooth_core.c` |
| Error correcting codes | Convolutional/Viterbi/LDPC | `wifi_coding.c` |

## Illinois — ECE 459 Communications Systems

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| Digital modulation (QPSK/QAM) | BPSK/QPSK/16/64/256QAM | `wifi_phy.c` |
| OFDM system design | Complete TX chain | `wifi_phy.c`, `example_wifi_ofdm.c` |
| Bluetooth system architecture | FHSS, GFSK, packets | `bluetooth_core.c` |

## Michigan — EECS 455 Wireless Communications

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| Channel models and path loss | Log-distance, Friis | `wifi_bt_core.c` |
| Multiple access techniques | CSMA/CA (WiFi), TDMA (BT SCO) | `wifi_mac.c`, `bluetooth_core.c` |
| Bluetooth piconet and scatternet | Link types, discovery | `bluetooth_core.c` |

## Georgia Tech — ECE 6601 Communications Networks

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| 802.11 MAC protocols | DCF, EDCA, RTS/CTS, Block ACK | `wifi_mac.c` |
| Bianchi throughput model | Saturated throughput analysis | `wifi_mac.c` |
| Frame aggregation (A-MSDU) | Aggregation + disassembly | `wifi_mac.c` |

## TU Munich — Communications Engineering

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| OFDM principles and implementation | Full OFDM TX chain | `wifi_phy.c` |
| Error vector magnitude (EVM) | EVM computation | `wifi_phy.c` |
| Bluetooth FHSS | 79-channel hop selection | `bluetooth_core.c` |

## ETH Zurich — 227-0436 Digital Communications

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| Signal space and constellation design | QAM mapping/demapping | `wifi_phy.c` |
| LDPC codes and iterative decoding | LDPC accumulator encoding | `wifi_coding.c` |
| CCM authenticated encryption | AES-CCMP full implementation | `wireless_security.c` |

## Tsinghua University — 通信原理 (Principles of Communications)

| Course Topic | Module Coverage | File |
|-------------|----------------|------|
| 数字调制 (Digital Modulation) | Complete QAM modulator | `wifi_phy.c` |
| 信道编码 (Channel Coding) | Conv + Viterbi + LDPC | `wifi_coding.c` |
| 无线局域网 (WLAN) | Full WiFi PHY + MAC | Multiple files |
| 蓝牙技术 (Bluetooth Technology) | BR/EDR + BLE | `bluetooth_core.c`, `bluetooth_ble.c` |
| 网络安全 (Network Security) | WPA2/WPA3, BT SSP | `wireless_security.c` |

## Cross-Cutting Topics

| Topic | Coverage Across Schools | Module Implementation |
|-------|------------------------|----------------------|
| Shannon Theory | MIT, Stanford, ETH, Tsinghua | C + Lean verification |
| OFDM | All 9 schools | Complete TX chain + examples |
| CSMA/CA | MIT, Stanford, Georgia Tech, Tsinghua | DCF + EDCA + Bianchi model |
| Security | Stanford, ETH | AES-CCMP, SAE, 4-way handshake, SSP |
