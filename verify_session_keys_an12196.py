#!/usr/bin/env python3
"""
Verify session key derivation with AN12196 Table 14 example
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

print("=== Session Key Derivation Verification ===\n")

# From AN12196 Table 14
k0 = unhexlify("00000000000000000000000000000000")
rnd_a = unhexlify("13C5DB8A5930439FC3DEF9A4C675360F")
rnd_b = unhexlify("B9E2FC789B64BF237CCCAA20EC7E6E48")

expected_enc_key = unhexlify("1309C877509E5A215007FF0ED19CA564")
expected_mac_key = unhexlify("4C6626F5E72EA694202139295C7A7FC7")

print("AN12196 Table 14 Example:")
print(f"K0:               {hexlify(k0).decode().upper()}")
print(f"RndA:             {hexlify(rnd_a).decode().upper()}")
print(f"RndB:             {hexlify(rnd_b).decode().upper()}")
print(f"Expected ENC Key: {hexlify(expected_enc_key).decode().upper()}")
print(f"Expected MAC Key: {hexlify(expected_mac_key).decode().upper()}\n")

# Build SV1 with MSB-first indexing
# RndA[15:14] = RndA[0:1] (first 2 bytes)
# RndA[13:8] = RndA[2:7] (next 6 bytes)
# RndB[15:10] = RndB[0:5 ] (first 6 bytes)
# RndB[9:0] = RndB[6:15] (last 10 bytes)
# RndA[7:0] = RndA[8:15] (last 8 bytes)

sv1 = bytearray([0xA5, 0x5A, 0x00, 0x01, 0x00, 0x80])
sv1.extend(rnd_a[0:2])  # RndA[15:14]
for i in range(6):
    sv1.append(rnd_a[2+i] ^ rnd_b[i])  # RndA[13:8] XOR RndB[15:10]
sv1.extend(rnd_b[6:16])  # RndB[9:0]
sv1.extend(rnd_a[8:16])  # RndA[7:0]

print(f"SV1: {hexlify(sv1).decode().upper()}")

# Expected SV1 from AN12196 step 25
expected_sv1 = unhexlify("A55A0001008013C56268A548D8FBBF237CCCAA20EC7E6E48C3DEF9A4C675360F")
print(f"Expected: {hexlify(expected_sv1).decode().upper()}")
print(f"SV1 Match: {sv1 == expected_sv1}\n")

if sv1 != expected_sv1:
    print("❌ SV1 construction is WRONG!\n")
    
    # Try old indexing method (reversed)
    print("Trying old (reversed) indexing:")
    sv1_old = bytearray([0xA5, 0x5A, 0x00, 0x01, 0x00, 0x80])
    sv1_old.extend([rnd_a[15], rnd_a[14]])  # RndA[15:14] as last bytes
    for i in range(6):
        sv1_old.append(rnd_a[13-i] ^ rnd_b[15-i])
    for i in range(10):
        sv1_old.append(rnd_b[9-i])
    for i in range(8):
        sv1_old.append(rnd_a[7-i])
    
    print(f"SV1 (old): {hexlify(sv1_old).decode().upper()}")
    print(f"Match: {sv1_old == expected_sv1}\n")
else:
    print("✅ SV1 construction is CORRECT!\n")
    
    # Derive session keys
    cobj = CMAC.new(k0, ciphermod=AES)
    cobj.update(bytes(sv1))
    calc_enc_key = cobj.digest()
    
    sv2 = bytearray(sv1)
    sv2[0] = 0x5A
    sv2[1] = 0xA5
    
    cobj2 = CMAC.new(k0, ciphermod=AES)
    cobj2.update(bytes(sv2))
    calc_mac_key = cobj2.digest()
    
    print(f"Calculated ENC Key: {hexlify(calc_enc_key).decode().upper()}")
    print(f"Expected ENC Key:   {hexlify(expected_enc_key).decode().upper()}")
    print(f"ENC Match:          {calc_enc_key == expected_enc_key}\n")
    
    print(f"Calculated MAC Key: {hexlify(calc_mac_key).decode().upper()}")
    print(f"Expected MAC Key:   {hexlify(expected_mac_key).decode().upper()}")
    print(f"MAC Match:          {calc_mac_key == expected_mac_key}")
