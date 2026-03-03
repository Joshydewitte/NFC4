#!/usr/bin/env python3
"""
Verify CMAC calculation using AN12196 Table 27 example
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

# From AN12196 Table 27
print("=== AN12196 Table 27 CMAC Verification ===\n")

session_mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")
mac_input = unhexlify("C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD")
expected_cmac_full = unhexlify("B7A60161F202EC3489BD4BEDEF64BB32")
expected_cmac_t = unhexlify("A6610234BDED6432")

print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"Expected CMAC:   {hexlify(expected_cmac_full).decode().upper()}")
print(f"Expected CMACt:  {hexlify(expected_cmac_t).decode().upper()}\n")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calculated_cmac = cobj.digest()
calculated_cmac_t = calculated_cmac[:8]

print(f"Calculated CMAC: {hexlify(calculated_cmac).decode().upper()}")
print(f"Calculated CMACt:{hexlify(calculated_cmac_t).decode().upper()}")
print(f"Full CMAC match: {calculated_cmac == expected_cmac_full}")
print(f"CMACt match:     {calculated_cmac_t == expected_cmac_t}\n")

if calculated_cmac != expected_cmac_full:
    print("❌ CMAC calculation does not match AN12196 example!")
    print("   This suggests an issue with the CMAC implementation or Python library.")
else:
    print("✅ CMAC calculation matches AN12196!")
    print("\nNow checking our actual ChangeKey attempt...")
    
    # Our actual values
    our_mac_key = unhexlify("92E63C9F131769B6A397B41B1541DE56")
    our_mac_input = unhexlify("C4000024F4563500DE66E0A7FC11B0C52554F090B58B5043E10D300ED915486C65563FCE4CE488F4")
    our_logged_cmac = unhexlify("18CADD8723BB7EEC")
    
    print(f"\nOur Session MAC Key: {hexlify(our_mac_key).decode().upper()}")
    print(f"Our MAC Input:       {hexlify(our_mac_input).decode().upper()}")
    print(f"Our Logged CMAC:     {hexlify(our_logged_cmac).decode().upper()}")
    
    cobj2 = CMAC.new(our_mac_key, ciphermod=AES)
    cobj2.update(our_mac_input)
    our_calc_cmac = cobj2.digest()[:8]
    
    print(f"Our Calculated CMAC: {hexlify(our_calc_cmac).decode().upper()}")
    print(f"Match:               {our_calc_cmac == our_logged_cmac}")
    
    if our_calc_cmac != our_logged_cmac:
        print("\n❌ Our CMAC doesn't match!")
        print("This means the ESP32 is calculating it differently.")
        print("Let me check if there's an issue with the CMAC implementation...")
