#!/usr/bin/env python3
"""
Verifies the IVc calculation method: ECB vs CBC-with-authIV
Uses real data from AN12196 Table 9 AND current user's failed scan.
"""
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from binascii import hexlify, unhexlify

def aes_ecb(key, data):
    return AES.new(key, AES.MODE_ECB).encrypt(data)

def aes_cbc_encrypt(key, iv, data):
    return AES.new(key, AES.MODE_CBC, iv=iv).encrypt(data)

def cmac_truncate(key, data):
    cobj = CMAC.new(key, ciphermod=AES)
    cobj.update(data)
    full = cobj.digest()
    return bytes([full[i] for i in [1,3,5,7,9,11,13,15]])

print("=" * 70)
print("TEST 1: AN12196 Table 9 CommMode.FULL example")
print("=" * 70)
# Values from AN12196 Table 9
sess_enc_t9    = unhexlify("7305E2CCA5B0377617CDBFEB96C9B358")
ti_t9          = unhexlify("856C1841")
cmd_ctr_t9     = 0
iv_input_t9    = bytes([0xA5,0x5A]) + ti_t9 + bytes([cmd_ctr_t9&0xFF,(cmd_ctr_t9>>8)&0xFF]) + bytes(8)
expected_ivc_t9 = unhexlify("81DACD2EB2D257FC556E8952DD665B58")

ivc_ecb  = aes_ecb(sess_enc_t9, iv_input_t9)
ivc_cbc0 = aes_cbc_encrypt(sess_enc_t9, bytes(16), iv_input_t9)  # same as ECB

print(f"IV Input:    {hexlify(iv_input_t9).decode().upper()}")
print(f"Expected IVc: {hexlify(expected_ivc_t9).decode().upper()}")
print(f"ECB result:   {hexlify(ivc_ecb).decode().upper()}  {'✅ MATCH' if ivc_ecb == expected_ivc_t9 else '❌ MISMATCH'}")

print()
print("=" * 70)
print("TEST 2: AN12196 Table 27 ChangeKey example (CmdCtr=3)")
print("=" * 70)
sess_enc_t27   = unhexlify("4CF3CB41A22583A61E89B158D252FC53")
ti_t27         = unhexlify("7614281A")
cmd_ctr_t27    = 3
iv_input_t27   = bytes([0xA5,0x5A]) + ti_t27 + bytes([cmd_ctr_t27&0xFF,(cmd_ctr_t27>>8)&0xFF]) + bytes(8)
expected_ivc_t27 = unhexlify("01602D579423B2797BE8B478B0B4D27B")

ivc_ecb_t27 = aes_ecb(sess_enc_t27, iv_input_t27)
print(f"IV Input:    {hexlify(iv_input_t27).decode().upper()}")
print(f"Expected IVc: {hexlify(expected_ivc_t27).decode().upper()}")
print(f"ECB result:   {hexlify(ivc_ecb_t27).decode().upper()}  {'✅ MATCH' if ivc_ecb_t27 == expected_ivc_t27 else '❌ MISMATCH'}")

print()
print("=" * 70)
print("TEST 3: Current user scan - what key did the card ACTUALLY store?")
print("=" * 70)
# From the user's log
sess_enc  = unhexlify("7FCBBD698FB4382C5D818762A6FA68B6")
sess_mac  = unhexlify("EE88FC271527082DBCDC86FCB1FDA337")
ti        = unhexlify("1BBFA6F8")
auth_iv   = unhexlify("46306B117F5201434ACCC7685690DDE9")  # currentIV set after auth
cmd_ctr   = 0
desired_key = unhexlify("B55AA9800372B4DA19D799D51D1EB002")

iv_input  = bytes([0xA5,0x5A]) + ti + bytes([cmd_ctr&0xFF,(cmd_ctr>>8)&0xFF]) + bytes(8)
print(f"IV Input:  {hexlify(iv_input).decode().upper()}")
print(f"Auth IV:   {hexlify(auth_iv).decode().upper()}")

# Method A: Our code - CBC with auth IV (WRONG)
ivc_wrong = aes_cbc_encrypt(sess_enc, auth_iv, iv_input)
print(f"\nIVc (our code, CBC+authIV): {hexlify(ivc_wrong).decode().upper()}")
print(f"  (Matches log IV={hexlify(ivc_wrong).decode().upper()[:32].upper()})")

# Method B: Correct - ECB (zero IV)
ivc_correct = aes_ecb(sess_enc, iv_input)
print(f"IVc (correct, ECB/zero):    {hexlify(ivc_correct).decode().upper()}")

# Plaintext
plain = desired_key + bytes([0x01, 0x80]) + bytes(14)

# Encryption A: our code (wrong IVc) - what we SENT to the card
enc_wrong = aes_cbc_encrypt(sess_enc, ivc_wrong, plain)
# Encryption B: correct IVc - what the card DECRYPTS to
enc_correct = aes_cbc_encrypt(sess_enc, ivc_correct, plain)

print(f"\nOur EncData (sent):  {hexlify(enc_wrong).decode().upper()}")
expected_enc_from_log = "8A6BB9EE0E1EA8CC2E6C186787AB6523C11F466CBEC755BF3DDB6D523386A6B8"
print(f"Log EncData (logged):{expected_enc_from_log}")
print(f"  Match: {'✅' if hexlify(enc_wrong).decode().upper() == expected_enc_from_log else '❌'}")

print(f"\nCard-decrypted key (what card STORED, using correct IVc):")
# Card receives enc_wrong, decrypts with ivc_correct
key_stored_by_card = aes_cbc_encrypt(sess_enc, ivc_correct, enc_wrong)  # CBC decrypt via ECB trick
# Actually let's do a proper CBC decrypt
from Crypto.Cipher import AES as _AES
dec = _AES.new(sess_enc, _AES.MODE_CBC, iv=ivc_correct)
key_stored_by_card = dec.decrypt(enc_wrong)
print(f"  Plaintext card got: {hexlify(key_stored_by_card).decode().upper()}")
print(f"  Desired key:        {hexlify(desired_key).decode().upper()}01800000000000000000000000000000")
print(f"  Key MATCH: {'✅ Card stored CORRECT key' if key_stored_by_card[:16] == desired_key else '❌ Card stored WRONG/GARBAGE key'}")

print()
print("=" * 70)
print("TEST 4: Verify what CORRECT EncData should be (with ECB/zero IV)")
print("=" * 70)
enc_correct_data = aes_cbc_encrypt(sess_enc, ivc_correct, plain)
print(f"Correct EncData: {hexlify(enc_correct_data).decode().upper()}")

# CMAC with CORRECT enc data
mac_input = bytes([0xC4]) + bytes([cmd_ctr&0xFF,(cmd_ctr>>8)&0xFF]) + ti + bytes([0x00]) + enc_correct_data
cobj = CMAC.new(sess_mac, ciphermod=AES)
cobj.update(mac_input)
cmac_full = cobj.digest()
cmac_t = bytes([cmac_full[i] for i in [1,3,5,7,9,11,13,15]])
print(f"Correct CMAC (truncated): {hexlify(cmac_t).decode().upper()}")

correct_apdu_data = bytes([0x00]) + enc_correct_data + cmac_t
print(f"Correct APDU payload (41 bytes): {hexlify(correct_apdu_data).decode().upper()}")
print(f"Full APDU: 90C4000029{hexlify(correct_apdu_data).decode().upper()}00")

print()
print("=" * 70)
print("CONCLUSION")
print("=" * 70)
if ivc_ecb == expected_ivc_t9 and ivc_ecb_t27 == expected_ivc_t27:
    print("✅ CONFIRMED: IVc calculation is AES-ECB (zero IV)")
    print("✅ AN12196 Table 9 AND Table 27 both confirmed with ECB")
    print()
    if hexlify(enc_wrong).decode().upper() == expected_enc_from_log:
        print("🚨 BUG CONFIRMED: Our code uses authIV for IVc → WRONG EncData!")
        print("🚨 Card stored GARBAGE key because it decrypts with ECB-based IVc")
        print("🚨 That's why re-auth fails: card has garbage key, not derived key")
        print()
        print("FIX: In changeKey() - replace 'currentIV' with zeros for IVc calculation")
        print("     uint8_t zeroIV[16] = {0};")
        print("     aesEncrypt(sessionEncKey, zeroIV, ivInput, 16, iv)  // not currentIV!")
