# mini-beamforming-massive-mimo

Beamforming and Massive MIMO module covering antenna array theory,
precoding algorithms, DOA estimation, MIMO channel models and capacity,
and adaptive beamforming.

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Partial+ (3 applications: 5G NR, GPS anti-jam, mmWave hybrid)
- L8: Complete (5 advanced topics)
- L9: Partial (3 topics documented)
- Score: 16/18

## Line Count

- include/ (6 headers): 1616 lines
- src/ (6 C files): 3122 lines
- **Total (include/ + src/): 4738 lines** (threshold: 3000)

## Nine-Level Knowledge Coverage

| Level | Name | Status | Count |
|-------|------|--------|-------|
| L1 | Definitions | COMPLETE | 19 C types |
| L2 | Core Concepts | COMPLETE | 15 concepts with implementations |
| L3 | Math Structures | COMPLETE | 16 matrix/vector operations |
| L4 | Fundamental Laws | COMPLETE | 8 theorems implemented |
| L5 | Algorithms | COMPLETE | 25 algorithms with complexity |
| L6 | Canonical Problems | COMPLETE | 5 end-to-end examples |
| L7 | Applications | PARTIAL+ | 3 applications |
| L8 | Advanced Topics | COMPLETE | 5 topics |
| L9 | Research Frontiers | PARTIAL | 3 topics documented |

## Core Definitions

- Complex numbers, vectors, matrices for baseband signal processing
- ULA/UPA/UCA array geometries with steering vectors
- MIMO channel types (Rayleigh i.i.d., correlated, Rician, mmWave)
- Precoding configurations (MRT, ZF, MMSE, SLNR, BD, SVD, Hybrid)
- Adaptive beamformer states (LMS, NLMS, RLS, CMA, SMI, Kalman)
- DOA estimation results (MUSIC, ESPRIT, Capon, Bartlett)

## Core Theorems

1. **Shannon MIMO Capacity**: C = max log2 det(I + H Q H^H), Telatar (1999)
2. **Waterfilling Optimality**: P_i = (mu - 1/gamma_i)^+, Cover & Thomas (2006)
3. **Eckart-Young-Mirsky**: Optimal rank-k SVD approximation (1936)
4. **Nyquist Spatial Sampling**: d < lambda/2 avoids grating lobes
5. **Cramer-Rao Bound (DOA)**: CRLB ~ 1/(N*SNR*M^3), Stoica & Nehorai (1989)
6. **Channel Hardening**: var(||h||^2)/E[||h||^2]^2 -> 0 as M -> infinity
7. **Favorable Propagation**: h_i^H h_j / M -> 0 as M -> infinity
8. **Massive MIMO Asymptotic Sum-Rate**: R -> K log2(1+SNR*(M-K)/K)

## Core Algorithms

- **Precoding**: MRT, ZF, MMSE, SLNR, Block Diagonalization, SVD-based optimal
- **DOA Estimation**: MUSIC, Root-MUSIC, ESPRIT, Capon/MVDR, Bartlett
- **Adaptive BF**: LMS, NLMS, RLS, CMA, SMI, Kalman
- **Source Detection**: MDL, AIC
- **Power Allocation**: Waterfilling (bisection)
- **Matrix Decomp**: SVD (one-sided Jacobi), Eigenvalue (cyclic Jacobi), Pseudo-inverse
- **Covariance Est**: Sample, Forward-Backward, Spatial Smoothing

## Canonical Problems

1. ULA beam pattern computation (beamwidth, sidelobe level)
2. MUSIC DOA estimation for two closely-spaced sources
3. MU-MIMO sum-rate comparison (MRT vs ZF vs MMSE)
4. Massive MIMO asymptotic properties
5. Null-steering beamformer with multiple interferers

## 9-School Course Mapping

| School | Course | Topics |
|--------|--------|--------|
| MIT | 6.450, 6.630 | MIMO capacity, waterfilling, array theory |
| Stanford | EE359 | Beamforming, MIMO, massive MIMO |
| Berkeley | EE123, EE117 | MUSIC, ESPRIT, EM array processing |
| Illinois | ECE 459 | Spatial multiplexing, ZF/MMSE |
| Michigan | EECS 455 | Adaptive beamforming, LMS/RLS |
| Georgia Tech | ECE 6601 | MU-MIMO, BD, SLNR |
| TU Munich | HF Engineering | Array factor, grating lobes |
| ETH | 227-0436 | MIMO channel models, info theory |
| Tsinghua | Comm Principles | Beamforming fundamentals |

## Build and Test

```
make clean && make && make test
```

## File Structure

- include/ (6 headers): types, array, precoder, doa, mimo, adaptive
- src/ (6 implementations): types, array, precoder, doa, mimo, adaptive
- tests/ (1 test suite): 36 assert-based tests
- examples/ (4 examples): beam pattern, MUSIC DOA, MU-MIMO, massive MIMO
- docs/ (5 documents): knowledge-graph, coverage-report, gap-report, course-alignment, course-tree

## References

- Molisch (2011) Wireless Communications, Ch.20
- Tse & Viswanath (2005) Fundamentals of Wireless Communication
- Bjornson et al. (2017) Massive MIMO Networks
- Heath et al. (2016) Foundations of MIMO Communication
- Balanis (2016) Antenna Theory: Analysis and Design
- Van Trees (2002) Optimum Array Processing
- Schmidt (1986) MUSIC Algorithm, IEEE TAP
- Roy & Kailath (1989) ESPRIT, IEEE TASSP
- Widrow & Stearns (1985) Adaptive Signal Processing
- Haykin (2002) Adaptive Filter Theory
- Telatar (1999) MIMO Capacity, ETT
- Marzetta (2010) Massive MIMO, IEEE TWC
