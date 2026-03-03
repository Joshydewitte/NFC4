#!/usr/bin/env python3
"""
Verify NTAG424 DNA AuthenticateEV2First cryptography
Using exact values from debug log
"""

from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

# From log output
key = unhexlify("00000000000000000000000000000000")  # Factory default key
enc_rnd_b = unhexlify("8D632AC2C3EA254A5033F0E64BAF0AD9")
dec_rnd_b = unhexlify("AD46553BEDBAA2776912B6BC6C0B45B4")
rnd_a = unhexlify("DE4CD04A99B7CB9DF27521C289B09839")
rnd_b_prime = unhexlify("46553BEDBAA2776912B6BC6C0B45B4AD")
enc_auth_data = unhexlify("B1D20D86296B00280B7DAF3E19C915B88D924E3DA2D56744E6BF7062D1B89634")
enc_response = unhexlify("48CE538BBC3F40FF444CB181250C08F67D60DEC1A4DAED33BEB22438ED0E6DD2")

print("=== NTAG424 AuthenticateEV2First Verification ===\n")

# Step 1: Verify RndB decryption
print("Step 1: Verify RndB decryption")
print(f"Encrypted RndB: {hexlify(enc_rnd_b).decode().upper()}")
zero_iv = bytes(16)
cipher = AES.new(key, AES.MODE_CBC, zero_iv)
dec_rnd_b_verify = cipher.decrypt(enc_rnd_b)
print(f"Decrypted RndB:  {hexlify(dec_rnd_b_verify).decode().upper()}")
print(f"Expected RndB:   {hexlify(dec_rnd_b).decode().upper()}")
print(f"Match: {dec_rnd_b_verify == dec_rnd_b}\n")

# Step 2: Verify RndB' rotation
print("Step 2: Verify RndB' rotation")
rnd_b_prime_verify = dec_rnd_b[1:] + dec_rnd_b[0:1]  # Rotate left
print(f"RndB':          {hexlify(rnd_b_prime_verify).decode().upper()}")
print(f"Expected RndB': {hexlify(rnd_b_prime).decode().upper()}")
print(f"Match: {rnd_b_prime_verify == rnd_b_prime}\n")

# Step 3: Verify auth data encryption (RndA || RndB')
print("Step 3: Verify auth data encryption")
auth_data = rnd_a + rnd_b_prime
print(f"Auth Data (RndA || RndB'): {hexlify(auth_data).decode().upper()}")
print(f"IV for encryption: {hexlify(enc_rnd_b).decode().upper()}")

# Encrypt with IV = enc_rnd_b (according to AN12196 Table 25)
cipher = AES.new(key, AES.MODE_CBC, enc_rnd_b)
enc_auth_data_verify = cipher.encrypt(auth_data)
print(f"Encrypted:      {hexlify(enc_auth_data_verify).decode().upper()}")
print(f"Expected:       {hexlify(enc_auth_data).decode().upper()}")
print(f"Match: {enc_auth_data_verify == enc_auth_data}\n")

if enc_auth_data_verify != enc_auth_data:
    print("❌ Auth data encryption MISMATCH!")
    print("Trying with zero IV instead...")
    cipher = AES.new(key, AES.MODE_CBC, zero_iv)
    enc_auth_data_zero_iv = cipher.encrypt(auth_data)
    print(f"With zero IV:   {hexlify(enc_auth_data_zero_iv).decode().upper()}")
    print(f"Match: {enc_auth_data_zero_iv == enc_auth_data}\n")

# Step 4: Decrypt card response
print("Step 4: Decrypt card response")
print(f"Encrypted Response: {hexlify(enc_response).decode().upper()}")

# IV for decryption = last block of enc_auth_data
iv_for_decrypt = enc_auth_data[16:32]
print(f"IV for decryption: {hexlify(iv_for_decrypt).decode().upper()}")

cipher = AES.new(key, AES.MODE_CBC, iv_for_decrypt)
dec_response = cipher.decrypt(enc_response)
print(f"Decrypted Response: {hexlify(dec_response).decode().upper()}")

# Parse response: TI (4) || RndA' (16) || PDcap2 (6) || PCDcap2 (6)
ti = dec_response[0:4]
rnd_a_prime = dec_response[4:20]
pdcap2 = dec_response[20:26]
pcdcap2 = dec_response[26:32]

print(f"\nParsed response:")
print(f"  TI:      {hexlify(ti).decode().upper()}")
print(f"  RndA':   {hexlify(rnd_a_prime).decode().upper()}")
print(f"  PDcap2:  {hexlify(pdcap2).decode().upper()}")
print(f"  PCDcap2: {hexlify(pcdcap2).decode().upper()}")

# Step 5: Verify RndA'
print("\nStep 5: Verify RndA'")
rnd_a_prime_expected = rnd_a[1:] + rnd_a[0:1]  # Rotate left
print(f"Expected RndA': {hexlify(rnd_a_prime_expected).decode().upper()}")
print(f"Received RndA': {hexlify(rnd_a_prime).decode().upper()}")
print(f"Match: {rnd_a_prime == rnd_a_prime_expected}")

if rnd_a_prime != rnd_a_prime_expected:
    print("\n❌ RndA' MISMATCH!")
    print("Trying decryption with different IVs...")
    
    # Try with zero IV
    cipher = AES.new(key, AES.MODE_CBC, zero_iv)
    dec_response_zero = cipher.decrypt(enc_response)
    rnd_a_prime_zero = dec_response_zero[4:20]
    print(f"With zero IV:       {hexlify(rnd_a_prime_zero).decode().upper()}")
    print(f"Match: {rnd_a_prime_zero == rnd_a_prime_expected}")
    
    # Try with first block of enc_auth_data
    iv_first_block = enc_auth_data[0:16]
    cipher = AES.new(key, AES.MODE_CBC, iv_first_block)
    dec_response_first = cipher.decrypt(enc_response)
    rnd_a_prime_first = dec_response_first[4:20]
    print(f"With first block:   {hexlify(rnd_a_prime_first).decode().upper()}")
    print(f"Match: {rnd_a_prime_first == rnd_a_prime_expected}")
    
    # Try with enc_rnd_b as IV
    cipher = AES.new(key, AES.MODE_CBC, enc_rnd_b)
    dec_response_enc_rnd_b = cipher.decrypt(enc_response)
    rnd_a_prime_enc_rnd_b = dec_response_enc_rnd_b[4:20]
    print(f"With enc_rnd_b:     {hexlify(rnd_a_prime_enc_rnd_b).decode().upper()}")
    print(f"Match: {rnd_a_prime_enc_rnd_b == rnd_a_prime_expected}")
else:
    print("✅ Authentication successful!")
