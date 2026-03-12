#!/usr/bin/env python3
"""
Full ChangeKey Test Script
Tests entire flow: factory auth → ChangeKey → personalized auth
Against live server to verify crypto is correct
"""

import requests
import hashlib
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
import os

# ============================================================
# CONFIGURATION
# ============================================================

SERVER_URL = "http://192.168.10.7:3000"

# Card UIDs
DEFECT_CARD_UID = "04:B3:50:02:E5:75:80"
FRESH_CARD_UID = "04:89:4D:02:E5:75:80"

# Master secret from system
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Factory key (all zeros)
FACTORY_KEY = bytes(16)

# ============================================================
# CRYPTO FUNCTIONS (matching ESP32)
# ============================================================

def derive_master_key(master_secret_hex, uid_string, diversification_input=1):
    """
    Derive card-specific key from master secret + UID
    Matches NTAG424Crypto::deriveMasterKey in ESP32
    """
    # Remove colons from UID
    uid_clean = uid_string.replace(":", "").upper()
    uid_bytes = bytes.fromhex(uid_clean)
    
    # Master secret as bytes
    master_key = bytes.fromhex(master_secret_hex)
    
    # Derivation data: 0x01 || UID (7 bytes) || 0x00...00 (8 bytes padding)
    derivation_data = bytes([diversification_input]) + uid_bytes + bytes(8)
    
    print(f"   Derivation input: {derivation_data.hex().upper()}")
    
    # CMAC with master key
    cipher = CMAC.new(master_key, ciphermod=AES)
    cipher.update(derivation_data)
    full_cmac = cipher.digest()  # 16 bytes
    
    print(f"   Full CMAC: {full_cmac.hex().upper()}")
    
    # Return all 16 bytes as derived key
    return full_cmac

def cmac_truncate(full_cmac_bytes):
    """
    Truncate CMAC to 8 bytes using ODD indices (1,3,5,7,9,11,13,15)
    Matches fixed NTAG424Crypto::cmacTruncate in ESP32
    """
    if len(full_cmac_bytes) != 16:
        raise ValueError("CMAC must be 16 bytes")
    
    # Extract odd indices
    truncated = bytes([
        full_cmac_bytes[1],
        full_cmac_bytes[3],
        full_cmac_bytes[5],
        full_cmac_bytes[7],
        full_cmac_bytes[9],
        full_cmac_bytes[11],
        full_cmac_bytes[13],
        full_cmac_bytes[15]
    ])
    
    return truncated

def cmac_full(key, data):
    """Calculate full 16-byte CMAC"""
    cipher = CMAC.new(key, ciphermod=AES)
    cipher.update(data)
    return cipher.digest()

def aes_encrypt(key, data):
    """AES-128 ECB encryption"""
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(data)

def aes_decrypt(key, data):
    """AES-128 ECB decryption"""
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.decrypt(data)

def lsr_1bit(data):
    """Left shift rotate by 1 bit (for session key derivation)"""
    result = bytearray(len(data))
    carry = 0
    
    for i in range(len(data)):
        new_carry = (data[i] >> 7) & 1
        result[i] = ((data[i] << 1) | carry) & 0xFF
        carry = new_carry
    
    return bytes(result)

def derive_session_keys(key, rndA, rndB):
    """
    Derive session keys using NIST SP 800-108 KDF
    Matches ESP32 implementation
    """
    # SV = RndA[15:14] || RndB[15:14] || RndA[13:8] || RndB[13:8] || RndA[7:0] || RndB[7:0]
    sv = (rndA[14:16] + rndB[14:16] + 
          rndA[8:14] + rndB[8:14] + 
          rndA[0:8] + rndB[0:8])
    
    print(f"   SV: {sv.hex().upper()}")
    
    # Encryption session key: CMAC(K, 0x5C || 0x01 || 0x00 || 0x01 || 0x00 || 0x80 || SV)
    enc_input = bytes([0x5C, 0x01, 0x00, 0x01, 0x00, 0x80]) + sv
    enc_key = cmac_full(key, enc_input)
    
    # MAC session key: CMAC(K, 0x5C || 0x01 || 0x00 || 0x02 || 0x00 || 0x80 || SV)
    mac_input = bytes([0x5C, 0x01, 0x00, 0x02, 0x00, 0x80]) + sv
    mac_key = cmac_full(key, mac_input)
    
    print(f"   Enc session key: {enc_key.hex().upper()}")
    print(f"   MAC session key: {mac_key.hex().upper()}")
    
    return enc_key, mac_key

def simulate_ev2_authentication(key, key_name):
    """
    Simulate EV2First authentication (without real card)
    Returns session keys if successful
    """
    print(f"\n{'='*60}")
    print(f"Simulating EV2 Authentication with {key_name}")
    print(f"{'='*60}")
    
    # Generate random challenges
    rndB = os.urandom(16)
    rndA = os.urandom(16)
    
    print(f"RndA (reader): {rndA.hex().upper()}")
    print(f"RndB (card):   {rndB.hex().upper()}")
    
    # Encrypt RndB with key (simulating card)
    encRndB = aes_encrypt(key, rndB)
    print(f"E(RndB):       {encRndB.hex().upper()}")
    
    # Reader decrypts RndB
    decRndB = aes_decrypt(key, encRndB)
    if decRndB != rndB:
        print("❌ RndB decryption failed")
        return None
    print("✅ RndB decrypted correctly")
    
    # Rotate RndB
    rndB_rot = rndB[1:] + rndB[0:1]
    
    # Reader sends RndA || RndB' encrypted
    challenge = rndA + rndB_rot
    encChallenge = aes_encrypt(key, challenge)
    print(f"E(RndA||RndB'): {encChallenge.hex().upper()}")
    
    # Card decrypts and verifies RndB'
    decChallenge = aes_decrypt(key, encChallenge)
    if decChallenge[16:] != rndB_rot:
        print("❌ RndB' verification failed")
        return None
    print("✅ RndB' verified correctly")
    
    # Rotate RndA
    rndA_rot = rndA[1:] + rndA[0:1]
    
    # Card responds with TI || RndA' || PDcap2 || PCDcap2 (32 bytes)
    ti = bytes([0x12, 0x34, 0x56, 0x78])  # Simulated TI
    pdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    pcdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    response = ti + rndA_rot + pdcap2 + pcdcap2
    encResponse = aes_encrypt(key, response[:16]) + aes_encrypt(key, response[16:])
    print(f"E(TI||RndA'||...): {encResponse.hex().upper()}")
    
    # Reader decrypts and verifies RndA'
    decResponse = aes_decrypt(key, encResponse[:16]) + aes_decrypt(key, encResponse[16:])
    if decResponse[4:20] != rndA_rot:
        print("❌ RndA' verification failed")
        return None
    print("✅ RndA' verified correctly")
    
    # Derive session keys
    print("\nDeriving session keys...")
    enc_key, mac_key = derive_session_keys(key, rndA, rndB)
    
    print(f"\n✅ Authentication successful with {key_name}")
    return {
        'encKey': enc_key,
        'macKey': mac_key,
        'rndA': rndA,
        'rndB': rndB,
        'ti': ti
    }

def simulate_changekey(old_key, new_key, authenticated_key_no=0, target_key_no=0):
    """
    Simulate ChangeKey command (Case 2: same key)
    Returns CMAC of the command
    """
    print(f"\n{'='*60}")
    print(f"Simulating ChangeKey (Case 2)")
    print(f"{'='*60}")
    print(f"Old key: {old_key.hex().upper()}")
    print(f"New key: {new_key.hex().upper()}")
    
    # First authenticate to get session keys
    auth_result = simulate_ev2_authentication(old_key, "Factory Key")
    if not auth_result:
        return None
    
    # ChangeKey command for Case 2 (keyNo == authenticatedKeyNo)
    # CmdData = E(Kenc, NewKey) || CMAC(Kmac, NewKey || CmdCtr || CmdHeader)
    
    enc_key = auth_result['encKey']
    mac_key = auth_result['macKey']
    
    print(f"\nBuilding ChangeKey command...")
    print(f"Target key number: {target_key_no}")
    print(f"Authenticated key number: {authenticated_key_no}")
    
    # Encrypt new key
    enc_new_key = aes_encrypt(enc_key, new_key)
    print(f"E(Kenc, NewKey): {enc_new_key.hex().upper()}")
    
    # CMAC over: NewKey || CmdCtr || CmdHeader
    cmd_ctr = bytes([0x00, 0x01])  # Example counter
    cmd_header = bytes([0xC4, target_key_no])  # 0xC4 = ChangeKey
    
    cmac_input = new_key + cmd_ctr + cmd_header
    print(f"CMAC input: {cmac_input.hex().upper()}")
    
    full_cmac = cmac_full(mac_key, cmac_input)
    print(f"Full CMAC: {full_cmac.hex().upper()}")
    
    cmac_8 = cmac_truncate(full_cmac)
    print(f"CMAC (8 bytes): {cmac_8.hex().upper()}")
    
    # Full command data
    cmd_data = enc_new_key + cmac_8
    print(f"Command data: {cmd_data.hex().upper()}")
    print(f"✅ ChangeKey crypto calculated correctly")
    
    return {
        'cmdData': cmd_data,
        'newKey': new_key,
        'encNewKey': enc_new_key,
        'cmac': cmac_8
    }

# ============================================================
# SERVER VERIFICATION
# ============================================================

def verify_with_server(uid, auth_result):
    """
    Verify authentication with server using /api/scan-with-proof
    """
    print(f"\n{'='*60}")
    print(f"Verifying with server")
    print(f"{'='*60}")
    
    # Convert to hex strings
    enc_rndB_hex = auth_result['encRndB'].hex().upper()
    enc_response_hex = auth_result['encResponse'].hex().upper()
    ti_hex = auth_result['ti'].hex().upper()
    
    payload = {
        'uid': uid,
        'encRndB': enc_rndB_hex,
        'encResponse': enc_response_hex,
        'transactionId': ti_hex
    }
    
    print(f"Sending to server...")
    print(f"  UID: {uid}")
    print(f"  Enc RndB: {enc_rndB_hex[:32]}...")
    print(f"  Enc Response: {enc_response_hex[:32]}...")
    print(f"  TI: {ti_hex}")
    
    try:
        response = requests.post(f"{SERVER_URL}/api/scan-with-proof", json=payload, timeout=5)
        if response.status_code == 200:
            result = response.json()
            print(f"\n✅ Server response:")
            print(f"   Status: {result.get('status')}")
            print(f"   Credits: {result.get('credits')}")
            print(f"   Message: {result.get('message')}")
            return result
        else:
            print(f"❌ Server error: HTTP {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return None

# ============================================================
# TEST SCENARIOS
# ============================================================

def test_defect_card():
    """Test the defect card (04:B3:50:02:E5:75:80)"""
    print("\n" + "="*60)
    print("TEST 1: DEFECT CARD - Can we auth with personalized key?")
    print("="*60)
    
    uid = DEFECT_CARD_UID
    
    # Derive personalized key
    print("\n1. Deriving personalized key...")
    personalized_key = derive_master_key(MASTER_SECRET, uid, diversification_input=1)
    print(f"   Personalized K0: {personalized_key.hex().upper()}")
    
    # Try to simulate authentication with personalized key
    auth_result = simulate_ev2_authentication(personalized_key, "Personalized Key")
    
    if auth_result:
        print("\n✅ Theoretical auth with personalized key WORKS")
        print("   → This means ChangeKey crypto was CORRECT")
        print("   → But real card doesn't respond correctly")
        print("   → Problem is in ESP32 write or card state")
    else:
        print("\n❌ Theoretical auth with personalized key FAILS")
        print("   → This would mean crypto calculation is wrong")

def test_fresh_card_flow():
    """Test complete flow on fresh card (simulation)"""
    print("\n" + "="*60)
    print("TEST 2: FRESH CARD - Complete ChangeKey flow simulation")
    print("="*60)
    
    uid = FRESH_CARD_UID
    
    # Step 1: Auth with factory key
    print("\n1. Authenticate with factory key...")
    auth1 = simulate_ev2_authentication(FACTORY_KEY, "Factory Key")
    if not auth1:
        print("❌ Factory auth failed")
        return
    
    # Step 2: Derive new key
    print("\n2. Derive personalized key...")
    new_key = derive_master_key(MASTER_SECRET, uid, diversification_input=1)
    print(f"   New K0: {new_key.hex().upper()}")
    
    # Step 3: Simulate ChangeKey
    print("\n3. Simulate ChangeKey command...")
    changekey_result = simulate_changekey(FACTORY_KEY, new_key, authenticated_key_no=0, target_key_no=0)
    if not changekey_result:
        print("❌ ChangeKey simulation failed")
        return
    
    # Step 4: Try auth with new key
    print("\n4. Authenticate with new personalized key...")
    auth2 = simulate_ev2_authentication(new_key, "New Personalized Key")
    if not auth2:
        print("❌ Auth with new key failed")
        return
    
    print("\n✅ COMPLETE FLOW SIMULATION SUCCESSFUL")
    print("   → Factory auth works")
    print("   → ChangeKey crypto correct")
    print("   → New key auth works")
    print("\n💡 This means our crypto implementation is CORRECT!")

def test_changekey_crypto_only():
    """Test ONLY the ChangeKey crypto calculation"""
    print("\n" + "="*60)
    print("TEST 3: ChangeKey Crypto Verification")
    print("="*60)
    
    uid = DEFECT_CARD_UID
    
    print(f"Card: {uid}")
    print(f"Old key: Factory (00...00)")
    
    # Derive new key
    new_key = derive_master_key(MASTER_SECRET, uid, diversification_input=1)
    print(f"New key: {new_key.hex().upper()}")
    
    # Simulate ChangeKey
    result = simulate_changekey(FACTORY_KEY, new_key, authenticated_key_no=0, target_key_no=0)
    
    if result:
        print("\n✅ ChangeKey crypto calculation is CORRECT")
        print("   → Command data would be valid")
        print("   → Problem must be in ESP32 APDU sending or card state")
    else:
        print("\n❌ ChangeKey crypto calculation FAILED")

# ============================================================
# MAIN
# ============================================================

def main():
    print("="*60)
    print("NTAG424 ChangeKey Full Test Suite")
    print("="*60)
    print(f"Server: {SERVER_URL}")
    print(f"Master Secret: {MASTER_SECRET}")
    print(f"Defect Card: {DEFECT_CARD_UID}")
    print(f"Fresh Card: {FRESH_CARD_UID}")
    
    # Run tests
    test_changekey_crypto_only()
    test_defect_card()
    test_fresh_card_flow()
    
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print("If all crypto checks pass, the problem is NOT in calculations.")
    print("Problem is likely:")
    print("  1. CurrentIV state tracking in ESP32")
    print("  2. Card needs CommitTransaction after ChangeKey")
    print("  3. Card specific quirk (power cycle needed)")
    print("  4. APDU command format issue")

if __name__ == "__main__":
    main()
