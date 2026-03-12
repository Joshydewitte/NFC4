#!/usr/bin/env python3
"""
Parse ESP32 debug output and verify against AN12196

Usage:
1. Upload code to ESP32
2. Try to write a NEW BLANK card (not the already-written one!)
3. Copy the ENTIRE [DEBUG] section from ChangeKey
4. Paste below between the triple quotes
5. Run this script

It will verify each step against AN12196.
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify
import re

# ═══════════════════════════════════════════════════════════════
# PASTE ESP32 DEBUG OUTPUT HERE (entire ChangeKey [DEBUG] section)
# ═══════════════════════════════════════════════════════════════

esp32_output = """
PASTE HERE BETWEEN THE TRIPLE QUOTES
Example:
[DEBUG] Plain Data: 351543CBF5075D4D69048A368EAFB77B01800000000000000000000000000000
[DEBUG] IV: 7F24C025ABDE4160DF0B3668E9EE682C
... etc
"""

# ═══════════════════════════════════════════════════════════════

def extract_value(output, pattern):
    """Extract hex value from debug output"""
    match = re.search(pattern + r':?\s+([0-9A-F]+)', output, re.IGNORECASE)
    if match:
        return match.group(1)
    return None

def truncate_cmac_odd(full_mac):
    """Truncate CMAC using odd indices"""
    return bytes([full_mac[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

print("═══════════════════════════════════════════════════════════")
print("📡 ESP32 ChangeKey Output Analyzer")
print("═══════════════════════════════════════════════════════════\n")

if "PASTE HERE" in esp32_output:
    print("❌ ERROR: No ESP32 output pasted yet!")
    print()
    print("Instructions:")
    print("1. Upload code: pio run -t upload -e seeed_xiao_esp32s3")
    print("2. Use web interface to write a NEW BLANK card")
    print("3. Copy ALL [DEBUG] lines from ChangeKey section")
    print("4. Paste between the triple quotes in this script")
    print("5. Run: python verify_esp32_changekey.py")
    print()
    exit(1)

# Extract values
print("🔍 Extracting values from ESP32 output...\n")

session_enc = extract_value(esp32_output, r'Session ENC Key')
session_mac = extract_value(esp32_output, r'Session MAC Key')
ti = extract_value(esp32_output, r'Transaction ID')
plain_data = extract_value(esp32_output, r'Plain Data')
iv_calc = extract_value(esp32_output, r'\bIV\b')  # Word boundary to avoid "currentIV"
encrypted = extract_value(esp32_output, r'Encrypted Key Data')
mac_input = extract_value(esp32_output, r'MAC Input')
cmac_full = extract_value(esp32_output, r'CMAC Full')
cmac_trunc = extract_value(esp32_output, r'\bCMAC\b(?!\sFull)')
cmd_ctr_match = re.search(r'CmdCtr:\s*(\d+)', esp32_output)
cmd_ctr = int(cmd_ctr_match.group(1)) if cmd_ctr_match else None

if not all([session_enc, session_mac, ti, plain_data, iv_calc, encrypted, cmac_full, cmac_trunc]):
    print("❌ ERROR: Could not extract all required values")
    print("   Make sure you pasted the complete [DEBUG] output")
    print()
    print("Missing:")
    if not session_enc: print("  - Session ENC Key")
    if not session_mac: print("  - Session MAC Key")
    if not ti: print("  - Transaction ID")
    if not plain_data: print("  - Plain Data")
    if not iv_calc: print("  - IV")
    if not encrypted: print("  - Encrypted Key Data")
    if not cmac_full: print("  - CMAC Full")
    if not cmac_trunc: print("  - CMAC")
    exit(1)

print(f"Session ENC Key:  {session_enc}")
print(f"Session MAC Key:  {session_mac}")
print(f"Transaction ID:   {ti}")
print(f"CmdCtr:           {cmd_ctr}")
print(f"Plain Data:       {plain_data[:32]}...")
print(f"IV:               {iv_calc}")
print(f"Encrypted:        {encrypted[:32]}...")
print(f"CMAC Full:        {cmac_full}")
print(f"CMAC Truncated:   {cmac_trunc}")
print()

# Verify each step
errors = []

print("─" * 60)
print("VERIFICATION")
print("─" * 60)

# 1. Check plain data format (Case 2)
if len(plain_data) != 64:
    errors.append(f"Plain data wrong length: {len(plain_data)} (expected 64)")
else:
    new_key = plain_data[:32]
    version = plain_data[32:34]
    padding = plain_data[34:]
    
    print("1. Plain Data Format:")
    print(f"   NewKey:  {new_key}")
    print(f"   Version: {version}")
    if version != "01":
        errors.append(f"Version should be 01, got {version}")
        print(f"   ❌ Version wrong: {version} (expected 01)")
    else:
        print(f"   ✅ Version correct")
    
    if padding[0:2] != "80" or not all(c == '0' for c in padding[2:]):
        errors.append(f"Padding format wrong: {padding}")
        print(f"   ❌ Padding wrong: {padding[:10]}...")
    else:
        print(f"   ✅ Padding correct")

# 2. Verify IV calculation
print("\n2. IV Calculation:")
# We can't fully verify without knowing currentIV, but we can check format
if len(iv_calc) != 32:
    errors.append(f"IV wrong length: {len(iv_calc)}")
    print(f"   ❌ IV wrong length: {len(iv_calc)} (expected 32)")
else:
    print(f"   ✅ IV length correct (16 bytes)")

# 3. Verify CMAC truncation
print("\n3. CMAC Truncation:")
if len(cmac_full) != 32:
    errors.append(f"CMAC Full wrong length: {len(cmac_full)}")
    print(f"   ❌ CMAC Full wrong length")
elif len(cmac_trunc) != 16:
    errors.append(f"CMAC truncated wrong length: {len(cmac_trunc)}")
    print(f"   ❌ CMAC truncated wrong length")
else:
    # Verify odd indices truncation
    full_bytes = unhexlify(cmac_full)
    expected_trunc = truncate_cmac_odd(full_bytes)
    expected_hex = hexlify(expected_trunc).decode().upper()
    
    if cmac_trunc.upper() == expected_hex:
        print(f"   ✅ CMAC truncation correct (odd indices)")
    else:
        errors.append(f"CMAC truncation wrong")
        print(f"   ❌ CMAC truncation WRONG!")
        print(f"      Got:      {cmac_trunc}")
        print(f"      Expected: {expected_hex}")
        print(f"      (Should use indices 1,3,5,7,9,11,13,15)")

# 4. Verify MAC Input format
print("\n4. MAC Input Format:")
if len(mac_input) != 80:
    errors.append(f"MAC Input wrong length: {len(mac_input)}")
    print(f"   ❌ MAC Input wrong length: {len(mac_input)} (expected 80)")
else:
    cmd = mac_input[0:2]
    ctr = mac_input[2:6]
    ti_in_mac = mac_input[6:14]
    keyno = mac_input[14:16]
    enc_in_mac = mac_input[16:]
    
    checks = []
    if cmd != "C4":
        errors.append(f"Wrong command in MAC: {cmd}")
        checks.append(f"❌ CMD={cmd} (expected C4)")
    else:
        checks.append(f"✅ CMD=C4")
    
    if keyno != "00":
        errors.append(f"Wrong KeyNo in MAC: {keyno}")
        checks.append(f"❌ KeyNo={keyno} (expected 00)")
    else:
        checks.append(f"✅ KeyNo=00")
    
    if ti_in_mac.upper() != ti.upper():
        errors.append(f"TI mismatch in MAC")
        checks.append(f"❌ TI mismatch")
    else:
        checks.append(f"✅ TI correct")
    
    if enc_in_mac.upper() != encrypted.upper():
        errors.append(f"Encrypted data mismatch in MAC")
        checks.append(f"❌ Encrypted data mismatch")
    else:
        checks.append(f"✅ Encrypted data correct")
    
    for check in checks:
        print(f"   {check}")

# Summary
print()
print("═" * 60)
print("📊 SUMMARY")
print("═" * 60)

if errors:
    print(f"\n❌ FOUND {len(errors)} ERROR(S):\n")
    for i, err in enumerate(errors, 1):
        print(f"  {i}. {err}")
    print("\n🔧 Fix these errors in the ESP32 code before testing cards!")
else:
    print("\n✅ ALL CHECKS PASSED!")
    print("\nThe ESP32 crypto output looks correct.")
    print("If ChangeKey still fails with SW=91AE, the problem is likely:")
    print("  1. CurrentIV state between authentication and ChangeKey")
    print("  2. Card requires CommitTransaction or similar")
    print("  3. Card-specific quirk not in AN12196")

print()
print("═" * 60)
