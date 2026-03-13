#!/usr/bin/env python3
"""
Calculate expected ChangeKey values for verification
Compares ESP32 output with expected cryptographic values
"""

import hmac
import hashlib
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from binascii import hexlify, unhexlify

# Test card UID (paste from ESP32 serial output)
TEST_UID = "04:89:4D:02:E5:75:80"

# Master secret
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Factory key
FACTORY_KEY = bytes(16)

# Example session values (will be different each time due to random RndA/RndB)
# But we can verify the key derivation

def derive_master_key(master_secret_hex, uid_string):
    """Derive K0 using HMAC-SHA256"""
    # Clean UID
    uid_clean = uid_string.replace(":", "").upper()
    uid_bytes = bytes.fromhex(uid_clean)
    
    master_secret = bytes.fromhex(master_secret_hex)
    
    # Data: UID || "K0" || 0x01
    data = uid_bytes + b"K0" + bytes([1])
    
    print(f"HMAC Input: {hexlify(data).decode().upper()}")
    
    # HMAC-SHA256
    h = hmac.new(master_secret, data, hashlib.sha256)
    hash_result = h.digest()
    
    # Take first 16 bytes
    return hash_result[:16]

def cmac_truncate(cmac_full):
    """Truncate CMAC to 8 bytes (odd indices)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

def verify_changekey_mac(session_mac_key_hex, cmd_counter, ti_hex, new_key_hex, enc_data_hex):
    """
    Verify ChangeKey MAC calculation
    """
    print("\n" + "="*60)
    print("CHANGEKEY MAC VERIFICATION")
    print("="*60)
    
    session_mac_key = bytes.fromhex(session_mac_key_hex)
    ti = bytes.fromhex(ti_hex)
    enc_data = bytes.fromhex(enc_data_hex)
    
    # MAC Input: Cmd || CmdCtr || TI || KeyNo || EncData
    mac_input = bytes([0xC4])  # ChangeKey
    mac_input += bytes([cmd_counter & 0xFF, (cmd_counter >> 8) & 0xFF])  # LSB first
    mac_input += ti
    mac_input += bytes([0x00])  # KeyNo = 0
    mac_input += enc_data
    
    print(f"\nMAC Input ({len(mac_input)} bytes):")
    print(f"  Cmd:       C4")
    print(f"  CmdCtr:    {cmd_counter & 0xFF:02X}{(cmd_counter >> 8) & 0xFF:02X} (LSB first, value={cmd_counter})")
    print(f"  TI:        {ti_hex}")
    print(f"  KeyNo:     00")
    print(f"  EncData:   {enc_data_hex}")
    print(f"\nFull: {hexlify(mac_input).decode().upper()}")
    
    # Calculate CMAC
    c = CMAC.new(session_mac_key, ciphermod=AES)
    c.update(mac_input)
    cmac_full = c.digest()
    
    cmac_trunc = cmac_truncate(cmac_full)
    
    print(f"\nCMAC Full:      {hexlify(cmac_full).decode().upper()}")
    print(f"CMAC Truncated: {hexlify(cmac_trunc).decode().upper()}")
    
    return cmac_trunc

def verify_commit_mac(session_mac_key_hex, cmd_counter, ti_hex):
    """
    Verify CommitTransaction MAC calculation
    """
    print("\n" + "="*60)
    print("COMMIT TRANSACTION MAC VERIFICATION")
    print("="*60)
    
    session_mac_key = bytes.fromhex(session_mac_key_hex)
    ti = bytes.fromhex(ti_hex)
    
    # MAC Input: Cmd || CmdCtr || TI || 0x00
    mac_input = bytes([0xC7])  # CommitTransaction
    mac_input += bytes([cmd_counter & 0xFF, (cmd_counter >> 8) & 0xFF])  # LSB first
    mac_input += ti
    mac_input += bytes([0x00])  # No data
    
    print(f"\nMAC Input ({len(mac_input)} bytes):")
    print(f"  Cmd:       C7")
    print(f"  CmdCtr:    {cmd_counter & 0xFF:02X}{(cmd_counter >> 8) & 0xFF:02X} (LSB first, value={cmd_counter})")
    print(f"  TI:        {ti_hex}")
    print(f"  NoData:    00")
    print(f"\nFull: {hexlify(mac_input).decode().upper()}")
    
    # Calculate CMAC
    c = CMAC.new(session_mac_key, ciphermod=AES)
    c.update(mac_input)
    cmac_full = c.digest()
    
    cmac_trunc = cmac_truncate(cmac_full)
    
    print(f"\nCMAC Full:      {hexlify(cmac_full).decode().upper()}")
    print(f"CMAC Truncated: {hexlify(cmac_trunc).decode().upper()}")
    
    return cmac_trunc

print("="*60)
print("NTAG424 ChangeKey Verification Tool")
print("="*60)

# Step 1: Derive new key
print(f"\n[1] KEY DERIVATION")
print(f"UID: {TEST_UID}")
print(f"Master Secret: {MASTER_SECRET}")

new_key = derive_master_key(MASTER_SECRET, TEST_UID)
print(f"\nDerived K0: {hexlify(new_key).decode().upper()}")

print("\n" + "="*60)
print("INSTRUCTIONS FOR ESP32 TESTING")
print("="*60)
print("""
1. Start write mode op ESP32 web interface
2. Scan een SCHONE factory kaart
3. Kopieer de volgende waarden uit de serial output:
   - SessionMAC key
   - Transaction ID (TI)
   - Encrypted data (na ChangeKey)
   - Command counter (CmdCtr)

4. Voer deze waarden hieronder in om de MAC te verifiëren
""")

print("\n" + "="*60)
print("MANUAL VERIFICATION")
print("="*60)

# voorbeeld waarden voor demonstratie
print("\nVoorbeeld met dummy waarden:")
print("(Vervang dit met echte waarden uit ESP32 serial output)")

example_session_mac = "4EE070EA9FBBD71059DA5869630DCC70"
example_ti = "5DF7BC27"
example_cmd_ctr = 0
example_enc_data = "74148508306B50A9B3C32D384558CBC1FE1B03749CA1216AE78B5A855C8A426C"

changekey_mac = verify_changekey_mac(
    example_session_mac, 
    example_cmd_ctr, 
    example_ti,
    hexlify(new_key).decode().upper(),
    example_enc_data
)

commit_mac = verify_commit_mac(
    example_session_mac,
    example_cmd_ctr + 1,  # After ChangeKey
    example_ti
)

print("\n" + "="*60)
print("EXPECTED APDU COMMANDS")
print("="*60)

print(f"\nChangeKey APDU:")
print(f"  90 C4 00 00 29 00 {example_enc_data} {hexlify(changekey_mac).decode().upper()} 00")

print(f"\nCommitTransaction APDU:")
print(f"  90 C7 00 00 08 {hexlify(commit_mac).decode().upper()} 00")

print("\n" + "="*60)
print("TROUBLESHOOTING")
print("="*60)
print("""
Als CommitTransaction SW=911C geeft:
1. Controleer of ChangeKey MAC EXACT overeenkomt met ESP32 output
2. Controleer CmdCtr byte order (moet LSB first zijn)
3. Controleer of TI correct is (4 bytes, MSB first)

Als CommitTransaction SW=9100 geeft:
✅ SUCCESS! Key change was committed!
   Nieuwe key werkt en is permanent opgeslagen
""")
