#!/usr/bin/env python3
"""
Calculate keys for multiple cards that were written with OLD firmware
"""

import hmac
import hashlib

def old_derivation_ascii(master_secret_hex, uid_hex, key_version=0x01):
    """
    OLD (INCORRECT) method: Used ASCII string concatenation
    """
    data_string = uid_hex + "K0" + str(key_version)
    data_ascii = data_string.encode('ascii')
    master_secret_ascii = master_secret_hex.encode('ascii')
    hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
    key = hmac_result[:16]
    return key.hex().upper()


def new_derivation_bytes(master_secret_hex, uid_hex, key_version=0x01):
    """
    NEW (CORRECT) method: Uses raw byte arrays
    """
    master_secret_bytes = bytes.fromhex(master_secret_hex)
    uid_bytes = bytes.fromhex(uid_hex)
    data = uid_bytes + b'K0' + bytes([key_version])
    hmac_result = hmac.new(master_secret_bytes, data, hashlib.sha256).digest()
    key = hmac_result[:16]
    return key.hex().upper()


# Master Secret
master_secret = "A54C788525178060913D4CFC06380D1B"

# Card UIDs (zonder dubbele punten)
cards = {
    "Card 1": "048F4F02E57580",
    "Card 2": "046C8302E57580",
    "Card 3": "04842802E57580",
    "Card 4": "041A7D02E57580",
    "Card 5": "043F7B02E57580"
}

print("=" * 80)
print("NTAG424 Key Calculator - Batch Mode")
print("=" * 80)
print(f"\nMaster Secret: {master_secret}")
print("\n" + "=" * 80)

for card_name, uid in cards.items():
    old_key = old_derivation_ascii(master_secret, uid)
    new_key = new_derivation_bytes(master_secret, uid)
    
    print(f"\n{card_name} (UID: {uid})")
    print("-" * 80)
    print(f"  OLD firmware key (currently on card): {old_key}")
    print(f"  NEW firmware key (correct):           {new_key}")
    
    if old_key == new_key:
        print("  ✅ Keys match - no problem!")
    else:
        print("  ❌ Keys are DIFFERENT!")

print("\n" + "=" * 80)
print("\n📝 TO FIX CARDS WITH OLD KEYS:")
print("   1. Use 'Write Cards' page")
print("   2. Enter UID")
print("   3. Enter Master Secret: A54C788525178060913D4CFC06380D1B")
print("   4. Check 'Factory Card' checkbox OFF")
print("   5. Enter 'Previous Key' = OLD firmware key from above")
print("   6. This will write the NEW correct key to the card")
print("=" * 80)
