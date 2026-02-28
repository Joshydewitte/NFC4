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

// Calculate CMAC using AES-128 according to NIST SP 800-38B
bool NTAG424Crypto::calculateCMAC(const uint8_t* key, const uint8_t* data,
                                  size_t dataLen, uint8_t* mac) {
    // CMAC-AES-128 implementation per NIST SP 800-38B
    // https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38B.pdf
    
    const uint8_t Rb = 0x87;  // Constant for AES-128
    uint8_t zeroIV[16] = {0};
    
    // Helper lambda: Left shift array by 1 bit (big-endian byte order)
    // In big-endian: byte[0] is MSB, byte[15] is LSB
    // Left shift: carry flows from MSB to LSB (index 0 -> 15)
    auto leftShift = [](const uint8_t* input, uint8_t* output) {
        uint8_t carry = 0;
        for (int i = 0; i < 16; i++) {  // Loop from MSB (0) to LSB (15)
            uint8_t newCarry = (input[i] & 0x80) >> 7;  // Extract MSB
            output[i] = (input[i] << 1) | carry;         // Shift and OR with previous carry
            carry = newCarry;                             // Update carry for next byte
        }
    };
    
    // Step 1: Generate subkeys K1 and K2
    // L = AES-128(K, 0^128)
    uint8_t L[16] = {0};
    uint8_t zeroBlock[16] = {0};
    if (!aesEncrypt(key, zeroIV, zeroBlock, 16, L)) {
        return false;
    }
    
    // K1 = L << 1
    uint8_t K1[16];
    leftShift(L, K1);
    if (L[0] & 0x80) {  // If MSB(L) = 1
        K1[15] ^= Rb;
    }
    
    // K2 = K1 << 1
    uint8_t K2[16];
    leftShift(K1, K2);
    if (K1[0] & 0x80) {  // If MSB(K1) = 1
        K2[15] ^= Rb;
    }
    
    // Step 2: Determine number of blocks
    size_t n = (dataLen + 15) / 16;  // Round up
    if (n == 0) {
        n = 1;  // At least one block
    }
    
    bool completeBlock = (dataLen > 0) && (dataLen % 16 == 0);
    
    // Step 3: Process all blocks except the last
    uint8_t state[16] = {0};
    
    for (size_t i = 0; i < n - 1; i++) {
        uint8_t block[16];
        memcpy(block, data + (i * 16), 16);
        
        // XOR with state (CBC mode)
        for (int j = 0; j < 16; j++) {
            block[j] ^= state[j];
        }
        
        // Encrypt
        if (!aesEncrypt(key, zeroIV, block, 16, state)) {
            return false;
        }
    }
    
    // Step 4: Process last block with subkey
    uint8_t lastBlock[16] = {0};
    size_t lastBlockLen = dataLen - ((n - 1) * 16);
    
    if (completeBlock) {
        // Complete last block: M_last = M_n XOR K1
        memcpy(lastBlock, data + ((n - 1) * 16), 16);
        for (int j = 0; j < 16; j++) {
            lastBlock[j] ^= K1[j];
        }
    } else {
        // Incomplete last block: M_last = (M_n || 10...0) XOR K2
        if (lastBlockLen > 0) {
            memcpy(lastBlock, data + ((n - 1) * 16), lastBlockLen);
        }
        lastBlock[lastBlockLen] = 0x80;  // Padding
        for (size_t j = lastBlockLen + 1; j < 16; j++) {
            lastBlock[j] = 0x00;
        }
        for (int j = 0; j < 16; j++) {
            lastBlock[j] ^= K2[j];
        }
    }
    
    // XOR with state (CBC mode)
    for (int j = 0; j < 16; j++) {
        lastBlock[j] ^= state[j];
    }
    
    // Final encryption
    uint8_t T[16];
    if (!aesEncrypt(key, zeroIV, lastBlock, 16, T)) {
        return false;
    }
    
    // Truncate MAC according to NTAG424 specification (NIST SP 800-38B):
    // "Even-numbered bytes" means positions 2, 4, 6, 8, 10, 12, 14, 16
    // Which corresponds to array indices 1, 3, 5, 7, 9, 11, 13, 15 (odd indices)
    // Retained in order (NOT reversed)
    mac[0] = T[1];
    mac[1] = T[3];
    mac[2] = T[5];
    mac[3] = T[7];
    mac[4] = T[9];
    mac[5] = T[11];
    mac[6] = T[13];
    mac[7] = T[15];
    
    return true;
}

// Calculate full 16-byte CMAC (not truncated) for session key derivation
bool NTAG424Crypto::calculateCMACFull(const uint8_t* key, const uint8_t* data,
                                      size_t dataLen, uint8_t* mac) {
    // Same algorithm as calculateCMAC but returns full 16-byte result
    const uint8_t Rb = 0x87;
    uint8_t zeroIV[16] = {0};
    
    auto leftShift = [](const uint8_t* input, uint8_t* output) {
        uint8_t carry = 0;
        for (int i = 0; i < 16; i++) {
            uint8_t newCarry = (input[i] & 0x80) >> 7;
            output[i] = (input[i] << 1) | carry;
            carry = newCarry;
        }
    };
    
    // Generate subkeys K1 and K2
    uint8_t L[16] = {0};
    uint8_t zeroBlock[16] = {0};
    if (!aesEncrypt(key, zeroIV, zeroBlock, 16, L)) {
        return false;
    }
    
    uint8_t K1[16];
    leftShift(L, K1);
    if (L[0] & 0x80) {
        K1[15] ^= Rb;
    }
    
    uint8_t K2[16];
    leftShift(K1, K2);
    if (K1[0] & 0x80) {
        K2[15] ^= Rb;
    }
    
    // Determine number of blocks
    size_t n = (dataLen + 15) / 16;
    if (n == 0) {
        n = 1;
    }
    
    bool completeBlock = (dataLen > 0) && (dataLen % 16 == 0);
    
    // Process all blocks except the last
    uint8_t state[16] = {0};
    
    for (size_t i = 0; i < n - 1; i++) {
        uint8_t block[16];
        memcpy(block, data + (i * 16), 16);
        
        for (int j = 0; j < 16; j++) {
            block[j] ^= state[j];
        }
        
        if (!aesEncrypt(key, zeroIV, block, 16, state)) {
            return false;
        }
    }
    
    // Process last block with subkey
    uint8_t lastBlock[16] = {0};
    size_t lastBlockLen = dataLen - ((n - 1) * 16);
    
    if (completeBlock) {
        memcpy(lastBlock, data + ((n - 1) * 16), 16);
        for (int j = 0; j < 16; j++) {
            lastBlock[j] ^= K1[j];
        }
    } else {
        if (lastBlockLen > 0) {
            memcpy(lastBlock, data + ((n - 1) * 16), lastBlockLen);
        }
        lastBlock[lastBlockLen] = 0x80;
        for (size_t j = lastBlockLen + 1; j < 16; j++) {
            lastBlock[j] = 0x00;
        }
        for (int j = 0; j < 16; j++) {
            lastBlock[j] ^= K2[j];
        }
    }
    
    for (int j = 0; j < 16; j++) {
        lastBlock[j] ^= state[j];
    }
    
    // Final encryption - return full 16 bytes
    if (!aesEncrypt(key, zeroIV, lastBlock, 16, mac)) {
        return false;
    }
    
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
// According to NTAG424 DNA datasheet Section 9.1.7 (NIST SP 800-108 KDF)
bool NTAG424Crypto::deriveSessionKeys(const uint8_t* rndA, const uint8_t* rndB,
                                     const uint8_t* key, uint8_t* encKey,
                                     uint8_t* macKey) {
    // Session Vector construction (32 bytes):
    // SV = Label || Counter || Length || Context
    // Where Context = RndA[15..14] || (RndA[13..8] XOR RndB[15..10]) || RndB[9..0] || RndA[7..0]
    
    // Build SV1 for Encryption Key
    // Label = 0xA5 0x5A, Counter = 0x00 0x01, Length = 0x00 0x80
    uint8_t sv1[32];
    sv1[0] = 0xA5;
    sv1[1] = 0x5A;
    sv1[2] = 0x00;  // Counter
    sv1[3] = 0x01;
    sv1[4] = 0x00;  // Length (128 bits)
    sv1[5] = 0x80;
    
    // Context (26 bytes):
    // RndA[15..14]
    sv1[6] = rndA[15];
    sv1[7] = rndA[14];
    
    // RndA[13..8] XOR RndB[15..10] (6 bytes)
    for (int i = 0; i < 6; i++) {
        sv1[8 + i] = rndA[13 - i] ^ rndB[15 - i];
    }
    
    // RndB[9..0] (10 bytes)
    for (int i = 0; i < 10; i++) {
        sv1[14 + i] = rndB[9 - i];
    }
    
    // RndA[7..0] (8 bytes)
    for (int i = 0; i < 8; i++) {
        sv1[24 + i] = rndA[7 - i];
    }
    
    // Derive ENC key using CMAC(Key, SV1)
    if (!calculateCMACFull(key, sv1, 32, encKey)) {
        return false;
    }
    
    // Build SV2 for MAC Key
    // Same as SV1 but with swapped label: 0x5A 0xA5
    uint8_t sv2[32];
    memcpy(sv2, sv1, 32);
    sv2[0] = 0x5A;
    sv2[1] = 0xA5;
    
    // Derive MAC key using CMAC(Key, SV2)
    if (!calculateCMACFull(key, sv2, 32, macKey)) {
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

// Calculate HMAC-SHA256
bool NTAG424Crypto::calculateHMAC_SHA256(const uint8_t* key, size_t keyLen,
                                        const uint8_t* data, size_t dataLen,
                                        uint8_t* output) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    int ret = mbedtls_md_setup(&ctx, md_info, 1); // 1 = HMAC mode
    if (ret != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_starts(&ctx, key, keyLen);
    if (ret != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_update(&ctx, data, dataLen);
    if (ret != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_finish(&ctx, output);
    if (ret != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    mbedtls_md_free(&ctx);
    return true;
}

// Derive master key from master secret using HMAC-SHA256
bool NTAG424Crypto::deriveMasterKey(const String& masterSecret, const String& uid,
                                   uint8_t* output, uint8_t keyVersion) {
    // Prepare data: UID || "K0" || version
    // Example: "04E1A2B3C4D5E6" + "K0" + "1" = "04E1A2B3C4D5E6K01"
    String data = uid + "K0" + String(keyVersion);
    
    // Convert to bytes
    const uint8_t* keyBytes = (const uint8_t*)masterSecret.c_str();
    size_t keyLen = masterSecret.length();
    
    const uint8_t* dataBytes = (const uint8_t*)data.c_str();
    size_t dataLen = data.length();
    
    // Calculate HMAC-SHA256
    uint8_t hmac[32];  // SHA256 = 32 bytes
    if (!calculateHMAC_SHA256(keyBytes, keyLen, dataBytes, dataLen, hmac)) {
        return false;
    }
    
    // Take first 16 bytes for AES-128 key
    memcpy(output, hmac, 16);
    
    return true;
}

// Helper for CMAC subkey generation (not used with mbedtls built-in CMAC)
void NTAG424Crypto::generateCMACSubkey(const uint8_t* k, uint8_t* k1, uint8_t* k2) {
    // This is a helper function if we need manual CMAC implementation
    // Currently not used as we use mbedtls_cipher_cmac
}

void NTAG424Crypto::leftShift(const uint8_t* input, uint8_t* output) {
    // Helper for CMAC (not used with mbedtls)
}
