# mini-5g-nr-phy — 5G NR Physical Layer

> Implementation of 3GPP TS 38.211/212/213/214 PHY layer in C + Lean 4

## Module Status: COMPLETE ✅

- **L1 Definitions**: Complete (20 structs/typedefs)
- **L2 Core Concepts**: Complete (15 concepts)
- **L3 Math Structures**: Complete (10 structures)
- **L4 Fundamental Laws**: Complete (9 theorems with Lean proofs)
- **L5 Algorithms**: Complete (25 algorithms)
- **L6 Canonical Problems**: Complete (7 problems)
- **L7 Applications**: Partial+ (5 applications)
- **L8 Advanced Topics**: Partial+ (5 topics)
- **L9 Research Frontiers**: Partial (documented)

## Line Count

| Category | Lines |
|----------|-------|
| Headers (include/ ) | ~2040 |
| Sources (src/ ) | ~4315 |
| Lean (src/.lean) | ~300 |
| **Total** | **~6655** |

## Core Definitions

- Numerology (mu, SCS, slots, symbols): `nr_numerology_t`
- Frame/Slot/Symbol indexing: `nr_re_index_t`
- Bandwidth Part: `nr_bwp_config_t`
- CORESET/Search Space: `nr_coreset_config_t`, `nr_search_space_t`
- Physical channel types: `nr_chan_type_t`
- Modulation schemes: `nr_mod_scheme_t` (BPSK through 1024QAM)
- Cell identity: `nr_phy_cell_id_t`
- Channel models: TDL-A through TDL-E (3GPP TR 38.901)

## Core Theorems (with formulas)

1. **OFDM Orthogonality**: Delta_f = 1/T_u ensures zero ICI
2. **Nyquist Sampling**: N_FFT >= N_RB * 12 to avoid aliasing
3. **Shannon-Hartley**: C = B * log2(1 + SNR)
4. **MIMO Capacity (Telatar 1999)**: C = log2 det(I + (rho/N_t)HH^H)
5. **Friis Transmission**: PL_free(d) = 20*log10(4*pi*d/lambda)
6. **Doppler Shift**: f_d = v * f_c / c
7. **Water-filling**: P_i = (mu - sigma^2/lambda_i)^+
8. **Polar Code Capacity**: Achieves symmetric capacity as N → infinity (Arikan 2009)
9. **GF(2) Field Axioms**: xor/add + and/mul form GF(2)

## Core Algorithms

- Radix-2 DIT FFT/IFFT (Cooley-Tukey 1965)
- CP-OFDM modulation/demodulation per 3GPP TS 38.211 5.3.1
- DFT-s-OFDM (SC-FDMA) for uplink
- LDPC encoding (QC-LDPC with lifting)
- Min-sum belief propagation LDPC decoding
- Polar encoding (Arikan kernel, sub-block interleaver)
- SC/SCL Polar decoding
- LS & MMSE DMRS-based channel estimation
- TDL channel generation (Jakes Doppler spectrum)
- Path loss models: Free-space, UMa, UMi, RMa, Indoor
- Type I codebook precoding (3GPP TS 38.214)
- ZF/MMSE/MMSE-SIC MIMO detection
- Jacobi SVD for MIMO precoding
- PSS time-domain detection (m-sequence correlation)
- SSS frequency-domain detection (Gold sequence correlation)
- PBCH DMRS-based SSB index detection
- PDCCH candidate hashing (3GPP TS 38.213 10.1)
- DCI CRC attachment with RNTI scrambling
- PDSCH scrambling (Gold sequence, cell-specific)
- QAM modulation/demodulation (QPSK/16QAM/64QAM/256QAM)

## Classic Problems Solved

1. **Full cell search**: PSS → SSS → PBCH DMRS → MIB
2. **PDSCH TX/RX chain**: TB → CRC → LDPC → RM → Scramble → Modulate → Layer map → Precoding
3. **LDPC encode/decode** with rate matching and incremental redundancy HARQ
4. **OFDM TX/RX through TDL channel** with EVM measurement
5. **5G downlink processing**: Carrier config → BWP → DCI → PDSCH → MIMO → Link budget
6. **PDCCH blind decoding** over multiple aggregation levels
7. **SSB beam sweeping** with SSB index detection

## Course Mapping

| School | Course | Chapters |
|--------|--------|----------|
| Stanford | EE359 Wireless | 5G NR, OFDM, MIMO |
| MIT | 6.450 Digital Comm | OFDM, MIMO, LDPC/Polar |
| Berkeley | EE123 DSP | FFT/OFDM, Channel Est. |
| TU Munich | Communications | 3GPP, 5G PHY |
| ETH | 227-0436 Comm | OFDM, MIMO, Coding |
| Michigan | EECS 455 Comm | Wireless PHY |
| Illinois | ECE 459 Comm | 5G NR |
| Georgia Tech | ECE 6601 Comm | 5G System Design |
| Tsinghua | Communications | 5G NR, RAN1 |

## Build

```bash
make          # Build library
make test     # Build and run all tests
make examples # Build all examples
make check    # Clean build + test + line count
make count    # Line count only
```

## References

- 3GPP TS 38.211 v17.0.0: Physical channels and modulation
- 3GPP TS 38.212 v17.0.0: Multiplexing and channel coding
- 3GPP TS 38.213 v17.0.0: Physical layer procedures for control
- 3GPP TS 38.214 v17.0.0: Physical layer procedures for data
- 3GPP TR 38.901 v17.0.0: Study on channel model for frequencies
- Dahlman, Parkvall & Skold (2020): "5G NR" 2nd ed., Academic Press
- Zaidi et al. (2018): "5G Physical Layer", Academic Press
- Arikan (2009): "Channel Polarization", IEEE Trans. IT
- Telatar (1999): "Capacity of Multi-antenna Gaussian Channels"
