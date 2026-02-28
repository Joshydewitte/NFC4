#!/usr/bin/env python3
"""
Verify NTAG424 ChangeKey CMAC calculation
Based on user's log output
"""

from Crypto.Cipher import AES
from Crypto.Util.strxor import strxor

def hex_to_bytes(hex_str):
    """Convert hex string to bytes"""
    return bytes.fromhex(hex_str.replace(' ', ''))

def bytes_to_hex(b):
    """Convert bytes to hex string"""
    return ' '.join(f'{x:02X}' for x in b)

def aes_encrypt_ecb(key, data):
    """AES-ECB encryption"""
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(data)

def cmac_aes(key, data):
    """
    Calculate CMAC according to NIST SP 800-38B
    Returns full 16-byte CMAC
    """
    Rb = bytes([0x87])
    
    def leftshift(input_bytes):
        """Left shift by 1 bit"""
        carry = 0
        output = bytearray(16)
        for i in range(16):
            new_carry = (input_bytes[i] & 0x80) >> 7
            output[i] = ((input_bytes[i] << 1) | carry) & 0xFF
            carry = new_carry
        return bytes(output)
    
    # Generate subkeys
    L = aes_encrypt_ecb(key, bytes(16))
    K1 = leftshift(L)
    if L[0] & 0x80:
        K1 = strxor(K1, bytes(15) + Rb)
    
    K2 = leftshift(K1)
    if K1[0] & 0x80:
        K2 = strxor(K2, bytes(15) + Rb)
    
    print(f"  L:  {bytes_to_hex(L)}")
    print(f"  K1: {bytes_to_hex(K1)}")
    print(f"  K2: {bytes_to_hex(K2)}")
    
    # Process message
    n = (len(data) + 15) // 16 or 1
    complete = len(data) > 0 and len(data) % 16 == 0
    
    state = bytes(16)
    
    # Process all blocks except last
    for i in range(n - 1):
        block = data[i*16:(i+1)*16]
        block = strxor(block, state)
        state = aes_encrypt_ecb(key, block)
    
    # Process last block
    last_block_len = len(data) - ((n - 1) * 16)
    last_block = bytearray(16)
    
    if complete:
        last_block[:] = data[(n-1)*16:]
        last_block = strxor(bytes(last_block), K1)
    else:
        if last_block_len > 0:
            last_block[:last_block_len] = data[(n-1)*16:]
        last_block[last_block_len] = 0x80
        last_block = strxor(bytes(last_block), K2)
    
    last_block = strxor(bytes(last_block), state)
    T = aes_encrypt_ecb(key, bytes(last_block))
    
    print(f"  Full CMAC (T): {bytes_to_hex(T)}")
    
    return T

def cmac_truncate(full_cmac):
    """
    Truncate CMAC according to NTAG424 spec
    Takes even-numbered bytes (positions 2,4,6,8,10,12,14,16)
    Which are indices 1,3,5,7,9,11,13,15
    """
    return bytes([full_cmac[1], full_cmac[3], full_cmac[5], full_cmac[7],
                  full_cmac[9], full_cmac[11], full_cmac[13], full_cmac[15]])

def main():
    print("=== NTAG424 ChangeKey CMAC Verification ===\n")
    
    # From user's log - second authentication (for ChangeKey)
    print("Session Keys (from log):")
    session_enc_key = hex_to_bytes("814324EF6839F15357A36D3779692690")
    session_mac_key = hex_to_bytes("E44655DEB1A2DB965E36ED42DEE2D8AB")
    print(f"  ENC: {bytes_to_hex(session_enc_key)}")
    print(f"  MAC: {bytes_to_hex(session_mac_key)}")
    
    print("\nMAC Input (from log):")
    mac_input = hex_to_bytes("C4000055290C2700AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C4510")
    print(f"  {bytes_to_hex(mac_input)}")
    print(f"  Length: {len(mac_input)} bytes")
    
    # Parse MAC input
    print("\nMAC Input breakdown:")
    print(f"  CMD:         {bytes_to_hex(mac_input[0:1])}")
    print(f"  CmdCtr:      {bytes_to_hex(mac_input[1:3])} (LSB first, value={mac_input[1] | (mac_input[2] << 8)})")
    print(f"  TI:          {bytes_to_hex(mac_input[3:7])}")
    print(f"  KeyNo:       {bytes_to_hex(mac_input[7:8])}")
    print(f"  Encrypted:   {bytes_to_hex(mac_input[8:40])}")
    
    print("\nCalculating CMAC...")
    full_cmac = cmac_aes(session_mac_key, mac_input)
    
    print("\nTruncating CMAC (indices 1,3,5,7,9,11,13,15):")
    truncated = cmac_truncate(full_cmac)
    print(f"  CMACt: {bytes_to_hex(truncated)}")
    
    print("\nExpected (from log):")
    expected = hex_to_bytes("8BB3439D27D98693")
    print(f"  CMACt: {bytes_to_hex(expected)}")
    
    if truncated == expected:
        print("\n✅ CMAC MATCHES! Algorithm is correct.")
    else:
        print("\n❌ CMAC MISMATCH! There's an error in the implementation.")
        print(f"\nDifference:")
        for i in range(8):
            if truncated[i] != expected[i]:
                print(f"  Byte {i}: calculated={truncated[i]:02X}, expected={expected[i]:02X}")

if __name__ == "__main__":
    main()
