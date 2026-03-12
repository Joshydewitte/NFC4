#!/usr/bin/env python3
"""
Test verschillende scenario's voor wat er op de kaarten kan staan
"""

import hmac
import hashlib

master_secret = "A54C788525178060913D4CFC06380D1B"
card1_uid = "048F4F02E57580"

print("=" * 80)
print("NTAG424 Key Possibilities Test")
print("=" * 80)
print()

# Scenario 1: Factory key
print("SCENARIO 1: Factory Key (kaarten nog niet gepersonaliseerd)")
print("-" * 80)
print("  Factory Key: 00000000000000000000000000000000")
print("  → Probeer met 'Factory Card' checkbox AAN")
print()

# Scenario 2: Oude firmware - ASCII methode (wat ik eerder berekende)
print("SCENARIO 2: Oude firmware ASCII methode")
print("-" * 80)
data_string = card1_uid + "K0" + str(1)
data_ascii = data_string.encode('ascii')
master_secret_ascii = master_secret.encode('ascii')
hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
old_key_ascii = hmac_result[:16].hex().upper()
print(f"  Key: {old_key_ascii}")
print()

# Scenario 3: Misschien gebruikte oude firmware string.c_str() maar wel byte HMAC?
print("SCENARIO 3: Hex string als bytes (incorrecte conversie)")
print("-" * 80)
# Als de oude code masterSecret.c_str() gebruikte maar als bytes interpreteerde
master_bytes_wrong = master_secret.encode('ascii')  # "A54C78..." als bytes
uid_bytes = bytes.fromhex(card1_uid)
data = uid_bytes + b'K0' + bytes([1])
hmac_result = hmac.new(master_bytes_wrong, data, hashlib.sha256).digest()
old_key_wrong = hmac_result[:16].hex().upper()
print(f"  Key: {old_key_wrong}")
print()

# Scenario 4: Correcte nieuwe methode
print("SCENARIO 4: Nieuwe (correcte) firmware")
print("-" * 80)
master_bytes = bytes.fromhex(master_secret)
uid_bytes = bytes.fromhex(card1_uid)
data = uid_bytes + b'K0' + bytes([1])
hmac_result = hmac.new(master_bytes, data, hashlib.sha256).digest()
new_key = hmac_result[:16].hex().upper()
print(f"  Key: {new_key}")
print()

print("=" * 80)
print("VRAAG: Weet je zeker dat je de kaarten al gepersonaliseerd hebt?")
print("       (dus dat je ze al eerder met Write Cards hebt geschreven)")
print()
print("Als je ze NIET eerder hebt geschreven:")
print("  → Gebruik Factory Card checkbox AAN")
print("  → Laat Previous Key leeg")
print("=" * 80)
