# Course Tree — mini-wifi-bluetooth

## Prerequisite Dependency Graph

```
                    ┌──────────────────────────┐
                    │ mini-wifi-bluetooth       │
                    │ (This Module)             │
                    └────────────┬─────────────┘
                                 │ depends on
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
          ▼                      ▼                      ▼
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│ mini-signal-     │   │ mini-communication│  │ mini-digital-    │
│ system-theory    │   │ -principle        │  │ signal-process   │
│ (Fourier, LTI)   │   │ (Modulation, BER) │  │ (FIR/IIR, FFT)   │
└────────┬────────┘   └────────┬─────────┘   └────────┬─────────┘
         │                     │                      │
         └─────────────────────┼──────────────────────┘
                               │
                               ▼
                    ┌─────────────────┐
                    │ mini-circuit-    │
                    │ analysis         │
                    │ (Impedance, S)   │
                    └─────────────────┘
```

## Internal Knowledge Dependency Tree

### WiFi Branch

```
WiFi PHY
├── OFDM parameters (L1)
│   └── Subcarrier spacing: Δf = BW/N_FFT
├── OFDM symbol construction (L6)
│   ├── IFFT (Radix-2 DIT) (L3/L5)
│   ├── Cyclic prefix (L2)
│   ├── Pilot scrambling (L2)
│   └── Constellation mapping (L2)
├── Channel coding (L5)
│   ├── Convolutional encoder (K=7) → Viterbi decoder
│   ├── LDPC encoder (QC-LDPC, dual-diagonal)
│   └── Interleaver / Deinterleaver
├── MIMO (L2/L8)
│   └── Alamouti STBC (2x1)
└── EVM measurement (L1)

WiFi MAC
├── CSMA/CA DCF (L2)
│   ├── Binary exponential backoff (L5)
│   ├── NAV (virtual carrier sense) (L2)
│   └── RTS/CTS handshake (L5)
├── EDCA QoS (L5)
│   └── AIFS + per-AC CW (L5)
├── Frame construction (L2/L6)
│   ├── Data / RTS / CTS / ACK frames
│   └── Frame parsing (address extraction)
├── Block ACK (L5)
├── A-MSDU aggregation (L5)
└── Bianchi throughput model (L4)
```

### Bluetooth Branch

```
Bluetooth BR/EDR
├── FHSS (L2)
│   ├── Hop selection kernel (L5)
│   ├── Hop sequence generation (L5)
│   └── AFH channel classification (L2)
├── GFSK (L3)
│   ├── Gaussian filter impulse response
│   ├── Phase accumulation modulator
│   └── Frequency discriminator demodulator
├── Clock & slot management (L1/L2)
├── Packet construction (L2)
│   ├── Access code (LAP-based)
│   ├── HEC (8-bit CRC)
│   └── CRC-16
├── SCO/eSCO scheduling (L2)
└── E0 stream cipher (L5)

Bluetooth BLE
├── Link Layer state machine (L2)
│   ├── Advertising / Scanning / Initiating / Connection
│   └── State transitions
├── Advertising data (L2)
│   └── TLV format (AD structures)
├── Channel hopping (L2)
│   └── AFH remapping for BLE data channels
├── GATT (L5)
│   ├── Service / Characteristic / Descriptor hierarchy
│   ├── UUID type system (16-bit/128-bit)
│   └── Read/Write/Discover operations
├── Security (L5/L6)
│   ├── LE Secure Connections (ECDH)
│   ├── AES-CCM encryption
│   └── f5 key derivation
└── Mesh networking (L8)
    ├── Managed flooding relay
    └── Network PDU cache
```

### Security Branch

```
Wireless Security
├── AES-128 (L5)
│   ├── S-box (GF(2⁸) inversion)
│   ├── MixColumns (GF(2⁸) algebra)
│   └── Key expansion
├── AES modes (L5)
│   ├── CBC-MAC (authentication)
│   ├── CTR mode (encryption)
│   └── CCM (CCMP for WPA2)
├── HMAC (L3)
│   ├── HMAC-SHA1
│   └── HMAC-SHA256
├── PBKDF2 (L5)
│   └── WPA2-PSK key derivation
├── WPA2 4-Way Handshake (L6)
│   ├── PRF key derivation
│   ├── PTK = KCK || KEK || TK
│   └── MIC verification
├── WPA3 SAE / Dragonfly (L6)
│   ├── Hunting-and-pecking PWE
│   ├── Commit + Confirm exchange
│   └── PMK derivation
└── Bluetooth SSP (L6)
    ├── Numeric Comparison
    └── Just Works link key
```

## Cross-Module Dependencies

| External Module | Used For | This Module's Consumer |
|----------------|---------|----------------------|
| mini-signal-system-theory | Fourier transform, LTI systems | OFDM (IFFT-based) |
| mini-communication-principle | Modulation theory, BER analysis | QAM, coding, throughput |
| mini-digital-signal-process | FIR/IIR filters, FFT algorithms | GFSK Gaussian filter, IFFT |
| mini-circuit-analysis | Impedance matching, S-params | Antenna matching (link budget) |
