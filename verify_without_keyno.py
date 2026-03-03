#!/usr/bin/env python3
"""
Verify with removed KeyNo from MAC input
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# Values from latest log
session_mac_key = bytes.fromhex("441A87E687DF63B37D208FA90D3C9AB1")
mac_input = bytes.fromhex("C40100DAB5E684510A67EA2BAA5922EBD6F9183C0B36E167A414600808D9BB6D19E569B1DA0D5C")

print("=" * 60)
print("VERIFY MAC INPUT WITHOUT KEYNO")
print("=" * 60)
print(f"Session MAC Key: {session_mac_key.hex().upper()}")
print(f"MAC Input ({len(mac_input)} bytes):")
print(mac_input.hex().upper())
print()

# Parse MAC input
print("MAC Input breakdown:")
print(f"  INS:       {mac_input[0]:02X}")
print(f"  CmdCtr:    {mac_input[1]:02X} {mac_input[2]:02X}")
print(f"  TI:        {mac_input[3:7].hex().upper()}")
print(f"  EncData:   {mac_input[7:].hex().upper()}")
print()

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()

print(f"CMAC Full (Python):     {cmac_full.hex().upper()}")
print(f"CMAC Full (ESP32 log):  C785ED677D3A68859F4D74B01A2D1778")
print()

if cmac_full.hex().upper() == "C785ED677D3A68859F4D208FA90D3C9AB1".upper()[:32]:
    print("✅ CMAC Full matches")
else:
    print("❌ CMAC Full MISMATCH")

cmac_t = bytes([cmac_full[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])
print(f"CMAC Trunc (Python): {cmac_t.hex().upper()}")
print(f"CMAC Trunc (ESP32):  85673A854DB02D78")
print()

if cmac_t.hex().upper() == "85673A854DB02D78":
    print("✅ CMAC Truncated matches")
else:
    print("❌ CMAC Truncated MISMATCH")

# Check if maybe Card expects KeyNo IN the MAC
print()
print("=" * 60)
print("TRY WITH KEYNO IN MAC INPUT (40 bytes):")
print("=" * 60)

mac_input_with_keyno = bytearray()
mac_input_with_keyno.append(0xC4)  # INS
mac_input_with_keyno.append(0x01)  # CmdCtr LSB
mac_input_with_keyno.append(0x00)  # CmdCtr MSB
mac_input_with_keyno.extend(bytes.fromhex("DAB5E684"))  # TI
mac_input_with_keyno.append(0x00)  # KeyNo
mac_input_with_keyno.extend(bytes.fromhex("510A67EA2BAA5922EBD6F9183C0B36E167A414600808D9BB6D19E569B1DA0D5C"))  # EncData

print(f"MAC Input WITH KeyNo ({len(mac_input_with_keyno)} bytes):")
print(mac_input_with_keyno.hex().upper())

cobj2 = CMAC.new(session_mac_key, ciphermod=AES)
cobj2.update(bytes(mac_input_with_keyno))
cmac_full2 = cobj2.digest()
cmac_t2 = bytes([cmac_full2[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

print(f"CMAC Full: {cmac_full2.hex().upper()}")
print(f"CMAC Trunc: {cmac_t2.hex().upper()}")
print()
print("Maybe the card EXPECTS KeyNo in MAC input after all?")
