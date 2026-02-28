#include "ntag424_handler.h"
#include "web_server.h"
#include <PN5180.h>

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
    commandCounter = 0;
    memset(transactionId, 0, 4);
    memset(sessionEncKey, 0, 16);
    memset(sessionMacKey, 0, 16);
    logDebug("ISO-DEP session reset");
}

// Authenticate with NTAG424 DNA using EV2 First
bool NTAG424Handler::authenticateEV2First(uint8_t keyNo, const uint8_t* key, AuthResult& result) {
    result.success = false;
    result.errorMessage = "";
    
    logDebug("Start EV2 First Authentication");
    logDebug("Key Number: " + String(keyNo));
    
    // Step 1: Send AuthenticateEV2First command
    // NTAG424 expects: 0x71 || KeyNo || LenCap (3 bytes total)
    uint8_t cmd[3];
    cmd[0] = CMD_AUTHENTICATE_EV2_FIRST;  // 0x71
    cmd[1] = keyNo;                        // Key number
    cmd[2] = 0x00;                         // LenCap (standard frame size)
    
    uint8_t response[64];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 3, response, responseLen)) {
        result.errorMessage = "Failed to send auth command";
        logError(result.errorMessage);
        return false;
    }
    
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
    
    logDebug("Generated RndA: " + NTAG424Crypto::bytesToHexString(rndA, 16));
    
    // Step 4: Rotate RndB left by 1 byte (RndB')
    uint8_t rndBPrime[16];
    NTAG424Crypto::rotateLeft(rndB, rndBPrime, 16);
    
    logDebug("Rotated RndB': " + NTAG424Crypto::bytesToHexString(rndBPrime, 16));
    
    // Step 5: Concatenate RndA || RndB' and encrypt
    uint8_t authData[32];
    memcpy(authData, rndA, 16);
    memcpy(authData + 16, rndBPrime, 16);
    
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
    
    logDebug("Encrypted Response: " + NTAG424Crypto::bytesToHexString(encResponse, 32));
    
    // Step 7: Decrypt response to get TI || RndA' || PDcap2 || PCDcap2
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
    commandCounter = 0;  // Reset to 0 after authentication
    memcpy(transactionId, ti, 4);
    memcpy(sessionEncKey, result.sessionEncKey, 16);
    memcpy(sessionMacKey, result.sessionMacKey, 16);
    
    result.success = true;
    logToWeb("✅ NTAG424 authenticatie succesvol", "success");
    
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
    // Case 1: KeyNo to be changed ≠ AuthKey → XOR + CRC32 + version
    // Case 2: KeyNo to be changed = AuthKey → NO XOR, only encrypt new key + version
    
    uint8_t plainData[32];
    
    // Determine which authenticated key we're using
    // In authenticateEV2First we authenticate with keyNo, so authKeyNo = keyNo
    uint8_t authKeyNo = keyNo;
    
    // FORCE CASE 1 FORMAT (XOR + CRC32) even for same key - test for factory cards
    bool forceCase1 = true;  // Test hypothesis
    
    if (forceCase1 || keyNo != authKeyNo) {
        // **CASE 1**: Different key - use XOR + CRC32
        logDebug(forceCase1 ? "ChangeKey: FORCING Case 1 format (XOR + CRC32)" : "ChangeKey Case 1: KeyNo != AuthKey");
        
        // XOR old key with new key
        uint8_t keyData[16];
        NTAG424Crypto::xorArrays(oldKey, newKey, keyData, 16);
        logDebug("XOR Key Data: " + NTAG424Crypto::bytesToHexString(keyData, 16));
        
        // Calculate CRC32 over new key (LSB first as per ISO14443)
        uint32_t crc = NTAG424Crypto::calculateCRC32(newKey, 16);
        logDebug("CRC32: " + String(crc, HEX));
        
        // Prepare plaintext: [XOR KeyData:16] [Version:1] [CRC32:4] [Padding:11]
        memcpy(plainData, keyData, 16);
        plainData[16] = 0x01;  // Key version
        plainData[17] = crc & 0xFF;         // CRC LSB first
        plainData[18] = (crc >> 8) & 0xFF;
        plainData[19] = (crc >> 16) & 0xFF;
        plainData[20] = (crc >> 24) & 0xFF;
        plainData[21] = 0x80;  // Padding indicator
        memset(plainData + 22, 0x00, 10);  // Zero padding
        
    } else {
        // **CASE 2**: Same key - NO XOR, just encrypt new key directly
        logDebug("ChangeKey Case 2: KeyNo == AuthKey");
        
        // Prepare plaintext: [NewKey:16] [Version:1] [Padding:15]
        memcpy(plainData, newKey, 16);
        plainData[16] = 0x00;  // Key version (0x00 for factory cards)
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
    macInput[0] = CMD_CHANGE_KEY;                // Cmd (1 byte)
    macInput[1] = commandCounter & 0xFF;         // CmdCtr LSB first (2 bytes)
    macInput[2] = (commandCounter >> 8) & 0xFF;
    memcpy(macInput + 3, transactionId, 4);      // TI (4 bytes, MSB first)
    macInput[7] = keyNo;                         // CmdHeader (1 byte)
    memcpy(macInput + 8, encKeyData, 32);        // EncCmdData (32 bytes)
    
    // Debug: Log complete MAC input for analysis
    logDebug("MAC Input: " + NTAG424Crypto::bytesToHexString(macInput, 40));
    
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
    
    uint8_t response[32];
    size_t responseLen = sizeof(response);
    
    if (!sendCommand(cmd, 42, response, responseLen)) {
        logError("ChangeKey command failed");
        authenticated = false;  // Authentication lost on error
        return false;
    }
    
    // Increment command counter after successful command
    commandCounter++;
    logDebug("CmdCtr incremented to: " + String(commandCounter));
    
    // Response should contain MAC, verify it
    // For now, just check if we got SW=9100
    logToWeb("✅ Master key succesvol geschreven!", "success");
    
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
