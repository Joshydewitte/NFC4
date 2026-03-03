#!/usr/bin/env python3
"""
Verify EXACT values from latest log
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# Exact values from latest log
session_enc_key = bytes.fromhex("9E711C4CBA851D357C34743736EDEB73")
session_mac_key = bytes.fromhex("868F01B3D9BBC1B829AB330A1E5120D9")
ti = bytes.fromhex("57CBF657")
cmd_ctr = 1
encrypted_data = bytes.fromhex("33E6224C2B562F65B7005CF5965AEB8B4E389639764AA29CE02DDF8A2FDD1B8D")

print("=" * 60)
print("VERIFY EXACT LOG VALUES")
print("=" * 60)
print(f"Session MAC Key: {session_mac_key.hex().upper()}")
print(f"TI: {ti.hex().upper()}")
print(f"CmdCtr: {cmd_ctr}")
print(f"Encrypted Data: {encrypted_data.hex().upper()}")
print()

# MAC input from log
mac_input_from_log = bytes.fromhex("C4010057CBF6570033E6224C2B562F65B7005CF5965AEB8B4E389639764AA29CE02DDF8A2FDD1B8D")

print(f"MAC Input from log ({len(mac_input_from_log)} bytes):")
print(mac_input_from_log.hex().upper())
print()

# Reconstruct MAC input
mac_input_calc = bytearray()
mac_input_calc.append(0xC4)  # CMD_CHANGE_KEY
mac_input_calc.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input_calc.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input_calc.extend(ti)  # TI
mac_input_calc.append(0x00)  # KeyNo
mac_input_calc.extend(encrypted_data)  # EncData

print(f"MAC Input calculated ({len(mac_input_calc)} bytes):")
print(mac_input_calc.hex().upper())
print()

if mac_input_from_log == bytes(mac_input_calc):
    print("✅ MAC Input matches log")
else:
    print("❌ MAC Input MISMATCH")
    print(f"Expected: {mac_input_from_log.hex().upper()}")
    print(f"Got:      {mac_input_calc.hex().upper()}")
print()

# Calculate CMAC
print("Calculating CMAC with pycryptodome...")
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input_from_log)
cmac_full = cobj.digest()

print(f"CMAC Full (Python):     {cmac_full.hex().upper()}")
print(f"CMAC Full (ESP32 log):  BB7ECC8B7AE03294E898F4D6B13A8D8E")
print()

if cmac_full.hex().upper() == "BB7ECC8B7AE03294E898F4D6B13A8D8E":
    print("✅ CMAC matches ESP32!")
else:
    print("❌ CMAC MISMATCH")
    
# Truncate
cmac_t = bytes([cmac_full[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])
print(f"CMAC Truncated (Python): {cmac_t.hex().upper()}")
print(f"CMAC Truncated (ESP32):  7E8BE09498D63A8E")
