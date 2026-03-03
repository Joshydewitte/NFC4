#!/usr/bin/env python3
"""
Create test case for ESP32 CMAC to debug
Generate expected values for AN12196 Table 27 that ESP32 should produce
"""

from Crypto.Hash import CMAC
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def truncate_cmac_even_bytes(cmac_full):
    """Even-numbered byte positions (indices 1,3,5,7,9,11,13,15)"""
    return bytes([cmac_full[1], cmac_full[3], cmac_full[5], cmac_full[7],
                  cmac_full[9], cmac_full[11], cmac_full[13], cmac_full[15]])

print("=== ESP32 CMAC Unit Test Values ===\n")
print("Test Case: AN12196 Table 27 ChangeKey Case 2\n")

# AN12196 Table 27 values
mac_key = unhexlify("5529860B2FC5FB6154B7F28361D30BF9")
mac_input = unhexlify("C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD")
expected_cmac_full = unhexlify("B7A60161F202EC3489BD4BEDEF64BB32")
expected_cmac_t = unhexlify("A6610234BDED6432")

# Calculate with Python
cobj = CMAC.new(mac_key, ciphermod=AES)
cobj.update(mac_input)
calc_cmac_full = cobj.digest()
calc_cmac_t = truncate_cmac_even_bytes(calc_cmac_full)

print(f"MAC Key:     {hexlify(mac_key).decode().upper()}")
print(f"MAC Input:   {hexlify(mac_input).decode().upper()}")
print(f"  Length: {len(mac_input)} bytes")
print(f"\nExpected CMAC Full: {hexlify(expected_cmac_full).decode().upper()}")
print(f"Calculated CMAC:    {hexlify(calc_cmac_full).decode().upper()}")
print(f"Match: {calc_cmac_full == expected_cmac_full}\n")

print(f"Expected CMAC Trunc: {hexlify(expected_cmac_t).decode().upper()}")
print(f"Calculated Trunc:    {hexlify(calc_cmac_t).decode().upper()}")
print(f"Match: {calc_cmac_t == expected_cmac_t}\n")

# Generate C code snippet for ESP32 test
print("\n=== C Code for ESP32 Unit Test ===\n")
print("```cpp")
print("// Test AN12196 Table 27 CMAC")
print(f"uint8_t testKey[16] = {{{', '.join(f'0x{b:02X}' for b in mac_key)}}};")
print(f"uint8_t testData[{len(mac_input)}] = {{")
for i in range(0, len(mac_input), 16):
    chunk = mac_input[i:i+16]
    print(f"    {', '.join(f'0x{b:02X}' for b in chunk)},")
print("};")
print(f"uint8_t expectedCMAC[16] = {{{', '.join(f'0x{b:02X}' for b in expected_cmac_full)}}};")
print(f"uint8_t expectedCMACt[8] = {{{', '.join(f'0x{b:02X}' for b in expected_cmac_t)}}};")
print("\nuint8_t calculatedFull[16];")
print("uint8_t calculatedTrunc[8];")
print(f"NTAG424Crypto::calculateCMACFull(testKey, testData, {len(mac_input)}, calculatedFull);")
print(f"NTAG424Crypto::calculateCMAC(testKey, testData, {len(mac_input)}, calculatedTrunc);")
print("\nif (memcmp(calculatedFull, expectedCMAC, 16) == 0) {")
print('    Serial.println("✅ CMAC Full CORRECT");')
print("} else {")
print('    Serial.println("❌ CMAC Full WRONG");')
print("}")
print("```")
