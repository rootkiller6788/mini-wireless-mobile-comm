# Knowledge Graph - mini-wireless-security

## L1: Definitions - COMPLETE (22 types)
All core security definitions implemented as C structs/enums across 6 headers:
aes_ctx_t, sha256_ctx_t, hmac_sha256_ctx_t, rc4_ctx_t, ccmp_ctx_t, gcmp_ctx_t,
eapol_header_t, eap_header_t, handshake_ctx_t, pmk_t, ptk_t, gtk_t,
key_mgmt_ctx_t, security_protocol_t, akm_suite_t, cipher_pairwise_t, sae_ctx_t,
wep_fms_state_t, pmkid_capture_t, krack_state_t, ids_state_t, wiretap_channel_t,
csi_sample_t, ris_config_t.  Lean: SecurityProtocol, AKMSuite, CipherSuite, HandshakeState.

## L2: Core Concepts - COMPLETE (20 concepts)
AES symmetric cipher, SHA-256 hash, HMAC auth, RC4 stream cipher, CCM/GCM authenticated encryption,
PBKDF2 key derivation, 4-way handshake, PMKID, WPA2 key hierarchy (PMK->PTK->KCK/KEK/TK),
WPA2 PRF, HKDF, security protocol negotiation, SAE Dragonfly, WEP FMS cryptanalysis,
dictionary attack, KRACK, wireless IDS, channel reciprocity key gen, AN precoding.

## L3: Math Structures - COMPLETE (11 structures)
GF(2^8) field (polynomial basis), GF(2^128) field (GCM), SPN (AES), Davies-Meyer compression,
Merkle-Damgard, WPA2 PRF iterative construction, mutual information / log formula,
ergodic capacity Monte Carlo, Rayleigh fading (Box-Muller), level-crossing quantization,
universal hashing (HMAC-based).

## L4: Fundamental Laws - COMPLETE (10 theorems)
1. Wyner Secrecy Capacity (C_s = max(log2(1+SNRm)-log2(1+SNRe), 0))
2. CCM Security Theorem (Jonsson 2002)
3. FMS Weak IV Statistical Theorem
4. PBKDF2 Work Factor (4096x for WPA2)
5. WPA3 No Silent Downgrade
6. Secrecy Capacity Non-Negative (C_s >= 0)
7. Handshake Progress Monotonic
8. WPA3 > WPA2 Security Hierarchy
9. Forward Secrecy (SAE ephemeral DH)
10. Channel Reciprocity (TDD)

## L5: Algorithms - COMPLETE (21 algorithms)
AES-128/256 enc/dec, AES key schedule, SHA-256, HMAC-SHA256, RC4 KSA+PRGA,
AES-CTR, AES-CBC-MAC, AES-CCM (CCMP), AES-GCM (GCMP), PBKDF2-HMAC-SHA256,
WPA2 PRF, HKDF extract+expand, 4-way handshake protocol, PMKID computation,
RSN IE build/parse, SAE commit/confirm, FMS WEP attack, PMKID dictionary attack,
channel key gen (quantize+reconcile+amplify), AN precoding (Gram-Schmidt),
RIS greedy phase optimization.

## L6: Canonical Problems - COMPLETE (5 examples)
example_wpa2_handshake.c, example_aes_ccmp.c, example_key_derivation.c,
example_secrecy_capacity.c, example_attack_demo.c.  All >30 lines with main+printf.

## L7: Applications - COMPLETE (4 domains)
WiFi security (iPhone, Android, IoT), Cellular (5G, SIM, AKA), Automotive (Detroit, ISO 21434),
Medical (NHS, ISO 27001).

## L8: Advanced Topics - COMPLETE (5 topics)
WPA3/SAE Dragonfly, AES-GCMP (Galois), MIMO artificial noise precoding,
KRACK nonce-reuse detection, SAE timing side-channel analysis.

## L9: Research Frontiers - PARTIAL (1 impl + 2 doc)
RIS-assisted PLS (implemented), Quantum-resistant wireless crypto (documented),
AI-based wireless IDS (documented).

## Score: 17/18
L1 Complete(2) + L2 Complete(2) + L3 Complete(2) + L4 Complete(2) + L5 Complete(2)
+ L6 Complete(2) + L7 Complete(2) + L8 Complete(2) + L9 Partial(1) = 17

## Verdict: COMPLETE (>16/18, L1-L8 all Complete, L4 not Missing)
