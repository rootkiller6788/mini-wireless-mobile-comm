# Gap Report -- mini-lora-nbiot

## Missing Items

### Medium Priority
1. LR-FHSS implementation (L9) -- LoRa 2.4 GHz modulation variant
2. NB-IoT Turbo decoder (L5) -- Full iterative turbo decoding
3. Channel estimation (L5) -- NRS-based channel estimation

### Low Priority
4. Satellite LoRa (L9) -- Doppler compensation for LEO
5. 6G RIS (L9) -- Reconfigurable Intelligent Surfaces
6. ML-based ADR (L8) -- ML for adaptive data rate

## Status
- [x] L1-L6 fully implemented with 6939 lines of C code
- [x] L7: 3 application profiles with encode/decode
- [x] L8: Multi-SF interference + capture effect + gateway model
- [ ] L8: Time-varying channel with Doppler
- [ ] L9: LR-FHSS documentation
