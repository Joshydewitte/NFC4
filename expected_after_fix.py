#!/usr/bin/env python3
"""
Verwachte output na CMAC fix
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES

print("="*80)
print("VERWACHTE OUTPUT NA CMAC FIX")
print("="*80)

# Test 1 data (zelfde als eerder)
SesAuthMACKey = bytes.fromhex("F18A26CEEF95B889887A9C100E1B5F68")
MAC_Input = bytes.fromhex("C4000074EBB6D3004DEF25FD976900D6703CABAA3BA77A8CA403F532DC1FE28FDBAE18064447C99B")

# Calculate CMAC Full
c = CMAC.new(SesAuthMACKey, ciphermod=AES)
c.update(MAC_Input)
cmac_full = c.digest()

# NEW: First 8 bytes (standard truncation)
cmac_truncated = cmac_full[:8]

print(f"\nMAC Input: {MAC_Input.hex().upper()}")
print(f"\nCMAC Full (16 bytes): {cmac_full.hex().upper()}")
print(f"CMAC (8 bytes):       {cmac_truncated.hex().upper()}")

print(f"\n{'='*80}")
print(f"VERWACHTE ESP32 OUTPUT NA FIX")
print(f"{'='*80}")
print(f"\n[DEBUG] CMAC Full: {cmac_full.hex().upper()}")
print(f"[DEBUG] CMAC:      {cmac_truncated.hex().upper()}")

print(f"\n{'='*80}")
print(f"VERGELIJKING")
print(f"{'='*80}")
print(f"\nOUD (fout):  7497C09A2F57E2FD")
print(f"NIEUW (fix): {cmac_truncated.hex().upper()}")

# Reconstruct full APDU
CMD = bytes.fromhex("C4")
KeyNo = bytes.fromhex("00")
EncData = bytes.fromhex("B0A0032672AD0CC8D2CAA07F3075186DCDE79B9E9B0EEA24E76219BAFB6B6717")

old_apdu = CMD + KeyNo + EncData + bytes.fromhex("070D8DFF3B602E4B")
new_apdu = CMD + KeyNo + EncData + cmac_truncated

print(f"\n{'='*80}")
print(f"CHANGEKEY APDU")
print(f"{'='*80}")
print(f"\nOud (fout):  {old_apdu.hex().upper()}")
print(f"Nieuw (fix): {new_apdu.hex().upper()}")

print(f"\n{'='*80}")
print(f"ACTIE")
print(f"{'='*80}")
print(f"\n1. Upload de gefixte code naar ESP32")
print(f"2. Test met factory kaart")
print(f"3. Verwacht:")
print(f"   CMAC (8 bytes): {cmac_truncated.hex().upper()}")
print(f"4. Als dit klopt → Enable schrijven (DEBUG_WRITE_MODE = false)")
print(f"5. Test opnieuw met factory kaart")
print(f"6. Verificatie moet SLAGEN met nieuwe key")

print(f"\n{'='*80}")
