#!/usr/bin/env python3
"""
Verify the EXACT ChangeKey that was sent to card 04:B3:50:02:E5:75:80
Using the EXACT values from ESP32 log
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify
import sys

# TODO: Fill in from ESP32 debug log:
# Look for these lines in the serial output of last ChangeKey attempt:
# [DEBUG] Plain Data: ...
# [DEBUG] IV: ...
# [DEBUG] Encrypted Key Data: ...
# [DEBUG] MAC Input: ...
# [DEBUG] CMAC Full: ...
# [DEBUG] CMAC: ...
# [DEBUG] >> APDU: ...

print("═══════════════════════════════════════════════════════════")
print("📋 ChangeKey Verification from ESP32 Log")
print("═══════════════════════════════════════════════════════════\n")

print("🔍 INSTRUCTIONS:")
print("1. Upload updated code to ESP32:")
print("   pio run -t upload -e seeed_xiao_esp32s3 --upload-port COM9")
print("2. Try writing card 04:B3:50:02:E5:75:80 again")
print("3. Copy these values from [DEBUG] output:")
print("   - Session ENC Key")
print("   - Session MAC Key")
print("   - Transaction ID (TI)")
print("   - CmdCtr")
print("   - Plain Data")
print("   - IV")
print("   - Encrypted Key Data")
print("   - CMAC Full")
print("   - CMAC")
print("4. Fill them in below and run this script\n")

print("═══════════════════════════════════════════════════════════\n")

# UID of problematic card
uid = "04:B3:50:02:E5:75:80"
print(f"Card UID: {uid}\n")

# Master secret (should be from systemConfig)
master_secret = "A54C788525178060913D4CFC06380D1B"  # TODO: Update if different

# Derive expected new key
from hashlib import sha256
import hmac

uid_clean = uid.replace(":", "")
uid_bytes = bytes.fromhex(uid_clean)
secret_bytes = bytes.fromhex(master_secret)

# Derivation: HMAC-SHA256(secret, UID + 'K0' + 0x01)
data = uid_bytes + b'K0' + bytes([0x01])
h = hmac.new(secret_bytes, data, sha256)
derived_key = h.digest()[:16]

print(f"Master Secret: {master_secret}")
print(f"Derived New Key: {hexlify(derived_key).decode().upper()}\n")

print("───────────────────────────────────────────────────────────")
print("TODO: Fill in values from ESP32 debug log below:")
print("───────────────────────────────────────────────────────────\n")

# TODO: Fill these from ESP32 log
session_enc_key = None  # "..." from [DEBUG] Session ENC Key
session_mac_key = None  # "..." from [DEBUG] Session MAC Key
ti = None              # "..." from [DEBUG] Transaction ID
cmd_ctr = None         # Value from [DEBUG] CmdCtr
plain_data = None      # "..." from [DEBUG] Plain Data
iv = None              # "..." from [DEBUG] IV
encrypted_data = None  # "..." from [DEBUG] Encrypted Key Data
cmac_full = None       # "..." from [DEBUG] CMAC Full
cmac_truncated = None  # "..." from [DEBUG] CMAC

if not all([session_enc_key, session_mac_key, ti, plain_data, iv, encrypted_data, cmac_full, cmac_truncated]):
    print("❌ ERROR: Fill in all values from ESP32 log first!")
    print("\nRun: pio run -t upload && <scan card> && <copy debug output>")
    sys.exit(1)

# Convert to bytes
session_enc_key = unhexlify(session_enc_key)
session_mac_key = unhexlify(session_mac_key)
ti = unhexlify(ti)
plain_data = unhexlify(plain_data)
iv_calculated = unhexlify(iv)
encrypted_data = unhexlify(encrypted_data)
cmac_full_expected = unhexlify(cmac_full)
cmac_expected = unhexlify(cmac_truncated)

# Verify everything
print("✓ VERIFYING CRYPTO...")
print()

# 1. Check plain data structure (Case 2)
if len(plain_data) != 32:
    print(f"❌ Plain data wrong length: {len(plain_data)} (expected 32)")
else:
    new_key_in_plain = plain_data[:16]
    version = plain_data[16]
    padding = plain_data[17:]
    
    print(f"Plain Data Structure:")
    print(f"  NewKey:  {hexlify(new_key_in_plain).decode().upper()}")
    print(f"  Version: {version:02X}")
    print(f"  Padding: {hexlify(padding).decode().upper()}")
    
    if new_key_in_plain == derived_key:
        print(f"  ✅ NewKey matches derived key")
    else:
        print(f"  ❌ NewKey MISMATCH!")
        print(f"     Expected: {hexlify(derived_key).decode().upper()}")
    
    if version == 0x01:
        print(f"  ✅ Version correct (0x01)")
    else:
        print(f"  ❌ Version wrong: {version:02X} (expected 0x01)")
    
    if padding[0] == 0x80 and all(b == 0 for b in padding[1:]):
        print(f"  ✅ Padding correct (0x80 + zeros)")
    else:
        print(f"  ❌ Padding wrong: {hexlify(padding).decode().upper()}")

print()

# More verification can be added here
print("───────────────────────────────────────────────────────────")
print("Run this script after filling in ESP32 debug values!")
