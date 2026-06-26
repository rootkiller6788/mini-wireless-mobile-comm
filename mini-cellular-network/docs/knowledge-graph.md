# Knowledge Graph ¡ª mini-cellular-network

## L1: Definitions (Complete)
- cell_type_t: Macro/Micro/Pico/Femto cell classification
- cell_status_t: Active/Sleep/Barred/Reserved/Degraded
- rat_type_t: GERAN/UTRAN/E-UTRAN/NR/WiFi
- nr_band_t: 16 NR bands (N1-N261, FR1+FR2)
- freq_range_t: FR1 (sub-6 GHz), FR2 (mmWave)
- network_element_t: 15 5GC/EPC elements
- rsrp_dbm_t, rsrq_db_t, rssi_dbm_t, sinr_db_t: measurement types
- cqi_t, pmi_t, ri_t: channel state feedback
- cell_meas_t, ue_meas_report_t: measurement report structures
- pci_t: Physical Cell Identity (N_ID1, N_ID2)
- numerology_t: 5 numerology (SCS 15/30/60/120/240 kHz)
- qos_profile_t: 5QI QoS profiles (3GPP TS 23.501)
- logical_ch_t, transport_ch_t, physical_ch_t: channel types
- rrc_state_t, emm_state_t, ecm_state_t: UE state machines
- ue_context_t: Core network subscriber context
- gnb_params_t: Base station physical parameters
- ue_params_t: UE physical parameters

## L2: Core Concepts (Complete)
- Hexagonal cell grid (axial coordinates q,r)
- Cell cluster generation (hex_ring, hex_filled_grid)
- Frequency reuse pattern N = i^2 + ij + j^2
- Co-channel reuse distance D/R = sqrt(3N)
- Co-channel interference computation
- Interference matrix for multi-cell deployments
- Neighbor Relation Table (NRT)
- Automatic Neighbor Relation (ANR)
- Cell selection/reselection parameters
- Inter-site distance (ISD) planning

## L3: Mathematical Structures (Complete)
- Hex coordinate conversion (axial to cartesian)
- Hex distance (cube coordinate metric)
- Free-space path loss (Friis equation)
- Okumura-Hata path loss model
- COST-231 Hata model
- 3GPP TR 38.901 UMa/UMi models
- Log-distance path loss with shadow fading
- SINR computation (signal, interference, noise)
- Interference summation (linear domain)
- Haversine distance (geo-coordinates)

## L4: Fundamental Laws (Complete)
- Shannon-Hartley theorem: C = B * log2(1 + SINR)
- Friis transmission equation: PL = 32.45 + 20*log10(d_km) + 20*log10(f_MHz)
- Link budget equation: MAPL, EIRP, RX sensitivity
- D/R ratio: D/R = sqrt(3N)
- Water-filling optimal power allocation

## L5: Algorithms/Methods (Complete)
- Round Robin scheduling (O(N_UE))
- Max C/I scheduling (O(N_UE * N_RB))
- Proportional Fair scheduling (O(N_UE * N_RB))
- EXP/PF delay-aware scheduling
- Jain Fairness Index
- Open-Loop Power Control (fractional path loss compensation)
- Closed-Loop Power Control (TPC accumulation)
- DL Water-filling power allocation
- A3-event handover with L3 filtering
- Time-To-Trigger (TTT) mechanism
- Mobility State Estimation (normal/medium/high)
- MRO (Mobility Robustness Optimization)
- Ping-pong handover detection
- CQI-to-MCS adaptive modulation
- Erlang B blocking probability
- Erlang C waiting probability
- M/D/1 queuing model
- Greedy site selection (set cover)

## L6: Canonical Problems (Complete)
- Cell planning with hex grid and frequency reuse
- Link budget: MAPL calculation and cell range estimation
- Scheduler comparison: RR vs Max-C/I vs PF
- Capacity dimensioning with Erlang B/C
- Coverage planning with shadow margin
- Inter-site distance planning

## L7: Applications (Complete)
- 5G NR gNB deployment planning (urban/suburban/rural)
- Greedy site selection for maximum coverage
- HetNet small cell deployment

## L8: Advanced Topics (Partial)
- HetNet capacity gain with small cell offloading
- Network slicing (documented, not fully implemented)

## L9: Research Frontiers (Partial)
- 6G cell-free massive MIMO (documented)
- O-RAN architecture (documented)
