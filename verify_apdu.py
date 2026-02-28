#!/usr/bin/env python3
"""
Count exact bytes in APDU to verify no mistakes
"""

def hex_to_bytes(hex_str):
    return bytes.fromhex(hex_str.replace(' ', ''))

# From user's log
apdu_hex = "90C400002900AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C45108BB3439D27D9869300"

apdu = hex_to_bytes(apdu_hex)

print(f"Total APDU length: {len(apdu)} bytes\n")

print(f"CLA:    {apdu[0]:02X}")
print(f"INS:    {apdu[1]:02X}")
print(f"P1:     {apdu[2]:02X}")
print(f"P2:     {apdu[3]:02X}")
print(f"Lc:     {apdu[4]:02X} (decimal: {apdu[4]})")

data_start = 5
data_len = apdu[4]
data = apdu[data_start:data_start+data_len]

print(f"\nData field: {len(data)} bytes (expected from Lc: {data_len})")

if len(data) == data_len:
    print("  ✅ Data length matches Lc")
else:
    print(f"  ❌ MISMATCH: Data = {len(data)}, Lc = {data_len}")

print(f"\nData breakdown:")
print(f"  KeyNo:     {data[0]:02X} (1 byte)")
print(f"  Encrypted: {' '.join(f'{b:02X}' for b in data[1:33])} ({len(data[1:33])} bytes)")
print(f"  CMAC:      {' '.join(f'{b:02X}' for b in data[33:41])} ({len(data[33:41])} bytes)")

print(f"\nLe:     {apdu[data_start + data_len]:02X}")

print(f"\nExpected structure:")
print(f"  KeyNo (1) + Encrypted (32) + CMAC (8) = 41 bytes")
print(f"  Actual: {1} + {len(data[1:33])} + {len(data[33:41])} = {1 + len(data[1:33]) + len(data[33:41])} bytes")

if 1 + len(data[1:33]) + len(data[33:41]) == 41:
    print("  ✅ Structure correct!")
else:
    print("  ❌ Structure incorrect!")

# Verify total APDU length
expected_total = 5 + data_len + 1  # Header(5) + Data + Le(1)
print(f"\nTotal APDU: {len(apdu)} (expected: {expected_total})")
if len(apdu) == expected_total:
    print("  ✅ Total APDU length correct!")
else:
    print(f"  ❌ MISMATCH!")
