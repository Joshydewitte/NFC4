#!/usr/bin/env python3
"""
Diagnose ChangeKey implementation against AN12196 Table 27
WITHOUT touching any cards!

This script will:
1. Generate test vectors for Case 2 (KeyNo == AuthKey)
2. Compare with AN12196 Table 27 example
3. Identify exact mismatches
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify
import sys

def bytes_to_hex(data):
    return hexlify(data).decode().upper()

def truncate_cmac_odd(full_mac):
    """Truncate CMAC using odd indices (1,3,5,7,9,11,13,15)"""
    return bytes([full_mac[i] for i in [1, 3, 5, 7, 9, 11, 13, 15]])

def derive_session_keys(rndA, rndB, key):
    """Derive session keys according to AN12196 Section 9.1.1"""
    sv1_label = bytes([0xA5, 0x5A, 0x00, 0x01, 0x00, 0x80])
    sv2_label = bytes([0x5A, 0xA5, 0x00, 0x01, 0x00, 0x80])
    
    rndA_tail = rndA[14:16]  # Last 2 bytes
    rndB_head = rndB[0:2]    # First 2 bytes
    xor_result = bytes([rndA[i] ^ rndB[i] for i in range(16)])
    
    sv1 = sv1_label + rndA_tail + xor_result + rndB_head
    sv2 = sv2_label + rndA_tail + xor_result + rndB_head
    
    cobj = CMAC.new(key, ciphermod=AES)
    cobj.update(sv1)
    enc_key = cobj.digest()
    
    cobj = CMAC.new(key, ciphermod=AES)
    cobj.update(sv2)
    mac_key = cobj.digest()
    
    return enc_key, mac_key

print("═══════════════════════════════════════════════════════════")
print("🔬 ChangeKey Diagnostic Tool - AN12196 Table 27 Test")
print("═══════════════════════════════════════════════════════════\n")

# AN12196 Table 27 example values
print("📖 Loading AN12196 Table 27 reference values...\n")

# Authentication values (steps 1-9 in Table 27)
old_key = unhexlify("00000000000000000000000000000000")
new_key = unhexlify("5004BF991F408672B1EF00F08F9E8647")
ti = unhexlify("7614281A")
cmd_ctr = 3
session_enc_key = unhexlify("4CF3CB41A22583A61E89B158D252FC53")
session_mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")

print(f"Old Key:         {bytes_to_hex(old_key)}")
print(f"New Key:         {bytes_to_hex(new_key)}")
print(f"Key Version:     01")
print(f"TI:              {bytes_to_hex(ti)}")
print(f"CmdCtr:          {cmd_ctr}")
print(f"SessionENCKey:   {bytes_to_hex(session_enc_key)}")
print(f"SessionMACKey:   {bytes_to_hex(session_mac_key)}")
print()

# ══════════════════════════════════════════════════════════════
# STEP 1: Prepare plaintext (Case 2)
# ══════════════════════════════════════════════════════════════
print("─" * 60)
print("STEP 1: Prepare Plaintext (Case 2)")
print("─" * 60)

plain_data = bytearray(32)
plain_data[0:16] = new_key      # NewKey (16 bytes)
plain_data[16] = 0x01           # Version
plain_data[17] = 0x80           # Padding indicator
# plain_data[18:32] already zeros

expected_plain = "5004BF991F408672B1EF00F08F9E864701800000000000000000000000000000"
calculated_plain = bytes_to_hex(plain_data)

print(f"Expected:   {expected_plain}")
print(f"Calculated: {calculated_plain}")
if calculated_plain == expected_plain:
    print("✅ MATCH\n")
else:
    print("❌ MISMATCH!\n")
    sys.exit(1)

# ══════════════════════════════════════════════════════════════
# STEP 2: Calculate IV
# ══════════════════════════════════════════════════════════════
print("─" * 60)
print("STEP 2: Calculate IV")
print("─" * 60)

# IV Input = A55A || TI || CmdCtr || Padding
iv_input = bytearray(16)
iv_input[0:2] = bytes([0xA5, 0x5A])
iv_input[2:6] = ti
iv_input[6] = cmd_ctr & 0xFF         # LSB first
iv_input[7] = (cmd_ctr >> 8) & 0xFF
# iv_input[8:16] already zeros

expected_iv_input = "A55A7614281A03000000000000000000"
calculated_iv_input = bytes_to_hex(iv_input)

print(f"IV Input:")
print(f"  Expected:   {expected_iv_input}")
print(f"  Calculated: {calculated_iv_input}")
if calculated_iv_input == expected_iv_input:
    print("  ✅ MATCH")
else:
    print("  ❌ MISMATCH!")
    sys.exit(1)

# Calculate IV using ZERO IV (first command after auth)
# AN12196 Note: For first command after auth, currentIV = last block of auth response
# But in Table 27, they show the calculation - let me check if they use zero or not
# Actually, looking at Table 27 Step 12, they directly show IVc result
# The IV is calculated as E(SessionENCKey, IV_Input)
# The currentIV for chaining should be the last block from authentication response
# For testing, let's assume currentIV = zeros (will need to verify this)

current_iv = bytes(16)  # Zero IV for first command after auth
cipher = AES.new(session_enc_key, AES.MODE_CBC, iv=current_iv)
iv_calculated = cipher.encrypt(bytes(iv_input))

expected_iv = "01602D579423B2797BE8B478B0B4D27B"
calculated_iv = bytes_to_hex(iv_calculated)

print(f"\nIV Calculated (using currentIV={bytes_to_hex(current_iv)[:16]}...):")
print(f"  Expected:   {expected_iv}")
print(f"  Calculated: {calculated_iv}")
if calculated_iv == expected_iv:
    print("  ✅ MATCH\n")
else:
    print("  ❌ MISMATCH!")
    print("\n⚠️  This might be because currentIV is not zero.")
    print("    In AN12196 Table 27, CmdCtr=3 means this is NOT the first command.")
    print("    The currentIV would be from a previous command's last block.\n")

# ══════════════════════════════════════════════════════════════
# STEP 3: Encrypt plaintext
# ══════════════════════════════════════════════════════════════
print("─" * 60)
print("STEP 3: Encrypt Plaintext")
print("─" * 60)

cipher = AES.new(session_enc_key, AES.MODE_CBC, iv=iv_calculated)
encrypted_data = cipher.encrypt(bytes(plain_data))

expected_encrypted = "C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD"
calculated_encrypted = bytes_to_hex(encrypted_data)

print(f"Expected:   {expected_encrypted}")
print(f"Calculated: {calculated_encrypted}")
if calculated_encrypted == expected_encrypted:
    print("✅ MATCH\n")
else:
    print("❌ MISMATCH!\n")
    if calculated_iv != expected_iv:
        print("    (Expected since IV was already wrong)\n")

# ══════════════════════════════════════════════════════════════
# STEP 4: Calculate CMAC
# ══════════════════════════════════════════════════════════════
print("─" * 60)
print("STEP 4: Calculate CMAC")
print("─" * 60)

# MAC Input = Cmd || CmdCtr || TI || KeyNo || EncData
mac_input = bytearray()
mac_input.append(0xC4)  # CMD
mac_input.append(cmd_ctr & 0xFF)         # CmdCtr LSB first
mac_input.append((cmd_ctr >> 8) & 0xFF)
mac_input.extend(ti)
mac_input.append(0x00)  # KeyNo
mac_input.extend(encrypted_data)

expected_mac_input = "C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD"
calculated_mac_input = bytes_to_hex(mac_input)

print(f"MAC Input:")
print(f"  Expected:   {expected_mac_input}")
print(f"  Calculated: {calculated_mac_input}")
if calculated_mac_input == expected_mac_input:
    print("  ✅ MATCH")
else:
    print("  ❌ MISMATCH!")

# Calculate CMAC
cobj = CMAC.new(session_mac_key, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()

expected_cmac_full = "B7A60161F202EC3489BD4BEDEF64BB32"
calculated_cmac_full = bytes_to_hex(cmac_full)

print(f"\nCMAC Full:")
print(f"  Expected:   {expected_cmac_full}")
print(f"  Calculated: {calculated_cmac_full}")
if calculated_cmac_full == expected_cmac_full:
    print("  ✅ MATCH")
else:
    print("  ❌ MISMATCH!")

# Truncate CMAC
cmac_truncated = truncate_cmac_odd(cmac_full)

expected_cmac_t = "A6610234BDED6432"
calculated_cmac_t = bytes_to_hex(cmac_truncated)

print(f"\nCMACt (truncated):")
print(f"  Expected:   {expected_cmac_t}")
print(f"  Calculated: {calculated_cmac_t}")
if calculated_cmac_t == expected_cmac_t:
    print("  ✅ MATCH\n")
else:
    print("  ❌ MISMATCH!\n")

# ══════════════════════════════════════════════════════════════
# SUMMARY
# ══════════════════════════════════════════════════════════════
print("═" * 60)
print("📊 DIAGNOSTIC SUMMARY")
print("═" * 60)
print()

all_match = (
    calculated_plain == expected_plain and
    calculated_iv_input == expected_iv_input and
    calculated_encrypted == expected_encrypted and
    calculated_mac_input == expected_mac_input and
    calculated_cmac_full == expected_cmac_full and
    calculated_cmac_t == expected_cmac_t
)

if all_match:
    print("✅ ALL CHECKS PASSED!")
    print()
    print("The crypto implementation matches AN12196 Table 27 perfectly.")
    print("The problem might be:")
    print("  1. CurrentIV tracking between commands")
    print("  2. Card-specific behavior not in specification")
    print("  3. Hidden state that needs reset/commit")
else:
    print("❌ SOME CHECKS FAILED")
    print()
    print("The crypto implementation has bugs that need fixing.")
    print("Review the mismatches above and fix the code accordingly.")

print()
print("═" * 60)
