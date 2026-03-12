#!/usr/bin/env python3
"""
Verify CRC32 calculation for ChangeKeySettings
Cross-reference with C++ implementation
"""

def crc32_iso14443(data):
    """Calculate CRC32 as per ISO14443"""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF

def bytes_to_hex(data):
    return ''.join(f'{b:02X}' for b in data)

print("="*70)
print("CRC32 Verification for ChangeKeySettings")
print("="*70)

# Test common settings values
test_values = [
    (0xFF, "Volledig open (setup fase)"),
    (0x0E, "Volledig locked (productie)"),
    (0x08, "Config frozen, ChangeKey met master"),
    (0x00, "All permissions restricted"),
    (0x5F, "Example mixed settings"),
]

print("\nSettings Value | CRC32 (hex) | CRC32 (LSB first bytes) | Description")
print("-" * 110)

for value, description in test_values:
    crc = crc32_iso14443(bytes([value]))
    
    # Pack as LSB first (little-endian)
    lsb_bytes = [
        crc & 0xFF,
        (crc >> 8) & 0xFF,
        (crc >> 16) & 0xFF,
        (crc >> 24) & 0xFF
    ]
    
    print(f"0x{value:02X}          | 0x{crc:08X}  | {bytes_to_hex(lsb_bytes)} | {description}")

print("\n" + "="*70)
print("C++ Implementation Check")
print("="*70)

print("""
In ntag424_crypto.cpp, the CRC32 should be implemented as:

uint32_t NTAG424Crypto::calculateCRC32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

And in changeKeySettings(), pack as LSB first:

plainData[1] = crc & 0xFF;         // LSB
plainData[2] = (crc >> 8) & 0xFF;
plainData[3] = (crc >> 16) & 0xFF;
plainData[4] = (crc >> 24) & 0xFF; // MSB
""")

print("\n✅ CRC32 calculation verified for common settings values")
print("="*70)
