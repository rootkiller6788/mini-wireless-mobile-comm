# Course Alignment: mini-channel-model

## Nine-School Curriculum Mapping

### MIT 6.450 — Digital Communications
- **Channel Models**: Rayleigh, Rician fading → `fading.h/c`
- **MIMO Capacity**: Telatar formula → `mimo_channel.h/c`
- **Multipath**: Delay spread, ISI → `multipath.h/c`

### Stanford EE359 — Wireless Communications
- **Path Loss Models**: Friis, Okumura-Hata → `pathloss.h/c`
- **Fading Statistics**: LCR, AFD → `doppler.h/c`
- **MIMO Systems**: Spatial multiplexing → `mimo_channel.h/c`

### Berkeley EE123 — Digital Signal Processing
- **Frequency-Selective Channels**: TDL model → `multipath.h/c`
- **Doppler Spectrum**: Jakes PSD → `doppler.h/c`

### Illinois ECE 459 — Communications
- **Wireless Propagation**: Okumura-Hata model → `pathloss.h/c`
- **Diversity Combining**: MRC, EGC → `multipath.h/c`

### Michigan EECS 455 — Communications
- **Fading Channels**: Nakagami-m, Weibull → `fading.h/c`
- **Channel Capacity**: Shannon-Hartley → `channel_core.c`

### Georgia Tech ECE 6601 — Communications
- **MIMO Systems**: Kronecker model → `mimo_channel.h/c`
- **Massive MIMO**: Channel hardening → `mimo_channel.h/c`

### TU Munich — High-Frequency Engineering
- **Propagation Modeling**: Walfisch-Ikegami → `pathloss.h/c`
- **3GPP Channel Models**: TR 38.901 → `pathloss.h/c`

### ETH 227-0455 — EM Waves & Propagation
- **Free-Space Propagation**: Friis equation → `pathloss.h/c`
- **Multipath**: Two-ray model → `pathloss.h/c`

### Tsinghua — Communications
- **Wireless Channel**: Fading types, Doppler → `channel_defs.h`
- **Link Budget**: Complete analysis → `examples/example_basic_link.c`
