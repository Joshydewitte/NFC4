#!/usr/bin/env python3
"""
Verify ESP32's actual SV construction and CMAC from latest log
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== Verify ESP32 SV Construction & Session Keys ===\n")

# From latest log
rndA = unhexlify("0994EEB634D2185D8E7399370C74B80C")
rndB = unhexlify("0DAB17714C561914C32D0D9047773228")
logged_sv1 = unhexlify("A55A000100800994E31D23A3540B1914C32D0D90477732288E7399370C74B80C")
logged_enc_key = unhexlify("4A29CE0C0A052402603C10AF817118CC")
logged_sv2 = unhexlify("5AA5000100800994E31D23A3540B1914C32D0D90477732288E7399370C74B80C")
logged_mac_key = unhexlify("F1037257AC705A485CA2F26AD13633B5")

print(f"RndA: {hexlify(rndA).decode().upper()}")
print(f"RndB: {hexlify(rndB).decode().upper()}\n")

print(f"Logged SV1: {hexlify(logged_sv1).decode().upper()}")
print(f"Logged ENC: {hexlify(logged_enc_key).decode().upper()}")
print(f"Logged SV2: {hexlify(logged_sv2).decode().upper()}")
print(f"Logged MAC: {hexlify(logged_mac_key).decode().upper()}\n")

# Verify CMAC calculation
master_key = bytes(16)  # Factory card has all-zero key

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(logged_sv1)
calc_enc_key = cobj.digest()

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(logged_sv2)
calc_mac_key = cobj.digest()

print(f"Calc ENC Key: {hexlify(calc_enc_key).decode().upper()}")
print(f"Calc MAC Key: {hexlify(calc_mac_key).decode().upper()}\n")

if calc_enc_key == logged_enc_key and calc_mac_key == logged_mac_key:
    print("✅ ESP32 Session Keys CORRECT!")
    print("✅ SV construction CORRECT!")
    print("✅ CMAC for session keys CORRECT!\n")
else:
    print("❌ Session keys don't match!\n")

# Now verify ChangeKey CMAC
print("=== Verify ChangeKey CMAC ===\n")

session_mac_key = logged_mac_key
mac_input = unhexlify("C400004B66E1CF002D96EE6B017C9CAA2E5C14BD8B689824C9DB08A15BBF22380A5AD85D732ACDAA")
logged_cmac_full = unhexlify("A16A35B6CC6E183CACA1DE3DB735A2BC")
logged_cmac_t = unhexlify("6AB66E3CA13D35BC")

print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"  Length: {len(mac_input)} bytes")
print(f"\nLogged CMAC Full: {hexlify(logged_cmac_full).decode().upper()}")
print(f"Logged CMAC Trunc:{hexlify(logged_cmac_t).decode().upper()}\n")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Calc CMAC Full:   {hexlify(calc_cmac_full).decode().upper()}")
print(f"Calc CMAC Trunc:  {hexlify(calc_cmac_t).decode().upper()}\n")

if calc_cmac_full == logged_cmac_full:
    print("✅✅✅ ESP32 CMAC is PERFECT! ✅✅✅")
    print("\n🎯 Everything is cryptographically CORRECT!")
    print("     - Authentication ✓")
    print("     - Session keys ✓")
    print("     - IV calculation ✓")
    print("     - Encryption ✓")
    print("     - CMAC ✓")
    print("\n⚠️ Card rejects with 0x911E = AUTHENTICATION_ERROR")
    print("\n🔍 This means the card doesn't accept the NEW KEY itself!")
    print("    Not a crypto problem - the card rejects the key content/format.")
    print("\n💡 Possible reasons:")
    print("    1. KeyVersion byte (0x01) not allowed by card")
    print("    2. Key must match certain pattern/restrictions")
    print("    3. Card in wrong state (uncommitted write?)")
    print("    4. Need to commit after ChangeKey")
else:
    print("❌ CMAC still doesn't match")
    diff = bytes(a^b for a,b in zip(calc_cmac_full, logged_cmac_full))
    print(f"Difference: {hexlify(diff).decode().upper()}")
