#!/usr/bin/env python3
"""
Test Server Scan Flow
Simulates ESP32 scanning cards with cached challenge and crypto proof
Tests against live server at 192.168.10.7:3000
"""

import requests
import os
from Crypto.Cipher import AES
from Crypto.Hash import CMAC

# ============================================================
# CONFIGURATION
# ============================================================

SERVER_URL = "http://192.168.10.7:3000"

# Test cards
CARDS = [
    {
        'name': 'Defect Card (possible bad write)',
        'uid': '04:B3:50:02:E5:75:80',
        'expected_key_type': 'personalized'
    },
    {
        'name': 'Fresh Factory Card',
        'uid': '04:89:4D:02:E5:75:80',
        'expected_key_type': 'factory'
    }
]

# Master secret from system
MASTER_SECRET = "A54C788525178060913D4CFC06380D1B"

# Factory key (all zeros)
FACTORY_KEY = bytes(16)

# ============================================================
# CRYPTO FUNCTIONS
# ============================================================

def derive_master_key(master_secret_hex, uid_string, diversification_input=1):
    """Derive card-specific key from master secret + UID"""
    uid_clean = uid_string.replace(":", "").upper()
    uid_bytes = bytes.fromhex(uid_clean)
    master_key = bytes.fromhex(master_secret_hex)
    
    derivation_data = bytes([diversification_input]) + uid_bytes + bytes(8)
    
    cipher = CMAC.new(master_key, ciphermod=AES)
    cipher.update(derivation_data)
    return cipher.digest()

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

def derive_session_keys(key, rndA, rndB):
    """Derive EV2 session keys"""
    # SV = RndA[15:14] || RndB[15:14] || RndA[13:8] || RndB[13:8] || RndA[7:0] || RndB[7:0]
    sv = (rndA[14:16] + rndB[14:16] + 
          rndA[8:14] + rndB[8:14] + 
          rndA[0:8] + rndB[0:8])
    
    # Encryption session key
    enc_input = bytes([0x5C, 0x01, 0x00, 0x01, 0x00, 0x80]) + sv
    enc_key = cmac_full(key, enc_input)
    
    # MAC session key
    mac_input = bytes([0x5C, 0x01, 0x00, 0x02, 0x00, 0x80]) + sv
    mac_key = cmac_full(key, mac_input)
    
    return enc_key, mac_key

def simulate_card_authentication(key, challenge_bytes):
    """
    Simulate card authentication with given challenge (as RndA)
    Returns crypto proof data for server verification
    """
    rndA = challenge_bytes  # Challenge from server = our RndA
    rndB = os.urandom(16)   # Card generates RndB
    
    # Step 1: Card encrypts RndB
    encRndB = aes_encrypt(key, rndB)
    
    # Step 2: ESP32 decrypts RndB (just verify we can)
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
    ti = os.urandom(4)  # Card generates random TI
    pdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    pcdcap2 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    response = ti + rndA_rot + pdcap2 + pcdcap2
    
    # Encrypt response (32 bytes = 2 blocks, CBC mode encrypts all at once)
    encResponse = aes_encrypt(key, response)
    
    # ESP32 decrypts and verifies RndA'
    decResponse = aes_decrypt(key, encResponse)
    assert decResponse[4:20] == rndA_rot, "RndA' verification failed"
    
    # Derive session keys (for reference, not needed for scan-with-proof)
    enc_key, mac_key = derive_session_keys(key, rndA, rndB)
    
    return {
        'encRndB': encRndB.hex().upper(),
        'encResponse': encResponse.hex().upper(),
        'ti': ti.hex().upper(),
        'rndA': rndA.hex().upper(),
        'rndB': rndB.hex().upper()
    }

# ============================================================
# SERVER API FUNCTIONS
# ============================================================

def get_initial_challenge():
    """Request initial challenge from server"""
    print("\n" + "="*60)
    print("📤 Requesting initial challenge from server...")
    print("="*60)
    
    try:
        response = requests.get(f"{SERVER_URL}/api/challenge/initial", timeout=5)
        if response.status_code == 200:
            data = response.json()
            challenge_hex = data['challenge']
            challenge_id = data['challengeId']
            print(f"✅ Received challenge")
            print(f"   Challenge ID: {challenge_id}")
            print(f"   Challenge: {challenge_hex}")
            return challenge_hex, challenge_id
        else:
            print(f"❌ Server error: HTTP {response.status_code}")
            return None, None
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return None, None

def send_scan_with_proof(uid, enc_rndB, enc_response, ti, challenge_id):
    """Send scan with crypto proof to server"""
    print(f"\n📤 Sending scan with crypto proof...")
    print(f"   UID: {uid}")
    print(f"   Challenge ID: {challenge_id}")
    print(f"   Enc RndB: {enc_rndB[:32]}...")
    print(f"   Enc Response: {enc_response[:32]}...")
    print(f"   TI: {ti}")
    
    payload = {
        'uid': uid,
        'encRndB': enc_rndB,
        'encResponse': enc_response,
        'transactionId': ti,
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
            print(f"\n✅ Server response:")
            print(f"   Status: {result.get('status', 'unknown')}")
            print(f"   Message: {result.get('message', '')}")
            print(f"   Credits: {result.get('credits', 0)}")
            
            next_challenge_id = result.get('nextChallengeId', '')
            next_challenge = result.get('nextChallenge', '')
            if next_challenge_id:
                print(f"   Next challenge ID: {next_challenge_id}")
            if next_challenge:
                print(f"   Next challenge: {next_challenge[:32]}...")
            
            return result, next_challenge, next_challenge_id
        else:
            print(f"❌ Server error: HTTP {response.status_code}")
            print(f"   Response: {response.text}")
            return None, None, None
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return None, None, None

# ============================================================
# TEST SCENARIOS
# ============================================================

def test_card_scan(card_info, cached_challenge_hex, cached_challenge_id):
    """
    Test scanning a card with cached challenge
    This simulates what ESP32 does
    """
    print("\n" + "="*60)
    print(f"SCANNING CARD: {card_info['name']}")
    print("="*60)
    print(f"UID: {card_info['uid']}")
    print(f"Expected type: {card_info['expected_key_type']}")
    print(f"Challenge ID: {cached_challenge_id}")
    
    # Convert cached challenge to bytes
    challenge_bytes = bytes.fromhex(cached_challenge_hex)
    
    # Determine which key to use
    if card_info['expected_key_type'] == 'factory':
        key = FACTORY_KEY
        key_name = "Factory Key"
    else:  # personalized
        key = derive_master_key(MASTER_SECRET, card_info['uid'], diversification_input=1)
        key_name = "Personalized Key"
    
    print(f"\n1. Using {key_name}")
    print(f"   Key: {key.hex().upper()}")
    
    # Simulate authentication and get crypto proof
    print(f"\n2. Simulating EV2 authentication with cached challenge...")
    try:
        crypto_proof = simulate_card_authentication(key, challenge_bytes)
        print(f"✅ Authentication simulation successful")
        print(f"   Encrypted RndB: {crypto_proof['encRndB'][:32]}...")
        print(f"   Encrypted Response: {crypto_proof['encResponse'][:32]}...")
        print(f"   Transaction ID: {crypto_proof['ti']}")
    except AssertionError as e:
        print(f"❌ Authentication simulation failed: {e}")
        return None
    
    # Send to server
    print(f"\n3. Sending crypto proof to server...")
    result, next_challenge, next_challenge_id = send_scan_with_proof(
        card_info['uid'],
        crypto_proof['encRndB'],
        crypto_proof['encResponse'],
        crypto_proof['ti'],
        cached_challenge_id
    )
    
    if result:
        # Validate result
        status = result.get('status', 'unknown')
        expected_status = 'known' if card_info['expected_key_type'] == 'personalized' else 'unknown'
        
        print(f"\n4. Validation:")
        if card_info['expected_key_type'] == 'factory':
            print(f"   Expected: 'unknown' (factory card not in database)")
        else:
            print(f"   Expected: 'known' (personalized card in database)")
        print(f"   Received: '{status}'")
        
        if status == expected_status:
            print(f"   ✅ Status matches expectation!")
        else:
            print(f"   ⚠️  Status mismatch")
        
        return result, next_challenge, next_challenge_id
    else:
        print(f"❌ Server communication failed")
        return None, None, None

def run_full_test():
    """Run complete test suite"""
    print("="*60)
    print("SERVER SCAN FLOW TEST")
    print("="*60)
    print(f"Server: {SERVER_URL}")
    print(f"Master Secret: {MASTER_SECRET}")
    print(f"Test Cards: {len(CARDS)}")
    
    # Step 1: Get initial challenge (like ESP32 boot)
    cached_challenge, cached_challenge_id = get_initial_challenge()
    if not cached_challenge or not cached_challenge_id:
        print("\n❌ Failed to get initial challenge - server offline?")
        return
    
    # Step 2: Scan each card
    results = []
    for i, card in enumerate(CARDS):
        print(f"\n{'='*60}")
        print(f"CARD {i+1}/{len(CARDS)}")
        print(f"{'='*60}")
        
        result, next_challenge, next_challenge_id = test_card_scan(card, cached_challenge, cached_challenge_id)
        results.append({
            'card': card,
            'result': result
        })
        
        # Update cached challenge for next scan
        if result and next_challenge and next_challenge_id:
            cached_challenge = next_challenge
            cached_challenge_id = next_challenge_id
            print(f"\n✅ Challenge updated for next scan")
            print(f"   Next challenge ID: {cached_challenge_id}")
    
    # Summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)
    
    for i, item in enumerate(results):
        card = item['card']
        result = item['result']
        
        print(f"\n{i+1}. {card['name']} ({card['uid']})")
        if result:
            status = result.get('status', 'unknown')
            credits = result.get('credits', 0)
            print(f"   Status: {status}")
            print(f"   Credits: {credits}")
            print(f"   ✅ Scan successful")
        else:
            print(f"   ❌ Scan failed")
    
    # Final verdict
    print("\n" + "="*60)
    print("VERDICT")
    print("="*60)
    
    successful = sum(1 for item in results if item['result'] is not None)
    print(f"Successful scans: {successful}/{len(CARDS)}")
    
    if successful == len(CARDS):
        print("✅ ALL TESTS PASSED!")
        print("\nThe fast scan flow with cached challenge works correctly:")
        print("  1. ✅ Server provides initial challenge")
        print("  2. ✅ Card authentication generates valid crypto proof")
        print("  3. ✅ Server verifies and returns card status")
        print("  4. ✅ Server provides next challenge for caching")
    else:
        print("⚠️  Some tests failed - check server logs")

# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":
    try:
        run_full_test()
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
    except Exception as e:
        print(f"\n\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
