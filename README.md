# Mini Wireless & Mobile Communications

A collection of **from-scratch, zero-dependency C implementations** of wireless communication theory, cellular networks, and mobile systems. Each module maps to Stanford, MIT, and other top-tier university courses, bridging theory and practice by translating textbook equations and 3GPP standards into runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-5g-nr-phy](mini-5g-nr-phy/) | OFDM modulation, LDPC/Polar coding, MIMO precoding, PDCCH blind decoding, SSB cell search, DMRS channel estimation | Stanford EE359, MIT 6.450 |
| [mini-beamforming-massive-mimo](mini-beamforming-massive-mimo/) | Adaptive beamforming (LMS/RLS/CMA), antenna array design, DOA estimation (MUSIC/ESPRIT), MIMO capacity, MU-MIMO precoding (MRT/ZF/MMSE) | Stanford EE359, Stanford EE264 |
| [mini-cellular-network](mini-cellular-network/) | Hexagonal grid modeling, frequency reuse, path loss & SINR, link budget, power control, packet scheduling, handover management | Stanford EE359, MIT 6.829 |
| [mini-channel-model](mini-channel-model/) | Large/small-scale fading, Doppler spectrum, tapped-delay-line multipath, MIMO channel correlation, path loss models | Stanford EE359, NYU ECE-GY 6013 |
| [mini-handover-mobility](mini-handover-mobility/) | Handover decision algorithms, protocol state machines, signal measurement, mobility models, parameter optimization | Stanford EE359, Aalto ELEC-E8004 |
| [mini-lora-nbiot](mini-lora-nbiot/) | LoRa chirp spread spectrum (CSS) PHY, LoRaWAN MAC (frame/device classes), NB-IoT 3GPP PHY, LPWAN channel models | Stanford EE359, TU Delft ET4386 |
| [mini-wifi-bluetooth](mini-wifi-bluetooth/) | WiFi OFDM PHY, CSMA/CA MAC, BLE PHY & GATT, Bluetooth BR/EDR (FHSS/GFSK), WPA2/WPA3 security | Stanford EE359, MIT 6.829 |
| [mini-wireless-security](mini-wireless-security/) | AES/SHA-256/HMAC cryptography, EAP/802.1X authentication, WEP/WPA2 attacks, key management, physical layer security | MIT 6.875, Stanford EE359 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Standards-driven** — implementations reference 3GPP TS 38.211/38.212/38.213, IEEE 802.11, and NIST FIPS
- **Practical demos** — OFDM modulators, MIMO capacity simulators, beam pattern plotters, handover state machines, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-5g-nr-phy
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-wireless-mobile-comm/
├── mini-5g-nr-phy/                 # 5G NR physical layer: OFDM, LDPC/Polar, MIMO, SSB, PDCCH
├── mini-beamforming-massive-mimo/  # Beamforming & massive MIMO: adaptive, DOA, precoding
├── mini-cellular-network/          # Cellular network: grid, reuse, link budget, scheduler
├── mini-channel-model/             # Wireless channel: fading, Doppler, multipath, path loss
├── mini-handover-mobility/         # Handover & mobility: decision, protocol, measurement
├── mini-lora-nbiot/                # LoRa & NB-IoT: CSS PHY, LoRaWAN MAC, NB-IoT PHY
├── mini-wifi-bluetooth/            # WiFi & Bluetooth: OFDM, CSMA/CA, BLE, BR/EDR, WPA
└── mini-wireless-security/         # Wireless security: crypto, auth, attacks, key mgmt
```

## License

MIT
