#!/usr/bin/env python3
"""
Test CMAC implementation step-by-step against NIST SP 800-38B
Compare with AN12196 Table 27 example
"""

from cryptography.hazmat.primitives.cmac import CMAC
from cryptography.hazmat.primitives.ciphers import algorithms
from cryptography.hazmat.backends import default_backend
from Crypto.Cipher import AES

def hex_dump(data, label=""):
    """Print data as hex"""
    if label:
        print(f"{label}: {data.hex().upper()}")
    else:
        print(data.hex().upper())

def left_shift(block):
    """Left shift block by 1 bit (big-endian)"""
    result = bytearray(16)
    overflow = 0
    # Process from right (LSB at index 15) to left (MSB at index 0)
    for i in range(15, -1, -1):
        new_overflow = 1 if (block[i] & 0x80) else 0
        result[i] = ((block[i] << 1) | overflow) & 0xFF
        overflow = new_overflow
    return bytes(result)

def generate_subkey(key):
    """Generate CMAC subkeys K1 and K2 per NIST SP 800-38B"""
    Rb = 0x87
    
    # Step 1: L = AES-ECB(K, 0^128)
    cipher = AES.new(key, AES.MODE_ECB)
    L = cipher.encrypt(bytes(16))
    hex_dump(L, "L")
    
    # Step 2: K1 = L << 1, XOR Rb if MSB(L) = 1
    K1 = left_shift(L)
    if L[0] & 0x80:  # MSB of L
        K1 = bytes([K1[i] ^ (Rb if i == 15 else 0) for i in range(16)])
    hex_dump(K1, "K1")
    
    # Step 3: K2 = K1 << 1, XOR Rb if MSB(K1) = 1
    K2 = left_shift(K1)
    if K1[0] & 0x80:  # MSB of K1
        K2 = bytes([K2[i] ^ (Rb if i == 15 else 0) for i in range(16)])
    hex_dump(K2, "K2")
    
    return L, K1, K2

def manual_cmac(key, data):
    """Manual CMAC calculation for debugging"""
    print("\n" + "="*60)
    print("MANUAL CMAC CALCULATION (NIST SP 800-38B)")
    print("="*60)
    
    hex_dump(key, "Key")
    hex_dump(data, f"Data ({len(data)} bytes)")
    
    # Generate subkeys
    L, K1, K2 = generate_subkey(key)
    
    # Process data
    n = (len(data) + 15) // 16
    if n == 0:
        n = 1
    print(f"\nNumber of blocks: {n}")
    
    complete_block = (len(data) > 0) and (len(data) % 16 == 0)
    print(f"Last block complete: {complete_block}")
    
    # Initialize state
    state = bytes(16)
    cipher = AES.new(key, AES.MODE_ECB)
    
    # Process all blocks except last
    for i in range(n - 1):
        block = data[i*16:(i+1)*16]
        hex_dump(block, f"\nBlock {i+1}")
        
        # XOR with state
        block = bytes([block[j] ^ state[j] for j in range(16)])
        hex_dump(block, "After XOR with state")
        
        # Encrypt
        state = cipher.encrypt(block)
        hex_dump(state, "After AES encrypt")
    
    # Process last block
    print(f"\n--- Last Block (block {n}) ---")
    last_block_len = len(data) - ((n - 1) * 16)
    print(f"Last block length: {last_block_len} bytes")
    
    if complete_block:
        # Complete block: XOR with K1
        last_block = bytearray(data[(n-1)*16:])
        hex_dump(bytes(last_block), "Last block (complete)")
        hex_dump(K1, "XOR with K1")
        for j in range(16):
            last_block[j] ^= K1[j]
        hex_dump(bytes(last_block), "After XOR K1")
    else:
        # Incomplete block: pad and XOR with K2
        last_block = bytearray(16)
        if last_block_len > 0:
            last_block[:last_block_len] = data[(n-1)*16:]
        last_block[last_block_len] = 0x80  # 10...0 padding
        hex_dump(bytes(last_block), "Last block after padding")
        hex_dump(K2, "XOR with K2")
        for j in range(16):
            last_block[j] ^= K2[j]
        hex_dump(bytes(last_block), "After XOR K2")
    
    # XOR with state
    hex_dump(state, "State before last block")
    for j in range(16):
        last_block[j] ^= state[j]
    hex_dump(bytes(last_block), "After XOR with state")
    
    # Final encryption
    mac = cipher.encrypt(bytes(last_block))
    hex_dump(mac, "Final CMAC")
    
    return mac

print("="*70)
print("TEST 1: AN12196 Table 27 Example")
print("="*70)

# AN12196 Table 27: CMAC example for session key derivation
# This is the reference we should match
key = bytes(16)  # Factory default key (all zeros)
sv1 = bytes.fromhex("A55A00010080CCD2A3BAB1ED0AF690532AE92D4EE106474C57B2E670BDD148E2")

print("\nAN12196 Session Key Derivation Example:")
manual_mac = manual_cmac(key, sv1)

# Compare with cryptography library
cmac = CMAC(algorithms.AES(key), backend=default_backend())
cmac.update(sv1)
library_mac = cmac.finalize()

print("\n" + "="*60)
print("COMPARISON:")
print("="*60)
hex_dump(manual_mac, "Manual CMAC  ")
hex_dump(library_mac, "Library CMAC ")

if manual_mac == library_mac:
    print("✅ Manual CMAC matches library!")
else:
    print("❌ Manual CMAC DOES NOT match library!")
    print("   This means our manual implementation has a bug")

print("\n" + "="*70)
print("TEST 2: Simple test vector")
print("="*70)

# Simple test: CMAC of empty string
test_data = b""
print(f"\nTest: CMAC of empty message")
test_mac = manual_cmac(key, test_data)

cmac = CMAC(algorithms.AES(key), backend=default_backend())
cmac.update(test_data)
ref_mac = cmac.finalize()

print("\n" + "="*60)
hex_dump(test_mac, "Manual  ")
hex_dump(ref_mac, "Library ")
print("✅ MATCH!" if test_mac == ref_mac else "❌ MISMATCH!")

print("\n" + "="*70)
print("TEST 3: One complete block (16 bytes)")
print("="*70)

test_data = bytes(16)
print(f"\nTest: CMAC of 16 zero bytes")
test_mac = manual_cmac(key, test_data)

cmac = CMAC(algorithms.AES(key), backend=default_backend())
cmac.update(test_data)
ref_mac = cmac.finalize()

print("\n" + "="*60)
hex_dump(test_mac, "Manual  ")
hex_dump(ref_mac, "Library ")
print("✅ MATCH!" if test_mac == ref_mac else "❌ MISMATCH!")
