"""
Test actual strtol behavior with strings containing colons
to accurately simulate the buggy firmware parsing
"""

# Simulate strtol(hex_string, NULL, 16) behavior
# strtol stops at the first invalid character and returns what it parsed so far
def simulate_strtol(s):
    """Simulate C strtol(s, NULL, 16) behavior"""
    valid_hex = "0123456789ABCDEFabcdef"
    result = ""
    for c in s:
        if c in valid_hex:
            result += c
        else:
            break  # Stop at first invalid char
    
    if result == "":
        return 0
    return int(result, 16)

# Test with the problematic strings from Card 5
test_cases = [
    ("04", "byte 0"),
    (":3", "byte 1"),  
    ("F:", "byte 2"),
    ("7B", "byte 3"),
    (":0", "byte 4"),
    ("2:", "byte 5"),
    ("E5", "byte 6"),
    (":7", "byte 7 (if parsed)"),
    ("5:", "byte 8 (if parsed)"),
    ("80", "byte 9 (if parsed)"),
]

print("Testing strtol simulation:")
print("-" * 50)
for hex_str, desc in test_cases:
    value = simulate_strtol(hex_str)
    print(f"{desc:20s} '{hex_str}' -> 0x{value:02X} ({value})")

print("\n" + "="*50)
print("Card 5 UID: 04:3F:7B:02:E5:75:80")
print("="*50)

uid_string = "04:3F:7B:02:E5:75:80"
print(f"UID string: '{uid_string}' (length={len(uid_string)})")
print(f"Characters: {[uid_string[i] for i in range(len(uid_string))]}")
print()

# Parse like the buggy firmware
buggy_bytes = []
for i in range(7):  # 7-byte UID
    idx1 = i * 2
    idx2 = i * 2 + 1
    if idx2 < len(uid_string):
        hex_pair = uid_string[idx1] + uid_string[idx2]
        value = simulate_strtol(hex_pair)
        buggy_bytes.append(value)
        print(f"Byte {i}: uid[{idx1}:{ idx2+1}] = '{hex_pair}' -> 0x{value:02X}")

print(f"\nBuggy UID bytes: {' '.join(f'{b:02X}' for b in buggy_bytes)}")

# Calculate HMAC-SHA256
import hmac
import hashlib

master_secret_hex = "A54C788525178060913D4CFC06380D1B"
master_secret_bytes = bytes.fromhex(master_secret_hex)

# HMAC input: UID || "K0" || 0x01
hmac_input = bytes(buggy_bytes) + b"K0" + bytes([0x01])
print(f"\nHMAC input: {hmac_input.hex().upper()}")

# Calculate HMAC-SHA256
mac = hmac.new(master_secret_bytes, hmac_input, hashlib.sha256).digest()
recovery_key = mac[:16]  # Truncate to 16 bytes

print(f"HMAC-SHA256: {mac.hex().upper()}")
print(f"Recovery key (truncated): {recovery_key.hex().upper()}")
