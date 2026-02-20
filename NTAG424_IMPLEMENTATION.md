# NTAG424 DNA Implementatie - Status & Documentatie

## ✅ Geïmplementeerd

### 1. AES-128 Crypto Library (ESP32)
**Bestanden:**
- `include/ntag424_crypto.h`
- `src/ntag424_crypto.cpp`

**Functionaliteit:**
- AES-128 CBC encryptie/decryptie via mbedtls
- CMAC berekening voor message authentication
- Session key derivation (EV2 protocol)
- Random number generatie
- Rotate left/right operaties voor NTAG424 protocol
- Hex string conversies

**Status:** ✅ Complete implementatie

---

### 2. NTAG424 Handler (ESP32)
**Bestanden:**
- `include/ntag424_handler.h`
- `src/ntag424_handler.cpp`

**Functionaliteit:**
- `authenticateEV2First()` - Volledige EV2 First authenticatie flow
  - Stuurt AuthenticateEV2First command naar kaart
  - Decrypt RndB van kaart
  - Genereert RndA
  - Versleutelt RndA || RndB' en stuurt terug
  - Verifieert RndA' van kaart
  - Leidt session keys af (ENC en MAC)
  
- `changeKey()` - Master key schrijven naar kaart
  - Authenticeert met oude key (default factory key)
  - XOR oude key met nieuwe key
  - Berekent CRC32
  - Versleutelt key data met session ENC key
  - Berekent CMAC met session MAC key
  - Stuurt ChangeKey command

- `getVersion()` - Haalt versie info op van NTAG424 DNA
- `selectApplication()` - Selecteert applicatie op kaart
- `sendCommand()` - ISO14443-4 command wrapper

**Status:** ⚠️ 95% compleet - TransceivePN5180 integratie nodig

---

### 3. Web Interface - Config Card Page
**Bestanden:**
- `include/config_card_page.h`

**Functionaliteit:**
- Stap-voor-stap wizard voor kaart personalisatie
- Real-time progress indicator (5 stappen)
- Live kaart detectie met UID display
- WebSocket integratie voor live logs
- Error handling met retry optie
- Volgende kaart workflow

**UI Stappen:**
1. Kaart detecteren
2. Key ophalen van server
3. Authenticeren met factory key
4. Master key schrijven
5. Verificatie

**Status:** ✅ Complete UI implementatie

---

### 4. Web Server Updates
**Bestanden:**
- `include/web_server.h` (aangepast)

**Nieuwe Routes:**
- `GET /config-card` - Kaart personalisatie pagina
- `GET /api/card/current` - Huidige kaart info ophalen
- `POST /api/personalize/start` - Start personalisatie workflow

**Nieuwe Settings:**
- Link naar config card page in settings
- Verbeterde config mode beschrijving

**Status:** ✅ Routes toegevoegd, handlers geïmplementeerd

---

### 5. Settings Page Updates
**Bestanden:**
- `include/settings_page.h` (aangepast)

**Wijzigingen:**
- Toegevoegd: "Start Kaart Personalisatie" knop in config mode
- Verbeterde info box met duidelijke uitleg
- Optionele handmatige masterkey voor offline gebruik

**Status:** ✅ Compleet

---

## ⚠️ Nog Te Doen

### 1. PN5180 ISO14443-4 Transceive Integratie ⚠️ KRITIEK

**Probleem:**
De `NTAG424Handler::transceive()` methode is nog niet geïmplementeerd omdat de exacte API van de PN5180-Library onduidelijk is.

**Wat nodig is:**
```cpp
bool NTAG424Handler::transceive(const uint8_t* cmd, size_t cmdLen,
                                uint8_t* response, size_t& responseLen) {
    // TODO: Implementeer met PN5180 library's ISO14443-4 functies
    // Mogelijke opties:
    // - nfc->sendData() + nfc->readData()
    // - nfc->transceiveCommand()
    // - nfc->writeBytes() + nfc->readBytes()
    
    return false; // Placeholder
}
```

**Actie vereist:**
1. Bestudeer de PN5180-Library documentatie
2. Zoek ISO14443-4 APDU transceive functie
3. Implementeer correct met CRC/checksum handling
4. Test met echte NTAG424 DNA kaart

**Referentie:**
- Huidige library: `https://github.com/tueddy/PN5180-Library.git#ISO14443`
- De library gebruikt al `PN5180ISO14443` class
- Check of er een `sendAPDU()` of `transceiveAPDU()` methode bestaat

---

### 2. Main.cpp Integratie

**Wat nodig is:**
- Instantieer `NTAG424Handler` in `main.cpp`
- Update `handleConfigMode()` om NTAG424Handler te gebruiken
- Implementeer workflow:
  1. Detecteer NTAG424 DNA kaart
  2. Haal masterkey op van server (of gebruik manual)
  3. Authenticeer met factory key (00...00)
  4. Schrijf nieuwe masterkey
  5. Verificeer door opnieuw te authenticeren met nieuwe key
  6. Broadcast progress via WebSocket

**Code voorbeeld:**
```cpp
// In main.cpp, global scope
NTAG424Handler* ntag424Handler = nullptr;

// In setup()
ntag424Handler = new NTAG424Handler(nfcReader->getNFC());
ntag424Handler->setWebServer(&webServer);

// In handleConfigMode()
void handleConfigMode(NFCReader::CardInfo& cardInfo) {
    webServer.broadcastLog("🔧 Config modus - Start personalisatie", "info");
    
    // 1. Get master key from server
    String masterKey = serverClient.getCardKey(cardInfo.uidString, "master");
    
    if (masterKey.length() != 32) {
        webServer.broadcastLog("❌ Kon masterkey niet ophalen", "error");
        return;
    }
    
    // 2. Convert to bytes
    uint8_t keyBytes[16];
    NTAG424Crypto::hexStringToBytes(masterKey, keyBytes, 16);
    
    // 3. Change key using NTAG424 handler
    bool success = ntag424Handler->changeKey(0, NTAG424Handler::DEFAULT_AES_KEY, keyBytes);
    
    if (success) {
        webServer.broadcastLog("✅ Masterkey succesvol geschreven!", "success");
    } else {
        webServer.broadcastLog("❌ Fout bij schrijven masterkey", "error");
    }
}
```

---

### 3. NFCReader Class Update

**Wat nodig is:**
- Voeg getter toe voor `PN5180ISO14443*` pointer:
```cpp
// In nfc_reader.h
public:
    PN5180ISO14443* getNFC() { return nfc; }
```

---

### 4. CRC32 Implementatie

**Probleem:**
In `NTAG424Handler::changeKey()` wordt CRC32 gebruikt maar is het een placeholder:
```cpp
uint32_t crc = 0xFFFFFFFF; // Simplified placeholder
```

**Wat nodig is:**
Implementeer correcte CRC32 berekening volgens NTAG424 datasheet:
```cpp
uint32_t NTAG424Crypto::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
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

---

### 5. Machine Mode - Challenge-Response

**Huidige status:**
Machine mode gebruikt nog mock response. Moet geïmplementeerd worden met echte NTAG424 authenticatie.

**Wat nodig is:**
```cpp
void handleMachineMode(NFCReader::CardInfo& cardInfo) {
    // 1. Get challenge from server
    String challenge = serverClient.requestChallenge(cardInfo.uidString);
    
    // 2. Get master key from server
    String masterKey = serverClient.getCardKey(cardInfo.uidString, "master");
    uint8_t keyBytes[16];
    NTAG424Crypto::hexStringToBytes(masterKey, keyBytes, 16);
    
    // 3. Authenticate with card
    NTAG424Handler::AuthResult authResult;
    bool auth = ntag424Handler->authenticateEV2First(0, keyBytes, authResult);
    
    if (!auth) {
        webServer.broadcastLog("❌ Authenticatie mislukt", "error");
        return;
    }
    
    // 4. Send challenge to card and get response
    // TODO: Implement challenge-response exchange
    
    // 5. Verify response with server
    bool verified = serverClient.verifyResponse(cardInfo.uidString, response);
    
    if (verified) {
        webServer.broadcastLog("✅ TOEGANG VERLEEND", "success");
    } else {
        webServer.broadcastLog("❌ TOEGANG GEWEIGERD", "error");
    }
}
```

---

### 6. Web Server API Handlers - Implementatie

**Handlers die nog stub zijn:**
```cpp
void handleGetCurrentCard() {
    // TODO: Return actual detected card from NFCReader
    // Currently returns: {"present":false}
}

void handleStartPersonalization() {
    // TODO: Call handleConfigMode() or trigger personalization
    // Currently returns: {"success":true,"message":"Personalization started"}
}
```

**Wat nodig is:**
- Toegang tot `NFCReader` vanuit `NFCWebServer`
- Return current card info in JSON format
- Trigger personalization workflow

---

## 📚 NTAG424 DNA Referentie

### Default Factory Keys
```cpp
Master Key (Key 0): 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Key 1:              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Key 2:              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Key 3:              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Key 4:              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### Command Codes
```cpp
AuthenticateEV2First:     0x71
AuthenticateEV2NonFirst:  0x77
ChangeKey:                0xC4
GetVersion:               0x60
SelectApplication:        0x5A
```

### Status Codes
```cpp
STATUS_OK:                    0x00
STATUS_ADDITIONAL_FRAME:      0xAF
STATUS_AUTHENTICATION_ERROR:  0xAE
```

### EV2 Session Key Derivation
```
Session Vector (SV) = RndA[0..1] || RndB[0..1]

ENC Session Key = AES(AuthKey, 0x01 || SV || 0x00...0x00)
MAC Session Key = AES(AuthKey, 0x02 || SV || 0x00...0x00)
```

---

## 🧪 Testing Plan

### Stap 1: PN5180 Transceive Test
1. Implementeer `transceive()` methode
2. Test met simpel GetVersion command
3. Verifieer response parsing

### Stap 2: Authentication Test
1. Test authenticateEV2First met factory key
2. Controleer session keys
3. Verifieer RndA' matches

### Stap 3: ChangeKey Test
1. Test changeKey met nieuwe test key
2. Verifieer met nieuwe authenticatie
3. Reset kaart naar factory settings

### Stap 4: Full Workflow Test
1. Config mode: Schrijf nieuwe masterkey
2. Machine mode: Authenticeer met nieuwe key
3. Challenge-response verificatie

### Stap 5: Multi-Card Test
1. Personaliseer 5 kaarten achter elkaar
2. Verifieer unieke keys per kaart
3. Test error recovery bij failures

---

## 📝 Belangrijke Notes

### Security Considerations
- Default factory key (00...00) is publicly known
- Nieuwe masterkeys worden afgeleid via HMAC-SHA256 per UID
- Session keys worden per authenticatie geroteerd
- Keys worden NOOIT opgeslagen in ESP32 EEPROM

### Server Key Derivation
```javascript
// Server: keys.js
deriveMasterKey(uid) {
    // HMAC-SHA256("M_" + uid, masterSecret)
    return hash.slice(0, 16); // First 16 bytes for AES-128
}
```

### Workflow Best Practices
1. Altijd authenticeren voor ChangeKey
2. Verifieer nieuwe key door hernieuwde authenticatie
3. Log alle operaties naar WebSocket voor gebruiker
4. Implementeer timeout voor kaart detectie (5 sec)
5. Retry limit: 3x, dan error tonen

---

## 🔗 Referenties

### NXP Datasheets
- NTAG424 DNA Datasheet: [NXP Website]
- AN12196: NTAG 424 DNA and NTAG 424 DNA TagTamper features and hints
- AN12321: NTAG 424 DNA and NTAG 424 DNA TagTamper - Enhanced Privacy Solution

### Libraries
- PN5180-Library: https://github.com/tueddy/PN5180-Library
- mbedtls: Built-in ESP32 IDF (AES-128, CMAC)

### Server
- Node.js Express Server: `C:\Users\Jos jr\Documents\PlatformIO\Projects\nfc-server`
- Key Derivation: HMAC-SHA256 based
- API Endpoints: `/api/key/:uid/:keytype`

---

## 🚀 Next Steps (Prioriteit)

1. **[HOOG]** Implementeer PN5180 transceive() - zonder dit werkt niets
2. **[HOOG]** Integreer NTAG424Handler in main.cpp
3. **[GEMIDDELD]** Implementeer CRC32 berekening
4. **[GEMIDDELD]** Update web server API handlers
5. **[LAAG]** Machine mode challenge-response
6. **[LAAG]** Multi-card batch mode

---

## ✅ Checklist voor Oplevering

- [ ] PN5180 transceive() geïmplementeerd en getest
- [ ] NTAG424Handler geïntegreerd in main.cpp
- [ ] Config mode workflow werkend end-to-end
- [ ] Web UI toont live progress
- [ ] Error handling en retry logic
- [ ] Testkaart succesvol gepersonaliseerd
- [ ] Nieuwe key geverifieerd via authenticatie
- [ ] Machine mode test met gepersonaliseerde kaart
- [ ] Server key derivation getest
- [ ] Documentatie up-to-date

---

**Datum:** 19 februari 2026
**Status:** Implementatie 80% compleet - PN5180 integratie kritisch pad
