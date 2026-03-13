#!/usr/bin/env python3
"""
Test successful (green/yellow) card scan
Simulates ESP32 scanning a properly personalized card
"""

import requests
import os
import hmac
import hashlib
from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# ============================================================
# CONFIGURATION
# ============================================================

SERVER_URL = "http://192.168.10.7:3000"

# Master secret (server derives keys from this)
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Test card from database
TEST_CARD = {
    'name': 'kaart #1',
    'uid': '048F4F02E57580',  # Without colons
    'active': True  # Should show green
}

# ============================================================
# CRYPTO FUNCTIONS
# ============================================================

def derive_transaction_key(master_secret_hex, uid_string):
    """
    Derive transaction key (K1) using HMAC-SHA256
    Formula: K1 = HMAC-SHA256(masterSecret, UID || "K1" || 0x01)
    """
    # Clean UID (remove colons if present)
    uid_clean = uid_string.replace(":", "").upper()
    uid_bytes = bytes.fromhex(uid_clean)
    
    # Master secret as bytes
    master_secret = bytes.fromhex(master_secret_hex)
    
    # Build data: UID || "K1" || version(1)
    data = uid_bytes + b"K1" + bytes([1])
    
    # Calculate HMAC-SHA256
    h = hmac.new(master_secret, data, hashlib.sha256)
    hash_result = h.digest()
    
    # Take first 16 bytes for AES-128
    return hash_result[:16]

def aes_encrypt(key, data):
    """AES-128 CBC encryption with zero IV (per NTAG424 spec)"""
    cipher = AES.new(key, AES.MODE_CBC, iv=bytes(16))
    return cipher.encrypt(data)

def aes_decrypt(key, data):
    """AES-128 CBC decryption with zero IV (per NTAG424 spec)"""
    cipher = AES.new(key, AES.MODE_CBC, iv=bytes(16))
    return cipher.decrypt(data)

def cmac_full(key, data):
    """Calculate full 16-byte CMAC"""
    cipher = CMAC.new(key, ciphermod=AES)
    cipher.update(data)
    return cipher.digest()

def simulate_card_authentication(key, challenge_bytes):
    """
    Simulate card authentication with given challenge (as RndA)
    Returns crypto proof data for server verification
    """
    rndA = challenge_bytes  # Challenge from server = our RndA
    rndB = os.urandom(16)   # Card generates RndB
    
    # Step 1: Card encrypts RndB
    encRndB = aes_encrypt(key, rndB)
    
    # Step 2: ESP32 decrypts RndB
    decRndB = aes_decrypt(key, encRndB)
    assert decRndB == rndB, "RndB decryption failed"
    
    # Step 3: Rotate RndB
    rndB_rot = rndB[1:] + rndB[0:1]
    
    # Step 4: ESP32 sends E(RndA || RndB')
    challenge = rndA + rndB_rot
    encChallenge = aes_encrypt(key, challenge)
    
    # Step 5: Card decrypts and verifies RndB'
    decChallenge = aes_decrypt(key, encChallenge)
    assert decChallenge[16:] == rndB_rot, "RndB' verification failed"
    
    # Step 6: Rotate RndA
    rndA_rot = rndA[1:] + rndA[0:1]
    
    # Step 7: Card responds with TI || RndA' || PDcap2 || PCDcap2
    ti = os.urandom(4)
    pdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    pcdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    response = ti + rndA_rot + pdcap2 + pcdcap2
    
    # Encrypt response
    encResponse = aes_encrypt(key, response)
    
    # ESP32 decrypts and verifies RndA'
    decResponse = aes_decrypt(key, encResponse)
    assert decResponse[4:20] == rndA_rot, "RndA' verification failed"
    
    return {
        'encRndB': encRndB.hex().upper(),
        'encResponse': encResponse.hex().upper(),
        'ti': ti.hex().upper(),
        'rndA': rndA.hex().upper(),
        'rndB': rndB.hex().upper()
    }

# ============================================================
# MAIN TEST
# ============================================================

def test_successful_scan():
    """Test a successful scan that should show green or yellow"""
    
    print("="*60)
    print("TEST SUCCESSFUL (GREEN/YELLOW) CARD SCAN")
    print("="*60)
    print(f"Server: {SERVER_URL}")
    print(f"Card: {TEST_CARD['name']}")
    print(f"UID: {TEST_CARD['uid']}")
    print(f"Active: {TEST_CARD['active']}")
    print(f"Expected color: {'🟢 GREEN' if TEST_CARD['active'] else '🟡 YELLOW-GREEN'}")
    print()
    
    # Step 1: Get challenge from server
    print("=" * 60)
    print("STEP 1: Get challenge from server")
    print("=" * 60)
    
    try:
        response = requests.get(f"{SERVER_URL}/api/challenge/initial", timeout=5)
        if response.status_code != 200:
            print(f"❌ Server error: HTTP {response.status_code}")
            return
        
        data = response.json()
        challenge_hex = data['challenge']
        challenge_id = data['challengeId']
        
        print(f"✅ Received challenge")
        print(f"   Challenge ID: {challenge_id}")
        print(f"   Challenge: {challenge_hex}")
        
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return
    
    # Step 2: Simulate authentication
    print()
    print("=" * 60)
    print("STEP 2: Derive transaction key and simulate authentication")
    print("=" * 60)
    
    challenge_bytes = bytes.fromhex(challenge_hex)
    
    # Derive K1 (transaction key) from master secret
    key = derive_transaction_key(MASTER_SECRET, TEST_CARD['uid'])
    
    print(f"Master Secret: {MASTER_SECRET}")
    print(f"UID: {TEST_CARD['uid']}")
    print(f"Derived K1 (Transaction Key): {key.hex().upper()}")
    print(f"Challenge (RndA): {challenge_hex}")
    
    try:
        crypto_proof = simulate_card_authentication(key, challenge_bytes)
        print(f"✅ Authentication successful")
        print(f"   Enc RndB: {crypto_proof['encRndB']}")
        print(f"   Enc Response: {crypto_proof['encResponse']}")
        print(f"   Transaction ID: {crypto_proof['ti']}")
    except Exception as e:
        print(f"❌ Authentication failed: {e}")
        return
    
    # Step 3: Send to server
    print()
    print("=" * 60)
    print("STEP 3: Send crypto proof to server")
    print("=" * 60)
    
    payload = {
        'uid': TEST_CARD['uid'],
        'encRndB': crypto_proof['encRndB'],
        'encResponse': crypto_proof['encResponse'],
        'transactionId': crypto_proof['ti'],
        'challengeId': challenge_id
    }
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/scan-with-proof", 
            json=payload, 
            timeout=5
        )
        
        if response.status_code == 200:
            result = response.json()
            
            print(f"✅ Server verification successful!")
            print()
            print(f"Status: {result.get('status', 'unknown')}")
            print(f"Message: {result.get('message', '')}")
            print(f"Credits: {result.get('credits', 0)}")
            
            # Color mapping
            status = result.get('status', 'unknown')
            color_map = {
                'known_active': '🟢 GREEN (Active Personalized)',
                'known_inactive': '🟡 YELLOW-GREEN (Inactive Personalized)',
                'factory': '🟠 ORANGE (Factory Default)',
                'challenge_failed': '🔴 RED (Challenge Failed)',
                'non_ntag424': '⚪ WHITE (Non-NTAG424)'
            }
            
            color = color_map.get(status, f'❓ UNKNOWN ({status})')
            
            print()
            print("=" * 60)
            print("RESULT")
            print("=" * 60)
            print(f"Dashboard color: {color}")
            
            if status == 'known_active':
                print("✅ SUCCESS! Card shows as GREEN (active)")
            elif status == 'known_inactive':
                print("✅ SUCCESS! Card shows as YELLOW-GREEN (inactive)")
            else:
                print(f"⚠️  Unexpected status: {status}")
            
        else:
            print(f"❌ Server error: HTTP {response.status_code}")
            print(f"Response: {response.text}")
            
    except Exception as e:
        print(f"❌ Connection error: {e}")

if __name__ == "__main__":
    try:
        test_successful_scan()
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
    except Exception as e:
        print(f"\n\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
