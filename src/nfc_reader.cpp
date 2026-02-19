#include "nfc_reader.h"
#include "web_server.h"

NFCReader::NFCReader(const PinConfig& pinConfig) 
    : pins(pinConfig), 
      webServer(nullptr),
      initialized(false),
      cardWasPresent(false),
      cardShown(false),
      consecutiveErrors(0),
      lastSuccessfulRead(0),
      lastPollCheck(0),
      errorFlag(false) {
    
    nfc = new PN5180ISO14443(pins.nss, pins.busy, pins.rst);
}

NFCReader::~NFCReader() {
    delete nfc;
}

bool NFCReader::begin() {
    Serial.println(F("\n=== Initialiseer PN5180 NFC Reader ==="));
    
    initializePins();
    configureSPI();
    
    Serial.println(F("Starting PN5180..."));
    nfc->begin();
    
    // Reconfigure SPI after nfc.begin()
    configureSPI();
    
    Serial.println(F("Performing hard reset..."));
    nfc->reset();
    delay(100);
    
    if (!verifyDevice()) {
        Serial.println(F("❌ PN5180 verification failed!"));
        logToWeb("PN5180 initialisatie mislukt!", "error");
        return false;
    }
    
    printDeviceInfo();
    
    Serial.println(F("Enabling RF field..."));
    nfc->setupRF();
    
    lastSuccessfulRead = millis();
    lastPollCheck = millis();
    initialized = true;
    
    Serial.println(F("✅ PN5180 initialized successfully"));
    logToWeb("PN5180 NFC Reader geïnitialiseerd", "success");
    
    return true;
}

void NFCReader::initializePins() {
    pinMode(pins.nss, OUTPUT);
    digitalWrite(pins.nss, HIGH);
    pinMode(pins.busy, INPUT);
    pinMode(pins.rst, OUTPUT);
    digitalWrite(pins.rst, HIGH);
    
    Serial.print(F("Pin Config: NSS=GPIO"));
    Serial.print(pins.nss);
    Serial.print(F(" BUSY=GPIO"));
    Serial.print(pins.busy);
    Serial.print(F(" RST=GPIO"));
    Serial.println(pins.rst);
}

void NFCReader::configureSPI() {
#if defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)
    Serial.print(F("Configuring SPI: SCK=GPIO"));
    Serial.print(pins.sck);
    Serial.print(F(" MISO=GPIO"));
    Serial.print(pins.miso);
    Serial.print(F(" MOSI=GPIO"));
    Serial.println(pins.mosi);
    SPI.begin(pins.sck, pins.miso, pins.mosi);
#elif defined(ARDUINO_ARCH_ESP32)
    Serial.println(F("Using default ESP32 SPI pins"));
    SPI.begin();
#endif
}

bool NFCReader::verifyDevice() {
    uint8_t productVersion[2];
    if (!nfc->readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion))) {
        return false;
    }
    
    if (productVersion[1] == 0xFF) {
        return false;
    }
    
    return true;
}

void NFCReader::printDeviceInfo() {
    uint8_t productVersion[2];
    nfc->readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
    Serial.print(F("Product version: "));
    Serial.print(productVersion[1]);
    Serial.print(".");
    Serial.println(productVersion[0]);
    
    uint8_t firmwareVersion[2];
    nfc->readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
    Serial.print(F("Firmware version: "));
    Serial.print(firmwareVersion[1]);
    Serial.print(".");
    Serial.println(firmwareVersion[0]);
    
    uint8_t eepromVersion[2];
    nfc->readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
    Serial.print(F("EEPROM version: "));
    Serial.print(eepromVersion[1]);
    Serial.print(".");
    Serial.println(eepromVersion[0]);
}

void NFCReader::loop() {
    if (!initialized) return;
    
    unsigned long currentMillis = millis();
    
    // Periodic health check
    if (currentMillis - lastPollCheck >= POLL_INTERVAL) {
        lastPollCheck = currentMillis;
        
        static uint8_t healthFailCount = 0;
        
        if (!checkHealth()) {
            healthFailCount++;
            
            if (healthFailCount % 10 == 1) {
                logToWeb("⚠️ PN5180 health check failed (x" + String(healthFailCount) + ")", "error");
            }
            
            reset();
            return;
        } else {
            if (healthFailCount > 0) {
                logToWeb("✅ PN5180 health check hersteld na " + String(healthFailCount) + " failures", "success");
                healthFailCount = 0;
            }
            lastSuccessfulRead = currentMillis;
        }
    }
    
    // Watchdog timeout check
    if (lastSuccessfulRead > 0 && (currentMillis - lastSuccessfulRead) >= MAX_NO_RESPONSE) {
        Serial.println(F("*** PN5180 watchdog timeout"));
        logToWeb("PN5180 watchdog timeout - reset", "error");
        reset();
        return;
    }
    
    // Handle errors
    if (errorFlag) {
        uint32_t irqStatus = nfc->getIRQStatus();
        handleError(irqStatus);
        errorFlag = false;
        delay(10);
        return;
    }
}

bool NFCReader::isCardPresent() {
    if (!initialized) return false;
    return nfc->isCardPresent();
}

bool NFCReader::readCard(CardInfo& cardInfo) {
    if (!initialized) return false;
    
    cardInfo.isPresent = nfc->isCardPresent();
    
    if (!cardInfo.isPresent) {
        if (cardWasPresent) {
            Serial.println(F("Card removed"));
            logToWeb("Kaart verwijderd", "info");
            cardShown = false;
            consecutiveErrors = 0;
            lastSuccessfulRead = millis();
        }
        cardWasPresent = false;
        return false;
    }
    
    cardWasPresent = true;
    
    if (!nfc->readCardSerial(cardInfo.uid)) {
        Serial.println(F("Error reading card UID"));
        logToWeb("❌ Fout bij lezen kaart UID", "error");
        errorFlag = true;
        return false;
    }
    
    // Success - reset error counter
    consecutiveErrors = 0;
    lastSuccessfulRead = millis();
    
    // Build UID string
    cardInfo.uidString = "";
    for (int i = 0; i < 7; i++) {
        if (cardInfo.uid[i] < 0x10) cardInfo.uidString += "0";
        cardInfo.uidString += String(cardInfo.uid[i], HEX);
        if (i < 6) cardInfo.uidString += ":";
    }
    
    // Determine card type
    cardInfo.cardType = getCardTypeDescription(cardInfo.uid);
    cardInfo.isSecure = (cardInfo.cardType.indexOf("SECURE") >= 0 || 
                         cardInfo.cardType.indexOf("DESFire") >= 0 ||
                         cardInfo.cardType.indexOf("NTAG424") >= 0);
    
    // Log only once per card presence
    if (!cardShown) {
        Serial.print(F("Card detected: "));
        Serial.println(cardInfo.uidString);
        Serial.print(F("Type: "));
        Serial.println(cardInfo.cardType);
        
        logToWeb("🔖 Kaart: " + cardInfo.uidString, "info");
        logToWeb("Type: " + cardInfo.cardType, "info");
        
        cardShown = true;
    }
    
    return true;
}

String NFCReader::getCardTypeDescription(const uint8_t* uid) const {
    bool is7ByteUID = detectCard7ByteUID(uid);
    
    Serial.print(F("  [DEBUG] UID[0]=0x"));
    Serial.print(uid[0], HEX);
    Serial.print(F(", 7-byte UID: "));
    Serial.println(is7ByteUID ? "YES" : "NO");
    
    // Detection based on UID patterns
    if (uid[0] == 0x04 && is7ByteUID) {
        return "DESFire/NTAG424 (SECURE)";
    }
    else if (uid[0] == 0x08 && !is7ByteUID) {
        return "Secure document (Passport/ID/License)";
    }
    else if (!is7ByteUID) {
        if ((uid[0] & 0xF0) == 0x00) {
            return "Mifare Classic (unsecure)";
        }
        return "NTAG/Ultralight (unsecure)";
    }
    
    return "Unknown card type";
}

bool NFCReader::detectCard7ByteUID(const uint8_t* uid) const {
    return (uid[6] != 0x00 || uid[5] != 0x00);
}

bool NFCReader::checkHealth() {
    if (!initialized) return false;
    
    return checkIRQStatus() && checkEEPROM();
}

bool NFCReader::checkIRQStatus() {
    uint32_t irqStatus = nfc->getIRQStatus();
    
    static unsigned long lastErrorLog = 0;
    const unsigned long ERROR_LOG_INTERVAL = 5000;
    bool shouldLog = (millis() - lastErrorLog) > ERROR_LOG_INTERVAL;
    
    // Test 1: SPI communication
    if (irqStatus == 0xFFFFFFFF) {
        if (shouldLog) {
            Serial.println(F("*** PN5180 SPI not responding!"));
            logToWeb("❌ PN5180 SPI reageert niet!", "error");
            lastErrorLog = millis();
        }
        return false;
    }
    
    // Test 2: Known problematic values
    if (irqStatus == 0xE4FB44CA) {
        if (shouldLog) {
            Serial.println(F("*** PN5180 problematic IRQ status!"));
            logToWeb("❌ PN5180 problematische IRQ status", "error");
            lastErrorLog = millis();
        }
        return false;
    }
    
    // Test 3: Healthy/normal status values
    if (irqStatus == 0x24006 || irqStatus == 0x24007 || 
        irqStatus == 0x4006 || irqStatus == 0x4007) {
        return true; // All OK
    }
    
    // Test 4: Critical error flags (excluding normal values)
    if (irqStatus & ((1<<16) | (1<<18))) { // TEMPSENS_ERROR | HV_ERROR
        if (shouldLog) {
            Serial.print(F("*** Critical error flags: 0x"));
            Serial.println(irqStatus, HEX);
            logToWeb("❌ PN5180 kritieke error: 0x" + String(irqStatus, HEX), "error");
            showIRQStatus(irqStatus);
            lastErrorLog = millis();
        }
        return false;
    }
    
    // Test 5: GENERAL_ERROR + RF_ACTIVE_ERROR combination
    if ((irqStatus & (1<<17)) && (irqStatus & (1<<10))) {
        if (shouldLog) {
            Serial.println(F("*** GENERAL_ERROR + RF_ACTIVE_ERROR"));
            logToWeb("❌ PN5180 general + RF error", "error");
            showIRQStatus(irqStatus);
            lastErrorLog = millis();
        }
        return false;
    }
    
    // Test 6: Stuck at zero detection
    static uint8_t zeroCount = 0;
    static uint32_t lastIrqStatus = 0;
    
    if (irqStatus == 0x00000000 && lastIrqStatus == 0x00000000) {
        zeroCount++;
        if (zeroCount > 3) {
            static unsigned long lastZeroLog = 0;
            if ((millis() - lastZeroLog) > 5000) {
                Serial.println(F("*** PN5180 stuck at zero!"));
                logToWeb("❌ PN5180 stuck - IRQ blijft 0x00", "error");
                lastZeroLog = millis();
            }
            zeroCount = 0;
            return false;
        }
    } else {
        zeroCount = 0;
    }
    lastIrqStatus = irqStatus;
    
    return true;
}

bool NFCReader::checkEEPROM() {
    static uint8_t eepromCheckCounter = 0;
    
    if (++eepromCheckCounter >= 5) {
        eepromCheckCounter = 0;
        
        uint8_t testByte[1];
        if (!nfc->readEEprom(PRODUCT_VERSION, testByte, 1)) {
            static unsigned long lastLog = 0;
            if ((millis() - lastLog) > 5000) {
                Serial.println(F("*** EEPROM read failed!"));
                logToWeb("❌ PN5180 EEPROM niet leesbaar", "error");
                lastLog = millis();
            }
            return false;
        }
        
        if (testByte[0] == 0xFF) {
            static unsigned long lastLog = 0;
            if ((millis() - lastLog) > 5000) {
                Serial.println(F("*** EEPROM corrupted (0xFF)!"));
                logToWeb("❌ PN5180 EEPROM corrupt", "error");
                lastLog = millis();
            }
            return false;
        }
    }
    
    return true;
}

void NFCReader::reset() {
    Serial.println(F("🔄 PN5180 hard reset..."));
    logToWeb("🔄 PN5180 reset...", "warning");
    
    nfc->reset();
    delay(100);
    
    // Verify reset
    uint8_t productVersion[2];
    nfc->readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
    
    if (productVersion[1] == 0xFF) {
        Serial.println(F("*** Reset failed - rebooting ESP32..."));
        logToWeb("❌ Reset mislukt - ESP32 herstarten...", "error");
        Serial.flush();
        delay(1000);
        ESP.restart();
    }
    
    nfc->setupRF();
    logToWeb("✅ PN5180 reset succesvol", "success");
    
    lastSuccessfulRead = millis();
    consecutiveErrors = 0;
    errorFlag = false;
}

void NFCReader::handleError(uint32_t irqStatus) {
    showIRQStatus(irqStatus);
    
    if ((irqStatus & RX_SOF_DET_IRQ_STAT) == 0) {
        Serial.println(F("*** No card detected in error state"));
        logToWeb("⚠️ Geen kaart in error state", "warning");
    }
    
    recoverFromError(irqStatus);
    
    consecutiveErrors++;
    
    if (consecutiveErrors >= REBOOT_ERRORS) {
        Serial.println(F("*** Too many errors - rebooting ESP32..."));
        logToWeb("❌ Te veel fouten - ESP32 herstarten...", "error");
        Serial.flush();
        delay(1000);
        ESP.restart();
    }
    
    if (consecutiveErrors >= MAX_ERRORS) {
        Serial.println(F("*** Too many errors - waiting..."));
        logToWeb("⚠️ Te veel fouten - wacht 2 sec...", "warning");
        delay(2000);
    }
}

void NFCReader::recoverFromError(uint32_t irqStatus) {
    // Clear IRQ status
    nfc->clearIRQStatus(0xFFFFFFFF);
    delay(10);
    
    // Check for collision
    if (irqStatus & (1<<10)) { // RF_ACTIVE_ERROR
        Serial.println(F("*** Collision detected"));
        logToWeb("⚠️ Collision - meerdere kaarten?", "warning");
    }
    
    logToWeb("🔄 PN5180 herstel...", "warning");
    nfc->reset();
    delay(50);
    nfc->setupRF();
}

void NFCReader::showIRQStatus(uint32_t irqStatus) {
    Serial.print(F("IRQ-Status 0x"));
    Serial.print(irqStatus, HEX);
    Serial.print(": [ ");
    
    String irqMsg = "IRQ: 0x" + String(irqStatus, HEX) + " [";
    
    if (irqStatus & (1<< 0)) { Serial.print(F("RQ ")); irqMsg += "RQ "; }
    if (irqStatus & (1<< 1)) { Serial.print(F("TX ")); irqMsg += "TX "; }
    if (irqStatus & (1<< 2)) { Serial.print(F("IDLE ")); irqMsg += "IDLE "; }
    if (irqStatus & (1<< 3)) { Serial.print(F("MODE_DETECTED ")); irqMsg += "MODE_DETECTED "; }
    if (irqStatus & (1<< 4)) { Serial.print(F("CARD_ACTIVATED ")); irqMsg += "CARD_ACTIVATED "; }
    if (irqStatus & (1<< 5)) { Serial.print(F("STATE_CHANGE ")); irqMsg += "STATE_CHANGE "; }
    if (irqStatus & (1<< 6)) { Serial.print(F("RFOFF_DET ")); irqMsg += "RFOFF_DET "; }
    if (irqStatus & (1<< 7)) { Serial.print(F("RFON_DET ")); irqMsg += "RFON_DET "; }
    if (irqStatus & (1<< 8)) { Serial.print(F("TX_RFOFF ")); irqMsg += "TX_RFOFF "; }
    if (irqStatus & (1<< 9)) { Serial.print(F("TX_RFON ")); irqMsg += "TX_RFON "; }
    if (irqStatus & (1<<10)) { Serial.print(F("RF_ACTIVE_ERROR ")); irqMsg += "RF_ACTIVE_ERROR "; }
    if (irqStatus & (1<<11)) { Serial.print(F("TIMER0 ")); irqMsg += "TIMER0 "; }
    if (irqStatus & (1<<12)) { Serial.print(F("TIMER1 ")); irqMsg += "TIMER1 "; }
    if (irqStatus & (1<<13)) { Serial.print(F("TIMER2 ")); irqMsg += "TIMER2 "; }
    if (irqStatus & (1<<14)) { Serial.print(F("RX_SOF_DET ")); irqMsg += "RX_SOF_DET "; }
    if (irqStatus & (1<<15)) { Serial.print(F("RX_SC_DET ")); irqMsg += "RX_SC_DET "; }
    if (irqStatus & (1<<16)) { Serial.print(F("TEMPSENS_ERROR ")); irqMsg += "TEMPSENS_ERROR "; }
    if (irqStatus & (1<<17)) { Serial.print(F("GENERAL_ERROR ")); irqMsg += "GENERAL_ERROR "; }
    if (irqStatus & (1<<18)) { Serial.print(F("HV_ERROR ")); irqMsg += "HV_ERROR "; }
    if (irqStatus & (1<<19)) { Serial.print(F("LPCD ")); irqMsg += "LPCD "; }
    
    Serial.println("]");
    irqMsg += "]";
    
    logToWeb(irqMsg, "warning");
}

void NFCReader::logToWeb(const String& message, const String& level) {
    if (webServer != nullptr) {
        webServer->broadcastLog(message, level);
    }
}
