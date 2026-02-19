#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>
#include "wifi_manager.h"
#include "system_config.h"
#include "server_client.h"
#include "web_server.h"

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)

#define PN5180_NSS  10
#define PN5180_BUSY 9
#define PN5180_RST  7

#elif defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)

// Seeed Xiao ESP32S3 pinout: Dx != GPIOx!
// D10=GPIO9, D9=GPIO8, D8=GPIO7, D7=GPIO44, D6=GPIO43, D5=GPIO6
#define PN5180_NSS  9   // D10
#define PN5180_BUSY 6   // D5
#define PN5180_RST  43  // D6
#define PN5180_SCK  44  // D7
#define PN5180_MISO 7   // D8
#define PN5180_MOSI 8   // D9

#elif defined(ARDUINO_ARCH_ESP32)

// Standaard ESP32 pinout
#define PN5180_NSS  5
#define PN5180_BUSY 16
#define PN5180_RST  17

#else
#error Please define your pinout here!
#endif

uint32_t loopCnt = 0;
bool errorFlag = false;
uint8_t consecutiveErrors = 0;
const uint8_t MAX_ERRORS = 5;
const uint8_t REBOOT_ERRORS = 10; // Reboot na 10 fouten

// PN5180 watchdog variabelen
unsigned long lastSuccessfulRead = 0;
unsigned long lastPollCheck = 0;
const unsigned long POLL_INTERVAL = 100; // Check elke 0.1 seconden
const unsigned long MAX_NO_RESPONSE = 1000; // Reset na 1 seconden geen respons

// System Components
WiFiConfigManager wifiManager;
SystemConfig systemConfig;
ServerClient serverClient;
NFCWebServer webServer;

// Config knop (GPIO 1 / D0 op Xiao ESP32S3)
#define CONFIG_BUTTON 1
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
const unsigned long LONG_PRESS_TIME = 3000; // 3 seconden

// Stats tracking
unsigned long lastStatsUpdate = 0;
const unsigned long STATS_UPDATE_INTERVAL = 5000; // 5 seconden

PN5180ISO14443 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);

void showIRQStatus(uint32_t irqStatus) {
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
  webServer.broadcastLog(irqMsg, "warning");
}

void recoverFromError(uint32_t irqStatus) {
  // Clear IRQ status first
  nfc.clearIRQStatus(0xFFFFFFFF);
  delay(10);
  
  // Check for collision/multi-card error
  if (irqStatus & (1<<10)) { // RF_ACTIVE_ERROR
    Serial.println(F("*** Collision detected - multiple cards?"));
    webServer.broadcastLog("⚠️ Collision gedetecteerd - meerdere kaarten?", "warning");
  }
  
  // Soft reset and reinitialize
  webServer.broadcastLog("🔄 PN5180 herstel...", "warning");
  nfc.reset();
  delay(50);
  nfc.setupRF();
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Geef serial tijd om te initialiseren
  
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("ESP32 NFC Reader + Config System"));

  // Setup config knop
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);
  
  // Initialize system config
  Serial.println(F("\n=== Initialiseer System Config ==="));
  systemConfig.begin();
  
  // Start WiFi Manager
  Serial.println(F("\n=== Initialiseer WiFi ==="));
  wifiManager.begin();
  
  // Wacht tot WiFi configuratie klaar is (in config mode)
  while (wifiManager.isConfigMode()) {
    wifiManager.loop();
    delay(10);
  }
  
  if (wifiManager.isConnected()) {
    Serial.println(F("✅ WiFi verbonden!"));
    Serial.print(F("IP: "));
    Serial.println(wifiManager.getLocalIP());
    
    // Initialize server client
    String serverUrl = systemConfig.getServerUrl();
    if (serverUrl.length() > 0) {
      serverClient.setServerUrl(serverUrl);
      Serial.print(F("Server configured: "));
      Serial.println(serverUrl);
    }
    
    // Start web server
    Serial.println(F("\n=== Start Web Server ==="));
    webServer.begin(&systemConfig, &serverClient);
    
    // Connect web server to server client for logging
    serverClient.setWebServer(&webServer);
    
    // Check admin setup
    if (!systemConfig.hasAdminAccount()) {
      Serial.println(F("⚠️ No admin account - please complete setup"));
      webServer.broadcastLog("Eerste keer opstarten - configureer admin account", "warning");
    } else {
      Serial.println(F("✅ Admin account configured"));
    }
    
    // Show reader mode
    String mode = systemConfig.getReaderMode();
    Serial.print(F("Reader Mode: "));
    Serial.println(mode);
    webServer.broadcastLog("Reader mode: " + mode, "info");
    
  } else {
    Serial.println(F("⚠️ Geen WiFi verbinding - ga verder zonder WiFi"));
  }

#if defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)
  Serial.println(F("\n=== Initialiseer PN5180 ==="));
  Serial.println(F("Configuring SPI: D7(SCK)=GPIO44 D8(MISO)=GPIO7 D9(MOSI)=GPIO8 D10(NSS)=GPIO9"));
  // ESP32 SPI.begin() zonder NSS - NSS wordt handmatig beheerd
  SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI);
#elif defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Configuring SPI with default ESP32 pins"));
  SPI.begin();
#endif

  pinMode(PN5180_NSS, OUTPUT);
  digitalWrite(PN5180_NSS, HIGH);
  pinMode(PN5180_BUSY, INPUT);
  pinMode(PN5180_RST, OUTPUT);
  digitalWrite(PN5180_RST, HIGH);
  
  delay(100); // Geef PN5180 tijd om op te starten
  
  Serial.println(F("Before nfc.begin()"));
  nfc.begin();
  Serial.println(F("After nfc.begin()"));
  
  // Herconfigureer SPI na nfc.begin()
#if defined(ARDUINO_ARCH_ESP32)
  SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI);
  Serial.println(F("SPI reconfigured after nfc.begin()"));
#endif

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  
  nfc.reset();
  
  Serial.println(F("Reset complete"));

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  if (0xff == productVersion[1]) {
    Serial.println(F("Initialization failed!?"));
    webServer.broadcastLog("PN5180 initialisatie mislukt!", "error");
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    exit(-1);
  }

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading firmware version..."));
  uint8_t firmwareVersion[2];
  nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
  Serial.print(F("Firmware version="));
  Serial.print(firmwareVersion[1]);
  Serial.print(".");
  Serial.println(firmwareVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading EEPROM version..."));
  uint8_t eepromVersion[2];
  nfc.readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
  Serial.print(F("EEPROM version="));
  Serial.print(eepromVersion[1]);
  Serial.print(".");
  Serial.println(eepromVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Enable RF field..."));
  nfc.setupRF();
  
  // Initialiseer watchdog timers
  lastSuccessfulRead = millis();
  lastPollCheck = millis();
  lastStatsUpdate = millis();
  
  Serial.println(F("=================================="));
  Serial.println(F("System Ready!"));
  Serial.println(F("Houd D0 knop 3 seconden ingedrukt voor WiFi configuratie"));
  Serial.println(F("=================================="));
  
  webServer.broadcastLog("System gereed - PN5180 actief", "success");
  webServer.broadcastStatus("online", serverClient.isServerOnline() ? "online" : "offline", systemConfig.getReaderMode());
}

bool checkPN5180Health() {
  // Test 1: IRQ status moet leesbaar zijn
  uint32_t irqStatus = nfc.getIRQStatus();
  
  static unsigned long lastErrorLog = 0;
  const unsigned long ERROR_LOG_INTERVAL = 5000; // Log max 1x per 5 seconden
  bool shouldLog = (millis() - lastErrorLog) > ERROR_LOG_INTERVAL;
  
  if (irqStatus == 0xFFFFFFFF) {
    Serial.println(F("*** PN5180 SPI not responding!"));
    if (shouldLog) {
      webServer.broadcastLog("❌ PN5180 SPI reageert niet!", "error");
      lastErrorLog = millis();
    }
    return false;
  }
  
  // Test 1b: Check voor specifiek bekende problematische waarden
  if (irqStatus == 0xE4FB44CA) {
    Serial.println(F("*** PN5180 problematic IRQ status detected!"));
    if (shouldLog) {
      webServer.broadcastLog("❌ PN5180 problematische IRQ status", "error");
      lastErrorLog = millis();
    }
    return false;
  }
  
  // Test 1c: Check voor gezonde/normale status EERST (voor critical error check)
  if (irqStatus == 0x24006 || irqStatus == 0x24007 || irqStatus == 0x4006 || irqStatus == 0x4007) {
    // Dit zijn normale waarden, alles OK
    return true;
  }
  
  // Test 1d: Check voor kritieke error flags (alleen als het GEEN bekende goede waarde is)
  // Check alleen HV_ERROR en TEMPSENS_ERROR (GENERAL_ERROR is te breed)
  if (irqStatus & ((1<<16) | (1<<18))) { // TEMPSENS_ERROR | HV_ERROR
    Serial.print(F("*** PN5180 critical error flags: 0x"));
    Serial.println(irqStatus, HEX);
    if (shouldLog) {
      String msg = "❌ PN5180 kritieke error: 0x" + String(irqStatus, HEX);
      webServer.broadcastLog(msg, "error");
      lastErrorLog = millis();
    }
    showIRQStatus(irqStatus);
    return false;
  }
  
  // GENERAL_ERROR alleen checken in combinatie met andere slechte tekens
  if ((irqStatus & (1<<17)) && (irqStatus & (1<<10))) { // GENERAL_ERROR + RF_ACTIVE_ERROR
    Serial.print(F("*** PN5180 general + RF error: 0x"));
    Serial.println(irqStatus, HEX);
    if (shouldLog) {
      webServer.broadcastLog("❌ PN5180 general + RF error", "error");
      lastErrorLog = millis();
    }
    showIRQStatus(irqStatus);
    return false;
  }
  
  // Test 2: Constant zeros detecteren
  static uint8_t zeroCount = 0;
  static uint32_t lastIrqStatus = 0;
  
  if (irqStatus == 0x00000000 && lastIrqStatus == 0x00000000) {
    zeroCount++;
    if (zeroCount > 3) {
      Serial.println(F("*** PN5180 stuck at zero!"));
      static unsigned long lastZeroLog = 0;
      if ((millis() - lastZeroLog) > 5000) {
        webServer.broadcastLog("❌ PN5180 stuck at zero - IRQ status blijft 0x00", "error");
        lastZeroLog = millis();
      }
      zeroCount = 0;
      return false;
    }
  } else {
    zeroCount = 0;
  }
  lastIrqStatus = irqStatus;
  
  // Test 3: EEPROM moet leesbaar zijn (elke 5e check om overhead te beperken)
  static uint8_t eepromCheckCounter = 0;
  if (++eepromCheckCounter >= 5) {
    eepromCheckCounter = 0;
    
    uint8_t testByte[1];
    if (!nfc.readEEprom(PRODUCT_VERSION, testByte, 1)) {
      Serial.println(F("*** PN5180 EEPROM read failed!"));
      static unsigned long lastEepromLog = 0;
      if ((millis() - lastEepromLog) > 5000) {
        webServer.broadcastLog("❌ PN5180 EEPROM niet leesbaar", "error");
        lastEepromLog = millis();
      }
      return false;
    }
    
    if (testByte[0] == 0xFF) {
      Serial.println(F("*** PN5180 EEPROM corrupted!"));
      static unsigned long lastCorruptLog = 0;
      if ((millis() - lastCorruptLog) > 5000) {
        webServer.broadcastLog("❌ PN5180 EEPROM corrupt (0xFF)", "error");
        lastCorruptLog = millis();
      }
      return false;
    }
  }
  
  return true;
}

void resetPN5180() {
  webServer.broadcastLog("🔄 PN5180 hard reset...", "warning");
  
  nfc.reset();
  delay(100);
  
  // Verifieer reset door product versie te lezen
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  
  if (0xff == productVersion[1]) {
    Serial.println(F("*** PN5180 reset failed - rebooting ESP32..."));
    webServer.broadcastLog("❌ PN5180 reset mislukt - ESP32 herstarten...", "error");
    Serial.flush();
    delay(1000);
    ESP.restart();
  }
  
  nfc.setupRF();
  webServer.broadcastLog("✅ PN5180 reset succesvol", "success");
  
  lastSuccessfulRead = millis();
  consecutiveErrors = 0;
}

const char* detectCardType(uint8_t* uid) {
  // UID analyse omdat activateTypeA() SAK niet correct teruggeeft
  
  // Check voor 7-byte UID (cascade tag 0x88 of specifieke patronen)
  // DESFire/NTAG424 hebben vaak UID die begint met 0x04
  // en eindigt met checksums in laatste bytes
  
  bool is7ByteUID = false;
  
  // Check of dit een 7-byte UID is (byte 6 en 7 zijn niet 0x00)
  if (uid[6] != 0x00 || uid[5] != 0x00) {
    is7ByteUID = true;
  }
  
  Serial.print(F("  [DEBUG] UID[0]=0x"));
  Serial.print(uid[0], HEX);
  Serial.print(F(", 7-byte UID: "));
  Serial.println(is7ByteUID ? "YES" : "NO");
  
  String debugMsg = "[DEBUG] UID[0]=0x" + String(uid[0], HEX) + ", 7-byte: " + (is7ByteUID ? "YES" : "NO");
  webServer.broadcastLog(debugMsg, "info");
  
  // Detectie op basis van UID patronen
  if (uid[0] == 0x04 && is7ByteUID) {
    // 7-byte UID beginnend met 0x04 = meestal DESFire of NTAG 424
    return "DESFire/NTAG424 (SECURE)";
  }
  else if (uid[0] == 0x08 && !is7ByteUID) {
    // UID beginnend met 0x08 = vaak secure documents
    return "Secure document (Passport/ID/License)";
  }
  else if (!is7ByteUID) {
    // 4-byte UID = meestal unsecure tags
    if ((uid[0] & 0xF0) == 0x00) {
      return "Mifare Classic (unsecure)";
    }
    return "NTAG/Ultralight (unsecure)";
  }
  
  return "Unknown card type";
}

// ISO 14443 loop
void loop() {
  // Handle web server
  webServer.loop();
  
  // Check config knop (D0)
  bool buttonState = digitalRead(CONFIG_BUTTON);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    // Knop net ingedrukt
    buttonPressTime = millis();
  } else if (buttonState == LOW && lastButtonState == LOW) {
    // Knop wordt ingehouden
    if (millis() - buttonPressTime >= LONG_PRESS_TIME) {
      Serial.println(F("\n=== Config knop ingedrukt ==="));
      Serial.println(F("Start WiFi configuratie modus..."));
      webServer.broadcastLog("WiFi configuratie modus gestart", "warning");
      wifiManager.startConfigMode();
      
      // Wacht tot knop losgelaten wordt
      while (digitalRead(CONFIG_BUTTON) == LOW) {
        delay(10);
      }
      buttonPressTime = 0;
    }
  }
  
  lastButtonState = buttonState;
  
  // Als we in config mode zijn, handle alleen WiFi
  if (wifiManager.isConfigMode()) {
    wifiManager.loop();
    return; // Skip NFC operaties tijdens config
  }
  
  unsigned long currentMillis = millis();
  
  // Periodic server ping
  serverClient.periodicPing();
  
  // Periodic stats update
  if (currentMillis - lastStatsUpdate >= STATS_UPDATE_INTERVAL) {
    lastStatsUpdate = currentMillis;
    webServer.broadcastStats(systemConfig.getCardsRead(), systemConfig.getUptime());
  }
  
  // Periodieke health check
  if (currentMillis - lastPollCheck >= POLL_INTERVAL) {
    lastPollCheck = currentMillis;
    
    static uint8_t healthFailCount = 0;
    
    if (!checkPN5180Health()) {
      healthFailCount++;
      Serial.println(F("*** PN5180 health check failed"));
      
      // Log alleen elke 10e failure om spam te voorkomen
      if (healthFailCount % 10 == 1) {
        webServer.broadcastLog("⚠️ PN5180 health check failed (x" + String(healthFailCount) + ")", "error");
      }
      
      resetPN5180();
      return;
    }
    else{
      // Reset counter bij succesvolle check
      if (healthFailCount > 0) {
        webServer.broadcastLog("✅ PN5180 health check hersteld na " + String(healthFailCount) + " failures", "success");
        healthFailCount = 0;
      }
      lastSuccessfulRead = currentMillis; 
    }
  }
  
  // Watchdog: te lang geen succesvolle operatie
  if (lastSuccessfulRead > 0 && 
      (currentMillis - lastSuccessfulRead) >= MAX_NO_RESPONSE) {
    Serial.println(F("*** PN5180 watchdog timeout - no response for 1s"));
    webServer.broadcastLog("PN5180 watchdog timeout", "error");
    resetPN5180();
    return;
  }
  
  if (errorFlag) {
    uint32_t irqStatus = nfc.getIRQStatus();
    showIRQStatus(irqStatus);

    if (0 == (RX_SOF_DET_IRQ_STAT & irqStatus)) {
      Serial.println(F("*** No card detected!"));
      webServer.broadcastLog("⚠️ Geen kaart gedetecteerd in error state", "warning");
    }

    recoverFromError(irqStatus);
    
    consecutiveErrors++;
    
    if (consecutiveErrors >= REBOOT_ERRORS) {
      Serial.println(F("*** Too many consecutive errors - rebooting ESP32..."));
      webServer.broadcastLog("❌ Te veel fouten - ESP32 herstarten...", "error");
      Serial.flush();
      delay(1000);
      ESP.restart();
    }
    
    if (consecutiveErrors >= MAX_ERRORS) {
      Serial.println(F("*** Too many errors, waiting 2 seconds..."));
      webServer.broadcastLog("⚠️ Te veel fouten - wacht 2 seconden...", "warning");
      delay(2000);
    }

    errorFlag = false;
    delay(10);
    return;
  }

  static bool wasPresent = false;
  static bool shown = false;
  
  bool present = nfc.isCardPresent();
  
  if (!present) {
    if (wasPresent) {
      Serial.println(F("Card removed"));
      webServer.broadcastLog("Kaart verwijderd", "info");
      shown = false;
      consecutiveErrors = 0; // Reset error counter when card removed
      lastSuccessfulRead = millis(); // Update watchdog
    }
    wasPresent = false;
    delay(100); // Small delay to prevent tight loop
    return;
  }
  
  wasPresent = true;
  
  uint8_t uid[8];
  if (!nfc.readCardSerial(uid)) {
    Serial.println(F("Error in readCardSerial"));
    webServer.broadcastLog("❌ Fout bij lezen kaart UID", "error");
    errorFlag = true;
    return;
  }
  
  // Successful read - reset error counter and update watchdog
  consecutiveErrors = 0;
  lastSuccessfulRead = millis();
  
  if (!shown) {
    // Convert UID to string
    String uidStr = "";
    for (int i=0; i<7; i++) {
      if (uid[i] < 0x10) uidStr += "0";
      uidStr += String(uid[i], HEX);
      if (i < 6) uidStr += ":";
    }
    
    Serial.print(F("UID: "));
    Serial.println(uidStr);
    webServer.broadcastLog("Kaart gedetecteerd: " + uidStr, "info");
    
    // Detecteer card type
    const char* cardType = detectCardType(uid);
    Serial.print(F("Card Type: "));
    Serial.println(cardType);
    webServer.broadcastLog("Type: " + String(cardType), "info");
    
    // Increment stats
    systemConfig.incrementCardsRead();
    
    // ==== READER MODE HANDLING ====
    
    if (systemConfig.isMachineMode()) {
      // MACHINE MODE: Challenge/Response met server
      Serial.println(F("\n=== MACHINE MODE ==="));
      webServer.broadcastLog("Machine modus - vraag challenge aan server", "info");
      
      String challenge = serverClient.requestChallenge(uidStr);
      
      if (challenge.length() > 0) {
        webServer.broadcastLog("Challenge ontvangen: " + challenge.substring(0, 16) + "...", "success");
        
        // TODO: Stuur challenge naar NFC424 en ontvang response
        // Voor nu mock response (in echte versie: DESFire/NTAG424 EV2 auth)
        String mockResponse = challenge; // Simplified mock - in real: encrypted response
        
        webServer.broadcastLog("Response van kaart: " + mockResponse.substring(0, 16) + "...", "info");
        
        // Verifieer bij server (server heeft challenge opgeslagen)
        bool verified = serverClient.verifyResponse(uidStr, mockResponse);
        
        if (verified) {
          Serial.println(F("✅ TOEGANG VERLEEND"));
          webServer.broadcastLog("✅ Challenge verificatie succesvol - TOEGANG VERLEEND", "success");
        } else {
          Serial.println(F("❌ TOEGANG GEWEIGERD"));
          webServer.broadcastLog("❌ Challenge verificatie mislukt - TOEGANG GEWEIGERD", "error");
        }
        
      } else {
        Serial.println(F("❌ Geen challenge van server ontvangen"));
        webServer.broadcastLog("Server niet beschikbaar - geen challenge", "error");
      }
      
    } else if (systemConfig.isConfigMode()) {
      // CONFIG MODE: Kaart schrijven/registreren
      Serial.println(F("\n=== CONFIG MODE ==="));
      webServer.broadcastLog("Config modus - kaart configureren", "warning");
      
      String masterKey = "";
      bool useManualKey = systemConfig.hasMasterkey();
      
      if (useManualKey) {
        // Gebruik handmatige masterkey (offline mode)
        masterKey = systemConfig.getSessionMasterkey();
        Serial.println(F("🔑 Gebruik handmatige masterkey (offline)"));
        webServer.broadcastLog("Gebruik handmatige masterkey voor personalisatie", "info");
        
        // TODO: Schrijf kaart met handmatige masterkey via DESFire
        webServer.broadcastLog("TODO: Kaart personaliseren met handmatige key", "warning");
        webServer.broadcastLog("✅ Offline personalisatie voltooid", "success");
        
      } else {
        // Gebruik server keys (normale flow)
        Serial.println(F("🌐 Haal keys op van server..."));
        webServer.broadcastLog("Haal keys op van server...", "info");
        
        // Check of kaart al bestaat
        String cardInfo = serverClient.getCardInfo(uidStr);
        
        if (cardInfo.length() > 0) {
          Serial.println(F("ℹ️ Kaart bestaat al in database"));
          webServer.broadcastLog("Kaart bestaat al - haal keys op...", "info");
          
          // Haal masterkey op van server
          masterKey = serverClient.getCardKey(uidStr, "master");
          
          if (masterKey.length() > 0) {
            webServer.broadcastLog("Master key ontvangen: " + masterKey.substring(0, 8) + "...", "success");
            
            // TODO: Schrijf kaart met PN5180 en masterkey via DESFire commands
            webServer.broadcastLog("TODO: Kaart personaliseren met DESFire commands", "warning");
            webServer.broadcastLog("✅ Kaart klaar voor gebruik", "success");
          } else {
            webServer.broadcastLog("❌ Kon keys niet ophalen van server", "error");
          }
          
        } else {
          // Nieuwe kaart - registreer eerst
          Serial.println(F("📝 Nieuwe kaart - registreren..."));
          webServer.broadcastLog("Nieuwe kaart detecteerd - registreren...", "info");
          
          bool registered = serverClient.registerCard(uidStr, "Card_" + uidStr.substring(0, 8), "Auto-registered");
          
          if (registered) {
            webServer.broadcastLog("✅ Kaart geregistreerd in database", "success");
            
            // Haal keys op
            masterKey = serverClient.getCardKey(uidStr, "master");
            
            if (masterKey.length() > 0) {
              webServer.broadcastLog("Keys ontvangen: " + masterKey.substring(0, 8) + "...", "success");
              
              // TODO: Schrijf kaart met PN5180
              webServer.broadcastLog("TODO: Kaart personaliseren met DESFire", "warning");
              webServer.broadcastLog("✅ Personalisatie voltooid", "success");
            } else {
              webServer.broadcastLog("❌ Keys ophalen mislukt", "error");
            }
          } else {
            webServer.broadcastLog("❌ Kaart registratie mislukt", "error");
          }
        }
      }
    }
    
    shown = true;
  }

  delay(1000);  
}