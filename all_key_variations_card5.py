#!/usr/bin/env python3
"""
Generate ALL possible key variations for Card 5
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"

# Card 5 - all possible UID variations
uid_variations = {
    "With colons uppercase": "04:3F:7B:02:E5:75:80",
    "With colons lowercase": "04:3f:7b:02:e5:75:80",
    "Without colons uppercase": "043F7B02E57580",
    "Without colons lowercase": "043f7b02e57580",
    "7 bytes hex": "043F7B02E57580",  # Same as without colons
    "8 bytes with 0x00": "043F7B02E5758000",  # Maybe padded to 8 bytes?
}

print("=" * 80)
print("ALL POSSIBLE KEYS FOR CARD 5 (UID variations)")
print("=" * 80)
print()

for description, uid in uid_variations.items():
    # Method 1: ASCII master secret + ASCII UID (old firmware)
    data_string = uid + "K0" + "1"
    data_ascii = data_string.encode('ascii')
    master_secret_ascii = master_secret.encode('ascii')
    hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
    key_method1 = hmac_result[:16].hex().upper()
    
    print(f"{description}:")
    print(f"  UID: {uid}")
    print(f"  Method 1 (ASCII/ASCII): {key_method1}")
    
    # Method 2: Bytes master secret + ASCII UID  
    try:
        master_bytes = bytes.fromhex(master_secret)
        data_ascii_only = uid.encode('ascii') + b'K0' + bytes([1])
        hmac_result = hmac.new(master_bytes, data_ascii_only, hashlib.sha256).digest()
        key_method2 = hmac_result[:16].hex().upper()
        print(f"  Method 2 (Bytes/ASCII): {key_method2}")
    except:
        pass
    
    # Method 3: Bytes master secret + Bytes UID (correct, new firmware)
    try:
        uid_clean = uid.replace(':', '').replace('0x', '')
        uid_bytes = bytes.fromhex(uid_clean)
        data_bytes = uid_bytes + b'K0' + bytes([1])
        hmac_result = hmac.new(master_bytes, data_bytes, hashlib.sha256).digest()
        key_method3 = hmac_result[:16].hex().upper()
        print(f"  Method 3 (Bytes/Bytes): {key_method3}")
    except Exception as e:
        print(f"  Method 3: Error - {e}")
    
    print()

print("=" * 80)
print("INSTRUCTIONS:")
print("Try each key above with Direct Key Mode")
print("Pay special attention to which ones get past step 1 vs fail at step 2")
print("=" * 80)
