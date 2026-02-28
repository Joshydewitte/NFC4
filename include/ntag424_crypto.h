#ifndef NTAG424_CRYPTO_H
#define NTAG424_CRYPTO_H

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "mbedtls/md.h"

/**
 * NTAG424 DNA EV2 Authentication & Crypto
 * 
 * Implements AES-128 encryption/decryption and helper functions
 * for NTAG424 DNA tag authentication and key management
 */
class NTAG424Crypto {
public:
    /**
     * AES-128 CBC Encryption (no padding)
     * @param key - 16-byte AES key
     * @param iv - 16-byte initialization vector
     * @param input - Input data (must be multiple of 16 bytes)
     * @param inputLen - Length of input data
     * @param output - Output buffer (must be >= inputLen)
     * @return true on success
     */
    static bool aesEncrypt(const uint8_t* key, const uint8_t* iv, 
                          const uint8_t* input, size_t inputLen, 
                          uint8_t* output);
    
    /**
     * AES-128 CBC Decryption (no padding)
     * @param key - 16-byte AES key
     * @param iv - 16-byte initialization vector
     * @param input - Input data
     * @param inputLen - Length of input data
     * @param output - Output buffer (must be >= inputLen)
     * @return true on success
     */
    static bool aesDecrypt(const uint8_t* key, const uint8_t* iv,
                          const uint8_t* input, size_t inputLen,
                          uint8_t* output);
    
    /**
     * Generate cryptographically secure random bytes
     * @param output - Output buffer
     * @param length - Number of bytes to generate
     */
    static void generateRandom(uint8_t* output, size_t length);
    
    /**
     * Rotate buffer left by 1 byte (for NTAG424 EV2 protocol)
     * Used in authentication: RndB' = RndB rotated left
     * @param input - Input buffer
     * @param output - Output buffer (can be same as input)
     * @param length - Buffer length
     */
    static void rotateLeft(const uint8_t* input, uint8_t* output, size_t length);
    
    /**
     * Rotate buffer right by 1 byte (inverse of rotateLeft)
     * @param input - Input buffer
     * @param output - Output buffer (can be same as input)
     * @param length - Buffer length
     */
    static void rotateRight(const uint8_t* input, uint8_t* output, size_t length);
    
    /**
     * Calculate CMAC using AES-128 according to NIST SP 800-38B
     * @param key - AES key (16 bytes)
     * @param data - Input data
     * @param dataLen - Length of input data
     * @param mac - Output MAC (8 bytes, truncated)
     * @return true on success
     */
    static bool calculateCMAC(const uint8_t* key, const uint8_t* data,
                             size_t dataLen, uint8_t* mac);
    
    /**
     * Calculate full 16-byte CMAC (not truncated)
     * Used for session key derivation
     * @param key - AES key (16 bytes)
     * @param data - Input data
     * @param dataLen - Length of input data
     * @param mac - Output MAC (16 bytes, full)
     * @return true on success
     */
    static bool calculateCMACFull(const uint8_t* key, const uint8_t* data,
                                  size_t dataLen, uint8_t* mac);
    
    /**
     * XOR two byte arrays
     * @param a - First array
     * @param b - Second array
     * @param output - Output buffer (can be same as a or b)
     * @param length - Length of arrays
     */
    static void xorArrays(const uint8_t* a, const uint8_t* b, 
                         uint8_t* output, size_t length);
    
    /**
     * Convert hex string to byte array
     * @param hexStr - Hex string (e.g., "0123456789ABCDEF")
     * @param output - Output buffer
     * @param maxLen - Maximum output length
     * @return Number of bytes written
     */
    static size_t hexStringToBytes(const String& hexStr, uint8_t* output, size_t maxLen);
    
    /**
     * Convert byte array to hex string
     * @param bytes - Byte array
     * @param length - Length of array
     * @return Hex string in uppercase
     */
    static String bytesToHexString(const uint8_t* bytes, size_t length);
    
    /**
     * Derive session keys from authentication ransoms (EV2)
     * @param rndA - Reader's random (16 bytes)
     * @param rndB - Card's random (16 bytes)  
     * @param key - Authentication key (16 bytes)
     * @param encKey - Output: Session encryption key (16 bytes)
     * @param macKey - Output: Session MAC key (16 bytes)
     * @return true on success
     */
    static bool deriveSessionKeys(const uint8_t* rndA, const uint8_t* rndB,
                                 const uint8_t* key, uint8_t* encKey, 
                                 uint8_t* macKey);
    
    /**
     * Calculate CRC32 for NTAG424 ChangeKey command
     * @param data - Data to calculate CRC over
     * @param length - Length of data
     * @return 32-bit CRC value
     */
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);
    
    /**
     * Derive master key from master secret using HMAC-SHA256
     * Formula: K0 = HMAC-SHA256(masterSecret, UID || "K0" || version)[0..15]
     * @param masterSecret - Master secret string
     * @param uid - Card UID as hex string (e.g., "04E1A2B3C4D5E6")
     * @param output - Output buffer for derived key (16 bytes)
     * @param keyVersion - Key version number (default: 1)
     * @return true on success
     */
    static bool deriveMasterKey(const String& masterSecret, const String& uid,
                               uint8_t* output, uint8_t keyVersion = 1);
    
    /**
     * Calculate HMAC-SHA256
     * @param key - Secret key
     * @param keyLen - Length of key
     * @param data - Data to authenticate
     * @param dataLen - Length of data
     * @param output - Output buffer for HMAC (32 bytes)
     * @return true on success
     */
    static bool calculateHMAC_SHA256(const uint8_t* key, size_t keyLen,
                                    const uint8_t* data, size_t dataLen,
                                    uint8_t* output);
                            
private:
    // Helper for CMAC subkey generation
    static void generateCMACSubkey(const uint8_t* k, uint8_t* k1, uint8_t* k2);
    static void leftShift(const uint8_t* input, uint8_t* output);
};

#endif // NTAG424_CRYPTO_H
