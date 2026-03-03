#!/usr/bin/env python3
"""
Verify session key derivation - CORRECTED VERSION
with XOR between RndA and RndB parts
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

print("=== Session Key Verification (Corrected with XOR) ===\n")

# From latest log
rndA = unhexlify("C69F27A83B28A2F27AC62469ECBD891B")
rndB = unhexlify("01A6A7C7FB64D82FEB428E554E014A25")

print(f"RndA: {hexlify(rndA).decode().upper()}")
print(f"RndB: {hexlify(rndB).decode().upper()}\n")

# SV1 for ENC key with XOR as per spec
sv1_enc = bytearray(32)
sv1_enc[0] = 0xA5
sv1_enc[1] = 0x5A
sv1_enc[2] = 0x00
sv1_enc[3] = 0x01  # Counter  
sv1_enc[4] = 0x00
sv1_enc[5] = 0x80

# RndA[15:14] = indices 0-1
sv1_enc[6] = rndA[0]
sv1_enc[7] = rndA[1]

# RndA[13:8] XOR RndB[15:10] = 6 bytes
# RndA[13:8] = indices 2-7, RndB[15:10] = indices 0-5
for i in range(6):
    sv1_enc[8 + i] = rndA[2 + i] ^ rndB[i]

# RndB[9:0] = 10 bytes (indices 6-15)
for i in range(10):
    sv1_enc[14 + i] = rndB[6 + i]

# RndA[7:0] = last 8 bytes (indices 8-15)
for i in range(8):
    sv1_enc[24 + i] = rndA[8 + i]

print(f"SV1 ENC: {hexlify(sv1_enc).decode().upper()}\n")

# Master key (all zeros for factory card)
master_key = bytes(16)

# Derive session ENC key  
cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv1_enc)
session_enc_key = cobj.digest()

print(f"Session ENC Key (calculated): {hexlify(session_enc_key).decode().upper()}")
print(f"Session ENC Key (logged):     7293D42702D8DD22B54993AE655C7D5A")
print(f"Match: {hexlify(session_enc_key).decode().upper() == '7293D42702D8DD22B54993AE655C7D5A'}\n")

# SV2 for MAC key (swap label bytes, Counter stays 0x01!)
sv1_mac = bytearray(sv1_enc)
sv1_mac[0] = 0x5A
sv1_mac[1] = 0xA5
# Counter at index 3 stays at 0x01

print(f"SV1 MAC: {hexlify(sv1_mac).decode().upper ()}\n")

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv1_mac)
session_mac_key = cobj.digest()

print(f"Session MAC Key (calculated): {hexlify(session_mac_key).decode().upper()}")
print(f"Session MAC Key (logged):     367FA77A7738BEDA481AA6D2497FDDF8")
print(f"Match: {hexlify(session_mac_key).decode().upper() == '367FA77A7738BEDA481AA6D2497FDDF8'}\n")

if hexlify(session_enc_key).decode().upper() == '7293D42702D8DD22B54993AE655C7D5A' and \
   hexlify(session_mac_key).decode().upper() == '367FA77A7738BEDA481AA6D2497FDDF8':
    print("✅✅✅ Session keys CORRECT! ✅✅✅")
    print("\nThis means the C++ session key derivation is CORRECT.")
else:
    print("❌ Session keys still don't match")
    print("\nThere's still a bug in the C++ session key derivation.")
