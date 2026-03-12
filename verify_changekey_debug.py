#!/usr/bin/env python3
"""
Verify ChangeKey crypto from debug output
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

def bytes_to_hex(data):
    return ''.join(f'{b:02X}' for b in data)

def aes_encrypt_cbc(key, iv, plaintext):
    cipher = AES.new(key, AES.MODE_CBC, iv)
    return cipher.encrypt(plaintext)

def aes_encrypt_ecb(key, data):
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(data)

def calculate_cmac_full(key, data):
    cobj = CMAC.new(key, ciphermod=AES)
    cobj.update(bytes(data))
    return cobj.digest()

def calculate_cmac_truncated(key, data):
    full_mac = calculate_cmac_full(key, data)
    # Odd indices (1, 3, 5, 7, 9, 11, 13, 15)
    truncated = bytes([full_mac[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])
    return truncated

print("="*70)
print("ChangeKey Debug Verification")
print("="*70)

# From debug output
session_enc_key = bytes.fromhex("FDB47969F4DA2432BC736F48CA24D550")
session_mac_key = bytes.fromhex("E6AA395AB4C6E94DF0BCAB93F6F0C272")
transaction_id = bytes.fromhex("3BC3E56C")  # MSB first
command_counter = 0
current_iv = bytes.fromhex("F4768368FD7A1C682A80E6C401E55CC6")

new_key = bytes.fromhex("351543CBF5075D4D69048A368EAFB77B")

print(f"\n[Session State]")
print(f"Session ENC Key: {bytes_to_hex(session_enc_key)}")
print(f"Session MAC Key: {bytes_to_hex(session_mac_key)}")
print(f"Transaction ID:  {bytes_to_hex(transaction_id)}")
print(f"Command Counter: {command_counter}")
print(f"Current IV:      {bytes_to_hex(current_iv)}")

# Step 1: Plain Data
plaintext = bytearray(32)
plaintext[0:16] = new_key
plaintext[16] = 0x01  # Version
plaintext[17] = 0x80  # Padding
# Rest is zeros

print(f"\n[Plain Data]")
print(f"{bytes_to_hex(plaintext)}")

# Step 2: Calculate IV
iv_input = bytearray(16)
iv_input[0] = 0xA5
iv_input[1] = 0x5A
iv_input[2:6] = transaction_id  # TI (MSB first)
iv_input[6] = command_counter & 0xFF  # CmdCtr LSB first
iv_input[7] = (command_counter >> 8) & 0xFF
# Rest is zeros

print(f"\n[IV Calculation]")
print(f"IV Input: {bytes_to_hex(iv_input)}")

# Encrypt IV input with current IV as CBC IV
calculated_iv = aes_encrypt_cbc(session_enc_key, current_iv, bytes(iv_input))
print(f"Calculated IV: {bytes_to_hex(calculated_iv)}")
print(f"Expected IV:   57BFFEF4EF45BC04592D650E1B1116DA")

# Step 3: Encrypt plaintext
encrypted_data = aes_encrypt_cbc(session_enc_key, calculated_iv, bytes(plaintext))
print(f"\n[Encrypted Data]")
print(f"Calculated: {bytes_to_hex(encrypted_data)}")
print(f"Expected:   3B26D72B7C58EE26AA7F9E2C2B4701F7496F71AF213CEA3BF97AB27928EF7D0F")

# Step 4: CMAC
CMD_CHANGE_KEY = 0xC4
key_no = 0x00

mac_input = bytearray()
mac_input.append(CMD_CHANGE_KEY)  # Cmd (1 byte)
mac_input.append(command_counter & 0xFF)  # CmdCtr LSB first (2 bytes)
mac_input.append((command_counter >> 8) & 0xFF)
mac_input.extend(transaction_id)  # TI (4 bytes, MSB first)
mac_input.append(key_no)  # KeyNo (1 byte)
mac_input.extend(encrypted_data)  # EncData (32 bytes)

print(f"\n[CMAC Calculation]")
print(f"MAC Input ({len(mac_input)} bytes):")
print(f"{bytes_to_hex(mac_input)}")
print(f"Expected: C400003BC3E56C003B26D72B7C58EE26AA7F9E2C2B4701F7496F71AF213CEA3BF97AB27928EF7D0F")

cmac_full = calculate_cmac_full(session_mac_key, mac_input)
print(f"\nCMAC Full: {bytes_to_hex(cmac_full)}")
print(f"Expected:  86F61F3E2EAD02A3418C97D9107235F4")

cmac_truncated = calculate_cmac_truncated(session_mac_key, mac_input)
print(f"\nCMAC Truncated: {bytes_to_hex(cmac_truncated)}")
print(f"Expected:       86F61F3E2EAD02A3")

# Check if everything matches
print("\n" + "="*70)
print("VERIFICATION RESULTS")
print("="*70)

iv_match = bytes_to_hex(calculated_iv) == "57BFFEF4EF45BC04592D650E1B1116DA"
enc_match = bytes_to_hex(encrypted_data) == "3B26D72B7C58EE26AA7F9E2C2B4701F7496F71AF213CEA3BF97AB27928EF7D0F"
mac_input_match = bytes_to_hex(mac_input) == "C400003BC3E56C003B26D72B7C58EE26AA7F9E2C2B4701F7496F71AF213CEA3BF97AB27928EF7D0F"
mac_full_match = bytes_to_hex(cmac_full) == "86F61F3E2EAD02A3418C97D9107235F4"
mac_trunc_match = bytes_to_hex(cmac_truncated) == "86F61F3E2EAD02A3"

print(f"IV matches:           {'✅ YES' if iv_match else '❌ NO'}")
print(f"Encrypted matches:    {'✅ YES' if enc_match else '❌ NO'}")
print(f"MAC Input matches:    {'✅ YES' if mac_input_match else '❌ NO'}")
print(f"CMAC Full matches:    {'✅ YES' if mac_full_match else '❌ NO'}")
print(f"CMAC Truncated matches: {'✅ YES' if mac_trunc_match else '❌ NO'}")

if all([iv_match, enc_match, mac_input_match, mac_full_match, mac_trunc_match]):
    print("\n✅ ALL CRYPTO IS CORRECT!")
    print("\nPossible reasons for SW=911E:")
    print("1. Card may require specific sequence (no plain commands after auth)")
    print("2. Card may have different session state than expected")
    print("3. Previous GetKeySettings may have corrupted session")
    print("4. Card firmware may have specific requirements")
else:
    print("\n❌ CRYPTO MISMATCH FOUND!")
    print("Review the differences above.")

print("="*70)
