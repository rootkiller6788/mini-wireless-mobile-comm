#!/usr/bin/env python3
"""
security_demo.py — Wireless Security Visualization Demo
Visualizes secrecy capacity vs SNR and WPA key hierarchy.
"""
import math
import sys

def secrecy_capacity(snr_main_db, snr_eve_db):
    gamma_m = 10 ** (snr_main_db / 10)
    gamma_e = 10 ** (snr_eve_db / 10)
    cs = math.log2(1 + gamma_m) - math.log2(1 + gamma_e)
    return max(0, cs)

def main():
    print("Wireless Secrecy Capacity — SNR Sweep")
    print("=" * 50)
    print(f"{'SNR_Bob(dB)':>12} {'SNR_Eve(dB)':>12} {'C_s(bps/Hz)':>14}")
    print("-" * 40)
    for snr_bob in range(-5, 31, 5):
        for snr_eve in range(-5, 21, 5):
            cs = secrecy_capacity(snr_bob, snr_eve)
            marker = " *" if cs > 1.0 else "  "
            print(f"{snr_bob:>12} {snr_eve:>12} {cs:>14.4f}{marker}")

    print()
    print("WPA Security Level Comparison")
    print("=" * 50)
    protocols = [
        ("Open", 0, "No encryption"),
        ("WEP", 1, "RC4, broken (FMS 2001)"),
        ("WPA", 2, "TKIP, deprecated"),
        ("WPA2", 3, "AES-CCMP, current"),
        ("WPA3", 4, "AES-GCMP + SAE"),
        ("WPA3-Enterprise", 5, "192-bit + 802.1X"),
    ]
    print(f"{'Protocol':<20} {'Level':>6}  {'Description'}")
    print("-" * 60)
    for name, level, desc in protocols:
        bar = "#" * level + "-" * (5 - level)
        print(f"{name:<20} [{bar}]  {desc}")

    print()
    print("Knowledge: L4 (Wyner theorem), L6 (WPA2/WPA3 canonical problems)")

if __name__ == "__main__":
    main()
