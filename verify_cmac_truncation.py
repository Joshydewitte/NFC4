#!/usr/bin/env python3
"""
Verify CMAC with even-byte truncation (AN12196 standard)
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """
    Truncate CMAC by taking even-numbered byte positions (1-indexed)
    Position 2,4,6,8,10,12,14,16 = indices 1,3,5,7,9,11,13,15
    """
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== CMAC Even-Byte Truncation Verification ===\n")

# Test with AN12196 Table 27 example
print("Test 1: AN12196 Table 27 Example")
session_mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")
mac_input = unhexlify("C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD")
expected_cmac_full = unhexlify("B7A60161F202EC3489BD4BEDEF64BB32")
expected_cmac_t = unhexlify("A6610234BDED6432")

cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Full CMAC (calc): {hexlify(calc_cmac_full).decode().upper()}")
print(f"Full CMAC (exp):  {hexlify(expected_cmac_full).decode().upper()}")
print(f"Full match:       {calc_cmac_full == expected_cmac_full}")
print(f"\nCMACt (calc):     {hexlify(calc_cmac_t).decode().upper()}")
print(f"CMACt (exp):      {hexlify(expected_cmac_t).decode().upper()}")
print(f"CMACt match:      {calc_cmac_t == expected_cmac_t}\n")

if calc_cmac_t == expected_cmac_t:
    print("✅ Even-byte truncation is CORRECT!\n")
    
    # Now check our actual ChangeKey
    print("Test 2: Our ChangeKey Attempt")
    our_mac_key = unhexlify("92E63C9F131769B6A397B41B1541DE56")
    our_mac_input = unhexlify("C4000024F4563500DE66E0A7FC11B0C52554F090B58B5043E10D300ED915486C65563FCE4CE488F4")
    our_logged_cmac = unhexlify("18CADD8723BB7EEC")
    
    cobj2 = CMAC.new(our_mac_key, ciphermod=AES)
    cobj2.update(our_mac_input)
    our_calc_cmac_full = cobj2.digest()
    our_calc_cmac_t = truncate_cmac_even_bytes(our_calc_cmac_full)
    
    print(f"Full CMAC:        {hexlify(our_calc_cmac_full).decode().upper()}")
    print(f"CMACt (calc):     {hexlify(our_calc_cmac_t).decode().upper()}")
    print(f"CMACt (logged):   {hexlify(our_logged_cmac).decode().upper()}")
    print(f"Match:            {our_calc_cmac_t == our_logged_cmac}\n")
    
    if our_calc_cmac_t != our_logged_cmac:
        print("❌ CMAC still doesn't match!")
        print("This means there's an error in the MAC input formatting or key derivation.")
        print("\nLet me verify the session keys...")
        
        # From log authentication
        ti = unhexlify("24F45635")
        rnd_a = unhexlify("606F7AB46D57645DB8F2EC82361B8727")
        rnd_b = unhexlify("1B697B3FB94B935707C7E9656A3003D2")
        k0 = unhexlify("00000000000000000000000000000000")  # Factory key
        
        # Derive session keys (SV method)
        from Crypto.Hash import CMAC
        
        # SV1 for ENC key
        sv1 = bytearray([0xA5, 0x5A, 0x00, 0x01, 0x00, 0x80])
        sv1.extend(rnd_a[0:2])  # RndA[15:14] - first 2 bytes
        # XOR RndA[13:8] with RndB[15:10]
        for i in range(6):
            sv1.append(rnd_a[2+i] ^ rnd_b[i])
        sv1.extend(rnd_b[6:16])  # RndB[9:0]
        sv1.extend(rnd_a[8:16])  # RndA[7:0]
        
        cobj_enc = CMAC.new(k0, ciphermod=AES)
        cobj_enc.update(bytes(sv1))
        derived_enc_key = cobj_enc.digest()
        
        # SV2 for MAC key
        sv2 = bytearray([0x5A, 0xA5, 0x00, 0x01, 0x00, 0x80])
        sv2.extend(rnd_a[0:2])
        for i in range(6):
            sv2.append(rnd_a[2+i] ^ rnd_b[i])
        sv2.extend(rnd_b[6:16])
        sv2.extend(rnd_a[8:16])
        
        cobj_mac = CMAC.new(k0, ciphermod=AES)
        cobj_mac.update(bytes(sv2))
        derived_mac_key = cobj_mac.digest()
        
        print(f"\nDerived ENC Key:  {hexlify(derived_enc_key).decode().upper()}")
        print(f"Expected ENC:     001FCB67A6B13BD3B76905A1CB07C44D")
        print(f"ENC match:        {hexlify(derived_enc_key).decode().upper() == '001FCB67A6B13BD3B76905A1CB07C44D'}")
        
        print(f"\nDerived MAC Key:  {hexlify(derived_mac_key).decode().upper()}")
        print(f"Expected MAC:     92E63C9F131769B6A397B41B1541DE56")
        print(f"MAC match:        {hexlify(derived_mac_key).decode().upper() == '92E63C9F131769B6A397B41B1541DE56'}")
