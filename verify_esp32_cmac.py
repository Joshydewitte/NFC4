#!/usr/bin/env python3
"""
Verify ESP32's CMAC calculation with actual log values
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ESP32 CMAC Verification ===\n")

# From latest log
session_mac_key = unhexlify("277E601AC845F96EB00C606249BCFAAD")
mac_input = unhexlify("C400002D7F1845001C9B16869FD2334677B1469A2DF02CBA7556CFD8B074B463600D2460F64EB414")
logged_cmac_full = unhexlify("55A25B8367F3E74A8EFD9C2C52AA2689")
logged_cmac_t = unhexlify("A283F34AFD2CAA89")

print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"Logged CMAC Full:{hexlify(logged_cmac_full).decode().upper()}")
print(f"Logged CMAC:     {hexlify(logged_cmac_t).decode().upper()}\n")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Calc CMAC Full:  {hexlify(calc_cmac_full).decode().upper()}")
print(f"Calc CMAC:       {hexlify(calc_cmac_t).decode().upper()}\n")

print(f"Full CMAC Match: {calc_cmac_full == logged_cmac_full}")
print(f"Trunc CMAC Match:{calc_cmac_t == logged_cmac_t}\n")

# Verify truncation
manual_trunc = truncate_cmac_even_bytes(logged_cmac_full)
print(f"Manual truncation of logged full CMAC: {hexlify(manual_trunc).decode().upper()}")
print(f"Matches logged CMAC: {manual_trunc == logged_cmac_t}\n")

if calc_cmac_full == logged_cmac_full:
    print("✅ ESP32 CMAC calculation is CORRECT!")
    print("✅ Truncation is CORRECT!")
    print("\n🔍 So why does the card reject with 0x911E?")
    print("\nPossible causes:")
    print("1. Command counter not incremented at right time")
    print("2. Card expects response MAC (not just command MAC)")
    print("3. APDU wrapping issue")
    print("4. Card state issue (authentication timeout?)")
else:
    print("❌ ESP32 CMAC calculation is WRONG!")
    print("The CMAC implementation has a bug.")
