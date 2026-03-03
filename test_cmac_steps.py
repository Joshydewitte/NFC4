#!/usr/bin/env python3
"""
Test ESP32 CMAC step-by-step to find the issue
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

# AN12196 Table 27 test case
mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")

print("=== Testing K1/K2 Subkey Generation ===\n")

# Step 1: L = AES-ECB(K, 0)
zero_block = bytes(16)
cipher = AES.new(mac_key, AES.MODE_ECB)
L = cipher.encrypt(zero_block)
print(f"L = AES-ECB(K, 0): {hexlify(L).decode().upper()}")

# Step 2: K1 = L << 1
def left_shift_1bit(data):
    """Left shift by 1 bit (big-endian)"""
    result = bytearray(16)
    carry = 0
    for i in range(16):
        new_carry = (data[i] & 0x80) >> 7
        result[i] = ((data[i] << 1) | carry) & 0xFF
        carry = new_carry
    return bytes(result)

K1 = bytearray(left_shift_1bit(L))
if L[0] & 0x80:  # MSB of L is 1
    K1[15] ^= 0x87
K1 = bytes(K1)
print(f"K1 = L << 1:       {hexlify(K1).decode().upper()}")

# Step 3: K2 = K1 << 1
K2 = bytearray(left_shift_1bit(K1))
if K1[0] & 0x80:  # MSB of K1 is 1
    K2[15] ^= 0x87
K2 = bytes(K2)
print(f"K2 = K1 << 1:      {hexlify(K2).decode().upper()}")

print("\n=== Testing Full CMAC (AN12196 Table 27) ===\n")

# Full MAC input from AN12196 Table 27
mac_input = unhexlify("C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD")
expected_cmac = unhexlify("B7A60161F202EC3489BD4BEDEF64BB32")

# Calculate CMAC manually to understand the process
print(f"Input length: {len(mac_input)} bytes")
print(f"Number of blocks: {(len(mac_input) + 15) // 16}")

# Process blocks
blocks = []
for i in range(0, len(mac_input), 16):
    block = mac_input[i:i+16]
    if len(block) < 16:
        # Pad incomplete block
        block = block + b'\x80' + b'\x00' * (15 - len(block))
    blocks.append(block)

print(f"Complete block: {len(mac_input) % 16 == 0}\n")

# CBC-MAC processing
state = bytes(16)
for i, block in enumerate(blocks[:-1]):
    # XOR with state
    xored = bytes(a ^ b for a, b in zip(block, state))
    # Encrypt with ECB
    state = cipher.encrypt(xored)
    print(f"Block {i+1}: {hexlify(state).decode().upper()}")

# Last block
last_block = blocks[-1]
if len(mac_input) % 16 == 0:
    # XOR with K1
    last_block = bytes(a ^ b for a, b in zip(last_block, K1))
else:
    # XOR with K2
    last_block = bytes(a ^ b for a, b in zip(last_block, K2))

# XOR with state
last_block = bytes(a ^ b for a, b in zip(last_block, state))
# Final encryption
final = cipher.encrypt(last_block)

print(f"CMAC result: {hexlify(final).decode().upper()}")
print(f"Expected:    {hexlify(expected_cmac).decode().upper()}")
print(f"Match: {final == expected_cmac}")
