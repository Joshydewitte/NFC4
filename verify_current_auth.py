#!/usr/bin/env python3
"""
Verify current authentication attempt with actual log values
"""

from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

# From latest log output
key = unhexlify("00000000000000000000000000000000")
enc_rnd_b = unhexlify("70F1725A0259FA4D702D5BBC01491C9E")
dec_rnd_b = unhexlify("D2429DAF134B615BDBD22AB740AF88B6")
rnd_a = unhexlify("283C77DD071F4F6B6D212410B2755575")
rnd_b_prime = unhexlify("429DAF134B615BDBD22AB740AF88B6D2")
auth_data = unhexlify("283C77DD071F4F6B6D212410B2755575429DAF134B615BDBD22AB740AF88B6D2")
enc_auth_data = unhexlify("E112E13D49E63AB096BD1356B6F9752BE423A18131A3F917D18C50BC2BB8922E")
enc_response = unhexlify("90B0D4A139D1F6125C73F6BAB9F558120B07DD95B922738BB11E277A1A2675B9")
iv_for_decrypt = unhexlify("E423A18131A3F917D18C50BC2BB8922E")

print("=== Current Authentication Debug ===\n")

# Verify auth data encryption
print("Step 1: Verify auth data encryption")
zero_iv = bytes(16)
cipher = AES.new(key, AES.MODE_CBC, zero_iv)
enc_auth_verify = cipher.encrypt(auth_data)
print(f"Enc Auth Data (calc): {hexlify(enc_auth_verify).decode().upper()}")
print(f"Enc Auth Data (log):  {hexlify(enc_auth_data).decode().upper()}")
print(f"Match: {enc_auth_verify == enc_auth_data}")

# Extract blocks
first_block = enc_auth_data[0:16]
second_block = enc_auth_data[16:32]
print(f"\nFirst block:  {hexlify(first_block).decode().upper()}")
print(f"Second block: {hexlify(second_block).decode().upper()}")
print(f"IV used:      {hexlify(iv_for_decrypt).decode().upper()}")
print(f"IV correct:   {iv_for_decrypt == second_block}\n")

# Try decryption with different IVs
print("Step 2: Try response decryption with different IVs")

# Try 1: Second block of enc_auth_data (current implementation)
print("\n[1] With second block of enc_auth_data (current):")
cipher = AES.new(key, AES.MODE_CBC, second_block)
dec_response = cipher.decrypt(enc_response)
print(f"Decrypted: {hexlify(dec_response).decode().upper()}")
rnd_a_prime = dec_response[4:20]
print(f"RndA':     {hexlify(rnd_a_prime).decode().upper()}")

# Expected RndA' = rotate left RndA
rnd_a_prime_expected = rnd_a[1:] + rnd_a[0:1]
print(f"Expected:  {hexlify(rnd_a_prime_expected).decode().upper()}")
print(f"Match:     {rnd_a_prime == rnd_a_prime_expected}")

# Try 2: Zero IV
print("\n[2] With zero IV:")
cipher = AES.new(key, AES.MODE_CBC, zero_iv)
dec_response = cipher.decrypt(enc_response)
print(f"Decrypted: {hexlify(dec_response).decode().upper()}")
rnd_a_prime = dec_response[4:20]
print(f"RndA':     {hexlify(rnd_a_prime).decode().upper()}")
print(f"Match:     {rnd_a_prime == rnd_a_prime_expected}")

# Try 3: First block of enc_auth_data
print("\n[3] With first block of enc_auth_data:")
cipher = AES.new(key, AES.MODE_CBC, first_block)
dec_response = cipher.decrypt(enc_response)
print(f"Decrypted: {hexlify(dec_response).decode().upper()}")
rnd_a_prime = dec_response[4:20]
print(f"RndA':     {hexlify(rnd_a_prime).decode().upper()}")
print(f"Match:     {rnd_a_prime == rnd_a_prime_expected}")

# Try 4: enc_rnd_b
print("\n[4] With enc_rnd_b:")
cipher = AES.new(key, AES.MODE_CBC, enc_rnd_b)
dec_response = cipher.decrypt(enc_response)
print(f"Decrypted: {hexlify(dec_response).decode().upper()}")
rnd_a_prime = dec_response[4:20]
print(f"RndA':     {hexlify(rnd_a_prime).decode().upper()}")
print(f"Match:     {rnd_a_prime == rnd_a_prime_expected}")

print("\n=== Analysis ===")
print("The mismatch suggests encryption/decryption is working correctly,")
print("but the card is responding with something we don't expect.")
print("\nLet me check if there's a protocol issue...")
