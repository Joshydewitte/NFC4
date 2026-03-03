#!/usr/bin/env python3
"""
Reverse engineer: what SV did ESP32 use to get the logged session keys?
Brute force approach: try different SV constructions
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify
import itertools

# Known values
rndA = unhexlify("C69F27A83B28A2F27AC62469ECBD891B")
rndB = unhexlify("01A6A7C7FB64D82FEB428E554E014A25")
master_key = bytes(16)
target_enc_key = unhexlify("7293D42702D8DD22B54993AE655C7D5A")
target_mac_key = unhexlify("367FA77A7738BEDA481AA6D2497FDDF8")

print("=== Reverse Engineering SV Construction ===\n")
print(f"Target ENC Key: {hexlify(target_enc_key).decode().upper()}")
print(f"Target MAC Key: {hexlify(target_mac_key).decode().upper()}\n")

# The spec says: RndA[15:14] || (RndA[13:8] XOR RndB[15:10]) || RndB[9:0] || RndA[7:0]
# But let's try WITHOUT XOR

sv1_no_xor = bytearray(32)
sv1_no_xor[0] = 0xA5
sv1_no_xor[1] = 0x5A
sv1_no_xor[2] = 0x00
sv1_no_xor[3] = 0x01
sv1_no_xor[4] = 0x00
sv1_no_xor[5] = 0x80

# Try: RndA[15:14] || RndA[13:8] || RndB[9:0] || RndA[7:0] (NO XOR)
sv1_no_xor[6] = rndA[0]
sv1_no_xor[7] = rndA[1]

# RndA[13:8] WITHOUT XOR
for i in range(6):
    sv1_no_xor[8 + i] = rndA[2 + i]

# RndB[9:0]
for i in range(10):
    sv1_no_xor[14 + i] = rndB[6 + i]

# RndA[7:0]
for i in range(8):
    sv1_no_xor[24 + i] = rndA[8 + i]

print(f"Test 1: NO XOR between RndA and RndB")
print(f"SV1: {hexlify(sv1_no_xor).decode().upper()}")

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv1_no_xor)
enc_key = cobj.digest()

sv2_no_xor = bytearray(sv1_no_xor)
sv2_no_xor[0] = 0x5A
sv2_no_xor[1] = 0xA5

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv2_no_xor)
mac_key = cobj.digest()

print(f"ENC Key: {hexlify(enc_key).decode().upper()}")
print(f"MAC Key: {hexlify(mac_key).decode().upper()}")

if enc_key == target_enc_key and mac_key == target_mac_key:
    print("✅✅✅ FOUND IT! ESP32 uses NO XOR! ✅✅✅\n")
else:
    print("❌ Not a match\n")
    
# Test 2: Maybe ESP32 swapped label construction for MAC?
sv2_alt = bytearray(sv1_no_xor)
sv2_alt[0] = 0xA5  # Same as ENC!
sv2_alt[1] = 0x5A
sv2_alt[3] = 0x02  # Different counter?

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv2_alt)
mac_key2 = cobj.digest()

print(f"Test 2: Label same, Counter changed to 0x02")
print(f"MAC Key: {hexlify(mac_key2).decode().upper()}")

if enc_key == target_enc_key and mac_key2 == target_mac_key:
    print("✅✅✅ FOUND IT! ESP32 uses Counter 0x02 for MAC! ✅✅✅\n")
else:
    print("❌ Not a match\n")
