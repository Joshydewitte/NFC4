# NTAG424 DNA - Quick Start Gids

## 🎯 Kritieke Stap: PN5180 Transceive Implementeren

Dit is de belangrijkste stap om de NTAG424 functionaliteit werkend te krijgen.

### Stap 1: Onderzoek PN5180 Library API

De library die je gebruikt: `https://github.com/tueddy/PN5180-Library.git#ISO14443`

**Zoek in de library naar:**
- `sendData()` en `readData()` methoden
- `transceiveCommand()` methode
- ISO14443-4 APDU commando functies
- Voorbeeldcode voor DESFire/NTAG424 communicatie

### Stap 2: Implementeer Transceive

Open: `src/ntag424_handler.cpp`

Zoek naar:
```cpp
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                               uint8_t* response, size_t& responseLen) {
    // TODO: Implementeer met PN5180 library's ISO14443-4 functies
    logError("Transceive not fully implemented - needs PN5180 ISO14443-4 support");
    return false;
}
```

Vervang met (voorbeeld - pas aan naar jouw library):
```cpp
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                               uint8_t* response, size_t& responseLen) {
    // Reset response length
    responseLen = 0;
    
    // Send command to card
    // Optie A: Als library sendData/readData heeft:
    if (!nfc->sendData(cmd, cmdLen)) {
        logError("Failed to send data to card");
        return false;
    }
    
    // Wait for card response (adjust timing as needed)
    delay(10);
    
    // Read response from card
    uint8_t tempBuffer[256];
    uint16_t bytesRead = 0;
    
    if (!nfc->readData(tempBuffer, &bytesRead)) {
        logError("Failed to read data from card");
        return false;
    }
    
    // Copy to response buffer
    if (bytesRead > 0) {
        memcpy(response, tempBuffer, min(bytesRead, (uint16_t)256));
        responseLen = bytesRead;
        return true;
    }
    
    return false;
    
    // OF
    
    // Optie B: Als library transceiveCommand heeft:
    // return nfc->transceiveCommand(cmd, cmdLen, response, &responseLen);
}
```

### Stap 3: Test Basis Communicatie

Maak een eenvoudige test in `main.cpp`:

```cpp
void testNTAG424Communication() {
    Serial.println("Testing NTAG424 GetVersion...");
    
    uint8_t versionInfo[28];
    if (ntag424Handler->getVersion(versionInfo)) {
        Serial.println("✅ GetVersion success!");
        Serial.print("Hardware: ");
        for (int i = 0; i < 7; i++) {
            Serial.print(versionInfo[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("❌ GetVersion failed!");
    }
}

// In setup(), na ntag424Handler initialisatie:
if (nfcReader->isCardPresent()) {
    testNTAG424Communication();
}
```

---

## 🔧 Stap 2: Integreer in Main.cpp

### 2.1 Update Global Variables

Open `src/main.cpp` en voeg toe na de andere globale variabelen:

```cpp
// ============ SYSTEM COMPONENTS ============

WiFiConfigManager wifiManager;
SystemConfig systemConfig;
ServerClient serverClient;
NFCWebServer webServer;
NFCReader* nfcReader = nullptr;
NTAG424Handler* ntag424Handler = nullptr;  // <-- NIEUW
```

### 2.2 Update Setup()

In `setup()`, na `nfcReader->begin()`:

```cpp
// Initialize NFC Reader
// ... existing code ...

if (!nfcReader->begin()) {
    // ... existing error handling ...
}

// Initialize NTAG424 Handler
Serial.println(F("\n=== NTAG424 Handler Initialization ==="));
ntag424Handler = new NTAG424Handler(nfcReader->getNFC());
ntag424Handler->setWebServer(&webServer);
Serial.println(F("✅ NTAG424 Handler ready"));

Serial.println(F("\n=================================="));
// ... rest of setup ...
```

### 2.3 Update handleConfigMode()

Vervang de functie in `main.cpp`:

```cpp
void handleConfigMode(NFCReader::CardInfo& cardInfo) {
    Serial.println(F("\n=== CONFIG MODE - NTAG424 PERSONALIZATION ==="));
    webServer.broadcastLog("🔧 Config modus - Start personalisatie", "info");
    
    // Step 1: Get master key from server
    webServer.broadcastLog("Stap 1: Haal masterkey op van server...", "info");
    
    String masterKeyHex = "";
    bool useManualKey = systemConfig.hasMasterkey();
    
    if (useManualKey) {
        // Use manual masterkey
        masterKeyHex = systemConfig.getSessionMasterkey();
        webServer.broadcastLog("Gebruik handmatige masterkey (offline)", "warning");
    } else {
        // Fetch from server
        masterKeyHex = serverClient.getCardKey(cardInfo.uidString, "master");
        
        if (masterKeyHex.length() != 32) {
            // Card not registered - register first
            webServer.broadcastLog("Kaart niet geregistreerd - registreren...", "info");
            
            bool registered = serverClient.registerCard(
                cardInfo.uidString,
                "Card_" + cardInfo.uidString.substring(0, 8),
                "Auto-registered"
            );
            
            if (!registered) {
                webServer.broadcastLog("❌ Kon kaart niet registreren", "error");
                return;
            }
            
            // Try again
            masterKeyHex = serverClient.getCardKey(cardInfo.uidString, "master");
        }
    }
    
    if (masterKeyHex.length() != 32) {
        webServer.broadcastLog("❌ Ongeldige masterkey ontvangen", "error");
        return;
    }
    
    webServer.broadcastLog("✅ Masterkey ontvangen: " + masterKeyHex.substring(0, 8) + "...", "success");
    
    // Step 2: Convert to bytes
    uint8_t newKey[16];
    size_t keyLen = NTAG424Crypto::hexStringToBytes(masterKeyHex, newKey, 16);
    
    if (keyLen != 16) {
        webServer.broadcastLog("❌ Key conversie mislukt", "error");
        return;
    }
    
    // Step 3: Change key on card
    webServer.broadcastLog("Stap 2: Schrijf masterkey naar kaart...", "info");
    
    bool success = ntag424Handler->changeKey(0, NTAG424Handler::DEFAULT_AES_KEY, newKey);
    
    if (!success) {
        webServer.broadcastLog("❌ ChangeKey mislukt - controleer kaart", "error");
        return;
    }
    
    webServer.broadcastLog("✅ Masterkey succesvol geschreven!", "success");
    
    // Step 4: Verify by authenticating with new key
    webServer.broadcastLog("Stap 3: Verificatie met nieuwe key...", "info");
    
    NTAG424Handler::AuthResult authResult;
    bool verified = ntag424Handler->authenticateEV2First(0, newKey, authResult);
    
    if (verified) {
        webServer.broadcastLog("✅ Verificatie succesvol - kaart gereed!", "success");
        Serial.println(F("✅ Card personalization complete!"));
    } else {
        webServer.broadcastLog("⚠️ Verificatie mislukt - key mogelijk niet correct geschreven", "warning");
        Serial.println(F("⚠️ Verification failed - key may not be written correctly"));
    }
}
```

### 2.4 Update NFCReader Header

Open `include/nfc_reader.h` en voeg toe in de public sectie:

```cpp
public:
    // ... existing public methods ...
    
    // Get PN5180 instance for advanced operations (NTAG424)
    PN5180ISO14443* getNFC() { return nfc; }
```

---

## 📝 Stap 3: Implementeer CRC32

Open `include/ntag424_crypto.h` en voeg toe:

```cpp
/**
 * Calculate CRC32 (for NTAG424 ChangeKey)
 * @param data - Data to calculate CRC over
 * @param length - Length of data
 * @return CRC32 value
 */
static uint32_t calculateCRC32(const uint8_t* data, size_t length);
```

Open `src/ntag424_crypto.cpp` en voeg implementatie toe:

```cpp
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

Open `src/ntag424_handler.cpp` en vervang in `changeKey()`:

```cpp
// OLD:
uint32_t crc = 0xFFFFFFFF; // Simplified placeholder

// NEW:
uint32_t crc = NTAG424Crypto::calculateCRC32(newKey, 16);
```

---

## 🧪 Stap 4: Test de Implementatie

### Test 1: Compileren
```bash
pio run
```

Verwachte output: Geen fouten

### Test 2: Upload naar ESP32
```bash
pio run --target upload --upload-port COMx
```

### Test 3: Monitor Serial Output
```bash
pio device monitor
```

### Test 4: Web Interface Test

1. Open browser: `http://<ESP32-IP>/`
2. Login met admin account
3. Ga naar Settings
4. Zet Reader Mode naar "Config Reader"
5. Klik "Start Kaart Personalisatie"
6. Houd NTAG424 DNA kaart tegen reader
7. Kijk naar live logs en progress indicator

### Test 5: Verificatie

Na succesvolle personalisatie:

1. Zet Reader Mode naar "Machine Reader"
2. Settings > Server URL instellen
3. Houd zelfde kaart tegen reader
4. Controleer of authenticatie slaagt

---

## 🐛 Troubleshooting

### Probleem: "Transceive not fully implemented"

**Oplossing:** Stap 1 nog niet gedaan - implementeer `transceive()` methode

### Probleem: "Failed to send auth command"

**Mogelijke oorzaken:**
- Kaart is geen NTAG424 DNA
- PN5180 niet correct geïnitialiseerd
- Kaart te ver van reader
- RF field niet actief

**Check:**
```cpp
// In main.cpp, na kaart detectie:
if (cardInfo.cardType.indexOf("NTAG424") < 0) {
    webServer.broadcastLog("⚠️ Geen NTAG424 DNA kaart gedetecteerd", "warning");
    return;
}
```

### Probleem: "Authentication failed: RndA' mismatch"

**Mogelijke oorzaken:**
- Verkeerde key gebruikt
- Kaart al gepersonaliseerd met andere key
- AES encryptie/decryptie fout

**Oplossing:**
1. Test eerst met DEFAULT_AES_KEY (00...00)
2. Check AES implementatie met test vectors
3. Verify RndA/RndB generatie

### Probleem: "ChangeKey returned error status"

**Mogelijke oorzaken:**
- Niet geauthenticeerd voor ChangeKey
- CRC32 incorrect
- CMAC incorrect
- Wrong key data format

**Debug:**
Voeg toe in `changeKey()`:
```cpp
Serial.println("Key Data: " + NTAG424Crypto::bytesToHexString(keyData, 16));
Serial.println("CRC32: 0x" + String(crc, HEX));
Serial.println("CMAC: " + NTAG424Crypto::bytesToHexString(mac, 8));
```

---

## 📚 Nuttige Serial Debug Commands

Voeg toe aan `main.cpp` voor debugging:

```cpp
void serialMenu() {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch(cmd) {
            case 't':
                // Test NTAG424 GetVersion
                testNTAG424Communication();
                break;
                
            case 'a':
                // Test Authentication with default key
                NTAG424Handler::AuthResult result;
                bool auth = ntag424Handler->authenticateEV2First(
                    0, NTAG424Handler::DEFAULT_AES_KEY, result
                );
                Serial.println(auth ? "Auth OK" : "Auth FAILED");
                break;
                
            case 'r':
                // Reset/reboot
                ESP.restart();
                break;
        }
    }
}

// In loop():
serialMenu();
```

---

## ✅ Checklist Voltooiing

- [ ] PN5180 library API bestudeerd
- [ ] `transceive()` geïmplementeerd
- [ ] Code compileert zonder errors
- [ ] NTAG424Handler geïntegreerd in main.cpp
- [ ] `getNFC()` toegevoegd aan NFCReader
- [ ] CRC32 berekening geïmplementeerd
- [ ] Test GetVersion command werkt
- [ ] Test authenticatie met factory key werkt
- [ ] Config mode workflow compleet
- [ ] Eerste kaart succesvol gepersonaliseerd
- [ ] Verificatie met nieuwe key werkt
- [ ] Web UI toont correct progress
- [ ] Machine mode test met nieuwe kaart

---

## 🎉 Klaar!

Als alle stappen gelukt zijn, heb je een volledig werkend NTAG424 DNA personalisatie systeem!

**Volgende stappen:**
1. Test met meerdere kaarten
2. Implementeer batch mode
3. Voeg credit/debit keys toe (Key 1, 2)
4. Implementeer machine mode challenge-response

**Vragen of problemen?**
Check `NTAG424_IMPLEMENTATION.md` voor gedetailleerde technische documentatie.

---

**Succes! 🚀**
