#!/usr/bin/env python3
"""
Calculate keys that were written with OLD (incorrect) firmware
vs keys that should be written with NEW (correct) firmware
"""

import hmac
import hashlib

def old_derivation_ascii(master_secret_hex, uid_hex, key_version=0x01):
    """
    OLD (INCORRECT) method: Used ASCII string concatenation
    This is what the old firmware did - it's WRONG
    """
    # The old code did: String data = uid + "K0" + String(keyVersion);
    # This created an ASCII string like "04D8E1629D498004854K01"
    # Then it hashed this ASCII string
    
    data_string = uid_hex + "K0" + str(key_version)
    data_ascii = data_string.encode('ascii')
    
    # Master secret was also used as ASCII hex string
    master_secret_ascii = master_secret_hex.encode('ascii')
    
    # HMAC-SHA256 with ASCII strings
    hmac_result = hmac.new(master_secret_ascii, data_ascii, hashlib.sha256).digest()
    
    # Truncate to 16 bytes
    key = hmac_result[:16]
    return key.hex().upper()


def new_derivation_bytes(master_secret_hex, uid_hex, key_version=0x01):
    """
    NEW (CORRECT) method: Uses raw byte arrays
    This is what the new firmware does
    """
    # Convert hex strings to byte arrays
    master_secret_bytes = bytes.fromhex(master_secret_hex)
    uid_bytes = bytes.fromhex(uid_hex)
    
    # Concatenate: UID_bytes || "K0" || version_byte
    data = uid_bytes + b'K0' + bytes([key_version])
    
    # HMAC-SHA256 with raw bytes
    hmac_result = hmac.new(master_secret_bytes, data, hashlib.sha256).digest()
    
    # Truncate to 16 bytes
    key = hmac_result[:16]
    return key.hex().upper()


def main():
    print("=" * 60)
    print("NTAG424 Key Derivation Calculator")
    print("=" * 60)
    print()
    
    # Input
    master_secret = input("Master Secret (32 hex chars): ").strip().upper()
    uid = input("Card UID (14 hex chars): ").strip().upper()
    
    if len(master_secret) != 32:
        print("❌ Master secret moet 32 hex karakters zijn!")
        return
    
    if len(uid) != 14:
        print("❌ UID moet 14 hex karakters zijn!")
        return
    
    print()
    print("-" * 60)
    print(f"Master Secret: {master_secret}")
    print(f"Card UID:      {uid}")
    print("-" * 60)
    print()
    
    # Calculate with both methods
    old_key = old_derivation_ascii(master_secret, uid)
    new_key = new_derivation_bytes(master_secret, uid)
    
    print("OLD FIRMWARE (incorrect ASCII method):")
    print(f"  Key on card: {old_key}")
    print()
    print("NEW FIRMWARE (correct byte method):")
    print(f"  Key derived: {new_key}")
    print()
    
    if old_key == new_key:
        print("✅ Keys are the same - no problem!")
    else:
        print("❌ Keys are DIFFERENT!")
        print()
        print("Your card has the OLD key stored on it.")
        print("To authenticate, you need to use the OLD key shown above.")
        print()
        print("To fix: Write new key with corrected firmware using")
        print(f"  Previous Key: {old_key}")
        print(f"  New Key:      {new_key}")


if __name__ == "__main__":
    main()
