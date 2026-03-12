#!/usr/bin/env python3
"""
Test of K0 correct op de kaart geschreven is door authenticatie te proberen
"""

import hmac
import hashlib

print("="*80)
print("KAART VERIFICATIE TEST")
print("="*80)

# Kaart info
CARD_UID = "04:87:80:02:E5:75:80"
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Clean UID
uid_clean = CARD_UID.replace(":", "").replace(" ", "").upper()
uid_bytes = bytes.fromhex(uid_clean)

print(f"\nKaart UID: {CARD_UID}")
print(f"Master Secret: {MASTER_SECRET}")

# Derive expected K0
data = uid_bytes + b"K0" + bytes([1])
hmac_result = hmac.new(bytes.fromhex(MASTER_SECRET), data, hashlib.sha256).digest()
expected_k0 = hmac_result[:16]

print(f"\n{'='*80}")
print(f"VERWACHTE KEY OP KAART")
print(f"{'='*80}")
print(f"K0: {expected_k0.hex().upper()}")

print(f"\n{'='*80}")
print(f"VERIFICATIE INSTRUCTIES VOOR ESP32")
print(f"{'='*80}")
print(f"\n1. Ga naar Config Mode")
print(f"2. Leg de kaart op de reader")
print(f"3. ESP32 zal proberen te authenticeren met:")
print(f"   - Factory key (00...00) → Zou MOETEN FALEN")
print(f"   - Personalized key ({expected_k0.hex().upper()}) → Zou MOETEN SLAGEN")
print(f"\n4. Als personalized key werkt:")
print(f"   ✅ Kaart correct gepersonaliseerd!")
print(f"   Server zou moeten tonen: 'GEPERSONALISEERD (custom keys)'")
print(f"\n5. Als beide falen:")
print(f"   ⚠️  Probleem met schrijven of key derivation")

print(f"\n{'='*80}")
print(f"SERVER API TEST")
print(f"{'='*80}")
print(f"\nTest de server key derivation:")
print(f"curl http://your-server:3000/api/key/{uid_clean}/master")
print(f"\nVerwachte output:")
print(f'{{"uid":"{uid_clean}","keyType":"master","version":1,"key":"{expected_k0.hex().upper()}"}}')

print(f"\n{'='*80}")
