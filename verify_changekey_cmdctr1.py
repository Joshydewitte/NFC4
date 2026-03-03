#!/usr/bin/env python3
"""
Verify ChangeKey calculation with CmdCtr=1
Using values from the latest log
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# Values from log
session_enc_key = bytes.fromhex("0864976A439AFAD9AD33985D2A2D0E36")
session_mac_key = bytes.fromhex("540078BAA9532DBEF16FB6E61B5D2691")
ti = bytes.fromhex("631188A6")
cmd_ctr = 1  # First post-auth command
key_no = 0x00
new_key = bytes.fromhex("10532710706128B62A990EE06ECC86A6")
key_version = 0x01

print("=" * 60)
print("CHANGEKEY VERIFICATION - CmdCtr=1")
print("=" * 60)
print(f"Session ENC Key: {session_enc_key.hex().upper()}")
print(f"Session MAC Key: {session_mac_key.hex().upper()}")
print(f"TI: {ti.hex().upper()}")
print(f"CmdCtr: {cmd_ctr}")
print(f"KeyNo: {key_no:02X}")
print(f"New Key: {new_key.hex().upper()}")
print()

# Step 1: Prepare plain data (Case 2: NewKey || KeyVer || Padding)
plain_data = bytearray(32)
plain_data[0:16] = new_key
plain_data[16] = key_version
plain_data[17] = 0x80  # Padding marker
# Rest is zeros

print(f"Plaintext: {plain_data.hex().upper()}")
print()

# Step 2: Calculate IV
# IV = E(SesAuthENCKey, A55A || TI || CmdCtr || zeros)
iv_input = bytearray(16)
iv_input[0:2] = bytes.fromhex("A55A")
iv_input[2:6] = ti
iv_input[6] = cmd_ctr & 0xFF  # LSB first
iv_input[7] = (cmd_ctr >> 8) & 0xFF
# Rest is zeros

print(f"IV Input: {iv_input.hex().upper()}")

cipher = AES.new(session_enc_key, AES.MODE_ECB)
iv = cipher.encrypt(bytes(iv_input))
print(f"IV: {iv.hex().upper()}")
print()

# Step 3: Encrypt command data
cipher = AES.new(session_enc_key, AES.MODE_CBC, iv)
encrypted_data = cipher.encrypt(bytes(plain_data))
print(f"Encrypted Data: {encrypted_data.hex().upper()}")
print()

# Step 4: Calculate CMAC
# MAC input = Cmd || CmdCtr || TI || KeyNo || EncData
mac_input = bytearray()
mac_input.append(0xC4)  # CMD_CHANGE_KEY
mac_input.append(cmd_ctr & 0xFF)  # CmdCtr LSB first
mac_input.append((cmd_ctr >> 8) & 0xFF)
mac_input.extend(ti)
mac_input.append(key_no)
mac_input.extend(encrypted_data)

print(f"MAC Input ({len(mac_input)} bytes): {mac_input.hex().upper()}")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input))
cmac_full = cobj.digest()

# Truncate to even-indexed bytes (indices 1,3,5,7,9,11,13,15)
cmac_t = bytes([cmac_full[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

print(f"CMAC Full: {cmac_full.hex().upper()}")
print(f"CMAC Truncated: {cmac_t.hex().upper()}")
print()

# Build complete APDU
apdu = bytearray()
apdu.extend(bytes.fromhex("90C4000029"))  # CLA INS P1 P2 Lc
apdu.append(key_no)
apdu.extend(encrypted_data)
apdu.extend(cmac_t)
apdu.append(0x00)  # Le

print("=" * 60)
print("COMPLETE APDU:")
print(f"{apdu.hex().upper()}")
print("=" * 60)
print()

# Compare with log
log_apdu = "90C4000029007C266BB2D99417CEBEE1CA41711C622D0E6EE90634238A0F3B72F405E6DEBFBBCA1AF8878AA4A39200"
print(f"Log APDU:  {log_apdu.upper()}")
print(f"Calc APDU: {apdu.hex().upper()}")
print()

if apdu.hex().upper() == log_apdu.upper():
    print("✅✅✅ APDU MATCHES PERFECTLY!")
else:
    print("❌ APDU MISMATCH")
    
    # Find difference
    for i in range(min(len(apdu), len(bytes.fromhex(log_apdu)))):
        if apdu[i] != bytes.fromhex(log_apdu)[i]:
            print(f"First difference at byte {i}: calc={apdu[i]:02X} vs log={bytes.fromhex(log_apdu)[i]:02X}")
            break
