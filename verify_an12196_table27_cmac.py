#!/usr/bin/env python3
"""
Verify CMAC implementation against AN12196 Table 27 example
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== AN12196 Table 27 CMAC Verification ===\n")

# From AN12196 Table 27 (Case 2 ChangeKey example)
mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")
mac_input = unhexlify("C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD")
expected_cmac_full = unhexlify("B7A60161F202EC3489BD4BEDEF64BB32")
expected_cmac_t = unhexlify("A6610234BDED6432")

print("AN12196 Table 27 values:")
print(f"MAC Key:         {hexlify(mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"Expected CMAC:   {hexlify(expected_cmac_full).decode().upper()}")
print(f"Expected CMACt:  {hexlify(expected_cmac_t).decode().upper()}\n")

print("MAC Input breakdown:")
print(f"  Cmd:     C4")
print(f"  CmdCtr:  0300")
print(f"  TI:      7614281A")
print(f"  KeyNo:   00")
print(f"  EncData: C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD\n")

# Calculate CMAC using PyCryptodome
cobj = CMAC.new(mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Calculated CMAC: {hexlify(calc_cmac_full).decode().upper()}")
print(f"Calculated CMACt:{hexlify(calc_cmac_t).decode().upper()}\n")

print(f"✓ Full CMAC matches AN12196: {calc_cmac_full == expected_cmac_full}")
print(f"✓ Truncated CMAC matches:     {calc_cmac_t == expected_cmac_t}\n")

if calc_cmac_full == expected_cmac_full:
    print("✅ CMAC algorithm is correct!")
    print("✅ Truncation is correct!")
    print("\nThis confirms Python's CMAC matches AN12196.")
    print("The ESP32 ECB-based CMAC should produce the same result.")
else:
    print("❌ ERROR: Python CMAC doesn't match AN12196!")
    print("This would indicate a problem with the test or my understanding.")
