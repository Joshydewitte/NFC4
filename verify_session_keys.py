#!/usr/bin/env python3
"""
Verify NTAG424 Session Key Derivation from log
"""

from Crypto.Cipher import AES

def hex_to_bytes(hex_str):
    return bytes.fromhex(hex_str.replace(' ', ''))

def bytes_to_hex(b):
    return ' '.join(f'{x:02X}' for x in b)

def cmac_full(key, data):
    """Calculate full 16-byte CMAC"""
    Rb = bytes([0x87])
    
    def leftshift(input_bytes):
        carry = 0
        output = bytearray(16)
        for i in range(16):
            new_carry = (input_bytes[i] & 0x80) >> 7
            output[i] = ((input_bytes[i] << 1) | carry) & 0xFF
            carry = new_carry
        return bytes(output)
    
    # Generate subkeys
    cipher = AES.new(key, AES.MODE_ECB)
    L = cipher.encrypt(bytes(16))
    K1 = leftshift(L)
    if L[0] & 0x80:
        K1 = bytes(a ^ b for a, b in zip(K1, bytes(15) + Rb))
    
    K2 = leftshift(K1)
    if K1[0] & 0x80:
        K2 = bytes(a ^ b for a, b in zip(K2, bytes(15) + Rb))
    
    # Process message
    n = (len(data) + 15) // 16 or 1
    complete = len(data) > 0 and len(data) % 16 == 0
    
    state = bytes(16)
    
    # Process all blocks except last
    for i in range(n - 1):
        block = data[i*16:(i+1)*16]
        block = bytes(a ^ b for a, b in zip(block, state))
        state = cipher.encrypt(block)
    
    # Process last block
    last_block_len = len(data) - ((n - 1) * 16)
    last_block = bytearray(16)
    
    if complete:
        last_block[:] = data[(n-1)*16:]
        last_block = bytes(a ^ b for a, b in zip(bytes(last_block), K1))
    else:
        if last_block_len > 0:
            last_block[:last_block_len] = data[(n-1)*16:]
        last_block[last_block_len] = 0x80
        last_block = bytes(a ^ b for a, b in zip(bytes(last_block), K2))
    
    last_block = bytes(a ^ b for a, b in zip(bytes(last_block), state))
    return cipher.encrypt(bytes(last_block))

def main():
    print("=== Session Key Derivation Verification ===\n")
    
    # From log
    print("From log:")
    key = hex_to_bytes("00000000000000000000000000000000")  # Factory default
    rndA = hex_to_bytes("20799F2956D6F9FB40BDC42D91E87A6B")
    rndB = hex_to_bytes("6E2722B3EC06A9EFC1D51CD16AA54925")
    
    print(f"Key:  {bytes_to_hex(key)}")
    print(f"RndA: {bytes_to_hex(rndA)}")
    print(f"RndB: {bytes_to_hex(rndB)}\n")
    
    # Build SV1 (Encryption Key)
    print("Building SV1 (A5 5A || ...):")
    sv1 = bytearray(32)
    sv1[0] = 0xA5
    sv1[1] = 0x5A
    sv1[2] = 0x00  # Counter LSB
    sv1[3] = 0x01  # Counter MSB
    sv1[4] = 0x00  # Length LSB
    sv1[5] = 0x80  # Length MSB
    
    # RndA[15..14]
    sv1[6] = rndA[15]
    sv1[7] = rndA[14]
    
    # RndA[13..8] XOR RndB[15..10]
    for i in range(6):
        sv1[8 + i] = rndA[13 - i] ^ rndB[15 - i]
    
    # RndB[9..0]
    for i in range(10):
        sv1[14 + i] = rndB[9 - i]
    
    # RndA[7..0]
    for i in range(8):
        sv1[24 + i] = rndA[7 - i]
    
    print(f"SV1: {bytes_to_hex(sv1)}\n")
    
    # Derive Session ENC Key
    enc_key = cmac_full(key, bytes(sv1))
    print(f"Calculated Session ENC Key: {bytes_to_hex(enc_key)}")
    
    expected_enc = hex_to_bytes("678901970A1021CAEDECE49BE3E23960")
    print(f"Expected from log:          {bytes_to_hex(expected_enc)}")
    
    if enc_key == expected_enc:
        print("✅ ENC Key MATCHES!\n")
    else:
        print("❌ ENC Key MISMATCH!\n")
        return
   
    # Build SV2 (MAC Key)
    print("Building SV2 (5A A5 || ...):")
    sv2 = bytearray(sv1)
    sv2[0] = 0x5A
    sv2[1] = 0xA5
    print(f"SV2: {bytes_to_hex(sv2)}\n")
    
    # Derive Session MAC Key
    mac_key = cmac_full(key, bytes(sv2))
    print(f"Calculated Session MAC Key: {bytes_to_hex(mac_key)}")
    
    expected_mac = hex_to_bytes("AC9F172042F8D946CCF89031C6780466")
    print(f"Expected from log:          {bytes_to_hex(expected_mac)}")
    
    if mac_key == expected_mac:
        print("✅ MAC Key MATCHES!\n")
    else:
        print("❌ MAC Key MISMATCH!\n")

if __name__ == "__main__":
    main()
