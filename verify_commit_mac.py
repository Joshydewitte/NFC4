#!/usr/bin/env python3
"""
Verify CommitTransaction CMAC calculation
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_odd(full_mac):
    """Truncate CMAC using odd indices (1,3,5,7,9,11,13,15)"""
    return bytes([full_mac[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

# From the log
session_mac_key = unhexlify("41FE24AA29DADA54136FCFA57EBB70DD")
ti = unhexlify("9364B942")
cmd_ctr = 1

# Build MAC input (7 bytes)
mac_input = bytearray()
mac_input.append(0xC7)  # Command
mac_input.append(cmd_ctr & 0xFF)  # CmdCtr LSB first
mac_input.append((cmd_ctr >> 8) & 0xFF)
mac_input.extend(ti)  # TI (4 bytes)

print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"TI: {hexlify(ti).decode().upper()}")
print(f"CmdCtr: {cmd_ctr}")
print()
print(f"MAC Input ({len(mac_input)} bytes):")
print(f"  {hexlify(mac_input).decode().upper()}")
print()

# Expected from log
expected_mac_input = "C701009364B942"
calculated_mac_input = hexlify(mac_input).decode().upper()

if calculated_mac_input == expected_mac_input:
    print("✅ MAC Input matches log")
else:
    print(f"❌ MAC Input mismatch!")
    print(f"   Expected: {expected_mac_input}")
    print(f"   Got:      {calculated_mac_input}")

print()

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()

print(f"CMAC Full: {hexlify(cmac_full).decode().upper()}")

# Truncate
cmac_t = truncate_cmac_odd(cmac_full)
print(f"CMAC Truncated: {hexlify(cmac_t).decode().upper()}")
print()

# Expected from log
expected_cmac = "B339AEF111C2F953"
calculated_cmac = hexlify(cmac_t).decode().upper()

if calculated_cmac == expected_cmac:
    print("✅ CMAC matches log - crypto is CORRECT!")
else:
    print(f"❌ CMAC mismatch!")
    print(f"   Expected: {expected_cmac}")
    print(f"   Got:      {calculated_cmac}")

print()
print("=" * 60)
print("CONCLUSION:")
print("=" * 60)

if calculated_cmac == expected_cmac:
    print("✅ All crypto calculations are CORRECT")
    print()
    print("🚨 PROBLEM: Card rejects valid CommitTransaction (SW=911C)")
    print()
    print("Possible causes:")
    print("1. ChangeKey was NOT actually accepted internally")
    print("2. Card requires additional command between ChangeKey and Commit")
    print("3. Timing issue - too fast between commands")
    print("4. Card firmware bug with EV2 authentication")
    print("5. Missing step in personalization flow")
else:
    print("❌ Crypto calculation is WRONG")
