# mini-cellular-network

**Cellular Network Architecture & Engineering Module**

Reference: Molisch (2011), 3GPP TS 38.300, Sesia et al. (2011), Goldsmith (2005)

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (NR deployment planning, site selection)
- L8: Partial (HetNet capacity gain, small cell deployment)
- L9: Partial (O-RAN, 6G cell-free documented, not implemented)

## Nine-Level Knowledge Coverage

| Level | Name | Status | Items |
|-------|------|--------|-------|
| L1 | Definitions | **Complete** | Cell types, RAT, NR bands, NE types, measurement quantities, PCI, numerology, 5QI, channel types, UE states, RRC/ECM/EMM |
| L2 | Core Concepts | **Complete** | Hexagonal grid, frequency reuse, co-channel interference, NRT, cell reselection |
| L3 | Math Structures | **Complete** | Hex coordinate system, path loss models (FSPL, Hata, COST-231, 3GPP TR38.901), SINR, log-distance |
| L4 | Fundamental Laws | **Complete** | Shannon-Hartley theorem, Friis equation, link budget, MAPL |
| L5 | Algorithms/Methods | **Complete** | RR/Max-CI/PF/EXP-PF scheduling, OLPC/CLPC, A3 handover, L3 filtering, CQI-AMC, Erlang B/C, M/D/1 queuing, water-filling |
| L6 | Canonical Problems | **Complete** | Cell planning, frequency reuse, link budget, coverage dimensioning, capacity planning |
| L7 | Applications | **Complete** | 5G NR gNB deployment, greedy site selection |
| L8 | Advanced Topics | **Partial** | HetNet small cells, capacity gain (2/5 implemented) |
| L9 | Research Frontiers | **Partial** | O-RAN, 6G cell-free documented only |

## Core Definitions (L1)

- : Macro / Micro / Pico / Femto
- : Base station physical parameters (43 struct members)
- : UE parameters (10 members)
- : Measurement report (RSRP, RSRQ, SINR, CQI)
- , , , 
- : 16 NR bands (FR1 + FR2)
- : 15 5GC/EPC network functions

## Core Theorems (L4)

1. **Shannon-Hartley**: C = B * log2(1 + SINR)
2. **Friis Transmission**: PL(dB) = 20*log10(d_km) + 20*log10(f_MHz) + 32.45
3. **D/R Ratio**: D/R = sqrt(3N) for reuse factor N
4. **Erlang B**: Blocking probability P_B = (A^C/C!) / sum_k(A^k/k!)
5. **Water-filling**: P_k = max(0, mu - 1/gamma_k)

## Core Algorithms (L5)

- Round Robin / Max C/I / Proportional Fair scheduling
- EXP/PF delay-aware scheduling
- Jain Fairness Index
- Open-loop & closed-loop power control
- Fractional path loss compensation
- A3-event handover with L3 filtering
- Time-to-trigger (TTT) and hysteresis
- MRO (Mobility Robustness Optimization)
- CQI-to-MCS adaptive modulation
- Erlang B/C traffic engineering
- M/D/1 packet queuing model
- Water-filling power allocation
- Greedy site selection (set cover)

## Classical Problems (L6)

1. **Cell Planning**: Hex grid deployment with frequency reuse N=1,3,4,7
2. **Link Budget**: MAPL calculation for 5G NR at 3.5 GHz
3. **Scheduler Comparison**: RR vs Max-C/I vs PF fairness-throughput trade-off
4. **Capacity Dimensioning**: Erlang B/C for voice/data traffic
5. **Coverage Planning**: Shadow margin, cell range from MAPL

## Nine-School Course Mapping

| School | Course | Topics Covered |
|--------|--------|---------------|
| MIT | 6.450 Digital Comm | Shannon capacity, scheduling |
| Stanford | EE359 Wireless | Cellular architecture, handover |
| Berkeley | EE123 DSP | OFDM numerology, link adaptation |
| Illinois | ECE 459 Comm | Frequency reuse, interference |
| Michigan | EECS 455 Comm | Power control, MCS selection |
| Georgia Tech | ECE 6601 Comm | Erlang capacity, queuing |
| TU Munich | Communications | Path loss, propagation models |
| ETH | 227-0436 Comm | Scheduling, resource allocation |
| Tsinghua | Comm Principles | Cellular network architecture |

## Files

| Directory | Files | Lines |
|-----------|-------|-------|
| include/ | 6 headers | ~1235 |
| src/ | 8 source files | ~1829 |
| tests/ | 1 test file | ~257 |
| examples/ | 3 examples | ~203 |
| demos/ | 1 demo | ~72 |
| benches/ | 1 bench | ~67 |
| docs/ | 5 doc files | ¡ª |

**Total include/ + src/: >3000 lines**

## Building

gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_defs.c -o src/cell_network_defs.o
gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_model.c -o src/cell_network_model.o
gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_link.c -o src/cell_network_link.o
gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_link.c -o src/cell_network_link.o
gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_link.c -o src/cell_network_link.o
gcc -std=c11 -Wall -Wextra -O2 -g -Iinclude -c src/cell_network_link.c -o src/cell_network_link.o
rm -f src/*.o
rm -f tests/test_cell_network
rm -f examples/example_cell_planning            examples/example_scheduler            examples/example_link_budget
rm -f demos/demo_capacity
rm -f benches/bench_scheduler
