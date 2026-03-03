#!/usr/bin/env python3
"""
Verify ChangeKey with corrected session keys
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ChangeKey CMAC Verification with Corrected Session Keys ===\n")

# From latest log
session_enc_key = unhexlify("C984ED028B79DFC4FA856836C87098AE")
session_mac_key = unhexlify("00EB22447D8912894D1D1321F5ADC5E3")
ti = unhexlify("15BAC604")
cmd_ctr = 0
enc_key_data = unhexlify("784A18E41E1F1A8FA5DA3B2178E90EC19CD9B2622BA2BD0BB43FFAA10751F3EF")
mac_input = unhexlify("C4000015BAC60400784A18E41E1F1A8FA5DA3B2178E90EC19CD9B2622BA2BD0BB43FFAA10751F3EF")
logged_cmac = unhexlify("27C0B095C7ED18C9")

print("Input parameters:")
print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"Logged CMAC:     {hexlify(logged_cmac).decode().upper()}\n")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Calculated Full: {hexlify(calc_cmac_full).decode().upper()}")
print(f"Calculated CMACt:{hexlify(calc_cmac_t).decode().upper()}")
print(f"Match:           {calc_cmac_t == logged_cmac}\n")

if calc_cmac_t == logged_cmac:
    print("✅ CMAC is CORRECT!")
    print("\nBut the card still rejects with 0x911E...")
    print("This suggests the encrypted data might be wrong.")
    print("\nLet me verify the IV calculation and encryption...")
    
    current_iv = unhexlify("C3C5CB479F39FCFB91C4CE8100FD7267")
    
    # Calculate IV
    iv_input = bytearray(16)
    iv_input[0] = 0xA5
    iv_input[1] = 0x5A
    iv_input[2:6] = ti
    iv_input[6] = cmd_ctr & 0xFF
    iv_input[7] = (cmd_ctr >> 8) & 0xFF
    
    print(f"\nIV Calculation:")
    print(f"IV Input:        {hexlify(iv_input).decode().upper()}")
    print(f"Current IV:      {hexlify(current_iv).decode().upper()}")
    
    cipher = AES.new(session_enc_key, AES.MODE_CBC, current_iv)
    calculated_iv = cipher.encrypt(bytes(iv_input))
    print(f"Calculated IV:   {hexlify(calculated_iv).decode().upper()}")
    
    logged_iv = unhexlify("AAFFEEF8C7B90FB7AEE84343277563A8")
    print(f"Logged IV:       {hexlify(logged_iv).decode().upper()}")
    print(f"IV Match:        {calculated_iv == logged_iv}")
    
    if calculated_iv == logged_iv:
        print("\n✅ IV calculation is correct!")
        
        # Verify encryption
        plain_data = unhexlify("124EC5CEE70A418133F244266C9EF2A801800000000000000000000000000000")
        cipher = AES.new(session_enc_key, AES.MODE_CBC, calculated_iv)
        calc_enc = cipher.encrypt(plain_data)
        
        print(f"\nEncryption:")
        print(f"Plain Data:      {hexlify(plain_data).decode().upper()}")
        print(f"Calculated Enc:  {hexlify(calc_enc).decode().upper()}")
        print(f"Logged Enc:      {hexlify(enc_key_data).decode().upper()}")
        print(f"Enc Match:       {calc_enc == enc_key_data}")
        
        if calc_enc == enc_key_data:
            print("\n✅ Encryption is correct!")
            print("\n🔍 Everything seems correct, but the card rejects it.")
            print("   Possible issues:")
            print("   1. The card expects a response CMAC in the command")
            print("   2. Command counter increment timing")
            print("   3. Different APDU wrapping")
            print("\nLet me check AN12196 Table 27 for the expected command format...")
else:
    print("❌ CMAC mismatch - need to debug MAC input format")
