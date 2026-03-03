#!/usr/bin/env python3
"""
Verify ESP32's CMAC calculation after ECB fix
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ESP32 CMAC Verification (After ECB Fix) ===\n")

# From latest log
session_mac_key = unhexlify("F377F93B9C67A96472CDEAA21C185814")
mac_input = unhexlify("C40000A8FC8F970038BCDE3B06742D9D6675D1D16CC34F5F14DF84B78CD19092DE7C58563DD8994F")
logged_cmac_full = unhexlify("3521CF61B50B72C69BA46C5360145478")
logged_cmac_t = unhexlify("21610BC6A4531478")

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

# Manual truncation check
manual_trunc = truncate_cmac_even_bytes(logged_cmac_full)
print(f"Manual truncation of logged full CMAC: {hexlify(manual_trunc).decode().upper()}")
print(f"Matches logged CMAC: {manual_trunc == logged_cmac_t}\n")

if calc_cmac_full == logged_cmac_full:
    print("✅ ESP32 CMAC calculation is CORRECT!")
    print("✅ Truncation is CORRECT!")
    print("\n❓ But the card still rejects with 0x911E...")
    print("\n🔍 Possible remaining issues:")
    print("1. KeyNo field missing in MAC input")
    print("2. Command format/APDU wrapping issue")
    print("3. Card expects different key version")
    print("4. Authentication session timed out")
else:
    print("❌ ESP32 CMAC still incorrect")
    print(f"Difference: {hexlify(bytes(a^b for a,b in zip(calc_cmac_full, logged_cmac_full))).decode().upper()}")
