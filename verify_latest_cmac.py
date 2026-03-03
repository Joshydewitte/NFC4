#!/usr/bin/env python3
"""
Verify latest CMAC calculation with ECB fix
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ESP32 CMAC Verification (Latest Test) ===\n")

# From latest log
session_mac_key = unhexlify("367FA77A7738BEDA481AA6D2497FDDF8")
mac_input = unhexlify("C40000D45571BE00ED78EF0AB9D0F659D3279B61D853864BCB6979CD3DE0C26CD4CDEB7919FB1E27")
logged_cmac_full = unhexlify("F934E96288DDD3CB51114A2E601BC0EE")
logged_cmac_t = unhexlify("3462DDCB112E1BEE")

print("MAC Input breakdown:")
print(f"  Cmd:     C4")
print(f"  CmdCtr:  0000")
print(f"  TI:      D45571BE")
print(f"  KeyNo:   00")
print(f"  EncData: ED78EF0AB9D0F659D3279B61D853864BCB6979CD3DE0C26CD4CDEB7919FB1E27")
print(f"\nSession MAC Key: {hexlify(session_mac_key).decode().upper()}")
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
print(f"Manual truncation: {hexlify(manual_trunc).decode().upper()}")
print(f"Matches logged:    {manual_trunc == logged_cmac_t}\n")

if calc_cmac_full == logged_cmac_full:
    print("✅ ✅ ✅ ESP32 CMAC is PERFECT! ✅ ✅ ✅")
    print("✅ CMAC algorithm correct (ECB mode)")
    print("✅ Truncation correct (even bytes)")
    print("✅ Field order correct (Cmd||CmdCtr||TI||KeyNo||EncData)")
    print("\n❓ But card STILL rejects with 0x911E...")
    print("\n🔍 This means the problem is NOT the CMAC!")
    print("\n⚠️ Possible issues:")
    print("   1. KeyVersion byte missing in encrypted data")
    print("   2. Encrypted data has wrong format")
    print("   3. IV calculation is wrong")
    print("   4. Card expects different padding")
    print("   5. New key is rejected by card (not crypto error)")
else:
    print("❌ ESP32 CMAC still doesn't match")
    diff = bytes(a^b for a,b in zip(calc_cmac_full, logged_cmac_full))
    print(f"Difference: {hexlify(diff).decode().upper()}")
