#include "ntag424_handler.h"
#include "web_server.h"
#include <PN5180.h>

// ============ DEBUG MODE ============
// When true: Prepare all crypto but DON'T write to card
// Show all debug output for verification before enabling real writes
#define DEBUG_WRITE_MODE false  // ✅ REAL WRITES ENABLED
// ====================================

// Default factory AES key (all zeros)
const uint8_t NTAG424Handler::DEFAULT_AES_KEY[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

NTAG424Handler::NTAG424Handler(PN5180ISO14443* nfcReader)
    : nfc(nfcReader), webServer(nullptr), isoDEPActive(false), lastStatusWord(0x0000),
      authenticated(false), commandCounter(0) {
    memset(transactionId, 0, 4);
    memset(sessionEncKey, 0, 16);
    memset(sessionMacKey, 0, 16);
}

// Activate ISO14443-4 card
bool NTAG424Handler::activateCard() {
    if (nfc == nullptr) {
        logError("NFC reader not initialized");
        return false;
    }
    
    // Check if card is present
    if (!nfc->isCardPresent()) {
        logError("No card present");
        return false;
    }
    
    logDebug("Activating ISO14443-4 card...");
    
    // Step 1: Activate Type A card (ISO14443-3: REQA, Anticollision, SELECT)
    uint8_t buffer[64];
    uint8_t uidLength = nfc->activateTypeA(buffer, 1);
    
    if (uidLength == 0) {
        logError("ISO14443-3 activation failed");
        return false;
    }
    
    logDebug("ISO14443-3 activation OK, UID length: " + String(uidLength));
    
    // Step 2: Send RATS (Request for Answer To Select) for ISO-DEP activation
    // RATS format: [0xE0] [PARAM: FSD(Frame Size) + CID info]
    // Param byte: (CID << 4) | FSDI
    // FSDI=8 means FSD=256 bytes, CID=0 (no CID)
    logDebug("Sending RATS for ISO-DEP activation...");
    
    uint8_t rats[2];
    rats[0] = 0xE0;  // RATS command
    rats[1] = 0x80;  // FSDI=8 (256 bytes), CID=0
    
    // Clear IRQ status before sending
    nfc->clearIRQStatus(0xFFFFFFFF);
    delay(5);
    
    // Send RATS command
    if (!nfc->sendData(rats, 2)) {
        logError("Failed to send RATS command");
        return false;
    }
    
    // Wait for response (ATS - Answer To Select)
    unsigned long startTime = millis();
    const unsigned long timeout = 300;
    
    uint32_t irqStatus = 0;
    while (millis() - startTime < timeout) {
        irqStatus = nfc->getIRQStatus();
        
        // Check for RX complete
        if (irqStatus & RX_IRQ_STAT) {
            break;
        }
        
        // Check for errors
        if (irqStatus & GENERAL_ERROR_IRQ_STAT) {
            logError("Error during RATS");
            return false;
        }
        
        delay(2);
    }
    
    if (!(irqStatus & RX_IRQ_STAT)) {
        logError("Timeout waiting for ATS response");
        logDebug("IRQ Status: 0x" + String(irqStatus, HEX));
        return false;
    }
    
    // Read ATS (Answer To Select) response
    uint32_t rxStatus;
    if (!nfc->readRegister(RX_STATUS, &rxStatus)) {
        logError("Failed to read RX status");
        return false;
    }
    
    uint16_t atsLen = (rxStatus >> 0) & 0x1FF; // Bits 0-8 contain RX bytes
    logDebug("Received ATS: " + String(atsLen) + " bytes");
    
    if (atsLen < 2) {
        logError("Invalid ATS length: " + String(atsLen));
        return false;
    }
    
    // Read ATS data
    uint8_t* atsData = nfc->readData(atsLen);
    if (atsData == nullptr) {
        logError("Failed to read ATS data");
        return false;
    }
    
    // Log ATS for debugging
    String atsHex = "ATS: ";
    for (int i = 0; i < atsLen && i < 20; i++) {
        if (atsData[i] < 0x10) atsHex += "0";
        atsHex += String(atsData[i], HEX);
        atsHex += " ";
    }
    logDebug(atsHex);
    
    // Verify ATS format
    // First byte should be TL (length byte)
    if (atsData[0] != atsLen && atsData[0] + 2 != atsLen) {
        // Some implementations include CRC in length, some don't
        logDebug("Warning: ATS length mismatch (TL=" + String(atsData[0]) + 
                 ", received=" + String(atsLen) + ")");
    }
    
    logDebug("✅ ISO-DEP (ISO14443-4) activation successful");
    
    // Step 3: Configure PN5180 for ISO-DEP communication
    // Ensure CRC is enabled for TX and RX
    if (!nfc->writeRegisterWithOrMask(CRC_RX_CONFIG, 0x01)) {
        logError("Warning: Failed to enable RX CRC");
    }
    
    if (!nfc->writeRegisterWithOrMask(CRC_TX_CONFIG, 0x01)) {
        logError("Warning: Failed to enable TX CRC");
    }
    
    // Step 4: Select NDEF Application (mandatory before authentication)
    // ISO7816 DF Name: D2760000850101h
    if (!selectNdefApplication()) {
        logError("Failed to select NDEF application");
        return false;
    }
    
    logDebug("NTAG424 DNA ready for native commands");
    
    logToWeb("✅ ISO-DEP protocol geactiveerd", "success");
    
    // Mark ISO-DEP session as active
    isoDEPActive = true;
    
    return true;
}

// Reset ISO-DEP session
void NTAG424Handler::resetSession() {
    isoDEPActive = false;
    authenticated = false;
    authenticatedKeyNo = 0xFF;  // Invalid key number
    commandCounter = 0;
    memset(transactionId, 0, 4);
    memset(sessionEncKey, 0, 16);
    memset(sessionMacKey, 0, 16);
    memset(currentIV, 0, 16);  // Reset IV for chained CBC
    logDebug("ISO-DEP session reset");
}

// Authenticate with NTAG424 DNA using EV2 First
bool NTAG424Handler::authenticateEV2First(uint8_t keyNo, const uint8_t* key, AuthResult& result) {
    result.success = false;
    result.errorMessage = "";
    
    logDebug("═══════════════════════════════════════════════════════");
    logDebug("START EV2 FIRST AUTHENTICATION - DETAILED DEBUG");
    logDebug("═══════════════════════════════════════════════════════");
    logDebug("Key Number: " + String(keyNo));
    logDebug("Key being used: " + NTAG424Crypto::bytesToHexString(key, 16));
    
    // Step 1: Send AuthenticateEV2First command
    // NTAG424 expects: 0x71 || KeyNo || LenCap (3 bytes total)
    uint8_t cmd[3];
    cmd[0] = CMD_AUTHENTICATE_EV2_FIRST;  // 0x71
    cmd[1] = keyNo;                        // Key number
    cmd[2] = 0x00;                         // LenCap (standard frame size)
    
    logDebug("Sending authentication command...");
    
    uint8_t response[64];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 3, response, responseLen)) {
        result.errorMessage = "Failed to send auth command - SW=" + String(lastStatusWord, HEX);
        logError(result.errorMessage);
        logError("⚠️ This usually means:");
        logError("  - Wrong key number");
        logError("  - Card not in correct state");
        logError("  - Card communication problem");
        return false;
    }
    
    logDebug("✓ Command sent successfully");
    logDebug("Response length: " + String(responseLen) + " bytes");
    
    // Response should be: [16 bytes encrypted RndB]
    // Note: Status word (SW1 SW2) is already removed by sendCommand()
    if (responseLen < 16) {
        result.errorMessage = "Invalid auth response length: " + String(responseLen);
        logError(result.errorMessage);
        return false;
    }
    
    // Extract encrypted RndB (16 bytes)
    uint8_t encRndB[16];
    memcpy(encRndB, response, 16);
    
    // Store encrypted RndB in result for server verification
    memcpy(result.encryptedRndB, encRndB, 16);
    
    logDebug("Encrypted RndB: " + NTAG424Crypto::bytesToHexString(encRndB, 16));
    
    // Step 2: Decrypt RndB with key
    uint8_t rndB[16];
    uint8_t zeroIV[16] = {0};
    
    if (!NTAG424Crypto::aesDecrypt(key, zeroIV, encRndB, 16, rndB)) {
        result.errorMessage = "Failed to decrypt RndB";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Decrypted RndB: " + NTAG424Crypto::bytesToHexString(rndB, 16));
    
    // Step 3: Generate RndA (16 bytes)
    uint8_t rndA[16];
    NTAG424Crypto::generateRandom(rndA, 16);
    
    // Store RndA in result for server verification
    memcpy(result.rndA, rndA, 16);
    
    logDebug("Generated RndA: " + NTAG424Crypto::bytesToHexString(rndA, 16));
    
    // Step 4: Rotate RndB left by 1 byte (RndB')
    uint8_t rndBPrime[16];
    NTAG424Crypto::rotateLeft(rndB, rndBPrime, 16);
    
    logDebug("Rotated RndB': " + NTAG424Crypto::bytesToHexString(rndBPrime, 16));
    
    // Step 5: Concatenate RndA || RndB' and encrypt
    // According to AN12196 Table 14, use zero IV (NOT encRndB!)
    uint8_t authData[32];
    memcpy(authData, rndA, 16);
    memcpy(authData + 16, rndBPrime, 16);
    
    logDebug("Auth Data (RndA || RndB'): " + NTAG424Crypto::bytesToHexString(authData, 32));
    
    uint8_t encAuthData[32];
    if (!NTAG424Crypto::aesEncrypt(key, zeroIV, authData, 32, encAuthData)) {
        result.errorMessage = "Failed to encrypt auth data";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Encrypted Auth Data: " + NTAG424Crypto::bytesToHexString(encAuthData, 32));
    
    // Step 6: Send encrypted (RndA || RndB') to card
    uint8_t cmd2[33];
    cmd2[0] = STATUS_ADDITIONAL_FRAME; // 0xAF
    memcpy(cmd2 + 1, encAuthData, 32);
    
    responseLen = sizeof(response);
    if (!sendCommand(cmd2, 33, response, responseLen)) {
        result.errorMessage = "Failed to send auth response";
        logError(result.errorMessage);
        return false;
    }
    
    // Response should be: E(K0, TI || RndA' || PDcap2 || PCDcap2)
    // According to AN12196: 32 bytes encrypted containing:
    // - TI (4 bytes)
    // - RndA' (16 bytes)
    // - PDcap2 (6 bytes)
    // - PCDcap2 (6 bytes)
    // Note: Status word (SW1 SW2) is already removed by sendCommand()
    if (responseLen < 32) {
        result.errorMessage = "Invalid final auth response length: " + String(responseLen);
        logError(result.errorMessage);
        return false;
    }
    
    // Extract encrypted response (32 bytes)
    uint8_t encResponse[32];
    memcpy(encResponse, response, 32);
    
    // Store encrypted response in result for server verification
    memcpy(result.encryptedResponse, encResponse, 32);
    
    logDebug("Encrypted Response: " + NTAG424Crypto::bytesToHexString(encResponse, 32));
    
    // Step 7: Decrypt response to get TI || RndA' || PDcap2 || PCDcap2
    // According to AN12196 Table 14: Card uses ZERO IV for response encryption (not CBC chaining)
    uint8_t decResponse[32];
    if (!NTAG424Crypto::aesDecrypt(key, zeroIV, encResponse, 32, decResponse)) {
        result.errorMessage = "Failed to decrypt final response";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Decrypted Response: " + NTAG424Crypto::bytesToHexString(decResponse, 32));
    
    // Extract TI (Transaction Identifier) - first 4 bytes
    uint8_t ti[4];
    memcpy(ti, decResponse, 4);
    
    // Store TI in result for server verification
    memcpy(result.transactionId, ti, 4);
    
    logDebug("Transaction ID: " + NTAG424Crypto::bytesToHexString(ti, 4));
    
    // Extract RndA' - bytes 4-19 (16 bytes)
    uint8_t rndAPrime[16];
    memcpy(rndAPrime, decResponse + 4, 16);
    
    logDebug("Decrypted RndA': " + NTAG424Crypto::bytesToHexString(rndAPrime, 16));
    
    // Extract PDcap2 - bytes 20-25 (6 bytes)
    uint8_t pdcap2[6];
    memcpy(pdcap2, decResponse + 20, 6);
    logDebug("PDcap2: " + NTAG424Crypto::bytesToHexString(pdcap2, 6));
    
    // Extract PCDcap2 - bytes 26-31 (6 bytes)
    uint8_t pcdcap2[6];
    memcpy(pcdcap2, decResponse + 26, 6);
    logDebug("PCDcap2: " + NTAG424Crypto::bytesToHexString(pcdcap2, 6));
    
    // Rotate RndA left to get expected RndA'
    uint8_t expectedRndAPrime[16];
    NTAG424Crypto::rotateLeft(rndA, expectedRndAPrime, 16);
    
    logDebug("Expected RndA': " + NTAG424Crypto::bytesToHexString(expectedRndAPrime, 16));
    
    // Verify RndA' matches
    if (memcmp(rndAPrime, expectedRndAPrime, 16) != 0) {
        result.errorMessage = "Authentication failed: RndA' mismatch";
        logError(result.errorMessage);
        logError("Expected: " + NTAG424Crypto::bytesToHexString(expectedRndAPrime, 16));
        logError("Received: " + NTAG424Crypto::bytesToHexString(rndAPrime, 16));
        return false;
    }
    
    logDebug("✅ RndA' verification successful!");
    
    // Step 8: Derive session keys
    if (!NTAG424Crypto::deriveSessionKeys(rndA, rndB, key, 
                                          result.sessionEncKey, 
                                          result.sessionMacKey)) {
        result.errorMessage = "Failed to derive session keys";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Session ENC Key: " + NTAG424Crypto::bytesToHexString(result.sessionEncKey, 16));
    logDebug("Session MAC Key: " + NTAG424Crypto::bytesToHexString(result.sessionMacKey, 16));
    
    // Store session state for secure messaging
    authenticated = true;
    authenticatedKeyNo = keyNo;  // Store which key was used
    commandCounter = 0;  // Reset to 0 after authentication
    memcpy(transactionId, ti, 4);
    memcpy(sessionEncKey, result.sessionEncKey, 16);
    memcpy(sessionMacKey, result.sessionMacKey, 16);
    
    // Initialize Current IV for EV2 secure messaging (AN12196 Table 27)
    // IV chaining starts AFTER authentication - use last block of auth response as initial IV
    memcpy(this->currentIV, encResponse + 16, 16);
    logDebug("Current IV initialized: " + NTAG424Crypto::bytesToHexString(this->currentIV, 16));
    
    result.success = true;
    logToWeb("✅ NTAG424 authenticatie succesvol", "success");
    
    return true;
}

/**
 * Authenticate with NTAG424 DNA using EV2 First with external RndA
 * This version accepts an external RndA (e.g., from server challenge)
 * and returns encrypted RndB and TI for server verification
 */
bool NTAG424Handler::authenticateEV2First(uint8_t keyNo, const uint8_t* key, const uint8_t* externalRndA, AuthResult& result) {
    result.success = false;
    result.errorMessage = "";
    
    logDebug("Start EV2 First Authentication (with external RndA)");
    logDebug("Key Number: " + String(keyNo));
    logDebug("External RndA: " + NTAG424Crypto::bytesToHexString(externalRndA, 16));
    
    // Step 1: Send AuthenticateEV2First command
    uint8_t cmd[3];
    cmd[0] = CMD_AUTHENTICATE_EV2_FIRST;
    cmd[1] = keyNo;
    cmd[2] = 0x00;
    
    uint8_t response[64];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 3, response, responseLen)) {
        result.errorMessage = "Failed to send auth command";
        logError(result.errorMessage);
        return false;
    }
    
    if (responseLen < 16) {
        result.errorMessage = "Invalid auth response length: " + String(responseLen);
        logError(result.errorMessage);
        return false;
    }
    
    // Extract and store encrypted RndB
    uint8_t encRndB[16];
    memcpy(encRndB, response, 16);
    memcpy(result.encryptedRndB, encRndB, 16);
    
    logDebug("Encrypted RndB: " + NTAG424Crypto::bytesToHexString(encRndB, 16));
    
    // Step 2: Decrypt RndB
    uint8_t rndB[16];
    uint8_t zeroIV[16] = {0};
    
    if (!NTAG424Crypto::aesDecrypt(key, zeroIV, encRndB, 16, rndB)) {
        result.errorMessage = "Failed to decrypt RndB";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Decrypted RndB: " + NTAG424Crypto::bytesToHexString(rndB, 16));
    
    // Step 3: Use external RndA instead of generating
    uint8_t rndA[16];
    memcpy(rndA, externalRndA, 16);
    memcpy(result.rndA, rndA, 16);
    
    // Step 4: Rotate RndB left by 1 byte (RndB')
    uint8_t rndBPrime[16];
    NTAG424Crypto::rotateLeft(rndB, rndBPrime, 16);
    
    logDebug("Rotated RndB': " + NTAG424Crypto::bytesToHexString(rndBPrime, 16));
    
    // Step 5: Concatenate RndA || RndB' and encrypt
    uint8_t authData[32];
    memcpy(authData, rndA, 16);
    memcpy(authData + 16, rndBPrime, 16);
    
    logDebug("Auth Data (RndA || RndB'): " + NTAG424Crypto::bytesToHexString(authData, 32));
    
    uint8_t encAuthData[32];
    if (!NTAG424Crypto::aesEncrypt(key, zeroIV, authData, 32, encAuthData)) {
        result.errorMessage = "Failed to encrypt auth data";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Encrypted Auth Data: " + NTAG424Crypto::bytesToHexString(encAuthData, 32));
    
    // Step 6: Send encrypted (RndA || RndB') to card
    uint8_t cmd2[33];
    cmd2[0] = STATUS_ADDITIONAL_FRAME;
    memcpy(cmd2 + 1, encAuthData, 32);
    
    responseLen = sizeof(response);
    if (!sendCommand(cmd2, 33, response, responseLen)) {
        result.errorMessage = "Failed to send auth response";
        logError(result.errorMessage);
        return false;
    }
    
    if (responseLen < 32) {
        result.errorMessage = "Invalid final auth response length: " + String(responseLen);
        logError(result.errorMessage);
        return false;
    }
    
    // Extract encrypted response
    uint8_t encResponse[32];
    memcpy(encResponse, response, 32);
    
    // Store encrypted response in result for server verification
    memcpy(result.encryptedResponse, encResponse, 32);
    
    logDebug("Encrypted Response: " + NTAG424Crypto::bytesToHexString(encResponse, 32));
    
    // Step 7: Decrypt response
    uint8_t decResponse[32];
    if (!NTAG424Crypto::aesDecrypt(key, zeroIV, encResponse, 32, decResponse)) {
        result.errorMessage = "Failed to decrypt final response";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Decrypted Response: " + NTAG424Crypto::bytesToHexString(decResponse, 32));
    
    // Extract and store TI
    uint8_t ti[4];
    memcpy(ti, decResponse, 4);
    memcpy(result.transactionId, ti, 4);
    
    logDebug("Transaction ID: " + NTAG424Crypto::bytesToHexString(ti, 4));
    
    // Extract RndA'
    uint8_t rndAPrime[16];
    memcpy(rndAPrime, decResponse + 4, 16);
    
    logDebug("Decrypted RndA': " + NTAG424Crypto::bytesToHexString(rndAPrime, 16));
    
    // Extract PDcap2 and PCDcap2
    uint8_t pdcap2[6];
    memcpy(pdcap2, decResponse + 20, 6);
    logDebug("PDcap2: " + NTAG424Crypto::bytesToHexString(pdcap2, 6));
    
    uint8_t pcdcap2[6];
    memcpy(pcdcap2, decResponse + 26, 6);
    logDebug("PCDcap2: " + NTAG424Crypto::bytesToHexString(pcdcap2, 6));
    
    // Verify RndA'
    uint8_t expectedRndAPrime[16];
    NTAG424Crypto::rotateLeft(rndA, expectedRndAPrime, 16);
    
    logDebug("Expected RndA': " + NTAG424Crypto::bytesToHexString(expectedRndAPrime, 16));
    
    if (memcmp(rndAPrime, expectedRndAPrime, 16) != 0) {
        result.errorMessage = "Authentication failed: RndA' mismatch";
        logError(result.errorMessage);
        logError("Expected: " + NTAG424Crypto::bytesToHexString(expectedRndAPrime, 16));
        logError("Received: " + NTAG424Crypto::bytesToHexString(rndAPrime, 16));
        return false;
    }
    
    logDebug("✅ RndA' verification successful!");
    
    // Step 8: Derive session keys
    if (!NTAG424Crypto::deriveSessionKeys(rndA, rndB, key, 
                                          result.sessionEncKey, 
                                          result.sessionMacKey)) {
        result.errorMessage = "Failed to derive session keys";
        logError(result.errorMessage);
        return false;
    }
    
    logDebug("Session ENC Key: " + NTAG424Crypto::bytesToHexString(result.sessionEncKey, 16));
    logDebug("Session MAC Key: " + NTAG424Crypto::bytesToHexString(result.sessionMacKey, 16));
    
    // Store session state
    authenticated = true;
    authenticatedKeyNo = keyNo;  // Store which key was used
    commandCounter = 0;
    memcpy(transactionId, ti, 4);
    memcpy(sessionEncKey, result.sessionEncKey, 16);
    memcpy(sessionMacKey, result.sessionMacKey, 16);
    
    // Initialize Current IV
    memcpy(this->currentIV, encResponse + 16, 16);
    logDebug("Current IV initialized: " + NTAG424Crypto::bytesToHexString(this->currentIV, 16));
    
    result.success = true;
    logToWeb("✅ NTAG424 authenticatie succesvol (externe RndA)", "success");
    
    return true;
}

// Change key on NTAG424 DNA
bool NTAG424Handler::changeKey(uint8_t keyNo, const uint8_t* oldKey, const uint8_t* newKey) {
    logToWeb("Start ChangeKey voor key " + String(keyNo), "info");
    
    // Step 1: Authenticate with old key (if not already authenticated)
    if (!authenticated) {
        AuthResult authResult;
        if (!authenticateEV2First(keyNo, oldKey, authResult)) {
            logError("Authentication failed: " + authResult.errorMessage);
            return false;
        }
        logToWeb("Authenticatie OK, schrijf nieuwe key...", "info");
    } else {
        logToWeb("Gebruik bestaande authenticatie sessie...", "info");
    }
    
    // Step 2: Prepare ChangeKey command data
    // According to AN12196 Section 6.16:
    // Case 1 (Table 26): KeyNo to be changed ≠ AuthKey → XOR + CRC32 + version
    // Case 2 (Table 27): KeyNo to be changed = AuthKey → NO XOR, only encrypt new key + version
    
    uint8_t plainData[32];
    
    // Determine Case 1 or Case 2 by comparing keyNo to authenticatedKeyNo
    if (keyNo != authenticatedKeyNo) {
        // **CASE 1** (AN12196 Table 26): Different key - use XOR + CRC32
        logDebug("ChangeKey Case 1 (AN12196 §6.16.1): KeyNo != AuthKey");
        
        // XOR old key with new key
        uint8_t keyData[16];
        NTAG424Crypto::xorArrays(oldKey, newKey, keyData, 16);
        logDebug("XOR Key Data: " + NTAG424Crypto::bytesToHexString(keyData, 16));
        
        // Calculate CRC32 over new key (LSB first as per ISO14443)
        uint32_t crc = NTAG424Crypto::calculateCRC32(newKey, 16);
        logDebug("CRC32: " + String(crc, HEX));
        
        // Prepare plaintext: [XOR KeyData:16] [Version:1] [CRC32:4] [Padding:11]
        memcpy(plainData, keyData, 16);
        plainData[16] = 0x01;  // Key version (AN12196 Table 26)
        plainData[17] = crc & 0xFF;         // CRC LSB first
        plainData[18] = (crc >> 8) & 0xFF;
        plainData[19] = (crc >> 16) & 0xFF;
        plainData[20] = (crc >> 24) & 0xFF;
        plainData[21] = 0x80;  // Padding indicator
        memset(plainData + 22, 0x00, 10);  // Zero padding
        
    } else {
        // **CASE 2** (AN12196 Table 27): Same key - NO XOR, just encrypt new key directly
        logDebug("ChangeKey Case 2 (AN12196 §6.16.2): KeyNo == AuthKey");
        
        // Prepare plaintext: [NewKey:16] [Version:1] [Padding:15]
        memcpy(plainData, newKey, 16);
        plainData[16] = 0x01;  // Key version (AN12196 Table 27)
        plainData[17] = 0x80;  // Padding indicator
        memset(plainData + 18, 0x00, 14);  // Zero padding
    }
    
    logDebug("Plain Data: " + NTAG424Crypto::bytesToHexString(plainData, 32));
    
    // Calculate IV for encryption according to AN12196 Section 9.1.4
    // IV = E(SesAuthENCKey, A55A || TI || CmdCtr || 0000000000000000)
    uint8_t ivInput[16];
    ivInput[0] = 0xA5;
    ivInput[1] = 0x5A;
    memcpy(ivInput + 2, transactionId, 4);
    ivInput[6] = commandCounter & 0xFF;        // CmdCtr LSB first
    ivInput[7] = (commandCounter >> 8) & 0xFF;
    memset(ivInput + 8, 0x00, 8);  // Zero padding
    
    uint8_t iv[16];
    // AN12196 Section 5.4 + Table 9 + Table 27 CONFIRMED:
    // IVc = E(KSesAuthENC, A55A||TI||CmdCtr||zeros) is AES-ECB (zero CBC IV).
    // Using currentIV (from auth) as CBC IV gives wrong IVc → card decrypts
    // with different IVc → stores garbage key → ChangeKey returns 9100 but
    // re-auth fails because card stored wrong key!
    uint8_t zeroIV[16] = {0};
    if (!NTAG424Crypto::aesEncrypt(sessionEncKey, zeroIV, ivInput, 16, iv)) {
        logError("Failed to calculate IV");
        return false;
    }
    
    logDebug("IV: " + NTAG424Crypto::bytesToHexString(iv, 16));
    logDebug("CmdCtr: " + String(commandCounter));
    
    // Encrypt command data with calculated IV
    uint8_t encKeyData[32];
    if (!NTAG424Crypto::aesEncrypt(sessionEncKey, iv, plainData, 32, encKeyData)) {
        logError("Failed to encrypt key data");
        return false;
    }
    
    logDebug("Encrypted Key Data: " + NTAG424Crypto::bytesToHexString(encKeyData, 32));
    
    // Calculate CMAC according to AN12196 Section 9.1.9
    // CMAC = MACt(SesAuthMACKey, Cmd || CmdCtr || TI || CmdHeader || EncCmdData)
    // Note: CmdCtr is LSB first (same as in IV input!)
    // Total: 1 + 2 + 4 + 1 + 32 = 40 bytes
    uint8_t macInput[40];
    // AN12196 Table 27 Step 14: MAC Input = Cmd || CmdCtr || TI || CmdHeader || EncCmdData
    macInput[0] = CMD_CHANGE_KEY;                // Cmd (1 byte)
    macInput[1] = commandCounter & 0xFF;         // CmdCtr LSB first (2 bytes)
    macInput[2] = (commandCounter >> 8) & 0xFF;
    memcpy(macInput + 3, transactionId, 4);      // TI (4 bytes, MSB first)
    macInput[7] = keyNo;                         // CmdHeader = KeyNo (1 byte)
    memcpy(macInput + 8, encKeyData, 32);        // EncCmdData (32 bytes)
    
    // Debug: Log complete MAC input for analysis
    logDebug("MAC Input: " + NTAG424Crypto::bytesToHexString(macInput, 40));
    
    // Calculate full CMAC first for debugging
    uint8_t macFull[16];
    if (!NTAG424Crypto::calculateCMACFull(sessionMacKey, macInput, 40, macFull)) {
        logError("Failed to calculate full CMAC");
        return false;
    }
    logDebug("CMAC Full: " + NTAG424Crypto::bytesToHexString(macFull, 16));
    
    uint8_t mac[8];
    if (!NTAG424Crypto::calculateCMAC(sessionMacKey, macInput, 40, mac)) {
        logError("Failed to calculate CMAC");
        return false;
    }
    
    logDebug("CMAC: " + NTAG424Crypto::bytesToHexString(mac, 8));
    
    // Step 3: Send ChangeKey command
    // Format: [Cmd] [KeyNo] [EncData:32] [MAC:8]
    uint8_t cmd[42];
    cmd[0] = CMD_CHANGE_KEY;
    cmd[1] = keyNo;
    memcpy(cmd + 2, encKeyData, 32);
    memcpy(cmd + 34, mac, 8);
    
#if DEBUG_WRITE_MODE
    // ═══════════════════════════════════════════════════════════════
    // DEBUG MODE: Show all data but DON'T write to card
    // ═══════════════════════════════════════════════════════════════
    Serial.println(F("\n╔══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║          🔍 DEBUG WRITE MODE - NOT WRITING TO CARD 🔍       ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
    
    Serial.println(F("\n[CHANGEKEY APDU DETAILS]"));
    Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    Serial.print(F("Full APDU (42 bytes): "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd, 42));
    Serial.println();
    Serial.print(F("  [00] Command:       0x"));
    Serial.println(cmd[0], HEX);
    Serial.print(F("  [01] KeyNo:         0x"));
    Serial.println(cmd[1], HEX);
    Serial.print(F("  [02-33] Encrypted:  "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd + 2, 32));
    Serial.print(F("  [34-41] MAC:        "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd + 34, 8));
    
    Serial.println(F("\n[SESSION STATE]"));
    Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    Serial.print(F("SesAuthENCKey: "));
    Serial.println(NTAG424Crypto::bytesToHexString(sessionEncKey, 16));
    Serial.print(F("SesAuthMACKey: "));
    Serial.println(NTAG424Crypto::bytesToHexString(sessionMacKey, 16));
    Serial.print(F("Transaction ID: "));
    Serial.println(NTAG424Crypto::bytesToHexString(transactionId, 4));
    Serial.print(F("Cmd Counter:    "));
    Serial.println(commandCounter);
    Serial.print(F("Current IV:     "));
    Serial.println(NTAG424Crypto::bytesToHexString(currentIV, 16));
    
    Serial.println(F("\n[CRYPTO VERIFICATION]"));
    Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    Serial.print(F("Plain Data (32 bytes): "));
    Serial.println(NTAG424Crypto::bytesToHexString(plainData, 32));
    Serial.print(F("IV Used:               "));
    Serial.println(NTAG424Crypto::bytesToHexString(iv, 16));
    Serial.print(F("Encrypted Result:      "));
    Serial.println(NTAG424Crypto::bytesToHexString(encKeyData, 32));
    Serial.print(F("MAC Input (40 bytes):  "));
    Serial.println(NTAG424Crypto::bytesToHexString(macInput, 40));
    Serial.print(F("CMAC Full (16 bytes):  "));
    Serial.println(NTAG424Crypto::bytesToHexString(macFull, 16));
    Serial.print(F("CMAC (8 bytes):        "));
    Serial.println(NTAG424Crypto::bytesToHexString(mac, 8));
    
    Serial.println(F("\n╔══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║   ⚠️  CARD NOT WRITTEN - VERIFY CRYPTO FIRST  ⚠️            ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
    Serial.println();
    
    // Simulate success (increment counter, update IV as if command succeeded)
    commandCounter++;
    memcpy(this->currentIV, encKeyData + 16, 16);
    
    logToWeb("🔍 DEBUG: ChangeKey voorbereid maar NIET geschreven naar kaart", "warning");
    return true;  // Simulate success
    
#else
    // ═══════════════════════════════════════════════════════════════
    // NORMAL MODE: Actually write to card
    // ═══════════════════════════════════════════════════════════════
    uint8_t response[32];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 42, response, responseLen)) {
        logError("ChangeKey command failed");
        authenticated = false;  // Authentication lost on error
        return false;
    }
    
    // ChangeKey response contains 8-byte MAC
    // TODO: Response MAC verification (format needs to be determined from tests)
    // For now, we rely on CommitTransaction to validate if change was accepted
    logDebug("═══════════════════════════════════════");
    logDebug("ChangeKey Response:");
    logDebug("Response length: " + String(responseLen) + " bytes");
    if (responseLen > 0) {
        logDebug("Response MAC: " + NTAG424Crypto::bytesToHexString(response, responseLen));
        logToWeb("ℹ️ ChangeKey response MAC: " + NTAG424Crypto::bytesToHexString(response, responseLen), "info");
    } else {
        logDebug("⚠️ No response data received!");
        logToWeb("⚠️ Geen response data van ChangeKey", "warning");
    }
    logDebug("═══════════════════════════════════════");
#endif
    
    // Increment command counter after successful command
    commandCounter++;
    logDebug("CmdCtr incremented to: " + String(commandCounter));
    
    // Update Current IV for next operation (EV2 chained CBC mode)
    // Use last ciphertext block (last 16 bytes of encrypted command data)
    memcpy(this->currentIV, encKeyData + 16, 16);
    logDebug("Current IV updated: " + NTAG424Crypto::bytesToHexString(this->currentIV, 16));
    
    logToWeb("✅ ChangeKey commando verzonden (wacht op CommitTransaction)", "info");
    
    return true;
}

// Get version information
bool NTAG424Handler::getVersion(uint8_t* versionInfo) {
    uint8_t cmd[1] = { CMD_GET_VERSION };
    uint8_t response[64];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 1, response, responseLen)) {
        return false;
    }
    
    // GetVersion returns data in multiple frames (SW=91AF means more data)
    // We need to fetch all frames using AF (Additional Frame) commands
    uint8_t fullVersion[28];
    size_t totalLen = 0;
    
    // Copy first frame
    if (responseLen > 0) {
        size_t copyLen = min(responseLen, sizeof(fullVersion) - totalLen);
        memcpy(fullVersion + totalLen, response, copyLen);
        totalLen += copyLen;
    }
    
    // Keep fetching with AF while status is 0x91AF (Additional Frame)
    int frameCount = 1;
    while ((lastStatusWord == 0x91AF) && frameCount < 5) {
        logDebug("Fetching additional frame " + String(frameCount + 1) + "...");
        
        // Send AF (Additional Frame) command
        uint8_t afCmd[1] = { STATUS_ADDITIONAL_FRAME };  // 0xAF
        responseLen = sizeof(response);
        
        if (!sendCommand(afCmd, 1, response, responseLen)) {
            logError("AF command failed with SW=" + String(lastStatusWord, HEX));
            break;
        }
        
        // Copy additional frame data
        if (responseLen > 0 && totalLen < sizeof(fullVersion)) {
            size_t copyLen = min(responseLen, sizeof(fullVersion) - totalLen);
            memcpy(fullVersion + totalLen, response, copyLen);
            totalLen += copyLen;
        }
        
        frameCount++;
        
        // Check if we got the final frame (SW != 91AF)
        if (lastStatusWord != 0x91AF) {
            logDebug("Last frame received (SW=" + String(lastStatusWord, HEX) + ")");
            break;
        }
    }
    
    logDebug("GetVersion complete: " + String(totalLen) + " bytes in " + 
             String(frameCount) + " frames");
    
    // Copy to output
    if (totalLen > 0 && versionInfo != nullptr) {
        memcpy(versionInfo, fullVersion, min((size_t)28, totalLen));
    }
    
    return true;
}

// Commit transaction (makes ChangeKey permanent)
bool NTAG424Handler::commitTransaction() {
    if (!authenticated) {
        logError("Not authenticated - cannot commit transaction");
        return false;
    }
    
    logDebug("═══════════════════════════════════════");
    logDebug("CommitTransaction");
    logDebug("═══════════════════════════════════════");
    
    // CommitTransaction command: 0xC7 (no parameters)
    // sendCommand expects NATIVE command format (INS + data), not full APDU
    // sendCommand will wrap it with CLA, P1, P2, Lc, Le
    
    uint8_t cmd[9];
    cmd[0] = 0xC7;  // INS: CommitTransaction
    // cmd[1..8] = MAC (filled below)
    
    // Calculate IV for MAC calculation
    // IV = E(SesAuthENCKey, A55A || TI || CmdCtr || 0000000000000000)
    uint8_t ivInput[16];
    ivInput[0] = 0xA5;
    ivInput[1] = 0x5A;
    memcpy(ivInput + 2, transactionId, 4);
    ivInput[6] = commandCounter & 0xFF;
    ivInput[7] = (commandCounter >> 8) & 0xFF;
    memset(ivInput + 8, 0x00, 8);
    
    uint8_t iv[16];
    // IVc calculation is AES-ECB (zero CBC IV) - same as changeKey fix
    uint8_t zeroIV[16] = {0};
    if (!NTAG424Crypto::aesEncrypt(sessionEncKey, zeroIV, ivInput, 16, iv)) {
        logError("Failed to calculate IV");
        return false;
    }
    
    logDebug("IV: " + NTAG424Crypto::bytesToHexString(iv, 16));
    
    // Calculate CMAC over: Cmd || CmdCtr || TI (7 bytes total, AN12196 Table 19)
    uint8_t macInput[7];
    macInput[0] = 0xC7;  // Command
    macInput[1] = commandCounter & 0xFF;
    macInput[2] = (commandCounter >> 8) & 0xFF;
    memcpy(macInput + 3, transactionId, 4);
    
    logDebug("MAC Input: " + NTAG424Crypto::bytesToHexString(macInput, 7));
    
    // Calculate 8-byte truncated CMAC
    uint8_t mac[8];
    if (!NTAG424Crypto::calculateCMAC(sessionMacKey, macInput, 7, mac)) {
        logError("Failed to calculate CMAC");
        return false;
    }
    
    logDebug("CMAC: " + NTAG424Crypto::bytesToHexString(mac, 8));
    
    // Fill MAC into command (native command format: INS + MAC)
    memcpy(cmd + 1, mac, 8);
    
    logDebug("Command: " + NTAG424Crypto::bytesToHexString(cmd, 9));
    
#if DEBUG_WRITE_MODE
    Serial.println(F("\n╔══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║   🔍 DEBUG MODE: CommitTransaction NOT SENT  🔍            ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
    
    // Simulate success
    commandCounter++;
    
    logToWeb("🔍 DEBUG: CommitTransaction voorbereid maar NIET gestuurd", "warning");
    return true;
#else
    // Send command
    uint8_t response[32];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 9, response, responseLen)) {
        logError("CommitTransaction command failed");
        authenticated = false;
        return false;
    }
    
    // Increment counter
    commandCounter++;
    logDebug("CmdCtr incremented to: " + String(commandCounter));
    
    logToWeb("✅ Transaction committed", "success");
    return true;
#endif
}

// Select application
bool NTAG424Handler::selectNdefApplication() {
    // ISO SELECT FILE command for NTAG424 DNA NDEF Application
    // According to AN12196, Table 11: ISO SELECT NDEF application using DF Name
    // DF Name: D2760000850101h (7 bytes)
    // Format: 00 A4 04 0C 07 D2760000850101 00
    
    const uint8_t dfName[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
    
    uint8_t cmd[13];
    cmd[0] = 0x00;  // CLA: ISO7816-4 standard class
    cmd[1] = 0xA4;  // INS: SELECT FILE
    cmd[2] = 0x04;  // P1: Select by DF name
    cmd[3] = 0x0C;  // P2: First or only occurrence
    cmd[4] = 0x07;  // Lc: Length of DF name (7 bytes)
    memcpy(cmd + 5, dfName, 7);  // DF Name: D2760000850101
    cmd[12] = 0x00;  // Le: Expect response
    
    logDebug(">> ISO SELECT NDEF Application");
    logDebug("   DF Name: " + NTAG424Crypto::bytesToHexString(dfName, 7));
    
    // For ISO7816 SELECT command, we send the raw command without wrapping
    uint8_t response[16];
    size_t responseLen = sizeof(response);
    
    // Send raw ISO7816 command (not wrapped as native command)
    return transceiveRaw(cmd, 13, response, responseLen);
}

bool NTAG424Handler::selectApplication(const uint8_t* aid) {
    // SelectApplication uses ISO7816 SELECT command (INS=0x5A) with AID
    // Must be sent as raw ISO7816 command, NOT wrapped in NTAG424 native format
    // Format: CLA INS P1 P2 Lc AID[3] Le
    
    uint8_t cmd[9];
    cmd[0] = 0x00;  // CLA: ISO7816-4 standard class
    cmd[1] = 0x5A;  // INS: SELECT APPLICATION
    cmd[2] = 0x04;  // P1: Select by name (DF name)
    cmd[3] = 0x00;  // P2: First or only occurrence
    cmd[4] = 0x03;  // Lc: Length of AID (3 bytes)
    memcpy(cmd + 5, aid, 3);  // AID: 3 bytes
    cmd[8] = 0x00;  // Le: Expect response
    
    logDebug(">> ISO SELECT Application");
    logDebug("   AID: " + NTAG424Crypto::bytesToHexString(aid, 3));
    
    uint8_t response[16];
    size_t responseLen = sizeof(response);
    
    // Send raw ISO7816 command (not wrapped as native command)
    return transceiveRaw(cmd, 9, response, responseLen);
}

// Send ISO14443-4 command (NTAG424 native commands wrapped in ISO7816 APDU)
bool NTAG424Handler::sendCommand(const uint8_t* cmd, size_t cmdLen,
                                uint8_t* response, size_t& responseLen) {
    // NTAG424 DNA requires native commands to be wrapped in ISO7816-4 APDU format:
    // Format: CLA INS P1 P2 [Lc Data] [Le]
    // - CLA = 0x90 (proprietary class for NXP native commands)
    // - INS = native command byte (0x60, 0x71, 0xC4, etc.)
    // - P1 = 0x00
    // - P2 = 0x00
    // - Lc = data length (if data present)
    // - Data = command parameters
    // - Le = 0x00 (expect response)
    
    if (cmd == nullptr || cmdLen == 0) {
        logError("Invalid command");
        return false;
    }
    
    uint8_t apdu[256];
    size_t apduLen = 0;
    
    apdu[0] = 0x90;  // CLA: Proprietary class
    apdu[1] = cmd[0];  // INS: Native command
    apdu[2] = 0x00;  // P1
    apdu[3] = 0x00;  // P2
    
    if (cmdLen > 1) {
        // Command has parameters (Case 4: command + data, expect response)
        // NTAG424 DNA requires Le byte even with data present (ISO7816-4 Case 4)
        apdu[4] = cmdLen - 1;  // Lc: Length of data
        memcpy(apdu + 5, cmd + 1, cmdLen - 1);  // Data
        apdu[5 + (cmdLen - 1)] = 0x00;  // Le: Expect response (up to 256 bytes)
        apduLen = 5 + (cmdLen - 1) + 1;  // CLA+INS+P1+P2 + Lc + Data + Le
    } else {
        // Command without parameters (Case 2: no data, expect response)
        apdu[4] = 0x00;  // Le: Expect response (up to 256 bytes)
        apduLen = 5;
    }
    
    logDebug(">> Cmd: " + NTAG424Crypto::bytesToHexString(cmd, cmdLen));
    logDebug(">> APDU: " + NTAG424Crypto::bytesToHexString(apdu, apduLen));
    
    uint8_t apduResponse[256];
    size_t apduResponseLen = sizeof(apduResponse);
    
    if (!transceive(apdu, apduLen, apduResponse, apduResponseLen)) {
        logError("Transceive failed");
        return false;
    }
    
    // Response format: [Data] SW1 SW2
    // SW1 SW2 = Status Word (2 bytes at end)
    if (apduResponseLen < 2) {
        logError("Response too short: " + String(apduResponseLen) + " bytes");
        return false;
    }
    
    // Extract status word (last 2 bytes)
    uint8_t sw1 = apduResponse[apduResponseLen - 2];
    uint8_t sw2 = apduResponse[apduResponseLen - 1];
    uint16_t statusWord = (sw1 << 8) | sw2;
    
    // Extract data (everything except last 2 bytes)
    size_t dataLen = apduResponseLen - 2;
    if (dataLen > 0 && response != nullptr) {
        memcpy(response, apduResponse, min(responseLen, dataLen));
        responseLen = min(responseLen, dataLen);
    } else {
        responseLen = 0;
    }
    
    logDebug("<< Rsp: " + NTAG424Crypto::bytesToHexString(response, responseLen) + 
             " SW=" + String(statusWord, HEX));
    
    // Store last status word
    lastStatusWord = statusWord;
    
    // Check status word
    if (statusWord == 0x9000) {
        // Success
        return true;
    }
    else if ((statusWord & 0xFF00) == 0x9100) {
        // NTAG424 DNA native status format: 0x91xx where xx is native status
        uint8_t nativeStatus = statusWord & 0x00FF;
        
        if (nativeStatus == 0x00) {
            // 0x9100 = Success (native status 0x00)
            return true;
        }
        else if (nativeStatus == 0xAF) {
            // 0x91AF = Additional Frame
            return true;
        }
        else if (nativeStatus == 0xAE) {
            logError("Authentication error (SW=91AE)");
            return false;
        }
        else if (nativeStatus == 0x7E) {
            logError("Length error (SW=917E)");
            return false;
        }
        else if (nativeStatus == 0xCA) {
            logError("Command aborted (SW=91CA)");
            return false;
        }
        else {
            logError("NTAG424 error, native status=0x" + String(nativeStatus, HEX));
            return false;
        }
    }
    else if (statusWord == 0x6700) {
        logError("Wrong length (SW=6700)");
        return false;
    }
    else if (statusWord == 0x6D00) {
        logError("INS not supported (SW=6D00)");
        return false;
    }
    else {
        logError("Command error, SW=" + String(statusWord, HEX));
        return false;
    }
}

// Transceive wrapper
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                               uint8_t* response, size_t& responseLen) {
    // Validate inputs
    if (nfc == nullptr) {
        logError("NFC reader not initialized");
        return false;
    }
    
    if (cmd == nullptr || cmdLen == 0 || response == nullptr) {
        logError("Invalid transceive parameters");
        return false;
    }
    
    // NOTE: We do NOT check isCardPresent() here because:
    // - After RATS activation, card is in ISO14443-4 (ISO-DEP) mode
    // - isCardPresent() does ISO14443-3 detection (REQA/anticollision)
    // - This would RESET the ISO-DEP session established by activateCard()
    // - If card is removed, we'll get a timeout anyway
    
    // For ISO-DEP, we need to wrap the command with a PCB (Protocol Control Byte)
    // PCB format for I-block: 0x0A (no chaining, block number 0) or 0x0B (block 1)
    // We'll use a simple static block toggle for now
    static uint8_t blockNumber = 0;
    
    // Build ISO-DEP I-block
    uint8_t isoCmd[256];
    isoCmd[0] = 0x02 | (blockNumber & 0x01); // I-block with block number toggle
    memcpy(isoCmd + 1, cmd, cmdLen);
    size_t isoCmdLen = cmdLen + 1;
    
    logDebug("Sending " + String(isoCmdLen) + " bytes...");
    
    // Clear any pending IRQ status
    nfc->clearIRQStatus(0xFFFFFFFF);
    delay(5);
    
    // Send command to card
    if (!nfc->sendData(isoCmd, isoCmdLen)) {
        logError("Failed to send command to card");
        return false;
    }
    
    // Wait for response (with timeout)
    unsigned long startTime = millis();
    const unsigned long timeout = 500; // 500ms timeout for NTAG424 crypto operations
    
    uint32_t irqStatus = 0;
    while (millis() - startTime < timeout) {
        irqStatus = nfc->getIRQStatus();
        
        // Check for RX complete
        if (irqStatus & RX_IRQ_STAT) {
            break;
        }
        
        // Check for errors
        if (irqStatus & GENERAL_ERROR_IRQ_STAT) {
            logError("General error during transceive");
            logDebug("IRQ Status: 0x" + String(irqStatus, HEX));
            return false;
        }
        
        delay(2);
    }
    
    if (!(irqStatus & RX_IRQ_STAT)) {
        logError("❌ Timeout waiting for card response");
        logDebug("IRQ Status: 0x" + String(irqStatus, HEX));
        return false;
    }
    
    // Get actual received length from RX_STATUS register
    uint32_t rxStatus;
    if (!nfc->readRegister(RX_STATUS, &rxStatus)) {
        logError("Failed to read RX status");
        return false;
    }
    
    uint16_t rxLen = (rxStatus >> 0) & 0x1FF; // Bits 0-8 contain RX bytes
    logDebug("Received " + String(rxLen) + " bytes");
    
    if (rxLen < 1) {
        logError("Empty response from card");
        return false;
    }
    
    // Read response from card
    uint8_t* rxData = nfc->readData(rxLen);
    if (rxData == nullptr) {
        logError("Failed to read response data");
        return false;
    }
    
    // Log raw response for debugging
    String rxHex = "RX: ";
    for (int i = 0; i < rxLen && i < 20; i++) {
        if (rxData[i] < 0x10) rxHex += "0";
        rxHex += String(rxData[i], HEX);
        rxHex += " ";
    }
    logDebug(rxHex);
    
    // Check first byte (PCB)
    uint8_t pcb = rxData[0];
    
    // Check if it's an I-block (0x02/0x03) or R-block (0xA2/0xA3) or S-block (0xC2/0xF2)
    if ((pcb & 0xE2) == 0x02) {
        // I-block response - normal data
        // Toggle block number for next transmission
        blockNumber = (blockNumber + 1) & 0x01;
        
        // Extract payload (skip PCB, exclude CRC which PN5180 handles)
        // PN5180 in ISO14443 mode with CRC enabled will strip the CRC automatically
        size_t payloadLen = rxLen - 1; // -1 for PCB
        size_t copyLen = min(responseLen, payloadLen);
        memcpy(response, rxData + 1, copyLen);
        responseLen = copyLen;
        
        return true;
    } 
    else if ((pcb & 0xF6) == 0xA2) {
        // R-block (ACK/NAK)
        logError("Received R-block (ACK/NAK): 0x" + String(pcb, HEX));
        return false;
    }
    else if ((pcb & 0xC7) == 0xC2) {
        // S-block (protocol control)
        logError("Received S-block: 0x" + String(pcb, HEX));
        return false;
    }
    else {
        logError("Invalid PCB byte: 0x" + String(pcb, HEX));
        return false;
    }
}

// Raw transceive for ISO7816 commands (no wrapping)
bool NTAG424Handler::transceiveRaw(const uint8_t* cmd, size_t cmdLen,
                                   uint8_t* response, size_t& responseLen) {
    // This is identical to transceive() - both use ISO-DEP I-block wrapping
    // The difference is semantic: transceiveRaw() is for standard ISO7816 commands
    // while transceive() is for wrapped native NTAG424 commands
    return transceive(cmd, cmdLen, response, responseLen);
}

// Change Key Settings (authenticated command)
// AN12196 Section 6.17: ChangeKeySettings command
bool NTAG424Handler::changeKeySettings(uint8_t settings) {
    if (!authenticated) {
        logError("ChangeKeySettings requires authentication");
        return false;
    }
    
    logDebug("ChangeKeySettings called with: 0x" + String(settings, HEX));
    logToWeb("Start ChangeKeySettings met waarde 0x" + String(settings, HEX), "info");
    
    // Step 1: Prepare plaintext data (16 bytes)
    // Format: [KeySettings:1] [CRC32:4] [Padding:11]
    uint8_t plainData[16];
    plainData[0] = settings;
    
    // Calculate CRC32 over the KeySettings byte (LSB first as per ISO14443)
    uint32_t crc = NTAG424Crypto::calculateCRC32(&settings, 1);
    logDebug("CRC32: " + String(crc, HEX));
    
    // Pack CRC32 in LSB first order
    plainData[1] = crc & 0xFF;
    plainData[2] = (crc >> 8) & 0xFF;
    plainData[3] = (crc >> 16) & 0xFF;
    plainData[4] = (crc >> 24) & 0xFF;
    
    // Add padding (0x80 followed by zeros)
    plainData[5] = 0x80;
    memset(plainData + 6, 0x00, 10);
    
    logDebug("Plain Data: " + NTAG424Crypto::bytesToHexString(plainData, 16));
    
    // Step 2: Calculate IV for encryption (AN12196 Section 9.1.4)
    // IV = E(SesAuthENCKey, A55A || TI || CmdCtr || 0000000000000000)
    uint8_t ivInput[16];
    ivInput[0] = 0xA5;
    ivInput[1] = 0x5A;
    memcpy(ivInput + 2, transactionId, 4);
    ivInput[6] = commandCounter & 0xFF;        // CmdCtr LSB first
    ivInput[7] = (commandCounter >> 8) & 0xFF;
    memset(ivInput + 8, 0x00, 8);  // Zero padding
    
    uint8_t iv[16];
    // EV2: Use current IV for chaining
    if (!NTAG424Crypto::aesEncrypt(sessionEncKey, currentIV, ivInput, 16, iv)) {
        logError("Failed to calculate IV");
        return false;
    }
    
    logDebug("IV: " + NTAG424Crypto::bytesToHexString(iv, 16));
    logDebug("CmdCtr: " + String(commandCounter));
    
    // Step 3: Encrypt plaintext data with calculated IV
    uint8_t encData[16];
    if (!NTAG424Crypto::aesEncrypt(sessionEncKey, iv, plainData, 16, encData)) {
        logError("Failed to encrypt key settings");
        return false;
    }
    
    logDebug("Encrypted Data: " + NTAG424Crypto::bytesToHexString(encData, 16));
    
    // Step 4: Calculate CMAC (AN12196 Section 9.1.9)
    // MAC Input = Cmd || CmdCtr || TI || EncCmdData
    // Total: 1 + 2 + 4 + 16 = 23 bytes
    uint8_t macInput[23];
    macInput[0] = CMD_CHANGE_KEY_SETTINGS;       // Cmd (1 byte)
    macInput[1] = commandCounter & 0xFF;         // CmdCtr LSB first (2 bytes)
    macInput[2] = (commandCounter >> 8) & 0xFF;
    memcpy(macInput + 3, transactionId, 4);      // TI (4 bytes, MSB first)
    memcpy(macInput + 7, encData, 16);           // EncCmdData (16 bytes)
    
    logDebug("MAC Input: " + NTAG424Crypto::bytesToHexString(macInput, 23));
    
    // Calculate full CMAC for debugging
    uint8_t macFull[16];
    if (!NTAG424Crypto::calculateCMACFull(sessionMacKey, macInput, 23, macFull)) {
        logError("Failed to calculate full CMAC");
        return false;
    }
    logDebug("CMAC Full: " + NTAG424Crypto::bytesToHexString(macFull, 16));
    
    // Truncate to 8 bytes
    uint8_t mac[8];
    if (!NTAG424Crypto::calculateCMAC(sessionMacKey, macInput, 23, mac)) {
        logError("Failed to calculate CMAC");
        return false;
    }
    
    logDebug("CMAC: " + NTAG424Crypto::bytesToHexString(mac, 8));
    
    // Step 5: Build command
    // Format: [Cmd:1] [EncData:16] [MAC:8] = 25 bytes
    uint8_t cmd[25];
    cmd[0] = CMD_CHANGE_KEY_SETTINGS;
    memcpy(cmd + 1, encData, 16);
    memcpy(cmd + 17, mac, 8);
    
    Serial.println(F("\n[CHANGE KEY SETTINGS]"));
    Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    Serial.print(F("Settings:       0x"));
    Serial.println(settings, HEX);
    Serial.print(F("Full APDU:      "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd, 25));
    Serial.print(F("  [00] Cmd:     0x"));
    Serial.println(cmd[0], HEX);
    Serial.print(F("  [01-16] Enc:  "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd + 1, 16));
    Serial.print(F("  [17-24] MAC:  "));
    Serial.println(NTAG424Crypto::bytesToHexString(cmd + 17, 8));
    Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    
    // Step 6: Send command to card
    uint8_t response[32];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 25, response, responseLen)) {
        logError("ChangeKeySettings command failed");
        authenticated = false;  // Authentication lost on error
        return false;
    }
    
    // Step 7: Update session state after successful command
    commandCounter++;
    logDebug("CmdCtr incremented to: " + String(commandCounter));
    
    // Update Current IV for next operation (EV2 chained CBC mode)
    // Use last ciphertext block (all 16 bytes since we only have one block)
    memcpy(this->currentIV, encData, 16);
    logDebug("Current IV updated: " + NTAG424Crypto::bytesToHexString(this->currentIV, 16));
    
    logToWeb("✅ Key settings succesvol gewijzigd!", "success");
    
    return true;
}

// Get Key Settings (authenticated command)
// AN12196 Section 6.7: GetKeySettings command
bool NTAG424Handler::getKeySettings(uint8_t& settings, uint8_t& maxKeys) {
    if (!authenticated) {
        logError("GetKeySettings requires authentication");
        return false;
    }
    
    logDebug("Getting key settings...");
    
    // GetKeySettings command (0x45) - no parameters
    uint8_t cmd[1] = { CMD_GET_KEY_SETTINGS };
    
    uint8_t response[32];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 1, response, responseLen)) {
        logError("GetKeySettings command failed");
        return false;
    }
    
    // Response format (authenticated response, needs decryption):
    // - Encrypted: [KeySettings:1] [MaxKeys:1] [Padding:14] (16 bytes encrypted + 8 byte MAC)
    // OR
    // - Plain (if no auth): [KeySettings:1] [MaxKeys:1]
    
    if (responseLen >= 2) {
        // For now, assume plain response for factory cards
        // TODO: Implement encrypted response decryption for authenticated sessions
        settings = response[0];
        maxKeys = response[1];
        
        logDebug("Key Settings: 0x" + String(settings, HEX));
        logDebug("Max Keys: " + String(maxKeys));
        
        // Decode KeySettings byte (AN12196 Table 20):
        // Bit 7: Configuration changeable (1 = allowed)
        // Bit 6: AuthKey changeable without master key auth
        // Bit 5: Free Directory List access
        // Bit 4: Free Create/Delete wo master key
        // Bit 3: Configuration frozen (1 = frozen, cannot change)
        // Bits 2-0: ChangeKey access rights (key number or 0xE=frozen)
        
        Serial.println(F("\n[KEY SETTINGS ANALYSIS]"));
        Serial.println(F("────────────────────────────────────────────────────────────"));
        
        bool configChangeable = (settings & 0x80) != 0;
        bool authKeyChangeable = (settings & 0x40) != 0;
        bool freeDirList = (settings & 0x20) != 0;
        bool freeCreateDelete = (settings & 0x10) != 0;
        bool configFrozen = (settings & 0x08) != 0;
        uint8_t changeKeyRights = settings & 0x0F;
        
        Serial.print(F("Configuration changeable:  "));
        Serial.println(configChangeable ? "✅ YES" : "❌ NO (frozen)");
        
        Serial.print(F("AuthKey changeable:        "));
        Serial.println(authKeyChangeable ? "✅ YES (without master)" : "⚠️  Needs master key");
        
        Serial.print(F("Free Directory List:       "));
        Serial.println(freeDirList ? "✅ YES" : "❌ NO");
        
        Serial.print(F("Free Create/Delete:        "));
        Serial.println(freeCreateDelete ? "✅ YES" : "❌ NO");
        
        Serial.print(F("Configuration frozen:      "));
        Serial.println(configFrozen ? "❌ YES (LOCKED!)" : "✅ NO");
        
        Serial.print(F("ChangeKey rights:          "));
        if (changeKeyRights == 0x0E) {
            Serial.println(F("🔒 FROZEN (ChangeKey disabled!)"));
        } else if (changeKeyRights == 0x0F) {
            Serial.println(F("🆓 FREE (no auth needed)"));
        } else {
            Serial.print(F("Key #"));
            Serial.print(changeKeyRights);
            Serial.println(F(" required"));
        }
        
        Serial.print(F("Maximum keys:              "));
        Serial.println(maxKeys);
        Serial.println(F("────────────────────────────────────────────────────────────"));
        
        // Check if ChangeKey is possible
        if (changeKeyRights == 0x0E || configFrozen) {
            Serial.println(F(""));
            Serial.println(F("⚠️  WARNING: ChangeKey may be BLOCKED!"));
            Serial.println(F("   This card may not allow key changes."));
            Serial.println(F("   Proceeding anyway - card will reject if locked."));
            Serial.println(F(""));
        } else {
            Serial.println(F("\n✅ ChangeKey should be allowed\n"));
        }
        
        return true;
    } else {
        logError("Invalid GetKeySettings response length: " + String(responseLen));
        return false;
    }
}

// Logging helpers
void NTAG424Handler::logToWeb(const String& message, const String& level) {
    if (webServer != nullptr) {
        webServer->broadcastLog(message, level);
    }
    
    if (level == "error") {
        Serial.println("[ERROR] " + message);
    } else if (level == "warning") {
        Serial.println("[WARN] " + message);
    } else {
        Serial.println("[INFO] " + message);
    }
}

void NTAG424Handler::logDebug(const String& message) {
    Serial.println("[DEBUG] " + message);
}

void NTAG424Handler::logError(const String& message) {
    logToWeb("❌ " + message, "error");
}
