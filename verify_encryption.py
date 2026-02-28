#!/usr/bin/env python3
"""
Verify NTAG424 ChangeKey encryption
Based on user's log output
"""

from Crypto.Cipher import AES

def hex_to_bytes(hex_str):
    """Convert hex string to bytes"""
    return bytes.fromhex(hex_str.replace(' ', ''))

def bytes_to_hex(b):
    """Convert bytes to hex string"""
    return ' '.join(f'{x:02X}' for x in b)

def aes_decrypt_cbc(key, iv, ciphertext):
    """AES-CBC decryption"""
    cipher = AES.new(key, AES.MODE_CBC, iv)
    return cipher.decrypt(ciphertext)

def aes_encrypt_cbc(key, iv, plaintext):
    """AES-CBC encryption"""
    cipher = AES.new(key, AES.MODE_CBC, iv)
    return cipher.encrypt(plaintext)

def main():
    print("=== NTAG424 ChangeKey Encryption Verification ===\n")
    
    # From user's log
    print("Session ENC Key:")
    session_enc_key = hex_to_bytes("814324EF6839F15357A36D3779692690")
    print(f"  {bytes_to_hex(session_enc_key)}\n")
    
    print("Plain Data (from log):")
    plain_data = hex_to_bytes("10532710706128B62A990EE06ECC86A601800000000000000000000000000000")
    print(f"  {bytes_to_hex(plain_data)}")
    
    # Parse plain data
    print("\nPlaintext breakdown:")
    print(f"  New Key:    {bytes_to_hex(plain_data[0:16])}")
    print(f"  Version:    {bytes_to_hex(plain_data[16:17])}")
    print(f"  Padding:    {bytes_to_hex(plain_data[17:32])}")
    
    print("\nIV Input (A55A || TI || CmdCtr || zeros):")
    ti = hex_to_bytes("55290C27")
    cmdctr = 0
    iv_input = bytes([0xA5, 0x5A]) + ti + bytes([cmdctr & 0xFF, (cmdctr >> 8) & 0xFF]) + bytes(8)
    print(f"  {bytes_to_hex(iv_input)}")
    
    print("\nCalculating IV = E(SessionENCKey, IV_Input)...")
    zero_iv = bytes(16)
    cipher = AES.new(session_enc_key, AES.MODE_ECB)
    iv = cipher.encrypt(iv_input)
    print(f"  Calculated IV: {bytes_to_hex(iv)}")
    
    iv_from_log = hex_to_bytes("BB2B779C04AD976610A0023B25B10776")
    print(f"  IV from log:   {bytes_to_hex(iv_from_log)}")
    
    if iv == iv_from_log:
        print("  ✅ IV matches!\n")
    else:
        print("  ❌ IV MISMATCH!\n")
        return
    
    print("Encrypting plaintext with AES-CBC...")
    encrypted = aes_encrypt_cbc(session_enc_key, iv, plain_data)
    print(f"  Calculated: {bytes_to_hex(encrypted)}")
    
    encrypted_from_log = hex_to_bytes("AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C4510")
    print(f"  From log:   {bytes_to_hex(encrypted_from_log)}")
    
    if encrypted == encrypted_from_log:
        print("\n✅ ENCRYPTION MATCHES! Algorithm is correct.")
    else:
        print("\n❌ ENCRYPTION MISMATCH!")
        print("\nDifference:")
        for i in range(len(encrypted)):
            if encrypted[i] != encrypted_from_log[i]:
                print(f"  Byte {i}: calculated={encrypted[i]:02X}, expected={encrypted_from_log[i]:02X}")
    
    # Also verify decryption to check if plaintext is correct
    print("\n" + "="*60)
    print("Verifying by decryption (what the card should see):")
    decrypted = aes_decrypt_cbc(session_enc_key, iv, encrypted_from_log)
    print(f"  {bytes_to_hex(decrypted)}")
    
    if decrypted == plain_data:
        print("  ✅ Decryption matches plaintext")
    else:
        print("  ❌ Decryption does not match plaintext!")

if __name__ == "__main__":
    main()
