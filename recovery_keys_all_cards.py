#!/usr/bin/env python3
"""
Calculate ALL card keys with buggy firmware parsing
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"
master_bytes = bytes.fromhex(master_secret)

cards = {
    "Card 1": "04:8F:4F:02:E5:75:80",
    "Card 2": "04:6C:83:02:E5:75:80",
    "Card 3": "04:84:28:02:E5:75:80",
    "Card 4": "04:1A:7D:02:E5:75:80",
    "Card 5": "04:3F:7B:02:E5:75:80"
}

print("=" * 80)
print("RECOVERY KEYS - ALL CARDS (Buggy Firmware)")
print("=" * 80)
print()

for card_name, uid_with_colons in cards.items():
    # Simulate buggy parsing
    uidLen = len(uid_with_colons) // 2
    uidBytes = []
    for i in range(uidLen):
        hex_str = uid_with_colons[i*2:i*2+2]
        try:
            uidBytes.append(int(hex_str, 16))
        except ValueError:
            uidBytes.append(0)
    
    # Calculate key
    data = bytes(uidBytes) + b'K0' + bytes([1])
    hmac_result = hmac.new(master_bytes, data, hashlib.sha256).digest()
    recovery_key = hmac_result[:16].hex().upper()
    
    print(f"{card_name}: {uid_with_colons}")
    print(f"  Recovery Key: {recovery_key}")
    print()

print("=" * 80)
print("RECOVERY PROCEDURE:")
print("  1. Factory Card: OFF")
print("  2. Direct Key Mode: ON")
print("  3. Previous Key: Use recovery key from above")
print("  4. Master Secret: A54C788525178060913D4CFC06380D1B")
print()
print("This will authenticate with the OLD buggy key and write the NEW correct key!")
print("=" * 80)
