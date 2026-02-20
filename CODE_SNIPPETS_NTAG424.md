# NTAG424 - Kritieke Code Snippets

## 1. PN5180 Transceive - MEEST KRITIEK! ⚠️

**Locatie:** `src/ntag424_handler.cpp` - regel ~185

**Huidige code:**
```cpp
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                               uint8_t* response, size_t& responseLen) {
    // Placeholder - needs actual PN5180 ISO14443-4 implementation
    logError("Transceive not fully implemented - needs PN5180 ISO14443-4 support");
    return false;
}
```

**Onderzoek eerst de PN5180 library:**

```cpp
// De PN5180-Library heeft waarschijnlijk een van deze methoden:

// Optie 1: ISO14443-4 APDU communicatie
bool sendAPDU(const uint8_t* apdu, uint16_t apduLen, uint8_t* response, uint16_t* responseLen);

// Optie 2: Generieke data exchange
bool transceive(const uint8_t* sendData, uint16_t sendLen, uint8_t* recvData, uint16_t* recvLen);

// Optie 3: Lage-niveau send/receive
bool sendData(const uint8_t* data, uint16_t len);
bool readData(uint8_t* data, uint16_t* len);
```

**Implementatie voorbeeld (pas aan naar jouw library):**

```cpp
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                               uint8_t* response, size_t& responseLen) {
    
    // STAP 1: Voorbereiden
    responseLen = 0;
    
    // STAP 2: Verstuur command naar kaart
    // Check of PN5180ISO14443 een transceive methode heeft:
    uint16_t rxLen = 0;
    uint8_t rxBuffer[256];
    
    // Probeer deze variant (als de library dit ondersteunt):
    PN5180ISO14443::ISO14443ErrorCode result = 
        nfc->transceiveCommand(cmd, cmdLen, rxBuffer, &rxLen);
    
    // OF als er geen transceiveCommand is, try sendData/readData:
    // if (!nfc->sendData(cmd, cmdLen)) {
    //     logError("Failed to send command");
    //     return false;
    // }
    // 
    // delay(10); // Wait for card processing
    // 
    // if (!nfc->readData(rxBuffer, &rxLen)) {
    //     logError("Failed to read response");
    //     return false;
    // }
    
    // STAP 3: Check result
    if (result != PN5180ISO14443::ISO14443_OK) {
        logError("ISO14443 error: " + String(result));
        return false;
    }
    
    // STAP 4: Kopieer response
    if (rxLen > 0 && rxLen <= 256) {
        memcpy(response, rxBuffer, rxLen);
        responseLen = rxLen;
        return true;
    }
    
    logError("Invalid response length: " + String(rxLen));
    return false;
}
```

**Debugging tip:**
Als je niet weet welke methode te gebruiken, zoek in de PN5180 library code:
```bash
# In VSCode terminal:
cd .pio/libdeps/seeed_xiao_esp32s3/PN5180-Library
grep -r "transceive\|sendData\|readData\|ISO14443" *.cpp *.h
```

---

## 2. NFCReader - getNFC() Methode

**Locatie:** `include/nfc_reader.h` - voeg toe in public sectie

```cpp
public:
    // ... bestaande methods ...
    
    /**
     * Get PN5180 instance for advanced NTAG424 operations
     * @return Pointer to PN5180ISO14443 instance
     */
    PN5180ISO14443* getNFC() { return nfc; }
```

---

## 3. Main.cpp - Global Variables

**Locatie:** `src/main.cpp` - na regel 49

```cpp
// ============ SYSTEM COMPONENTS ============

WiFiConfigManager wifiManager;
SystemConfig systemConfig;
ServerClient serverClient;
NFCWebServer webServer;
NFCReader* nfcReader = nullptr;

// VOEG TOE:
NTAG424Handler* ntag424Handler = nullptr;
```

---

## 4. Main.cpp - Setup() Initialisatie

**Locatie:** `src/main.cpp` in `setup()` - na nfcReader->begin()

```cpp
// Initialize NFC Reader
Serial.println(F("\n=== NFC Reader Initialization ==="));
// ... existing nfcReader init code ...

if (!nfcReader->begin()) {
    Serial.println(F("❌ NFC Reader initialization failed!"));
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    while(1) delay(1000);
}

// VOEG TOE:
// Initialize NTAG424 Handler
Serial.println(F("\n=== NTAG424 Handler Initialization ==="));
ntag424Handler = new NTAG424Handler(nfcReader->getNFC());
ntag424Handler->setWebServer(&webServer);
Serial.println(F("✅ NTAG424 Handler ready"));

Serial.println(F("\n=================================="));
Serial.println(F("✅ System Ready!"));
// ... rest of code ...
```

---

## 5. Main.cpp - handleConfigMode() COMPLETE REPLACEMENT

**Locatie:** `src/main.cpp` - vervang hele functie (vanaf regel ~167)

```cpp
// ============ CONFIG MODE HANDLER ============

void handleConfigMode(NFCReader::CardInfo& cardInfo) {
    Serial.println(F("\n=== CONFIG MODE - NTAG424 PERSONALIZATION ==="));
    webServer.broadcastLog("🔧 Config modus - Start NTAG424 personalisatie", "info");
    
    // Check if it's actually an NTAG424 DNA card
    if (cardInfo.cardType.indexOf("NTAG424") < 0 && 
        cardInfo.cardType.indexOf("DESFire") < 0 &&
        cardInfo.cardType.indexOf("SECURE") < 0) {
        webServer.broadcastLog("⚠️ Geen NTAG424 DNA kaart gedetecteerd", "warning");
        webServer.broadcastLog("Card type: " + cardInfo.cardType, "info");
        return;
    }
    
    // Step 1: Obtain master key
    String masterKeyHex = "";
    bool useManualKey = systemConfig.hasMasterkey();
    
    if (useManualKey) {
        // Manual key (offline mode)
        masterKeyHex = systemConfig.getSessionMasterkey();
        Serial.println(F("🔑 Using manual masterkey (offline)"));
        webServer.broadcastLog("Gebruik handmatige masterkey", "warning");
    } else {
        // Fetch from server
        Serial.println(F("🌐 Fetching masterkey from server..."));
        webServer.broadcastLog("Stap 1/4: Haal masterkey op van server...", "info");
        
        masterKeyHex = serverClient.getCardKey(cardInfo.uidString, "master");
        
        if (masterKeyHex.length() != 32) {
            // Card not in database - register first
            Serial.println(F("📝 Card not registered - registering..."));
            webServer.broadcastLog("Kaart niet bekend - registreren bij server...", "info");
            
            bool registered = serverClient.registerCard(
                cardInfo.uidString,
                "Card_" + cardInfo.uidString.substring(0, 8),
                "Auto-registered via config mode"
            );
            
            if (!registered) {
                Serial.println(F("❌ Registration failed"));
                webServer.broadcastLog("❌ Kon kaart niet registreren bij server", "error");
                return;
            }
            
            webServer.broadcastLog("✅ Kaart geregistreerd", "success");
            
            // Fetch key again
            masterKeyHex = serverClient.getCardKey(cardInfo.uidString, "master");
        }
    }
    
    // Validate key
    if (masterKeyHex.length() != 32) {
        Serial.println(F("❌ Invalid masterkey received"));
        webServer.broadcastLog("❌ Ongeldige masterkey ontvangen van server", "error");
        return;
    }
    
    Serial.print(F("✅ Master key: "));
    Serial.print(masterKeyHex.substring(0, 8));
    Serial.println("...");
    webServer.broadcastLog("✅ Masterkey: " + masterKeyHex.substring(0, 8) + "...", "success");
    
    // Step 2: Convert hex string to bytes
    uint8_t newKeyBytes[16];
    size_t keyLen = NTAG424Crypto::hexStringToBytes(masterKeyHex, newKeyBytes, 16);
    
    if (keyLen != 16) {
        Serial.println(F("❌ Key conversion failed"));
        webServer.broadcastLog("❌ Key conversie mislukt", "error");
        return;
    }
    
    // Step 3: Authenticate and change key
    Serial.println(F("🔐 Authenticating with factory key..."));
    webServer.broadcastLog("Stap 2/4: Authenticeer met factory key...", "info");
    
    // First authenticate to verify card communication
    NTAG424Handler::AuthResult authResult;
    bool authenticated = ntag424Handler->authenticateEV2First(
        0, 
        NTAG424Handler::DEFAULT_AES_KEY, 
        authResult
    );
    
    if (!authenticated) {
        Serial.println(F("❌ Authentication failed: ") + authResult.errorMessage);
        webServer.broadcastLog("❌ Authenticatie mislukt: " + authResult.errorMessage, "error");
        return;
    }
    
    Serial.println(F("✅ Authentication successful"));
    webServer.broadcastLog("✅ Authenticatie succesvol", "success");
    
    // Now change the key
    Serial.println(F("✍️ Writing new master key to card..."));
    webServer.broadcastLog("Stap 3/4: Schrijf nieuwe masterkey...", "info");
    
    bool keyChanged = ntag424Handler->changeKey(
        0,  // Key number 0 (master key)
        NTAG424Handler::DEFAULT_AES_KEY,  // Old key
        newKeyBytes  // New key
    );
    
    if (!keyChanged) {
        Serial.println(F("❌ ChangeKey failed"));
        webServer.broadcastLog("❌ Schrijven masterkey mislukt", "error");
        return;
    }
    
    Serial.println(F("✅ Master key written successfully"));
    webServer.broadcastLog("✅ Masterkey geschreven!", "success");
    
    // Step 4: Verify by authenticating with new key
    Serial.println(F("🔍 Verifying new key..."));
    webServer.broadcastLog("Stap 4/4: Verificatie nieuwe key...", "info");
    
    NTAG424Handler::AuthResult verifyResult;
    bool verified = ntag424Handler->authenticateEV2First(
        0,
        newKeyBytes,
        verifyResult
    );
    
    if (verified) {
        Serial.println(F("✅✅✅ CARD PERSONALIZATION COMPLETE! ✅✅✅"));
        webServer.broadcastLog("✅ Verificatie succesvol!", "success");
        webServer.broadcastLog("🎉 Kaart gepersonaliseerd en gereed voor gebruik!", "success");
        
        // Log to server if connected
        systemConfig.incrementCardsRead();
    } else {
        Serial.println(F("⚠️ Verification failed: ") + verifyResult.errorMessage);
        webServer.broadcastLog("⚠️ Verificatie mislukt - key mogelijk niet correct!", "warning");
        webServer.broadcastLog("Error: " + verifyResult.errorMessage, "warning");
    }
    
    // Wait for card removal
    Serial.println(F("\n👉 Remove card and present next card to personalize"));
    webServer.broadcastLog("Verwijder kaart - klaar voor volgende kaart", "info");
}
```

---

## 6. NTAG424Crypto - CRC32 Implementatie

**Locatie:** `include/ntag424_crypto.h` - voeg toe bij de andere static methods

```cpp
/**
 * Calculate CRC32 for NTAG424 ChangeKey command
 * @param data - Data to calculate CRC over
 * @param length - Length of data
 * @return 32-bit CRC value  
 */
static uint32_t calculateCRC32(const uint8_t* data, size_t length);
```

**Locatie:** `src/ntag424_crypto.cpp` - voeg toe na de andere methods

```cpp
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
```

**Locatie:** `src/ntag424_handler.cpp` in `changeKey()` - vervang placeholder

```cpp
// OLD (regel ~140):
uint32_t crc = 0xFFFFFFFF; // Simplified placeholder

// NEW:
uint32_t crc = NTAG424Crypto::calculateCRC32(newKey, 16);
```

---

## 7. Web Server - API Handler Updates

**Locatie:** `include/web_server.h` - vervang de stub handlers

### handleGetCurrentCard() - regel ~360

```cpp
void handleGetCurrentCard() {
    // Check if card is present
    NFCReader::CardInfo cardInfo;
    bool present = false;
    
    // This requires access to nfcReader instance
    // For now, return not present
    // TODO: Add nfcReader pointer to NFCWebServer class
    
    String json = "{\"present\":false}";
    httpServer.send(200, "application/json", json);
}
```

**Better implementation** (requires nfcReader pointer in class):

```cpp
private:
    WebServer httpServer;
    WebSocketsServer wsServer;
    SystemConfig* config;
    ServerClient* serverClient;
    NFCReader* nfcReader;  // ADD THIS
    
// Then in begin():
void begin(SystemConfig* cfg, ServerClient* srv, NFCReader* nfc) {
    config = cfg;
    serverClient = srv;
    nfcReader = nfc;  // ADD THIS
    // ...
}

// Then in handleGetCurrentCard():
void handleGetCurrentCard() {
    NFCReader::CardInfo cardInfo;
    
    if (nfcReader != nullptr && nfcReader->readCard(cardInfo) && cardInfo.isPresent) {
        String json = "{";
        json += "\"present\":true,";
        json += "\"uid\":\"" + cardInfo.uidString + "\",";
        json += "\"type\":\"" + cardInfo.cardType + "\",";
        json += "\"secure\":" + String(cardInfo.isSecure ? "true" : "false");
        json += "}";
        httpServer.send(200, "application/json", json);
    } else {
        httpServer.send(200, "application/json", "{\"present\":false}");
    }
}
```

---

## 8. Compileer en Test

```bash
# Clean build
pio run --target clean

# Build
pio run

# Upload
pio run --target upload --upload-port COM9

# Monitor
pio device monitor
```

---

## 9. Test Procedure

### Test 1: Basis Communicatie
1. Upload code naar ESP32
2. Open Serial Monitor
3. Houd NTAG424 DNA kaart tegen reader
4. Kijk of kaart gedetecteerd wordt als NTAG424/SECURE

### Test 2: GetVersion Command
Voeg toe in `setup()` voor test:
```cpp
// Test NTAG424 communication
if (nfcReader->isCardPresent()) {
    Serial.println("Testing NTAG424 GetVersion...");
    uint8_t versionInfo[28];
    if (ntag424Handler->getVersion(versionInfo)) {
        Serial.println("✅ GetVersion OK!");
    } else {
        Serial.println("❌ GetVersion failed!");
    }
}
```

### Test 3: Factory Key Authentication
```cpp
// Test authentication with factory key
NTAG424Handler::AuthResult result;
bool auth = ntag424Handler->authenticateEV2First(
    0, NTAG424Handler::DEFAULT_AES_KEY, result
);
Serial.println(auth ? "✅ Auth OK" : "❌ Auth FAILED: " + result.errorMessage);
```

### Test 4: Full Personalization
1. Ga naar web interface
2. Login
3. Settings > Config Reader Mode
4. Klik "Start Kaart Personalisatie"
5. Houd kaart tegen reader
6. Volg stappen in UI
7. Check logs voor success

### Test 5: Verification
Na personalisatie:
1. Zet mode naar Machine Reader
2. Houd zelfde kaart tegen reader
3. Kijk of nieuwe key gebruikt wordt voor authenticatie

---

## 🎯 Success Criteria

✅ Code compileert zonder errors  
✅ GetVersion command werkt  
✅ Authenticatie met factory key werkt  
✅ ChangeKey command succesvol  
✅ Verificatie met nieuwe key werkt  
✅ Web UI toont progress correct  
✅ Logs zichtbaar in browser  
✅ Multiple kaarten kunnen gepersonaliseerd worden  

---

**Wanneer dit allemaal werkt, is de implementatie compleet! 🎉**
