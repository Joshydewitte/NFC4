"""
Calculate correct recovery keys for all 5 cards
using accurate strtol simulation
"""
import hmac
import hashlib

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

def parse_uid_buggy(uid_string):
    """Parse UID string with the buggy firmware behavior"""
    buggy_bytes = []
    for i in range(7):  # 7-byte UID
        idx1 = i * 2
        idx2 = i * 2 + 1
        if idx2 < len(uid_string):
            hex_pair = uid_string[idx1] + uid_string[idx2]
            value = simulate_strtol(hex_pair)
            buggy_bytes.append(value)
    return bytes(buggy_bytes)

def calculate_recovery_key(uid_string, master_secret_hex):
    """Calculate recovery key using buggy UID parsing"""
    master_secret_bytes = bytes.fromhex(master_secret_hex)
    buggy_uid = parse_uid_buggy(uid_string)
    
    # HMAC input: UID || "K0" || 0x01
    hmac_input = buggy_uid + b"K0" + bytes([0x01])
    
    # Calculate HMAC-SHA256 and truncate to 16 bytes
    mac = hmac.new(master_secret_bytes, hmac_input, hashlib.sha256).digest()
    return mac[:16]

# Master secret
master_secret = "A54C788525178060913D4CFC06380D1B"

# All 5 cards
cards = [
    ("Card 1", "04:8F:4F:02:E5:75:80"),
    ("Card 2", "04:6C:83:02:E5:75:80"),
    ("Card 3", "04:84:28:02:E5:75:80"),
    ("Card 4", "04:1A:7D:02:E5:75:80"),
    ("Card 5", "04:3F:7B:02:E5:75:80"),
]

print("="*70)
print("CORRECT RECOVERY KEYS FOR ALL CARDS")
print("="*70)
print()

for name, uid in cards:
    buggy_uid = parse_uid_buggy(uid)
    recovery_key = calculate_recovery_key(uid, master_secret)
    
    print(f"{name}: {uid}")
    print(f"  Buggy UID bytes: {buggy_uid.hex().upper()}")
    print(f"  Recovery key:    {recovery_key.hex().upper()}")
    print()

print("="*70)
print("SUMMARY - Copy these keys to Direct Key Mode:")
print("="*70)
for name, uid in cards:
    recovery_key = calculate_recovery_key(uid, master_secret)
    print(f"{uid} -> {recovery_key.hex().upper()}")
