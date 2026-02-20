#ifndef NTAG424_HANDLER_H
#define NTAG424_HANDLER_H

#include <Arduino.h>
#include <PN5180ISO14443.h>
#include "ntag424_crypto.h"

// Forward declaration
class NFCWebServer;

/**
 * NTAG424 DNA Handler
 * 
 * Implements NTAG424 DNA specific commands:
 * - EV2 First authentication
 * - Key management (ChangeKey)
 * - Secure messaging
 */
class NTAG424Handler {
public:
    // NTAG424 DNA command codes
    static const uint8_t CMD_AUTHENTICATE_EV2_FIRST = 0x71;
    static const uint8_t CMD_AUTHENTICATE_EV2_NON_FIRST = 0x77;
    static const uint8_t CMD_CHANGE_KEY = 0xC4;
    static const uint8_t CMD_GET_VERSION = 0x60;
    static const uint8_t CMD_SELECT_APPLICATION = 0x5A;
    
    // Status codes
    static const uint8_t STATUS_OK = 0x00;
    static const uint8_t STATUS_ADDITIONAL_FRAME = 0xAF;
    static const uint8_t STATUS_AUTHENTICATION_ERROR = 0xAE;
    
    // Default factory keys
    static const uint8_t DEFAULT_AES_KEY[16];
    
    // Authentication result
    struct AuthResult {
        bool success;
        uint8_t sessionEncKey[16];
        uint8_t sessionMacKey[16];
        String errorMessage;
    };
    
    NTAG424Handler(PN5180ISO14443* nfcReader);
    
    /**
     * Activate ISO14443-4 (ISO-DEP) protocol
     * Must be called before any NTAG424 commands
     * @return true on success
     */
    bool activateCard();
    
    /**
     * Reset ISO-DEP session state
     * Call this when card is removed or new card is detected
     */
    void resetSession();
    
    /**
     * Set web server for logging
     */
    void setWebServer(NFCWebServer* ws) { webServer = ws; }
    
    /**
     * Authenticate with NTAG424 DNA using EV2 First
     * @param keyNo - Key number (0-4)
     * @param key - AES-128 key (16 bytes)
     * @param result - Output: authentication result and session keys
     * @return true on success
     */
    bool authenticateEV2First(uint8_t keyNo, const uint8_t* key, AuthResult& result);
    
    /**
     * Change a key on NTAG424 DNA
     * @param keyNo - Key number to change (0-4)
     * @param oldKey - Old key for authentication (16 bytes)
     * @param newKey - New key to write (16 bytes)
     * @return true on success
     */
    bool changeKey(uint8_t keyNo, const uint8_t* oldKey, const uint8_t* newKey);
    
    /**
     * Get version information from NTAG424 DNA
     * @param versionInfo - Output buffer (at least 28 bytes)
     * @return true on success
     */
    bool getVersion(uint8_t* versionInfo);
    
    /**
     * Select NDEF application using ISO SELECT FILE command
     * Must be called after activateCard() and before authentication
     * @return true on success
     */
    bool selectNdefApplication();
    
    /**
     * Select application (AID)
     * @param aid - Application ID (3 bytes)
     * @return true on success
     */
    bool selectApplication(const uint8_t* aid);
    
    /**
     * Send ISO14443-4 command and receive response
     * @param cmd - Command buffer
     * @param cmdLen - Command length
     * @param response - Response buffer
     * @param responseLen - Max response length, updated with actual length
     * @return true on success (status 0x00 or 0xAF)
     */
    bool sendCommand(const uint8_t* cmd, size_t cmdLen, 
                    uint8_t* response, size_t& responseLen);
    
private:
    PN5180ISO14443* nfc;
    NFCWebServer* webServer;
    bool isoDEPActive;  // Track if ISO-DEP session is active
    uint16_t lastStatusWord;  // Last status word from card
    
    // Secure messaging state (valid after successful authentication)
    bool authenticated;
    uint8_t transactionId[4];      // TI - Transaction Identifier
    uint16_t commandCounter;       // CmdCtr - Command Counter (starts at 0 after auth)
    uint8_t sessionEncKey[16];     // Session encryption key
    uint8_t sessionMacKey[16];     // Session MAC key
    
    // Helper methods
    void logToWeb(const String& message, const String& level = "info");
    void logDebug(const String& message);
    void logError(const String& message);
    
    /**
     * Send command with CRC and receive response
     * Wrapper around PN5180's transceive
     */
    bool transceive(const uint8_t* cmd, size_t cmdLen,
                   uint8_t* response, size_t& responseLen);
    
    /**
     * Send raw ISO7816 command without wrapping
     * Used for ISO SELECT FILE and other standard commands
     */
    bool transceiveRaw(const uint8_t* cmd, size_t cmdLen,
                      uint8_t* response, size_t& responseLen);
};

#endif // NTAG424_HANDLER_H
