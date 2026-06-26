# Course Dependency Tree — mini-handover-mobility

Prerequisite knowledge tree for handover and mobility management.

## Prerequisites (External Dependencies)

```
mini-handover-mobility
│
├── mini-signal-system-theory (0.)
│   ├── Fourier analysis → Doppler spectrum
│   ├── Convolution → L3 filtering
│   └── Stochastic processes → fading models
│
├── mini-communication-principle (5.)
│   ├── Modulation schemes → CQI, BLER
│   ├── Channel coding → SINR requirements
│   └── Multiple access → PRB allocation
│
├── mini-digital-signal-process (6.)
│   ├── IIR filters → L3 filtering
│   ├── Kalman filter → RSRP tracking
│   └── Spectral estimation → Doppler measurement
│
├── mini-electromagnetic-wave (7.)
│   ├── Friis equation → path loss
│   ├── Multipath propagation → fading
│   └── Doppler shift → mobility effects
│
└── mini-wireless-mobile-comm (11.) [parent module]
    ├── Cellular architecture → cell layout
    ├── RAN protocols → RRC/X2/S1/N2
    └── Mobility principles → location management
```

## Internal Dependency Tree

```
handover_types.h (L1)
│
├── signal_measurement.h/.c (L1, L3, L4)
│   ├── Depends on: handover_types.h
│   ├── Provides: RSSI, RSRP, RSRQ, SINR, path loss, fading, Kalman
│   │
│   ├── mobility_model.h/.c (L3, L4)
│   │   ├── Depends on: handover_types.h
│   │   └── Provides: 6 mobility models, MSD, Doppler, coherence time
│   │
│   ├── handover_decision.h/.c (L2, L4, L5)
│   │   ├── Depends on: handover_types.h
│   │   └── Provides: A3/A5 events, TTT, TOPSIS, GRA, WSM, ping-pong
│   │
│   ├── handover_optimize.h/.c (L5, L7, L8)
│   │   ├── Depends on: handover_types.h, handover_decision.h
│   │   └── Provides: Hysteresis/TTT/CIO opt, CHO, DAPS, MRO, ML pred
│   │
│   └── handover_protocol.h/.c (L2, L6)
│       ├── Depends on: handover_types.h
│       └── Provides: RRC FSM, LTE X2, NR N2, WiFi FT/11k, MIH VHO
```

## Knowledge Flow (Learning Path)

```
Step 1: L1 Definitions (handover_types.h)
        └── Understand all data types, enums, and structs

Step 2: L3 Math + L4 Laws (signal_measurement, mobility_model)
        └── Learn propagation, fading, mobility models

Step 3: L2 Core Concepts + L5 Algorithms (handover_decision)
        └── Implement handover decision algorithms

Step 4: L6 Canonical Problems (handover_protocol)
        └── Implement protocol-level procedures

Step 5: L7 Applications + L8 Advanced (handover_optimize)
        └── Optimize parameters, implement advanced features

Step 6: L9 Research Frontiers (docs/knowledge-graph.md)
        └── Review future research directions
```

## Forward Dependencies (What Depends on This Module)

```
mini-handover-mobility → Provides handover and mobility functions to:
│
├── mini-wireless-mobile-comm (11.)
│   └── System-level wireless network simulation
│
├── mini-iot-edge-computing (15.)
│   └── IoT mobility management for edge computing
│
├── mini-navigation-positioning (14.)
│   └── Mobility state detection for positioning
│
└── mini-radar-remote-sensing (13.)
    └── Doppler processing for radar sensing
```
