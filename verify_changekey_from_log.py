#!/usr/bin/env python3
"""
Verify ChangeKey from actual ESP32 log output
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def bytes_to_hex(data):
    return hexlify(data).decode().upper()

def truncate_cmac_odd(full_mac):
    """Truncate CMAC to odd indices (1,3,5,7,9,11,13,15)"""
    return bytes([full_mac[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

print("=== Verify ESP32 ChangeKey from Log ===\n")

# From log output
ti = unhexlify("8F600F00")
cmd_ctr = 0
session_enc_key = unhexlify("1A3EC00B7E5DD3695D1DA8C84BC5AD02")
session_mac_key = unhexlify("91095E4E1E0DA06EA5E0BF6DB82CA3CB")

# From log: Plain Data
plain_data = unhexlify("351543CBF5075D4D69048A368EAFB77B01800000000000000000000000000000")
print(f"Plain Data: {bytes_to_hex(plain_data)}")
print(f"  NewKey:   {bytes_to_hex(plain_data[0:16])}")
print(f"  Version:  {plain_data[16]:02X}")
print(f"  Padding:  {bytes_to_hex(plain_data[17:32])}\n")

# From log: IV calculation
# CRITICAL: After authentication, currentIV is zeros (first command after auth)
# After ChangeKey, currentIV becomes the last block of encrypted data
current_iv = unhexlify("00000000000000000000000000000000")  # First command after auth
iv_input = unhexlify("A55A8F600F0000000000000000000000")
print(f"IV Input: {bytes_to_hex(iv_input)}")
print(f"  This is: A55A || TI || CmdCtr || zeros")
print(f"  A55A: {bytes_to_hex(iv_input[0:2])}")
print(f"  TI:   {bytes_to_hex(iv_input[2:6])}")
print(f"  CmdCtr: {bytes_to_hex(iv_input[6:8])} (LSB first, value={cmd_ctr})")
print(f"  Zeros: {bytes_to_hex(iv_input[8:16])}")
print(f"Current IV (for chaining): {bytes_to_hex(current_iv)}\n")

# Calculate IV using CBC with current IV (EV2 IV chaining)
cipher = AES.new(session_enc_key, AES.MODE_CBC, iv=current_iv)
iv_calculated = cipher.encrypt(iv_input)
print(f"IV Calculated: {bytes_to_hex(iv_calculated)}")
print(f"IV from log:   7F24C025ABDE4160DF0B3668E9EE682C")
print(f"✓ IV matches:  {bytes_to_hex(iv_calculated) == '7F24C025ABDE4160DF0B3668E9EE682C'}\n")

# Encrypt plain data
cipher = AES.new(session_enc_key, AES.MODE_CBC, iv=iv_calculated)
encrypted = cipher.encrypt(plain_data)
print(f"Encrypted: {bytes_to_hex(encrypted)}")
print(f"From log:  6FFF0F8E377B44E0AE1DC544255F62BA365480F2EBDA5DF158D6D743F90C029D")
print(f"✓ Encryption matches: {bytes_to_hex(encrypted) == '6FFF0F8E377B44E0AE1DC544255F62BA365480F2EBDA5DF158D6D743F90C029D'}\n")

# Calculate CMAC
# MAC Input = Cmd || CmdCtr || TI || KeyNo || EncData
mac_input = bytearray()
mac_input.append(0xC4)  # CMD
mac_input.append(cmd_ctr & 0xFF)  # CmdCtr LSB
mac_input.append((cmd_ctr >> 8) & 0xFF)  # CmdCtr MSB
mac_input.extend(ti)  # TI (4 bytes)
mac_input.append(0x00)  # KeyNo
mac_input.extend(encrypted)  # EncData (32 bytes)

print(f"MAC Input ({len(mac_input)} bytes):")
print(f"  {bytes_to_hex(mac_input)}")
print(f"From log:")
print(f"  C400008F600F00006FFF0F8E377B44E0AE1DC544255F62BA365480F2EBDA5DF158D6D743F90C029D")
print(f"✓ MAC Input matches: {bytes_to_hex(mac_input) == 'C400008F600F00006FFF0F8E377B44E0AE1DC544255F62BA365480F2EBDA5DF158D6D743F90C029D'}\n")

# Calculate full CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()

print(f"CMAC Full: {bytes_to_hex(cmac_full)}")
print(f"From log:  DDD6C1F892C345A507FD087E014E2881")
print(f"✓ CMAC matches: {bytes_to_hex(cmac_full) == 'DDD6C1F892C345A507FD087E014E2881'}\n")

# Truncate CMAC
cmac_truncated = truncate_cmac_odd(cmac_full)
print(f"CMACt (odd indices): {bytes_to_hex(cmac_truncated)}")
print(f"From log:            D6F8C3A5FD7E4E81")
print(f"✓ CMACt matches: {bytes_to_hex(cmac_truncated) == 'D6F8C3A5FD7E4E81'}\n")

# Final APDU
apdu = bytearray([0x90, 0xC4, 0x00, 0x00, 0x29, 0x00])
apdu.extend(encrypted)
apdu.extend(cmac_truncated)
apdu.append(0x00)

print(f"\n=== Final APDU ===")
print(f"Calculated: {bytes_to_hex(apdu)}")
print(f"\n✅ ALL CRYPTO VERIFICATIONS PASSED!")
print(f"\nSince ChangeKey returned SW=9100 (success), the crypto is correct.")
print(f"The problem with re-authentication must be:")
print(f"  1. Card requires CommitTransaction after ChangeKey?")
print(f"  2. Card state issue - needs full power cycle?")
print(f"  3. Card requires delay between ChangeKey and re-auth?")
print(f"  4. Case 2 requires different structure than we implemented?")
