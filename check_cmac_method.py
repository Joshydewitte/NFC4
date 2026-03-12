#!/usr/bin/env python3
"""
Verifieer CMAC berekening uit Test 1
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES

print("="*80)
print("CMAC VERIFICATIE - TEST 1")
print("="*80)

# Test 1 data
SesAuthMACKey = bytes.fromhex("F18A26CEEF95B889887A9C100E1B5F68")
MAC_Input = bytes.fromhex("C4000074EBB6D3004DEF25FD976900D6703CABAA3BA77A8CA403F532DC1FE28FDBAE18064447C99B")

print(f"\nSesAuthMACKey: {SesAuthMACKey.hex().upper()}")
print(f"MAC Input:     {MAC_Input.hex().upper()}")
print(f"Length:        {len(MAC_Input)} bytes")

# Calculate CMAC Full (16 bytes)
c = CMAC.new(SesAuthMACKey, ciphermod=AES)
c.update(MAC_Input)
cmac_full = c.digest()

print(f"\n[PYTHON CMAC]")
print(f"CMAC Full (16 bytes): {cmac_full.hex().upper()}")

# Compare with ESP32
esp32_cmac_full = "5974209704C0509ACE2F0557D8E242FD"
print(f"\n[ESP32 OUTPUT]")
print(f"CMAC Full (16 bytes): {esp32_cmac_full}")

if cmac_full.hex().upper() == esp32_cmac_full:
    print(f"✅ CMAC Full is CORRECT!")
else:
    print(f"❌ CMAC Full MISMATCH!")
    print(f"   Python:  {cmac_full.hex().upper()}")
    print(f"   ESP32:   {esp32_cmac_full}")

# Test different truncation methods
print(f"\n" + "="*80)
print(f"TRUNCATION METHODS")
print(f"="*80)

# Method 1: First 8 bytes (standard)
truncated_first8 = cmac_full[:8]
print(f"\nMethod 1: First 8 bytes (standard)")
print(f"  {truncated_first8.hex().upper()}")

# Method 2: Odd indices (even-numbered bytes per NTAG424)
truncated_odd = bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                       cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])
print(f"\nMethod 2: Odd indices (bytes 1,3,5,7,9,11,13,15)")
print(f"  {truncated_odd.hex().upper()}")

# Method 3: Even indices
truncated_even = bytes([cmac_full[0], cmac_full[2], cmac_full[4], cmac_full[6],
                        cmac_full[8], cmac_full[10], cmac_full[12], cmac_full[14]])
print(f"\nMethod 3: Even indices (bytes 0,2,4,6,8,10,12,14)")
print(f"  {truncated_even.hex().upper()}")

# Compare with ESP32
esp32_cmac = "7497C09A2F57E2FD"
print(f"\n[ESP32 OUTPUT]")
print(f"CMAC (8 bytes): {esp32_cmac}")

print(f"\n[COMPARISON]")
if truncated_first8.hex().upper() == esp32_cmac:
    print(f"✅ Method 1 (first 8) MATCHES ESP32")
if truncated_odd.hex().upper() == esp32_cmac:
    print(f"✅ Method 2 (odd indices) MATCHES ESP32")
if truncated_even.hex().upper() == esp32_cmac:
    print(f"✅ Method 3 (even indices) MATCHES ESP32")

# Check what AN12196 says
print(f"\n" + "="*80)
print(f"AN12196 SPECIFICATION")
print(f"="*80)
print(f"\nAN12196 Section 9.1.9 states:")
print(f"  'MACt = Truncated MAC (typically first 8 bytes)'")
print(f"\nBUT some NXP specs use 'even-numbered bytes' truncation")
print(f"(bytes at positions 2,4,6,8,10,12,14,16 = indices 1,3,5,7,9,11,13,15)")
print(f"\nFor AES-CMAC, the STANDARD truncation is FIRST N bytes.")
print(f"\nFor NTAG424 DNA, we need to check which method the CARD expects!")

print(f"\n" + "="*80)
print(f"PROBLEM DIAGNOSIS")
print(f"="*80)
print(f"\nIf ESP32 uses Method 2 but card expects Method 1:")
print(f"  → ChangeKey APDU has wrong MAC")
print(f"  → Card might accept it but NOT write the key")
print(f"  → Or card returns SW=9100 even though MAC is wrong")
print(f"\nSOLUTION: Change truncation to first 8 bytes (Method 1)")

print(f"\n" + "="*80)
