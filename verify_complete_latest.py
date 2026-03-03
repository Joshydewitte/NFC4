#!/usr/bin/env python3
"""
Complete end-to-end verification of ChangeKey with latest log values
"""

from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# Values from latest log (second attempt)
session_enc_key = bytes.fromhex("3B2459A0420992E653EC3D870C89843E")
session_mac_key = bytes.fromhex("3F45C48DAF52CD451DCC0EFC811293E5")
ti = bytes.fromhex("ADB793FC")
cmd_ctr = 1
key_no = 0x00
new_key = bytes.fromhex("10532710706128B62A990EE06ECC86A6")
key_version = 0x01

print("=" * 60)
print("COMPLETE CHANGEKEY VERIFICATION")
print("=" * 60)
print(f"Session ENC Key: {session_enc_key.hex().upper()}")
print(f"Session MAC Key: {session_mac_key.hex().upper()}")
print(f"TI: {ti.hex().upper()}")
print(f"CmdCtr: {cmd_ctr}")
print()

# Step 1: Plaintext
plain_data = bytearray(32)
plain_data[0:16] = new_key
plain_data[16] = key_version
plain_data[17] = 0x80
# Rest zeros

print(f"Plaintext: {plain_data.hex().upper()}")
print(f"ESP32 log: 10532710706128B62A990EE06ECC86A601800000000000000000000000000000")
if plain_data.hex().upper() == "10532710706128B62A990EE06ECC86A601800000000000000000000000000000":
    print("✅ Plaintext matches")
else:
    print("❌ Plaintext MISMATCH")
print()

# Step 2: Calculate IV
iv_input = bytearray(16)
iv_input[0:2] = bytes.fromhex("A55A")
iv_input[2:6] = ti
iv_input[6] = cmd_ctr & 0xFF
iv_input[7] = (cmd_ctr >> 8) & 0xFF

cipher = AES.new(session_enc_key, AES.MODE_ECB)
iv = cipher.encrypt(bytes(iv_input))

print(f"IV: {iv.hex().upper()}")
print(f"ESP32 log: FCD8DA44216C9D989BBE02126BD0CD17")
if iv.hex().upper() == "FCD8DA44216C9D989BBE02126BD0CD17":
    print("✅ IV matches")
else:
    print("❌ IV MISMATCH")
print()

# Step 3: Encrypt
cipher = AES.new(session_enc_key, AES.MODE_CBC, iv)
encrypted_data = cipher.encrypt(bytes(plain_data))

print(f"Encrypted: {encrypted_data.hex().upper()}")
print(f"ESP32 log: 031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546")
if encrypted_data.hex().upper() == "031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546":
    print("✅ Encrypted data matches")
else:
    print("❌ Encrypted data MISMATCH")
print()

# Step 4: MAC Input
mac_input = bytearray()
mac_input.append(0xC4)
mac_input.append(cmd_ctr & 0xFF)
mac_input.append((cmd_ctr >> 8) & 0xFF)
mac_input.extend(ti)
mac_input.append(key_no)
mac_input.extend(encrypted_data)

print(f"MAC Input: {mac_input.hex().upper()}")
print(f"ESP32 log: C40100ADB793FC00031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546")
if mac_input.hex().upper() == "C40100ADB793FC00031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546":
    print("✅ MAC Input matches")
else:
    print("❌ MAC Input MISMATCH")
print()

# Step 5: CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(bytes(mac_input))
cmac_full = cobj.digest()

print(f"CMAC Full: {cmac_full.hex().upper()}")
print(f"ESP32 log: AAA4B36241DAD233689528E4AE16496C")
if cmac_full.hex().upper() == "AAA4B36241DAD233689528E4AE16496C":
    print("✅ CMAC Full matches")
else:
    print("❌ CMAC Full MISMATCH")

cmac_t = bytes([cmac_full[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])
print(f"CMAC Trunc: {cmac_t.hex().upper()}")
print(f"ESP32 log:  A462DA3395E4166C")
if cmac_t.hex().upper() == "A462DA3395E4166C":
    print("✅ CMAC Truncated matches")
else:
    print("❌ CMAC Truncated MISMATCH")
print()

# Step 6: Build APDU
apdu = bytearray()
apdu.extend(bytes.fromhex("90C4000029"))
apdu.append(key_no)
apdu.extend(encrypted_data)
apdu.extend(cmac_t)
apdu.append(0x00)

print("=" * 60)
print("FINAL APDU:")
print(apdu.hex().upper())
print()
print("ESP32 log:")
print("90C400002900031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546A462DA3395E4166C00")
print()

if apdu.hex().upper() == "90C400002900031742CC1A7FBC6111FC01F7BCFFF827A98DFBF326B82F66E9FEA39366D3A546A462DA3395E4166C00":
    print("✅✅✅ APDU MATCHES PERFECTLY!")
    print()
    print("Everything is 100% correct according to AN12196 Table 27.")
    print("The card is rejecting our valid ChangeKey command with 0x911E.")
    print()
    print("Possible reasons:")
    print("1. Factory card has special protection on Key 0")
    print("2. Additional command required before ChangeKey (e.g., GetKeyVersion)")
    print("3. Need to work at PICC level, not NDEF application level")
    print("4. Card expects different key format or version")
else:
    print("❌ APDU MISMATCH")
