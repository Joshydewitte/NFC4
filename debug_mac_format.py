#!/usr/bin/env python3
"""
Debug MAC input format by trying different CmdCtr byte orders
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== MAC Input Format Debug ===\n")

session_mac_key = unhexlify("00EB22447D8912894D1D1321F5ADC5E3")
ti = unhexlify("15BAC604")
cmd_ctr = 0
key_no = 0x00
enc_key_data = unhexlify("784A18E41E1F1A8FA5DA3B2178E90EC19CD9B2622BA2BD0BB43FFAA10751F3EF")
logged_cmac = unhexlify("27C0B095C7ED18C9")

# Try 1: CmdCtr LSB first (0x0000)
print("Try 1: CmdCtr LSB first")
mac_input_1 = bytearray()
mac_input_1.append(0xC4)  # Cmd
mac_input_1.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input_1.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input_1.extend(ti)  # TI
mac_input_1.append(key_no)  # KeyNo
mac_input_1.extend(enc_key_data)  # EncData

print(f"MAC Input: {hexlify(mac_input_1).decode().upper()}")
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input_1))
cmac_full = cobj.digest()
cmac_t = truncate_cmac_even_bytes(cmac_full)
print(f"CMAC:      {hexlify(cmac_t).decode().upper()}")
print(f"Expected:  {hexlify(logged_cmac).decode().upper()}")
print(f"Match:     {cmac_t == logged_cmac}\n")

# Try 2: CmdCtr MSB first (0x0000)
print("Try 2: CmdCtr MSB first")
mac_input_2 = bytearray()
mac_input_2.append(0xC4)  # Cmd
mac_input_2.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input_2.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input_2.extend(ti)  # TI
mac_input_2.append(key_no)  # KeyNo
mac_input_2.extend(enc_key_data)  # EncData

print(f"MAC Input: {hexlify(mac_input_2).decode().upper()}")
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input_2))
cmac_full = cobj.digest()
cmac_t = truncate_cmac_even_bytes(cmac_full)
print(f"CMAC:      {hexlify(cmac_t).decode().upper()}")
print(f"Expected:  {hexlify(logged_cmac).decode().upper()}")
print(f"Match:     {cmac_t == logged_cmac}\n")

# Try 3: Without KeyNo in MAC input
print("Try 3: Without KeyNo (Cmd || CmdCtr || TI || EncData)")
mac_input_3 = bytearray()
mac_input_3.append(0xC4)  # Cmd
mac_input_3.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input_3.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input_3.extend(ti)  # TI
mac_input_3.extend(enc_key_data)  # EncData (no KeyNo!)

print(f"MAC Input: {hexlify(mac_input_3).decode().upper()}")
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input_3))
cmac_full = cobj.digest()
cmac_t = truncate_cmac_even_bytes(cmac_full)
print(f"CMAC:      {hexlify(cmac_t).decode().upper()}")
print(f"Expected:  {hexlify(logged_cmac).decode().upper()}")
print(f"Match:     {cmac_t == logged_cmac}\n")

# Try 4: KeyNo before encrypted data (but after TI)
print("Try 4: Cmd || KeyNo || CmdCtr || TI || EncData")
mac_input_4 = bytearray()
mac_input_4.append(0xC4)  # Cmd
mac_input_4.append(key_no)  # KeyNo first
mac_input_4.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input_4.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input_4.extend(ti)  # TI
mac_input_4.extend(enc_key_data)  # EncData

print(f"MAC Input: {hexlify(mac_input_4).decode().upper()}")
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input_4))
cmac_full = cobj.digest()
cmac_t = truncate_cmac_even_bytes(cmac_full)
print(f"CMAC:      {hexlify(cmac_t).decode().upper()}")
print(f"Expected:  {hexlify(logged_cmac).decode().upper()}")
print(f"Match:     {cmac_t == logged_cmac}\n")
