# Course Dependency Tree — 5G NR PHY

```
5G NR PHY (this module)
├── mini-communication-principle/ (L1-L4 fundamentals)
│   ├── Shannon-Hartley theorem
│   ├── Modulation: QPSK/QAM
│   └── Channel capacity
├── mini-digital-signal-process/ (L3-L5 DSP)
│   ├── FFT/IFFT (radix-2 Cooley-Tukey)
│   ├── FIR/IIR filters
│   └── Multirate processing
├── mini-wireless-mobile-comm/mini-channel-model/ (L7)
│   ├── TDL/CDL channel models
│   ├── Path loss models (UMa/UMi/RMa)
│   └── Doppler/Jakes spectrum
├── mini-wireless-mobile-comm/mini-beamforming-massive-mimo/ (L8)
│   ├── SVD precoding
│   ├── Type I/II codebooks
│   └── Massive MIMO properties
└── mini-wireless-mobile-comm/mini-wireless-security/ (L7-L8)
    ├── AES encryption for user plane
    └── Physical layer security concepts
```

## Forward Dependencies

- **6G RIS designs** depend on this module's MIMO-OFDM framework
- **AI-native PHY** research builds on the channel estimation pipeline
- **ORAN split** implementations require the PDSCH/PDCCH chain
