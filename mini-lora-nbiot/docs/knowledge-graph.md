# Knowledge Graph -- mini-lora-nbiot

## L1: Definitions (Complete)
- Spreading Factor (SF), Bandwidth (BW), Coding Rate (CR)
- Symbol Rate, Chip Rate, Preamble, Sync Word
- NB-IoT: NPSS, NSSS, NPBCH, NPDCCH, NPDSCH, NPUSCH, NPRACH
- Resource Unit, Subcarrier Spacing, CE Level, MCL, eDRX, PSM
- ISM bands, EIRP, RSSI, SNR, Link Margin, Path Loss Exponent

## L2: Core Concepts (Complete)
- Chirp Spread Spectrum modulation principle
- Orthogonal Spreading Factors
- LoRaWAN Class A/B/C device operation
- NB-IoT OFDM/SC-FDMA frame structure
- PSM vs eDRX power saving
- Coverage Enhancement levels
- Fading channel types
- LPWAN taxonomy: UNB vs CSS vs OFDMA

## L3: Math Structures (Complete)
- Complex baseband chirp: s(t) = exp(j*pi*mu*t^2)
- Chirp rate: mu = BW^2 / 2^SF
- Instantaneous frequency wrapping (sawtooth)
- Zadoff-Chu CAZAC sequences
- OFDM/SC-FDMA: DFT-precoded OFDM
- Rayleigh/Rician fading statistics
- Log-distance path loss model
- Okumura-Hata model
- AES-128 CMAC for LoRaWAN MIC

## L4: Fundamental Laws (Complete)
- Shannon-Hartley: C = BW*log2(1+SNR)
- Friis equation: PL = 20*log10(4*pi*d/lambda)
- Link budget: P_rx = P_tx + G_tx + G_rx - PL - L_misc
- CSS Processing gain: G_p = 10*log10(2^SF)
- Receiver sensitivity: S = -174 + 10*log10(BW) + NF + SNR_min
- MCL: MCL = P_tx - S_rx
- Energy per bit: E_b = P_tx / R_b
- Duty cycle limits (ETSI EN 300 220)

## L5: Algorithms (Complete)
- FFT-based dechirping demodulation
- Hamming(7,4) FEC with single-error correction
- CRC-16-CCITT
- Data whitening (9-bit LFSR)
- Gray indexing
- Diagonal interleaving
- NPSS auto-correlation detection
- NSSS PCI decoder (504 hypotheses)
- TBCC rate-1/3 encoder
- ADR algorithm
- Jakes sum-of-sinusoids fading
- Box-Muller AWGN
- LoRaWAN frame construction

## L6: Canonical Problems (Complete)
- LoRa packet time-on-air (Semtech formula)
- NB-IoT cell search (NPSS->NSSS->NPBCH->SIB1)
- Link budget analysis and max range
- Battery life estimation
- LoRaWAN OTAA join procedure
- Duty cycle compliance
- Energy-optimal SF selection

## L7: Applications (Partial+)
- Smart metering (water/gas/electricity)
- Asset tracking with LoRaWAN TDOA/RSSI
- Agriculture sensor networks
- LPWAN technology comparison

## L8: Advanced Topics (Partial+)
- Multi-SF interference analysis
- LoRa capture effect
- Gateway throughput model
- NB-IoT inter-cell interference SINR
- CE level selection algorithm

## L9: Research Frontiers (Partial)
- LR-FHSS
- Satellite LoRa
- 5G NR RedCap vs NB-IoT evolution
