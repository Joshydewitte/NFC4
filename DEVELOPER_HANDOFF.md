# NTAG424 DNA - Handoff

**Status:** Code compileert. 3 stappen nodig.

---

## SYSTEEM

ESP32S3 + PN5180 → NTAG424 DNA kaarten met AES-128.

**Modi:**
- Machine: Kaarten scannen
- Config: Master keys schrijven

**Stack:** main.cpp → NTAG424Handler → PN5180ISO14443
**Crypto:** NTAG424Crypto (AES/CMAC)

---

## DONE

✅ AES/CMAC/session keys (`ntag424_crypto.cpp`)  
✅ EV2 auth protocol (`ntag424_handler.cpp`)  
✅ ChangeKey command  
✅ Web UI `/config-card`  
✅ Node.js server (port 3000)

---

## TODO - 3 STAPPEN

### STAP 1: Implementeer transceive()

**Bestand:** `src/ntag424_handler.cpp` regel ~160  
**Functie:** `bool NTAG424Handler::sendCommand()`

**Code:**
```cpp
bool NTAG424Handler::sendCommand(const uint8_t* cmd, size_t cmdLen, 
                                  uint8_t* response, size_t& responseLen) {
    uint8_t* result = nullptr;
    uint16_t resultLen = 0;
    
    ISO14443ErrorCode err = nfc->issueISO14443Command(
        (uint8_t*)cmd, cmdLen, &result, &resultLen
    );
    
    if (err != ISO14443_ERROR_SUCCESS) {
        logError("ISO14443 error: " + String((int)err));
        return false;
    }
    
    memcpy(response, result, min(responseLen, (size_t)resultLen));
    responseLen = resultLen;
    return true;
}
```

**Test:** `getVersion()` en `selectApplication()`

---

### STAP 2: Integreer in main.cpp

**A) Global** (na regel ~40):
```cpp
NTAG424Handler* ntag424Handler = nullptr;
```

**B) getNFC()** in `include/nfc_reader.h`:
```cpp
PN5180ISO14443* getNFC() { return &nfc14443; }
```

**C) Setup()** (na regel ~100):
```cpp
ntag424Handler = new NTAG424Handler(nfcReader->getNFC());
ntag424Handler->setWebServer(&webServer);
```

**D) handleConfigMode()** (regel ~200):
```cpp
void handleConfigMode(NFCReader::CardInfo& cardInfo) {
    // 1. getVersion() - check NTAG424
    // 2. serverClient.getCardKey() - haal key op
    // 3. authenticateEV2First() - auth met factory key
    // 4. changeKey() - schrijf nieuwe key
    // 5. Verify met nieuwe key
    // 6. serverClient.registerCard()
    
    // Zie CODE_SNIPPETS_NTAG424.md
}
```

---

### STAP 3: Test

1. Upload: `pio run --target upload --upload-port COM9`
2. Server: `cd nfc-server && node server.js`
3. Config mode: Hou button 3 sec ingedrukt
4. Browser: `http://[ESP32-IP]/config-card`
5. Kaart: Leg factory NTAG424 DNA op reader
6. Start: Klik "Start Personalisatie"
7. Check: Logs moeten authenticate → write → verify tonen

---

## REFERENTIE

**Docs:**
- `CODE_SNIPPETS_NTAG424.md` - Klaar om te copy-pasten
- `NTAG424_IMPLEMENTATION.md` - Protocol details

**Code:**
- `src/ntag424_handler.cpp` regel ~160
- `include/nfc_reader.h` regel ~50
- `src/main.cpp` regel ~100 & ~200

**Extra:** CRC32 staat in CODE_SNIPPETS_NTAG424.md

---

## CHECKLIST

- [ ] sendCommand() werkt
- [ ] getNFC() toegevoegd
- [ ] Handler in main.cpp
- [ ] handleConfigMode() af
- [ ] Kaart gepersonaliseerd via UI