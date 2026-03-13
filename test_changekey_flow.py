#!/usr/bin/env python3
"""
Test NTAG424 DNA ChangeKey Flow
Simulates ESP32 behavior: Authenticate → ChangeKey → CommitTransaction → Verify
Tests against REAL CARD via NFC reader
"""

import nfc
import os
import hmac
import hashlib
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from binascii import hexlify, unhexlify

# ============================================================
# CONFIGURATION
# ============================================================

# Master secret (same as ESP32)
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Factory key
FACTORY_KEY = bytes(16)  # All zeros

# Test with a UID (will be read from card)
TEST_UID = None  # Will be populated from card

# ============================================================
# CRYPTO FUNCTIONS
# ============================================================

def derive_master_key(master_secret_hex, uid_bytes):
    """Derive K0 using HMAC-SHA256 (same as ESP32)"""
    master_secret = bytes.fromhex(master_secret_hex)
    
    # Data: UID || "K0" || 0x01
    data = uid_bytes + b"K0" + bytes([1])
    
    # HMAC-SHA256
    h = hmac.new(master_secret, data, hashlib.sha256)
    hash_result = h.digest()
    
    # Take first 16 bytes
    return hash_result[:16]

def aes_encrypt_cbc(key, iv, data):
    """AES-128 CBC encryption"""
    cipher = AES.new(key, AES.MODE_CBC, iv)
    return cipher.encrypt(data)

def aes_decrypt_cbc(key, iv, data):
    """AES-128 CBC decryption"""
    cipher = AES.new(key, AES.MODE_CBC, iv)
    return cipher.decrypt(data)

def cmac_full(key, data):
    """Calculate full 16-byte CMAC"""
    c = CMAC.new(key, ciphermod=AES)
    c.update(data)
    return c.digest()

def cmac_truncate(cmac_full):
    """Truncate CMAC to 8 bytes (odd indices: 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

def derive_session_keys(key, rndA, rndB):
    """Derive EV2 session keys"""
    # SV1 for encryption key
    sv1 = (bytes([0xA5, 0x5A, 0x00, 0x01, 0x00, 0x80]) + 
           rndA[14:16] + rndB[14:16] + 
           rndA[8:14] + rndB[8:14] + 
           rndA[0:8] + rndB[0:8])
    
    enc_key = cmac_full(key, sv1)
    
    # SV2 for MAC key
    sv2 = (bytes([0x5A, 0xA5, 0x00, 0x01, 0x00, 0x80]) + 
           rndA[14:16] + rndB[14:16] + 
           rndA[8:14] + rndB[8:14] + 
           rndA[0:8] + rndB[0:8])
    
    mac_key = cmac_full(key, sv2)
    
    return enc_key, mac_key

# ============================================================
# NFC CARD COMMUNICATION
# ============================================================

def apdu(tag, cla, ins, p1, p2, data=b'', le=0):
    """Send APDU command to card"""
    cmd = bytes([cla, ins, p1, p2])
    
    if len(data) > 0:
        cmd += bytes([len(data)]) + data
    
    if le > 0 or len(data) > 0:
        cmd += bytes([le])
    
    print(f"\n>> APDU: {hexlify(cmd).decode().upper()}")
    
    response = tag.transceive(cmd)
    print(f"<< Response: {hexlify(response).decode().upper()}")
    
    if len(response) < 2:
        raise Exception("Response too short")
    
    sw = response[-2:]
    data = response[:-2]
    
    sw_int = (sw[0] << 8) | sw[1]
    
    return data, sw_int

def select_application(tag):
    """Select NDEF application"""
    print("\n=== SELECT NDEF APPLICATION ===")
    aid = bytes([0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01])
    data, sw = apdu(tag, 0x90, 0x5A, 0x00, 0x00, data=aid, le=0)
    
    if sw != 0x9000:
        raise Exception(f"Select failed: SW={sw:04X}")
    
    print("✅ Application selected")
    return True

def authenticate_ev2_first(tag, key_no, key):
    """
    Perform EV2First authentication (AN12196 Section 6.5.6)
    Returns: (session_enc_key, session_mac_key, transaction_id, cmd_counter, current_iv)
    """
    print(f"\n=== AUTHENTICATE EV2 FIRST (Key {key_no}) ===")
    
    # Step 1: Send AuthenticateEV2First command
    print("\n[Step 1] Send AuthenticateEV2First")
    cmd_data = bytes([key_no, 0x00])
    enc_rndB, sw = apdu(tag, 0x90, 0x71, 0x00, 0x00, cmd_data, le=0)
    
    if sw != 0x91AF:
        raise Exception(f"Auth step 1 failed: SW={sw:04X}")
    
    if len(enc_rndB) != 16:
        raise Exception(f"Encrypted RndB wrong length: {len(enc_rndB)}")
    
    print(f"Encrypted RndB: {hexlify(enc_rndB).decode().upper()}")
    
    # Step 2: Decrypt RndB
    print("\n[Step 2] Decrypt RndB")
    rndB = aes_decrypt_cbc(key, bytes(16), enc_rndB)
    print(f"RndB: {hexlify(rndB).decode().upper()}")
    
    # Step 3: Generate RndA and rotate RndB
    print("\n[Step 3] Generate RndA and rotate RndB")
    rndA = os.urandom(16)
    print(f"RndA: {hexlify(rndA).decode().upper()}")
    
    rndB_rot = rndB[1:] + rndB[0:1]
    print(f"RndB': {hexlify(rndB_rot).decode().upper()}")
    
    # Step 4: Encrypt RndA || RndB'
    print("\n[Step 4] Encrypt RndA || RndB'")
    auth_payload = rndA + rndB_rot
    enc_payload = aes_encrypt_cbc(key, bytes(16), auth_payload)
    print(f"Encrypted: {hexlify(enc_payload).decode().upper()}")
    
    # Step 5: Send encrypted response
    print("\n[Step 5] Send encrypted response (AF command)")
    enc_response, sw = apdu(tag, 0x90, 0xAF, 0x00, 0x00, enc_payload, le=0)
    
    if sw != 0x9100:
        raise Exception(f"Auth step 2 failed: SW={sw:04X}")
    
    if len(enc_response) != 32:
        raise Exception(f"Response wrong length: {len(enc_response)}")
    
    print(f"Encrypted Response: {hexlify(enc_response).decode().upper()}")
    
    # Step 6: Decrypt response
    print("\n[Step 6] Decrypt and verify response")
    response = aes_decrypt_cbc(key, bytes(16), enc_response)
    print(f"Decrypted: {hexlify(response).decode().upper()}")
    
    ti = response[0:4]
    rndA_rot_received = response[4:20]
    pdcap2 = response[20:26]
    pcdcap2 = response[26:32]
    
    print(f"Transaction ID: {hexlify(ti).decode().upper()}")
    print(f"RndA' received: {hexlify(rndA_rot_received).decode().upper()}")
    
    # Verify RndA'
    rndA_rot_expected = rndA[1:] + rndA[0:1]
    print(f"RndA' expected: {hexlify(rndA_rot_expected).decode().upper()}")
    
    if rndA_rot_received != rndA_rot_expected:
        raise Exception("RndA' verification failed!")
    
    print("✅ RndA' verified")
    
    # Step 7: Derive session keys
    print("\n[Step 7] Derive session keys")
    session_enc_key, session_mac_key = derive_session_keys(key, rndA, rndB)
    
    print(f"Session ENC Key: {hexlify(session_enc_key).decode().upper()}")
    print(f"Session MAC Key: {hexlify(session_mac_key).decode().upper()}")
    
    # Initial IV for chaining = last 16 bytes of encrypted response
    current_iv = enc_response[16:32]
    print(f"Initial IV: {hexlify(current_iv).decode().upper()}")
    
    # Command counter starts at 0
    cmd_counter = 0
    
    print("\n✅ Authentication successful")
    
    return session_enc_key, session_mac_key, ti, cmd_counter, current_iv

def change_key(tag, key_no, old_key, new_key, session_enc_key, session_mac_key, ti, cmd_counter, current_iv):
    """
    Execute ChangeKey command (AN12196 Section 6.16.2 - Case 2)
    Returns: (success, new_cmd_counter, new_iv)
    """
    print(f"\n=== CHANGEKEY (Key {key_no}) ===")
    
    # Plaintext: NewKey || Version || Padding
    plaintext = new_key + bytes([0x01]) + bytes([0x80]) + bytes(14)
    print(f"Plaintext: {hexlify(plaintext).decode().upper()}")
    
    # Calculate IV for encryption
    # IV_input = A55A || TI || CmdCtr || Padding
    iv_input = bytes([0xA5, 0x5A])
    iv_input += ti  # 4 bytes
    iv_input += bytes([cmd_counter & 0xFF, (cmd_counter >> 8) & 0xFF])  # LSB first
    iv_input += bytes(8)  # Padding
    
    print(f"IV Input: {hexlify(iv_input).decode().upper()}")
    
    # Calculate IV
    iv = aes_encrypt_cbc(session_enc_key, current_iv, iv_input)
    print(f"IV: {hexlify(iv).decode().upper()}")
    
    # Encrypt plaintext
    enc_data = aes_encrypt_cbc(session_enc_key, iv, plaintext)
    print(f"Encrypted Data: {hexlify(enc_data).decode().upper()}")
    
    # Calculate CMAC
    # MAC Input: Cmd || CmdCtr || TI || KeyNo || EncData
    mac_input = bytes([0xC4])  # ChangeKey command
    mac_input += bytes([cmd_counter & 0xFF, (cmd_counter >> 8) & 0xFF])  # LSB first
    mac_input += ti  # 4 bytes
    mac_input += bytes([key_no])  # 1 byte
    mac_input += enc_data  # 32 bytes
    
    print(f"MAC Input: {hexlify(mac_input).decode().upper()}")
    
    mac_full = cmac_full(session_mac_key, mac_input)
    print(f"CMAC Full: {hexlify(mac_full).decode().upper()}")
    
    mac_trunc = cmac_truncate(mac_full)
    print(f"CMAC Truncated: {hexlify(mac_trunc).decode().upper()}")
    
    # Build native command: Cmd || KeyNo || EncData || MAC
    native_cmd = bytes([0xC4, key_no]) + enc_data + mac_trunc
    
    print(f"\nNative Command ({len(native_cmd)} bytes):")
    print(f"  Cmd: C4")
    print(f"  KeyNo: {key_no:02X}")
    print(f"  EncData: {hexlify(enc_data).decode().upper()}")
    print(f"  MAC: {hexlify(mac_trunc).decode().upper()}")
    
    # Send as APDU
    print("\nSending ChangeKey command...")
    response, sw = apdu(tag, 0x90, 0xC4, 0x00, 0x00, 
                       bytes([key_no]) + enc_data + mac_trunc, le=0)
    
    if sw != 0x9100:
        print(f"❌ ChangeKey failed: SW={sw:04X}")
        return False, cmd_counter, current_iv
    
    print(f"✅ ChangeKey accepted: SW={sw:04X}")
    print(f"Response data ({len(response)} bytes): {hexlify(response).decode().upper()}")
    
    # Update state
    new_cmd_counter = cmd_counter + 1
    new_iv = enc_data[16:32]  # Last ciphertext block
    
    print(f"Command counter: {cmd_counter} → {new_cmd_counter}")
    print(f"New IV: {hexlify(new_iv).decode().upper()}")
    
    return True, new_cmd_counter, new_iv

def commit_transaction(tag, session_mac_key, ti, cmd_counter):
    """
    Execute CommitTransaction command
    Returns: success
    """
    print("\n=== COMMIT TRANSACTION ===")
    
    # Calculate CMAC
    # MAC Input: Cmd || CmdCtr || TI || 0x00
    mac_input = bytes([0xC7])  # CommitTransaction
    mac_input += bytes([cmd_counter & 0xFF, (cmd_counter >> 8) & 0xFF])  # LSB first
    mac_input += ti  # 4 bytes
    mac_input += bytes([0x00])  # No data
    
    print(f"MAC Input: {hexlify(mac_input).decode().upper()}")
    
    mac_full = cmac_full(session_mac_key, mac_input)
    print(f"CMAC Full: {hexlify(mac_full).decode().upper()}")
    
    mac_trunc = cmac_truncate(mac_full)
    print(f"CMAC Truncated: {hexlify(mac_trunc).decode().upper()}")
    
    # Send command
    print("\nSending CommitTransaction...")
    response, sw = apdu(tag, 0x90, 0xC7, 0x00, 0x00, mac_trunc, le=0)
    
    if sw == 0x9100:
        print(f"✅ CommitTransaction successful: SW={sw:04X}")
        return True
    elif sw == 0x911C:
        print(f"❌ CommitTransaction failed: SW=911C (Command not allowed - no changes)")
        print("   This means ChangeKey was NOT accepted by the card!")
        return False
    else:
        print(f"❌ CommitTransaction failed: SW={sw:04X}")
        return False

# ============================================================
# MAIN TEST
# ============================================================

def main():
    print("="*60)
    print("NTAG424 DNA ChangeKey Flow Test")
    print("="*60)
    
    # Connect to NFC reader
    print("\nWaiting for card...")
    clf = nfc.ContactlessFrontend('usb')
    
    if not clf:
        print("❌ No NFC reader found")
        return
    
    tag = clf.connect(rdwr={'on-connect': lambda tag: False})
    
    if not tag:
        print("❌ No card detected")
        return
    
    print(f"✅ Card detected: {tag}")
    
    # Get UID
    uid = tag.identifier
    uid_hex = hexlify(uid).decode().upper()
    print(f"UID: {uid_hex}")
    
    # Add colons for display
    uid_display = ':'.join([uid_hex[i:i+2] for i in range(0, len(uid_hex), 2)])
    print(f"UID (formatted): {uid_display}")
    
    # Derive new key from master secret
    new_key = derive_master_key(MASTER_SECRET, uid)
    print(f"\nDerived K0 (new key): {hexlify(new_key).decode().upper()}")
    
    try:
        # Step 1: Select application
        select_application(tag)
        
        # Step 2: Authenticate with factory key
        session_enc, session_mac, ti, cmd_ctr, iv = authenticate_ev2_first(tag, 0, FACTORY_KEY)
        
        # Step 3: ChangeKey
        success, cmd_ctr, iv = change_key(tag, 0, FACTORY_KEY, new_key,
                                         session_enc, session_mac, ti, cmd_ctr, iv)
        
        if not success:
            print("\n❌ ChangeKey command failed - stopping")
            return
        
        # Step 4: CommitTransaction
        commit_success = commit_transaction(tag, session_mac, ti, cmd_ctr)
        
        if not commit_success:
            print("\n❌ CommitTransaction failed - key change was NOT permanent")
            print("   Card MAC was incorrect or card rejected the change")
            return
        
        print("\n" + "="*60)
        print("✅ SUCCESS - Key change was committed!")
        print("="*60)
        
        # Step 5: Verify with new key
        print("\nVerifying new key works...")
        try:
            session_enc2, session_mac2, ti2, cmd_ctr2, iv2 = authenticate_ev2_first(tag, 0, new_key)
            print("\n✅✅✅ NEW KEY WORKS! Card successfully personalized! ✅✅✅")
        except Exception as e:
            print(f"\n❌ New key verification failed: {e}")
            print("   This should not happen if CommitTransaction was successful!")
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        clf.close()

if __name__ == "__main__":
    main()
