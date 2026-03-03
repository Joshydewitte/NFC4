#!/usr/bin/env python3
"""Verify CMAC output from current ESP32 test run"""

from cryptography.hazmat.primitives.cmac import CMAC
from cryptography.hazmat.primitives.ciphers import algorithms
from cryptography.hazmat.backends import default_backend

# From current test output
sv1_hex = "A55A00010080CCD2A3BAB1ED0AF690532AE92D4EE106474C57B2E670BDD148E2"
esp32_enc_key = "EE66C7AC2E21F706E7970995A146322A"

sv2_hex = "5AA500010080CCD2A3BAB1ED0AF690532AE92D4EE106474C57B2E670BDD148E2"
esp32_mac_key = "C891B5130FCB74736480A2E5E5D6DBA7"

# Factory default key (all zeros)
key = bytes(16)

print("="*60)
print("VERIFYING CURRENT ESP32 CMAC OUTPUT")
print("="*60)

# Convert to bytes
sv1 = bytes.fromhex(sv1_hex)
sv2 = bytes.fromhex(sv2_hex)

print(f"\nSV1 ({len(sv1)} bytes): {sv1.hex().upper()}")
print(f"SV2 ({len(sv2)} bytes): {sv2.hex().upper()}")
print(f"Key: {key.hex().upper()}")

# Calculate CMAC using cryptography library (which follows NIST SP 800-38B)
cmac = CMAC(algorithms.AES(key), backend=default_backend())
cmac.update(sv1)
cmac_sv1 = cmac.finalize()

cmac = CMAC(algorithms.AES(key), backend=default_backend())
cmac.update(sv2)
cmac_sv2 = cmac.finalize()

print("\n" + "="*60)
print("PYTHON CMAC (CORRECT):")
print("="*60)
print(f"CMAC(SV1) = {cmac_sv1.hex().upper()}")
print(f"CMAC(SV2) = {cmac_sv2.hex().upper()}")

print("\n" + "="*60)
print("ESP32 CMAC (FROM LOG):")
print("="*60)
print(f"ENC Key   = {esp32_enc_key}")
print(f"MAC Key   = {esp32_mac_key}")

print("\n" + "="*60)
print("COMPARISON:")
print("="*60)

if cmac_sv1.hex().upper() == esp32_enc_key.upper():
    print("✅ ENC Key MATCHES! CMAC is now correct!")
else:
    print(f"❌ ENC Key MISMATCH!")
    print(f"   Expected: {cmac_sv1.hex().upper()}")
    print(f"   Got:      {esp32_enc_key.upper()}")
    print(f"   Diff at byte: ", end="")
    for i in range(min(len(cmac_sv1.hex()), len(esp32_enc_key))):
        if i % 2 == 0 and cmac_sv1.hex().upper()[i:i+2] != esp32_enc_key.upper()[i:i+2]:
            print(f"{i//2} ", end="")
    print()

if cmac_sv2.hex().upper() == esp32_mac_key.upper():
    print("✅ MAC Key MATCHES! CMAC is now correct!")
else:
    print(f"❌ MAC Key MISMATCH!")
    print(f"   Expected: {cmac_sv2.hex().upper()}")
    print(f"   Got:      {esp32_mac_key.upper()}")

print("\n" + "="*60)
print("CONCLUSION:")
print("="*60)
if cmac_sv1.hex().upper() != esp32_enc_key.upper():
    print("❌ CMAC is STILL BROKEN after static function rewrite")
    print("   Need to investigate the CMAC implementation further")
else:
    print("✅ CMAC IS NOW FIXED!")
    print("   The issue was the lambda functions")
    print("   Problem must be elsewhere (MAC input format?)")
