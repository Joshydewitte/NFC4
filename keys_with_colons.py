#!/usr/bin/env python3
"""
Calculate keys with UID INCLUDING colons (as old firmware did)
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"

# Cards WITH colons (as they appear in uidString)
cards_with_colons = {
    "Card 1": "04:8F:4F:02:E5:75:80",
    "Card 2": "04:6C:83:02:E5:75:80",
    "Card 3": "04:84:28:02:E5:75:80",
    "Card 4": "04:1A:7D:02:E5:75:80",
    "Card 5": "04:3F:7B:02:E5:75:80"
}

print("=" * 80)
print("KEYS WITH COLONS IN UID (OLD FIRMWARE)")
print("=" * 80)
print()
print("Master Secret: " + master_secret)
print()
print("=" * 80)

for card_name, uid_with_colons in cards_with_colons.items():
    # Old firmware - ASCII method WITH colons
    data_string = uid_with_colons + "K0" + str(1)
    data_ascii = data_string.encode('ascii')
    master_secret_ascii = master_secret.encode('ascii')
    hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
    old_key_with_colons = hmac_result[:16].hex().upper()
    
    print(f"{card_name} (UID: {uid_with_colons})")
    print(f"  Key: {old_key_with_colons}")
    print()

print("=" * 80)
print("TRY THESE KEYS WITH:")
print("  - Factory Card: OFF")
print("  - Direct Key Mode: ON")
print("  - Previous Key: Use key from above")
print("=" * 80)
