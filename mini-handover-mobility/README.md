# mini-handover-mobility

**Handover & Mobility Management in Wireless Communications**

Part of `mini-electronic-info` / `11. mini-wireless-mobile-comm`

## Module Status: COMPLETE ✅

| Level | Status | Description |
|-------|--------|-------------|
| L1 Definitions | **Complete** | 6 typedef structs (MeasurementQuantity, CellInfo, UEContext, HandoverParams, HandoverDecision, HandoverStatistics), 4 enums, all core handover/mobility definitions |
| L2 Core Concepts | **Complete** | RRC state machine, handover phases, mobility states, handover classification |
| L3 Math Structures | **Complete** | 6 mobility models (Random Walk, RWP, Gauss-Markov, Levy Walk, Directional, RPGM), 5 path loss models, L3 IIR filter, Kalman filter |
| L4 Fundamental Laws | **Complete** | Friis equation, Doppler effect, hysteresis decision bound, log-distance path loss, Clarke's coherence time, A3/A5 event formulas |
| L5 Algorithms | **Complete** | A3/A5 events, TTT, ping-pong detection, TOPSIS, WSM, GRA, Kalman filter, hysteresis/TTT/CIO optimization |
| L6 Canonical Problems | **Complete** | LTE X2 handover, NR N2 handover, WiFi 802.11r FT, WiFi 802.11k, IEEE 802.21 MIH VHO, HO failure recovery, latency model |
| L7 Applications | **Partial+** | MRO (SON), admission control, energy-efficient HO, 5G NR HO optimization (3 applications) |
| L8 Advanced Topics | **Partial+** | Conditional Handover (CHO), DAPS, ML-based cell prediction, load-balancing CIO (4 topics) |
| L9 Research Frontiers | **Partial** | Documented in knowledge-graph.md (6G mobility, LEO satellite HO, AI/ML HO) |

**Total Score: 16/18 (COMPLETE)**

## Core Definitions (L1)

- **HandoverType**: Hard, Soft, Softer, Seamless, Horizontal, Vertical
- **HandoverTrigger**: A1-A6, B1-B2 (3GPP measurement events)
- **HandoverPhase**: Idle, Measurement, Report, Preparation, Execution, Completion, Failure
- **MobilityState**: Stationary, Normal, Medium, High-speed
- **MeasurementQuantity**: RSRP, RSRQ, SINR, RSSI, CQI (3GPP TS 36.214)
- **CellIdentity**: PCI, CGI, TAC, EARFCN, NR-ARFCN

## Core Theorems & Laws (L4)

1. **Hysteresis Ping-Pong Bound**:
   P(pingpong) ≤ exp(-H²/(2σ²)) · T/T_corr

2. **Friis Transmission Equation**:
   PL(dB) = 20·log10(4πd/λ)

3. **Doppler Shift**:
   f_d = (v/c) · f_c · cos(θ)

4. **Clarke's Coherence Time**:
   T_c ≈ 0.423 / f_d_max

5. **Handover Rate Estimate**:
   λ_HO ≈ 2·v / (π·R)

6. **Level-Crossing Rate (Rice)**:
   N(R) = N_m · exp(-(R-μ)²/(2σ²))

7. **Okumura-Hata Urban Path Loss**:
   PL = 69.55 + 26.16·log10(f) - 13.82·log10(h_b) - a(h_m) + [44.9 - 6.55·log10(h_b)]·log10(d)

## Core Algorithms (L5)

1. **Event A3 Handover** (3GPP TS 36.331 §5.5.4.4)
2. **Event A5 Handover** (3GPP TS 36.331 §5.5.4.6)
3. **Time-To-Trigger (TTT)** with sliding window
4. **Ping-Pong Detection** (3GPP TS 32.425)
5. **TOPSIS** Multi-Criteria Decision (Hwang & Yoon, 1981)
6. **Weighted Sum Model (WSM)** for network selection
7. **Grey Relational Analysis (GRA)** (Deng, 1982)
8. **Kalman Filter** for RSRP tracking (Kalman, 1960)
9. **3GPP Layer-3 Filtering** (TS 36.331 §5.5.3.2)
10. **Hysteresis Margin Optimization** (grid search with Q-function)

## Canonical Problems (L6)

1. **LTE X2 Intra-frequency Handover** (3GPP TS 36.300)
2. **5G NR N2-based Handover** (3GPP TS 38.300)
3. **WiFi 802.11r Fast BSS Transition Roaming**
4. **WiFi 802.11k Assisted Roaming** (Neighbor Report)
5. **IEEE 802.21 MIH Vertical Handover** (LTE ↔ WiFi ↔ 5G)
6. **Handover Failure Recovery** (RRC Re-establishment)

## 九校课程映射 (Nine-School Curriculum Mapping)

| School | Course | Topics Covered |
|--------|--------|---------------|
| MIT | 6.450 Digital Comm | Handover fundamentals, mobility models, channel models |
| Stanford | EE359 Wireless | Handover algorithms, MIMO HO, 5G NR mobility |
| Berkeley | EE123 DSP | Kalman filtering, measurement processing, L3 filtering |
| Illinois | ECE 459 Comm | Protocol state machines, X2/S1 signaling |
| Michigan | EECS 455 Comm | Vehicular mobility, Doppler, high-speed HO |
| Georgia Tech | ECE 6601 Comm | Multi-criteria VHO, network selection |
| TU Munich | Communications | Self-organizing networks, MRO, SON |
| ETH | 227-0436 Comm | Advanced HO: CHO, DAPS, URLLC mobility |
| 清华 | 通信原理 | Handover procedures, 3GPP protocol analysis |

## File Structure

```
mini-handover-mobility/
├── Makefile              # Build system (make test/build/examples/bench/demo)
├── README.md             # This file
├── include/              # 6 header files
│   ├── handover_types.h          # Core type definitions (L1)
│   ├── handover_decision.h       # Decision algorithms (L2, L5)
│   ├── mobility_model.h          # Mobility models (L3)
│   ├── signal_measurement.h      # Radio measurements (L1, L3, L4)
│   ├── handover_optimize.h       # Optimization & advanced (L7, L8)
│   └── handover_protocol.h       # Protocol state machines (L2, L6)
├── src/                  # 6 implementation files
│   ├── handover_types.c          # Type utilities & statistics
│   ├── handover_decision.c       # Decision algorithms (A3/A5/TOPSIS/GRA/WSM)
│   ├── mobility_model.c          # RW, RWP, Gauss-Markov, Levy, Directional, RPGM
│   ├── signal_measurement.c      # RSSI/RSRP/RSRQ/SINR, 5 path loss models, Kalman
│   ├── handover_optimize.c       # Hysteresis/TTT/CIO opt, CHO, DAPS, MRO, ML pred
│   └── handover_protocol.c       # LTE X2/NR N2/WiFi FT/802.11k/802.21 procedures
├── tests/                # Comprehensive test suite (27 tests)
│   └── test_handover.c
├── examples/             # 3 end-to-end examples
│   ├── example_lte_a3.c           # LTE A3 handover simulation
│   ├── example_wifi_roaming.c     # WiFi 802.11r FT roaming
│   └── example_vertical_handover.c # TOPSIS/GRA VHO (LTE↔5G↔WiFi)
├── benches/
│   └── bench_handover.c  # Algorithm performance benchmarks
├── demos/
│   └── demo_handover_trace.c     # 7-cell RWP mobility HO trace
└── docs/
    ├── knowledge-graph.md
    ├── coverage-report.md
    ├── gap-report.md
    ├── course-alignment.md
    └── course-tree.md
```

## Build & Run

```bash
make          # Build static library libhandover.a
make test     # Build and run test suite
make examples # Build all examples
make bench    # Build and run benchmarks
make demo     # Build and run demo
make clean    # Clean build artifacts
```

## References

- 3GPP TS 36.331 (LTE RRC Protocol Specification)
- 3GPP TS 38.331 (NR RRC Protocol Specification)
- 3GPP TS 36.214 (LTE Physical Layer Measurements)
- 3GPP TS 38.215 (NR Physical Layer Measurements)
- 3GPP TR 38.901 (5G Channel Model)
- IEEE 802.11r-2008 (Fast BSS Transition)
- IEEE 802.21-2017 (Media Independent Handover)
- Molisch, "Wireless Communications" (2011)
- Rappaport, "Wireless Communications: Principles and Practice" (2002)
- Holma & Toskala, "LTE for UMTS" (2009)
