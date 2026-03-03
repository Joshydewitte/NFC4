#!/usr/bin/env python3
"""
Verify session key derivation for latest test
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

print("=== Session Key Verification (Latest Test) ===\n")

# From latest log
rndA = unhexlify("C69F27A83B28A2F27AC62469ECBD891B")
rndB = unhexlify("01A6A7C7FB64D82FEB428E554E014A25")

print(f"RndA: {hexlify(rndA).decode().upper()}")
print(f"RndB: {hexlify(rndB).decode().upper()}\n")

# SV1 = 0xA5 || 0x5A || 0x00 || 0x01 || 0x00 || 0x80 || RndA[15:14] || RndA[13:8] || RndB[9:0] || RndA[7:0]
# AN12196 notation: RndA[15] is the FIRST byte (MSB), RndA[0] is the LAST byte (LSB)
# So: RndA[15:14] = indices 0-1, RndA[13:8] = indices 2-7, RndA[7:0] = indices 8-15

sv1_enc = bytearray(32)
sv1_enc[0] = 0xA5
sv1_enc[1] = 0x5A
sv1_enc[2] = 0x00
sv1_enc[3] = 0x01
sv1_enc[4] = 0x00
sv1_enc[5] = 0x80
sv1_enc[6] = rndA[0]    # RndA[15]
sv1_enc[7] = rndA[1]    # RndA[14]
sv1_enc[8] = rndA[2]    # RndA[13]
sv1_enc[9] = rndA[3]    # RndA[12]
sv1_enc[10] = rndA[4]   # RndA[11]
sv1_enc[11] = rndA[5]   # RndA[10]
sv1_enc[12] = rndA[6]   # RndA[9]
sv1_enc[13] = rndA[7]   # RndA[8]
sv1_enc[14] = rndB[6]   # RndB[9]
sv1_enc[15] = rndB[7]   # RndB[8]
sv1_enc[16] = rndB[8]   # RndB[7]
sv1_enc[17] = rndB[9]   # RndB[6]
sv1_enc[18] = rndB[10]  # RndB[5]
sv1_enc[19] = rndB[11]  # RndB[4]
sv1_enc[20] = rndB[12]  # RndB[3]
sv1_enc[21] = rndB[13]  # RndB[2]
sv1_enc[22] = rndB[14]  # RndB[1]
sv1_enc[23] = rndB[15]  # RndB[0]
sv1_enc[24] = rndA[8]   # RndA[7]
sv1_enc[25] = rndA[9]   # RndA[6]
sv1_enc[26] = rndA[10]  # RndA[5]
sv1_enc[27] = rndA[11]  # RndA[4]
sv1_enc[28] = rndA[12]  # RndA[3]
sv1_enc[29] = rndA[13]  # RndA[2]
sv1_enc[30] = rndA[14]  # RndA[1]
sv1_enc[31] = rndA[15]  # RndA[0]

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

# SV2 for MAC key
sv1_mac = bytearray(sv1_enc)
sv1_mac[3] = 0x02  # Change label to 0x02 for MAC key

cobj = CMAC.new(master_key, ciphermod=AES)
cobj.update(sv1_mac)
session_mac_key = cobj.digest()

print(f"Session MAC Key (calculated): {hexlify(session_mac_key).decode().upper()}")
print(f"Session MAC Key (logged):     367FA77A7738BEDA481AA6D2497FDDF8")
print(f"Match: {hexlify(session_mac_key).decode().upper() == '367FA77A7738BEDA481AA6D2497FDDF8'}")
