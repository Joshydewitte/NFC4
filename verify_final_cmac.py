#!/usr/bin/env python3
"""
Verify ESP32's CMAC with corrected field order
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ESP32 CMAC Verification (Corrected Field Order) ===\n")

# From latest log
session_mac_key = unhexlify("B2832B03DE5067DC3C380CDCD17EF7AC")
mac_input = unhexlify("C40037DD06560000B8404F5BCA43926D0F3A8A01992983A8A8FA3A0C944A27C36455D1C53FA0814E")
logged_cmac_full = unhexlify("75A61FCAA5F4A290EDF52FEDFB65D996")
logged_cmac_t = unhexlify("A6CAF490F5ED6596")

print("MAC Input breakdown:")
print(f"  Cmd:        C4")
print(f"  KeyNo:      00")
print(f"  TI:         37DD0656")
print(f"  CmdCtr:     0000")
print(f"  EncCmdData: B8404F5BCA43926D0F3A8A01992983A8A8FA3A0C944A27C36455D1C53FA0814E")
print(f"\nSession MAC Key: {hexlify(session_mac_key).decode().upper()}")
print(f"MAC Input:       {hexlify(mac_input).decode().upper()}")
print(f"Logged CMAC Full:{hexlify(logged_cmac_full).decode().upper()}")
print(f"Logged CMAC:     {hexlify(logged_cmac_t).decode().upper()}\n")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"Calc CMAC Full:  {hexlify(calc_cmac_full).decode().upper()}")
print(f"Calc CMAC:       {hexlify(calc_cmac_t).decode().upper()}\n")

# Manual truncation check
manual_trunc = truncate_cmac_even_bytes(logged_cmac_full)
print(f"Manual truncation: {hexlify(manual_trunc).decode().upper()}")
print(f"Matches logged:    {manual_trunc == logged_cmac_t}\n")

if calc_cmac_full == logged_cmac_full:
    print("✅ ESP32 CMAC calculation is PERFECT!")
    print("✅ Truncation is CORRECT!")
    print("\n❓ But card still rejects with 0x911E...")
    print("\n🔍 Let me check AN12196 Table 27 more carefully...")
else:
    print("❌ ESP32 CMAC still incorrect")
    print(f"\nDifference: {hexlify(bytes(a^b for a,b in zip(calc_cmac_full, logged_cmac_full))).decode().upper()}")
    
# Check AN12196 Table 27 format
print("\n📖 AN12196 Table 27 ChangeKey Case 2:")
print("MAC Input = Cmd || KeyNo || TI || CmdCtr || EncCmdData")
print("           = C4  || 00    || TI || 0000   || 32 bytes")
print("\nBut wait... is KeyVersion included?")
print("Let me check if KeyNo should be '00' or if KeyVersion byte is needed...")
