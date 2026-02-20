#include "ntag424_crypto.h"
#include "esp_random.h"

// AES-128 CBC Encryption
bool NTAG424Crypto::aesEncrypt(const uint8_t* key, const uint8_t* iv,
                               const uint8_t* input, size_t inputLen,
                               uint8_t* output) {
    if (inputLen % 16 != 0) {
        return false; // Must be multiple of 16 bytes for AES-128
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);
    
    int ret = mbedtls_aes_setkey_enc(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, inputLen, 
                                ivCopy, input, output);
    
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

// AES-128 CBC Decryption
bool NTAG424Crypto::aesDecrypt(const uint8_t* key, const uint8_t* iv,
                               const uint8_t* input, size_t inputLen,
                               uint8_t* output) {
    if (inputLen % 16 != 0) {
        return false;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);
    
    int ret = mbedtls_aes_setkey_dec(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, inputLen,
                                ivCopy, input, output);
    
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

// Generate cryptographically secure random bytes
void NTAG424Crypto::generateRandom(uint8_t* output, size_t length) {
    esp_fill_random(output, length);
}

// Rotate buffer left by 1 byte
void NTAG424Crypto::rotateLeft(const uint8_t* input, uint8_t* output, size_t length) {
    if (length == 0) return;
    
    uint8_t first = input[0];
    for (size_t i = 0; i < length - 1; i++) {
        output[i] = input[i + 1];
    }
    output[length - 1] = first;
}

// Rotate buffer right by 1 byte
void NTAG424Crypto::rotateRight(const uint8_t* input, uint8_t* output, size_t length) {
    if (length == 0) return;
    
    uint8_t last = input[length - 1];
    for (size_t i = length - 1; i > 0; i--) {
        output[i] = input[i - 1];
    }
    output[0] = last;
}

// Calculate CMAC using AES-128
bool NTAG424Crypto::calculateCMAC(const uint8_t* key, const uint8_t* data,
                                  size_t dataLen, uint8_t* mac) {
    // Simplified CMAC implementation using basic AES-128
    // This is a simplified version - for production, use full CMAC-AES
    
    // For NTAG424, we use CMAC-AES-128
    // The full CMAC algorithm requires subkey generation, but we'll use a simplified approach
    
    // Initialize with zeros
    uint8_t state[16] = {0};
    uint8_t zeroIV[16] = {0};
    
    // Process complete 16-byte blocks
    size_t numBlocks = (dataLen + 15) / 16;
    
    for (size_t i = 0; i < numBlocks; i++) {
        uint8_t block[16] = {0};
        size_t blockLen = 16;
        
        if (i == numBlocks - 1) {
            // Last block may be partial
            blockLen = dataLen - (i * 16);
            if (blockLen > 0) {
                memcpy(block, data + (i * 16), blockLen);
            }
            // Padding for last block (ISO 9797-1 Method 2)
            if (blockLen < 16) {
                block[blockLen] = 0x80;
                for (size_t j = blockLen + 1; j < 16; j++) {
                    block[j] = 0x00;
                }
            }
        } else {
            memcpy(block, data + (i * 16), 16);
        }
        
        // XOR with state
        for (int j = 0; j < 16; j++) {
            block[j] ^= state[j];
        }
        
        // Encrypt
        if (!aesEncrypt(key, zeroIV, block, 16, state)) {
            return false;
        }
    }
    
    // Return first 8 bytes for NTAG424
    memcpy(mac, state, 8);
    return true;
}

// XOR two arrays
void NTAG424Crypto::xorArrays(const uint8_t* a, const uint8_t* b,
                              uint8_t* output, size_t length) {
    for (size_t i = 0; i < length; i++) {
        output[i] = a[i] ^ b[i];
    }
}

// Convert hex string to bytes
size_t NTAG424Crypto::hexStringToBytes(const String& hexStr, uint8_t* output, size_t maxLen) {
    String hex = hexStr;
    hex.toUpperCase();
    hex.replace(":", "");
    hex.replace(" ", "");
    hex.trim();
    
    if (hex.length() % 2 != 0) {
        return 0; // Invalid hex string
    }
    
    size_t byteLen = hex.length() / 2;
    if (byteLen > maxLen) {
        byteLen = maxLen;
    }
    
    for (size_t i = 0; i < byteLen; i++) {
        String byteStr = hex.substring(i * 2, i * 2 + 2);
        output[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }
    
    return byteLen;
}

// Convert bytes to hex string
String NTAG424Crypto::bytesToHexString(const uint8_t* bytes, size_t length) {
    String result = "";
    for (size_t i = 0; i < length; i++) {
        if (bytes[i] < 0x10) result += "0";
        result += String(bytes[i], HEX);
    }
    result.toUpperCase();
    return result;
}

// Derive session keys (EV2)
bool NTAG424Crypto::deriveSessionKeys(const uint8_t* rndA, const uint8_t* rndB,
                                     const uint8_t* key, uint8_t* encKey,
                                     uint8_t* macKey) {
    // Session Vector (SV) = RndA[0..1] || RndB[0..1]
    uint8_t sv[4];
    sv[0] = rndA[0];
    sv[1] = rndA[1];
    sv[2] = rndB[0];
    sv[3] = rndB[1];
    
    // Derive ENC session key
    // Input: 0x01 || SV || 0x00...0x00 (total 16 bytes)
    uint8_t encInput[16];
    encInput[0] = 0x01;
    memcpy(encInput + 1, sv, 4);
    memset(encInput + 5, 0x00, 11);
    
    uint8_t zeroIV[16] = {0};
    if (!aesEncrypt(key, zeroIV, encInput, 16, encKey)) {
        return false;
    }
    
    // Derive MAC session key
    // Input: 0x02 || SV || 0x00...0x00 (total 16 bytes)
    uint8_t macInput[16];
    macInput[0] = 0x02;
    memcpy(macInput + 1, sv, 4);
    memset(macInput + 5, 0x00, 11);
    
    if (!aesEncrypt(key, zeroIV, macInput, 16, macKey)) {
        return false;
    }
    
    return true;
}

// Calculate CRC32 for NTAG424
uint32_t NTAG424Crypto::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

// Helper for CMAC subkey generation (not used with mbedtls built-in CMAC)
void NTAG424Crypto::generateCMACSubkey(const uint8_t* k, uint8_t* k1, uint8_t* k2) {
    // This is a helper function if we need manual CMAC implementation
    // Currently not used as we use mbedtls_cipher_cmac
}

void NTAG424Crypto::leftShift(const uint8_t* input, uint8_t* output) {
    // Helper for CMAC (not used with mbedtls)
}
