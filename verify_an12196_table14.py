#!/usr/bin/env python3
"""
Verify NTAG424 DNA AuthenticateEV2First using AN12196 Table 14 examples
"""

from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

# From AN12196 Table 14
key = unhexlify("00000000000000000000000000000000")
enc_rnd_b = unhexlify("A04C124213C186F22399D33AC2A30215")
dec_rnd_b = unhexlify("B9E2FC789B64BF237CCCAA20EC7E6E48")
rnd_a = unhexlify("13C5DB8A5930439FC3DEF9A4C675360F")
rnd_b_prime = unhexlify("E2FC789B64BF237CCCAA20EC7E6E48B9")
expected_enc_auth = unhexlify("35C3E05A752E0144BAC0DE51C1F22C56B34408A23D8AEA266CAB947EA8E0118D")

print("=== AN12196 Table 14 Verification ===\n")

# Step 1: Verify RndB decryption with zero IV
print("Step 1: Verify RndB decryption")
zero_iv = bytes(16)
cipher = AES.new(key, AES.MODE_CBC, zero_iv)
dec_rnd_b_verify = cipher.decrypt(enc_rnd_b)
print(f"Decrypted RndB: {hexlify(dec_rnd_b_verify).decode().upper()}")
print(f"Expected:       {hexlify(dec_rnd_b).decode().upper()}")
print(f"Match: {dec_rnd_b_verify == dec_rnd_b}\n")

# Step 2: Try encrypting (RndA || RndB') with zero IV
print("Step 2: Encrypt (RndA || RndB') with zero IV")
auth_data = rnd_a + rnd_b_prime
cipher = AES.new(key, AES.MODE_CBC, zero_iv)
enc_auth_zero_iv = cipher.encrypt(auth_data)
print(f"Encrypted:      {hexlify(enc_auth_zero_iv).decode().upper()}")
print(f"Expected:       {hexlify(expected_enc_auth).decode().upper()}")
print(f"Match: {enc_auth_zero_iv == expected_enc_auth}\n")

if enc_auth_zero_iv != expected_enc_auth:
    # Step 3: Try with IV = enc_rnd_b
    print("Step 3: Encrypt (RndA || RndB') with IV = enc_rnd_b")
    cipher = AES.new(key, AES.MODE_CBC, enc_rnd_b)
    enc_auth_enc_rnd_b = cipher.encrypt(auth_data)
    print(f"Encrypted:      {hexlify(enc_auth_enc_rnd_b).decode().upper()}")
    print(f"Expected:       {hexlify(expected_enc_auth).decode().upper()}")
    print(f"Match: {enc_auth_enc_rnd_b == expected_enc_auth}\n")
    
    if enc_auth_enc_rnd_b != expected_enc_auth:
        # Step 4: Try with IV = dec_rnd_b (plaintext RndB)
        print("Step 4: Encrypt (RndA || RndB') with IV = dec_rnd_b")
        cipher = AES.new(key, AES.MODE_CBC, dec_rnd_b)
        enc_auth_dec_rnd_b = cipher.encrypt(auth_data)
        print(f"Encrypted:      {hexlify(enc_auth_dec_rnd_b).decode().upper()}")
        print(f"Expected:       {hexlify(expected_enc_auth).decode().upper()}")
        print(f"Match: {enc_auth_dec_rnd_b == expected_enc_auth}\n")

print("\n=== Conclusion ===")
if enc_auth_zero_iv == expected_enc_auth:
    print("✅ AN12196 uses ZERO IV for encrypting (RndA || RndB')")
    print("   This means the IV chaining in my implementation is WRONG!")
