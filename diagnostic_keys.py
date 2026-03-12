#!/usr/bin/env python3
"""
Generate test command voor ESP32 serial monitor om alle mogelijke keys te proberen
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"
uid = "048F4F02E57580"  # Card 1

print("=" * 80)
print("DIAGNOSTIC KEY LIST FOR CARD 1")
print("=" * 80)
print()
print("UID: " + uid)
print("Master Secret: " + master_secret)
print()
print("-" * 80)
print()

# Factory key
print("1. FACTORY KEY (default)")
print("   00000000000000000000000000000000")
print()

# Old firmware - ASCII method
data_string = uid + "K0" + str(1)
data_ascii = data_string.encode('ascii')
master_secret_ascii = master_secret.encode('ascii')
hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
old_key_ascii = hmac_result[:16].hex().upper()
print("2. OLD FIRMWARE - ASCII method (master as ASCII)")
print("   " + old_key_ascii)
print()

# Hex string as bytes
master_bytes_wrong = master_secret.encode('ascii')
uid_bytes = bytes.fromhex(uid)
data = uid_bytes + b'K0' + bytes([1])
hmac_result = hmac.new(master_bytes_wrong, data, hashlib.sha256).digest()
old_key_wrong = hmac_result[:16].hex().upper()
print("3. OLD FIRMWARE - Hex as bytes (master as ASCII bytes)")
print("   " + old_key_wrong)
print()

# New firmware - correct
master_bytes = bytes.fromhex(master_secret)
uid_bytes = bytes.fromhex(uid)
data = uid_bytes + b'K0' + bytes([1])
hmac_result = hmac.new(master_bytes, data, hashlib.sha256).digest()
new_key = hmac_result[:16].hex().upper()
print("4. NEW FIRMWARE - Correct byte method")
print("   " + new_key)
print()

# Maybe UID has colons in derivation?
uid_with_colons = "04:8F:4F:02:E5:75:80"
uid_colons_ascii = uid_with_colons.encode('ascii')
data_ascii_colons = uid_colons_ascii + b'K0' + bytes([1])
hmac_result = hmac.new(master_secret_ascii, data_ascii_colons, hashlib.sha256).digest()
key_with_colons = hmac_result[:16].hex().upper()
print("5. UID WITH COLONS in derivation (ASCII)")
print("   " + key_with_colons)
print()

# Maybe keyVersion is 0 instead of 1?
data = uid_bytes + b'K0' + bytes([0])
hmac_result = hmac.new(master_bytes, data, hashlib.sha256).digest()
key_version_0 = hmac_result[:16].hex().upper()
print("6. NEW FIRMWARE with keyVersion=0 (instead of 1)")
print("   " + key_version_0)
print()

print("=" * 80)
print()
print("TEST PROCEDURE:")
print("1. Go to Write Cards page")
print("2. Check 'Factory Card' OFF")
print("3. Check 'Direct Key Mode' ON")
print("4. Try each key above one by one as 'Previous Key'")
print("5. Watch serial monitor for authentication result")
print()
print("If NONE work, the card may be:")
print("  - Corrupted/damaged")
print("  - Using a different master secret")
print("  - Actually still factory (try Factory Card checkbox ON)")
print("=" * 80)
