#!/usr/bin/env python3
"""
Verify CMAC with fixed leftShift
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# Values from latest log (after leftShift fix)
session_mac_key = bytes.fromhex("96F97FD4457123A6AC43B5C5850823CD")
mac_input = bytes.fromhex("C40100F4FDAEE5006AC7C5415CA7E0D13C447F2A4B1655D612AA38CE68D43B9DB5163316C19FCD07")

print("=" * 60)
print("VERIFY CMAC AFTER LEFTSHIFT FIX")
print("=" * 60)
print(f"Session MAC Key: {session_mac_key.hex().upper()}")
print(f"MAC Input ({len(mac_input)} bytes):")
print(mac_input.hex().upper())
print()

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()

print(f"CMAC Full (Python):     {cmac_full.hex().upper()}")
print(f"CMAC Full (ESP32 log):  B5293E61B10DD1F50A643689581E81D4")
print()

if cmac_full.hex().upper() == "B5293E61B10DD1F50A643689581E81D4":
    print("✅ CMAC Full MATCHES!")
else:
    print("❌ CMAC Full MISMATCH")

# Truncate
cmac_t = bytes([cmac_full[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])
print()
print(f"CMAC Truncated (Python): {cmac_t.hex().upper()}")
print(f"CMAC Truncated (ESP32):  29610DF564891ED4")
print()

if cmac_t.hex().upper() == "29610DF564891ED4":
    print("✅✅✅ CMAC TRUNCATED MATCHES PERFECTLY!")
else:
    print("❌ CMAC Truncated MISMATCH")
