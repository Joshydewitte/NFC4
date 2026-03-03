#!/usr/bin/env python3
"""
Verify ChangeKey IV calculation and encryption
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from binascii import hexlify, unhexlify

# From log
session_enc_key = unhexlify("001FCB67A6B13BD3B76905A1CB07C44D")
session_mac_key = unhexlify("92E63C9F131769B6A397B41B1541DE56")
current_iv = unhexlify("1A18813ECE924F02BB5553D00F71A4C5")
ti = unhexlify("24F45635")
cmd_ctr = 0
plain_data = unhexlify("124EC5CEE70A418133F244266C9EF2A801800000000000000000000000000000")

print("=== ChangeKey IV and Encryption Verification ===\n")

print("Input parameters:")
print(f"Session ENC Key: {hexlify(session_enc_key).decode().upper()}")
print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"Current IV:      {hexlify(current_iv).decode().upper()}")
print(f"TI:              {hexlify(ti).decode().upper()}")
print(f"CmdCtr:          {cmd_ctr}")
print(f"Plain Data:      {hexlify(plain_data).decode().upper()}\n")

# Step 1: Calculate IV according to AN12196 Section 9.1.4
# IV = E(SesAuthENCKey, A55A || TI || CmdCtr || 0000000000000000)
print("Step 1: Calculate IV for encryption")
iv_input = bytearray(16)
iv_input[0] = 0xA5
iv_input[1] = 0x5A
iv_input[2:6] = ti
iv_input[6] = cmd_ctr & 0xFF        # CmdCtr LSB first
iv_input[7] = (cmd_ctr >> 8) & 0xFF
# Rest is zero padding (already initialized)

print(f"IV Input:        {hexlify(iv_input).decode().upper()}")

# Encrypt IV input with current IV
cipher = AES.new(session_enc_key, AES.MODE_CBC, current_iv)
calculated_iv = cipher.encrypt(bytes(iv_input))
print(f"Calculated IV:   {hexlify(calculated_iv).decode().upper()}")

# From log
logged_iv = unhexlify("DE358D47649B6FF3FF2057BA33A6E3A2")
print(f"Logged IV:       {hexlify(logged_iv).decode().upper()}")
print(f"Match:           {calculated_iv == logged_iv}\n")

if calculated_iv != logged_iv:
    print("❌ IV calculation MISMATCH!")
    print("\nTrying with zero IV instead of current IV:")
    zero_iv = bytes(16)
    cipher = AES.new(session_enc_key, AES.MODE_CBC, zero_iv)
    iv_with_zero = cipher.encrypt(bytes(iv_input))
    print(f"IV with zero IV: {hexlify(iv_with_zero).decode().upper()}")
    print(f"Match:           {iv_with_zero == logged_iv}\n")
    
    if iv_with_zero == logged_iv:
        print("✅ FOUND IT! Should use ZERO IV for IV calculation, not current IV!")
        print("   This is wrong - AN12196 clearly shows IV chaining.")
        print("\nLet me check if the IV input format is correct...")
        
        # Check different CmdCtr formats
        print("\nTrying MSB first for CmdCtr:")
        iv_input_msb = bytearray(16)
        iv_input_msb[0] = 0xA5
        iv_input_msb[1] = 0x5A
        iv_input_msb[2:6] = ti
        iv_input_msb[6] = (cmd_ctr >> 8) & 0xFF  # MSB first
        iv_input_msb[7] = cmd_ctr & 0xFF
        print(f"IV Input (MSB):  {hexlify(iv_input_msb).decode().upper()}")
        cipher = AES.new(session_enc_key, AES.MODE_CBC, current_iv)
        iv_msb = cipher.encrypt(bytes(iv_input_msb))
        print(f"IV with MSB:     {hexlify(iv_msb).decode().upper()}")
        print(f"Match:           {iv_msb == logged_iv}")

# Step 2: Encrypt plain data
print("\nStep 2: Encrypt plain data")
cipher = AES.new(session_enc_key, AES.MODE_CBC, calculated_iv)
enc_key_data = cipher.encrypt(plain_data)
print(f"Encrypted:       {hexlify(enc_key_data).decode().upper()}")

# From log
logged_enc = unhexlify("DE66E0A7FC11B0C52554F090B58B5043E10D300ED915486C65563FCE4CE488F4")
print(f"Logged:          {hexlify(logged_enc).decode().upper()}")
print(f"Match:           {enc_key_data == logged_enc}")

# Step 3: Calculate CMAC
print("\nStep 3: Calculate CMAC")
mac_input = bytearray()
mac_input.append(0xC4)  # ChangeKey command
mac_input.append(cmd_ctr & 0xFF)
mac_input.append((cmd_ctr >> 8) & 0xFF)
mac_input.extend(ti)
mac_input.append(0x00)  # KeyNo
mac_input.extend(enc_key_data)

print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")

cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input))
cmac_full = cobj.digest()
cmac = cmac_full[:8]  # First 8 bytes

print(f"CMAC (full):     {hexlify(cmac_full).decode().upper()}")
print(f"CMAC (8 bytes):  {hexlify(cmac).decode().upper()}")

logged_cmac = unhexlify("18CADD8723BB7EEC")
print(f"Logged CMAC:     {hexlify(logged_cmac).decode().upper()}")
print(f"Match:           {cmac == logged_cmac}")
