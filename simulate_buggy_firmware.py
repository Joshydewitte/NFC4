#!/usr/bin/env python3
"""
Simulate OLD BUGGY firmware hex parsing with colons
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"

# Card 5 with colons
uid_with_colons = "04:3F:7B:02:E5:75:80"

print("=" * 80)
print("SIMULATE OLD BUGGY FIRMWARE")
print("=" * 80)
print()
print(f"UID string: {uid_with_colons}")
print(f"UID length: {len(uid_with_colons)} chars")
print()

# Simulate old firmware bug
uidLen = len(uid_with_colons) // 2  # 20 / 2 = 10
print(f"Old firmware uidLen: {uidLen} bytes (should be 7!)")
print()

# Parse with bug (including colons in positions)
uidBytes = []
for i in range(uidLen):
    hex_str = uid_with_colons[i*2:i*2+2]
    try:
        val = int(hex_str, 16)
        uidBytes.append(val)
        print(f"  i={i}: '{hex_str}' → 0x{val:02X}")
    except ValueError:
        uidBytes.append(0)
        print(f"  i={i}: '{hex_str}' → 0x00 (parse error)")

print()
print("Buggy UID bytes:", ' '.join([f"{b:02X}" for b in uidBytes]))
print()

# Calculate key with buggy UID
master_bytes = bytes.fromhex(master_secret)
data = bytes(uidBytes) + b'K0' + bytes([1])

print(f"HMAC data: {data.hex().upper()}")
print()

hmac_result = hmac.new(master_bytes, data, hashlib.sha256).digest()
old_buggy_key = hmac_result[:16].hex().upper()

print("=" * 80)
print(f"KEY ON CARD 5 (from buggy firmware): {old_buggy_key}")
print("=" * 80)
print()
print("TO RECOVER:")
print("  1. Factory Card: OFF")
print("  2. Direct Key Mode: ON")
print("  3. Previous Key:", old_buggy_key)
print("=" * 80)
