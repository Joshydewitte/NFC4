#!/usr/bin/env python3
"""
Complete ChangeKey verification byte-by-byte
"""

from Crypto.Cipher import AES
from Crypto.Util.strxor import strxor

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
    
    cipher = AES.new(key, AES.MODE_ECB)
    L = cipher.encrypt(bytes(16))
    K1 = leftshift(L)
    if L[0] & 0x80:
        K1 = strxor(K1, bytes(15) + Rb)
    
    K2 = leftshift(K1)
    if K1[0] & 0x80:
        K2 = strxor(K2, bytes(15) + Rb)
    
    n = (len(data) + 15) // 16 or 1
    complete = len(data) > 0 and len(data) % 16 == 0
    
    state = bytes(16)
    for i in range(n - 1):
        block = data[i*16:(i+1)*16]
        block = strxor(block, state)
        state = cipher.encrypt(block)
    
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
    return cipher.encrypt(bytes(last_block))

def cmac_truncate(full_cmac):
    return bytes([full_cmac[1], full_cmac[3], full_cmac[5], full_cmac[7],
                  full_cmac[9], full_cmac[11], full_cmac[13], full_cmac[15]])

def main():
    print("=== Complete ChangeKey Verification ===\n")
    
    # From log
    session_enc = hex_to_bytes("678901970A1021CAEDECE49BE3E23960")
    session_mac = hex_to_bytes("AC9F172042F8D946CCF89031C6780466")
    ti = hex_to_bytes("EE54BDB2")
    cmdctr = 0
    
    print(f"Session ENC Key: {bytes_to_hex(session_enc)}")
    print(f"Session MAC Key: {bytes_to_hex(session_mac)}")
    print(f"TI: {bytes_to_hex(ti)}")
    print(f"CmdCtr: {cmdctr}\n")
    
    # Plain data
    new_key = hex_to_bytes("10532710706128B62A990EE06ECC86A6")
    plain_data = new_key + bytes([0x01, 0x80]) + bytes(14)
    print(f"Plain Data: {bytes_to_hex(plain_data)}\n")
    
    # Calculate IV
    iv_input = bytes([0xA5, 0x5A]) + ti + bytes([cmdctr & 0xFF, (cmdctr >> 8) & 0xFF]) + bytes(8)
    print(f"IV Input: {bytes_to_hex(iv_input)}")
    
    cipher = AES.new(session_enc, AES.MODE_ECB)
    iv = cipher.encrypt(iv_input)
    print(f"IV: {bytes_to_hex(iv)}")
    
    expected_iv = hex_to_bytes("08E7214CA550627CE9E87EFA0E1A97A9")
    print(f"Expected IV from log: {bytes_to_hex(expected_iv)}")
    if iv == expected_iv:
        print("✅ IV MATCHES\n")
    else:
        print("❌ IV MISMATCH\n")
        return
    
    # Encrypt
    cipher = AES.new(session_enc, AES.MODE_CBC, iv)
    encrypted = cipher.encrypt(plain_data)
    print(f"Encrypted: {bytes_to_hex(encrypted)}")
    
    expected_enc = hex_to_bytes("9302C47F28F747EAC5B8A0B791264D74BDA5DC2973E67A42FA14CBE88B4B125E")
    print(f"Expected from log: {bytes_to_hex(expected_enc)}")
    if encrypted == expected_enc:
        print("✅ ENCRYPTION MATCHES\n")
    else:
        print("❌ ENCRYPTION MISMATCH\n")
        return
    
    # MAC Input
    keyno = 0
    mac_input = bytes([0xC4]) + bytes([cmdctr & 0xFF, (cmdctr >> 8) & 0xFF]) + ti + bytes([keyno]) + encrypted
    print(f"MAC Input ({len(mac_input)} bytes):")
    print(f"  {bytes_to_hex(mac_input)}")
    
    expected_mac_input = hex_to_bytes("C40000EE54BDB2009302C47F28F747EAC5B8A0B791264D74BDA5DC2973E67A42FA14CBE88B4B125E")
    print(f"Expected from log:")
    print(f"  {bytes_to_hex(expected_mac_input)}")
    if mac_input == expected_mac_input:
        print("✅ MAC INPUT MATCHES\n")
    else:
        print("❌ MAC INPUT MISMATCH\n")
        return
    
    # Calculate CMAC
    full_cmac = cmac_full(session_mac, mac_input)
    print(f"Full CMAC: {bytes_to_hex(full_cmac)}")
    
    cmac_t = cmac_truncate(full_cmac)
    print(f"CMACt: {bytes_to_hex(cmac_t)}")
    
    expected_cmac = hex_to_bytes("B8B7E5472DB4532A")
    print(f"Expected from log: {bytes_to_hex(expected_cmac)}")
    if cmac_t == expected_cmac:
        print("✅ CMAC MATCHES\n")
    else:
        print("❌ CMAC MISMATCH\n")
        return
    
    # Build complete command
    cmd = bytes([0xC4, keyno]) + encrypted + cmac_t
    print(f"Command ({len(cmd)} bytes):")
    print(f"  {bytes_to_hex(cmd)}\n")
    
    # Build APDU
    apdu = bytes([0x90, 0xC4, 0x00, 0x00, len(cmd) - 1]) + cmd[1:] + bytes([0x00])
    print(f"APDU ({len(apdu)} bytes):")
    print(f"  {bytes_to_hex(apdu)}\n")
    
    expected_apdu = hex_to_bytes("90C4000029009302C47F28F747EAC5B8A0B791264D74BDA5DC2973E67A42FA14CBE88B4B125EB8B7E5472DB4532A00")
    print(f"Expected APDU from log:")
    print(f"  {bytes_to_hex(expected_apdu)}\n")
    
    if apdu == expected_apdu:
        print("✅✅✅ COMPLETE APDU MATCHES! ✅✅✅")
        print("\nAll cryptographic operations are 100% CORRECT!")
        print("The issue must be elsewhere (timing, card state, etc.)")
    else:
        print("❌ APDU MISMATCH")
        for i in range(len(apdu)):
            if i >= len(expected_apdu) or apdu[i] != expected_apdu[i]:
                print(f"  Byte {i}: got {apdu[i]:02X}, expected {expected_apdu[i]:02X if i < len(expected_apdu) else '??'}")

if __name__ == "__main__":
    main()
