#!/usr/bin/env python3
"""5G NR PHY Visualization Demo"""
import math, sys

def nr_numerology(mu):
    scs = 15 * (2 ** mu)
    slots = 2 ** mu
    sym_per_slot = 14
    t_sym = 1.0 / (scs * 1000)
    cp_nominal = 4.69e-6 if mu == 0 else 2.34e-6
    return {"mu": mu, "scs_khz": scs, "slots_per_subframe": slots,
            "symbols_per_slot": sym_per_slot,
            "symbol_duration_us": t_sym * 1e6,
            "nominal_cp_us": cp_nominal * 1e6}

def mcs_table(table_id=1):
    entries = []
    if table_id == 1:
        data = [(2,120/1024),(2,157/1024),(2,193/1024),(2,251/1024),
                (2,308/1024),(2,379/1024),(2,449/1024),(2,526/1024),
                (2,602/1024),(2,679/1024),(4,340/1024),(4,378/1024)]
    else:
        data = [(2,120/1024),(2,193/1024),(2,308/1024),(2,449/1024)]
    for i, (qm, rate) in enumerate(data):
        se = qm * rate
        entries.append({"mcs": i, "Qm": qm, "code_rate": rate, "SE_bps_per_hz": se})
    return entries

def pathloss_uma(d_m, fc_ghz=3.5, is_los=True):
    if is_los:
        if d_m < 5000:
            return 28.0 + 22 * math.log10(d_m) + 20 * math.log10(fc_ghz)
    return 13.54 + 39.08 * math.log10(d_m) + 20 * math.log10(fc_ghz)

def shannon_capacity(bw_hz, snr_db):
    snr_lin = 10 ** (snr_db / 10)
    return bw_hz * math.log2(1 + snr_lin)

def main():
    print("=" * 60)
    print("  5G NR PHY — Numerology & Link Budget Demo")
    print("=" * 60)
    print()
    print("--- Numerology ---")
    for mu in range(5):
        n = nr_numerology(mu)
        print(f"  mu={mu}: SCS={n['scs_khz']:6.0f} kHz, "
              f"{n['slots_per_subframe']:2d} slots/subframe, "
              f"{n['symbol_duration_us']:6.1f} us/sym")
    print()
    print("--- MCS Table (64QAM) ---")
    for e in mcs_table(1)[:8]:
        print(f"  MCS={e['mcs']:2d}: Qm={e['Qm']}, "
              f"rate={e['code_rate']:.3f}, SE={e['SE_bps_per_hz']:.3f} bps/Hz")
    print()
    print("--- Path Loss (UMa LOS, 3.5 GHz) ---")
    for d in [50, 100, 200, 500, 1000]:
        pl = pathloss_uma(d, 3.5, True)
        snr_100mhz = 43 - pl + 15 - (-174 + 10*math.log10(100e6) + 7)
        cap = shannon_capacity(100e6, snr_100mhz)
        print(f"  d={d:4d}m: PL={pl:.1f} dB, SNR={snr_100mhz:.1f} dB, "
              f"C={cap/1e9:.2f} Gbps")
    print()
    print("--- MIMO Capacity (Nt=4, Nr=4, i.i.d. Rayleigh) ---")
    for snr_db in range(0, 31, 5):
        snr_lin = 10 ** (snr_db/10)
        cap_siso = math.log2(1 + snr_lin)
        cap_mimo_4x4 = 4 * math.log2(1 + snr_lin)
        print(f"  SNR={snr_db:2d} dB: SISO={cap_siso:.2f}, "
              f"4x4={cap_mimo_4x4:.2f} bps/Hz")
    print()
    print("=" * 60)
    print("  Done.")
    print("=" * 60)

if __name__ == "__main__":
    main()
