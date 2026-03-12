#!/usr/bin/env python3
"""
Debug recovery key derivation probleem
"""

import hmac
import hashlib

CARD_UID = "04:87:80:02:e5:75:80"
uid_bytes = bytes.fromhex(CARD_UID.replace(":", ""))

print("="*80)
print("RECOVERY KEY PROBLEEM DEBUG")
print("="*80)

# Test 1: Wat de ESP32 nu doet
print("\n[WAT ESP32 NU DOET]")
print("-" * 80)

# ESP32 zegt: "Previous master secret: 24D086AA..."
# Dit suggereert dat je 24D086AA... hebt ingevuld

possible_inputs = [
    ("K0 key (fout!)", "24D086AA673E88BFADA9FEB3DBF5B83A"),
    ("Truncated K0", "24D086AA00000000000000000000000000"),
    ("Alleen eerste 8", "24D086AA24D086AA24D086AA24D086AA"),
]

for desc, test_secret in possible_inputs:
    data = uid_bytes + b"K0" + bytes([1])
    hmac_result = hmac.new(bytes.fromhex(test_secret), data, hashlib.sha256).digest()
    derived_k0 = hmac_result[:16]
    
    print(f"\nTest: {desc}")
    print(f"  Input secret:  {test_secret}")
    print(f"  Derived K0:    {derived_k0.hex().upper()}")
    
    if derived_k0.hex().upper() == "CDBB36B48A6B12CC0A1B1984E76F99C2":
        print(f"  ✅ MATCH! Dit is wat je hebt ingevuld!")

# Test 2: Wat het MOET zijn
print("\n" + "="*80)
print("[WAT HET ZOU MOETEN ZIJN]")
print("-" * 80)

CORRECT_OLD_SECRET = "A54C788525178060913D4CFC06380D1B"
data = uid_bytes + b"K0" + bytes([1])
hmac_result = hmac.new(bytes.fromhex(CORRECT_OLD_SECRET), data, hashlib.sha256).digest()
correct_old_k0 = hmac_result[:16]

print(f"\nCorrect Previous Master Secret: {CORRECT_OLD_SECRET}")
print(f"Correct Derived Old K0:         {correct_old_k0.hex().upper()}")

# Test 3: De nieuwe key
print("\n" + "="*80)
print("[NIEUWE KEY (voor reset naar factory)]")
print("-" * 80)

NEW_SECRET = "00000000000000000000000000000000"
hmac_result = hmac.new(bytes.fromhex(NEW_SECRET), data, hashlib.sha256).digest()
new_k0 = hmac_result[:16]

print(f"\nNew Master Secret:     {NEW_SECRET}")
print(f"New Derived K0:        {new_k0.hex().upper()}")
print(f"ESP32 berekende:       CDF172D3C9F8328B93A40E94D3C6A8F8")

if new_k0.hex().upper() == "CDF172D3C9F8328B93A40E94D3C6A8F8":
    print("✅ New key derivation is correct!")
else:
    print("❌ New key derivation mismatch!")

# Samenvatting
print("\n" + "="*80)
print("OPLOSSING")
print("="*80)
print("\nOm de kaart te recoveren naar factory, vul in:")
print(f"  ☑️  Factory Card:     UIT (unchecked)")
print(f"  ☑️  Direct Key Mode:  UIT (unchecked)")
print(f"  📝 Previous Key:     {CORRECT_OLD_SECRET}")
print(f"  📝 Master Secret:    {NEW_SECRET}")
print(f"\nDan zal authenticatie werken met: {correct_old_k0.hex().upper()}")
print(f"En schrijven:                      {new_k0.hex().upper()}")

print("\n" + "="*80)
print("OF: DIRECT KEY MODE")
print("="*80)
print("\nAlternatief: gebruik de AES key direct:")
print(f"  ☑️  Factory Card:     UIT (unchecked)")
print(f"  ☑️  Direct Key Mode:  AAN (checked)  ✅")
print(f"  📝 Previous Key:     {correct_old_k0.hex().upper()}")
print(f"  📝 Master Secret:    {NEW_SECRET}")
print(f"\nDan gebruikt hij de key direct zonder derivation!")

print("\n" + "="*80)
