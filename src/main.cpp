#include <Arduino.h>
#include "system_config.h"
#include "web_server.h"
#include "server_client.h"
#include "wifi_manager.h"
#include "nfc_reader.h"
#include "ntag424_handler.h"
#include "ntag424_crypto.h"

// ============ PIN CONFIGURATION ============

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)
  #define PN5180_NSS  10
  #define PN5180_BUSY 9
  #define PN5180_RST  7
  #define PN5180_SCK  13
  #define PN5180_MISO 12
  #define PN5180_MOSI 11

#elif defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)
  // Seeed Xiao ESP32S3 pinout: Dx != GPIOx!
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
  #define PN5180_SCK  18
  #define PN5180_MISO 19
  #define PN5180_MOSI 23

#else
  #error Please define your pinout here!
#endif

// Config button (GPIO 1 / D0 on Xiao ESP32S3)
#define CONFIG_BUTTON 1

// ============ SYSTEM COMPONENTS ============

WiFiConfigManager wifiManager;
SystemConfig systemConfig;
ServerClient serverClient;
NFCWebServer webServer;
NFCReader* nfcReader = nullptr;
NTAG424Handler* ntag424Handler = nullptr;

// ============ BUTTON HANDLING ============

bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
const unsigned long LONG_PRESS_TIME = 3000; // 3 seconds

// ============ STATS TRACKING ============

unsigned long lastStatsUpdate = 0;
const unsigned long STATS_UPDATE_INTERVAL = 5000; // 5 seconds

// ============ APPLICATION LOGIC ============

void handleMachineMode(NFCReader::CardInfo& cardInfo);
void handleConfigMode(NFCReader::CardInfo& cardInfo);
void handleWriteMode(NFCReader::CardInfo& cardInfo);
void checkConfigButton();
void handleSerialCommands();  // Test command handler

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("ESP32 NFC Reader System v2.0"));
  Serial.println(F("=================================="));
  
  // Setup config button
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);
  
  // Initialize system configuration
  Serial.println(F("\n=== System Configuration ==="));
  systemConfig.begin();
  
  // Initialize WiFi
  Serial.println(F("\n=== WiFi Initialization ==="));
  wifiManager.begin();
  
  // Wait for WiFi configuration to complete
  while (wifiManager.isConfigMode()) {
    wifiManager.loop();
    delay(10);
  }
  
  // Setup network services
  if (wifiManager.isConnected()) {
    Serial.println(F("✅ WiFi connected!"));
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
    Serial.println(F("\n=== Web Server ==="));
    webServer.begin(&systemConfig, &serverClient);
    serverClient.setWebServer(&webServer);
    
    // Check admin setup
    if (!systemConfig.hasAdminAccount()) {
      Serial.println(F("⚠️ No admin - complete setup via web interface"));
      webServer.broadcastLog("Eerste keer opstarten - configureer admin", "warning");
    } else {
      Serial.println(F("✅ Admin configured"));
    }
    
    // Show reader mode
    String mode = systemConfig.getReaderMode();
    Serial.print(F("Reader Mode: "));
    Serial.println(mode);
    webServer.broadcastLog("Reader mode: " + mode, "info");
  } else {
    Serial.println(F("⚠️ No WiFi - continuing in offline mode"));
  }
  
  // Initialize NFC Reader
  Serial.println(F("\n=== NFC Reader Initialization ==="));
  NFCReader::PinConfig pinConfig = {
    .nss = PN5180_NSS,
    .busy = PN5180_BUSY,
    .rst = PN5180_RST,
    .sck = PN5180_SCK,
    .miso = PN5180_MISO,
    .mosi = PN5180_MOSI
  };
  
  nfcReader = new NFCReader(pinConfig);
  nfcReader->setWebServer(&webServer);
  
  if (!nfcReader->begin()) {
    Serial.println(F("❌ NFC Reader initialization failed!"));
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    while(1) delay(1000);
  }
    // Initialize NTAG424 Handler
  Serial.println(F("\n=== NTAG424 Handler Initialization ==="));
  PN5180ISO14443* nfcInstance = nfcReader->getNFC();
  
  if (nfcInstance == nullptr) {
    Serial.println(F("⚠️ Could not get PN5180 instance - NTAG424 functions disabled"));
    webServer.broadcastLog("NTAG424 functies niet beschikbaar", "warning");
  } else {
    ntag424Handler = new NTAG424Handler(nfcInstance);
    ntag424Handler->setWebServer(&webServer);
    Serial.println(F("✅ NTAG424 Handler ready"));
  }
    Serial.println(F("\n=================================="));
  Serial.println(F("✅ System Ready!"));
  Serial.println(F("Hold D0 button for 3s to enter WiFi config"));
  Serial.println(F("==================================\n"));
  
  webServer.broadcastLog("System gereed - wachtend op kaarten", "success");
  webServer.broadcastStatus("online", 
                            serverClient.isServerOnline() ? "online" : "offline", 
                            systemConfig.getReaderMode());
}

void loop() {
  // Handle serial commands first
  handleSerialCommands();
  
  // Handle web server
  webServer.loop();
  
  // Check config button
  checkConfigButton();
  
  // If in WiFi config mode, skip NFC operations
  if (wifiManager.isConfigMode()) {
    wifiManager.loop();
    return;
  }
  
  // Periodic server ping
  serverClient.periodicPing();
  
  // Periodic stats update
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatsUpdate >= STATS_UPDATE_INTERVAL) {
    lastStatsUpdate = currentMillis;
    webServer.broadcastStats(systemConfig.getCardsRead(), systemConfig.getUptime());
  }
  
  // NFC Reader loop (handles health checks and watchdog)
  nfcReader->loop();
  
  // Card detection and processing
  NFCReader::CardInfo cardInfo;
  if (nfcReader->readCard(cardInfo)) {
    // Increment stats
    systemConfig.incrementCardsRead();
    
    // Check if write mode is active (takes priority over reader mode)
    if (systemConfig.isWriteActive()) {
      handleWriteMode(cardInfo);
    }
    // Handle based on reader mode
    else if (systemConfig.isMachineMode()) {
      handleMachineMode(cardInfo);
    } else if (systemConfig.isConfigMode()) {
      handleConfigMode(cardInfo);
    }
  }
  
  delay(100); // Small delay to prevent tight loop
}

// ============ BUTTON HANDLING ============

void checkConfigButton() {
  bool buttonState = digitalRead(CONFIG_BUTTON);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
  } 
  else if (buttonState == LOW && lastButtonState == LOW) {
    if (millis() - buttonPressTime >= LONG_PRESS_TIME) {
      Serial.println(F("\n=== Config Button Pressed ==="));
      Serial.println(F("Starting WiFi configuration mode..."));
      webServer.broadcastLog("WiFi configuratie modus gestart", "warning");
      wifiManager.startConfigMode();
      
      // Wait for button release
      while (digitalRead(CONFIG_BUTTON) == LOW) {
        delay(10);
      }
      buttonPressTime = 0;
    }
  }
  
  lastButtonState = buttonState;
}

// ============ MACHINE MODE HANDLER ============

void handleMachineMode(NFCReader::CardInfo& cardInfo) {
  Serial.println(F("\n=== MACHINE MODE ==="));
  webServer.broadcastLog("Machine modus - vraag challenge aan server", "info");
  
  String challenge = serverClient.requestChallenge(cardInfo.uidString);
  
  if (challenge.length() > 0) {
    webServer.broadcastLog("Challenge ontvangen: " + challenge.substring(0, 16) + "...", "success");
    
    // TODO: Send challenge to NFC424 and receive response
    // For now mock response (in real version: DESFire/NTAG424 EV2 auth)
    String mockResponse = challenge; // Simplified mock
    
    webServer.broadcastLog("Response van kaart: " + mockResponse.substring(0, 16) + "...", "info");
    
    // Verify with server
    bool verified = serverClient.verifyResponse(cardInfo.uidString, mockResponse);
    
    if (verified) {
      Serial.println(F("✅ ACCESS GRANTED"));
      webServer.broadcastLog("✅ Challenge verificatie succesvol - TOEGANG VERLEEND", "success");
    } else {
      Serial.println(F("❌ ACCESS DENIED"));
      webServer.broadcastLog("❌ Challenge verificatie mislukt - TOEGANG GEWEIGERD", "error");
    }
    
  } else {
    Serial.println(F("❌ No challenge received from server"));
    webServer.broadcastLog("Server niet beschikbaar - geen challenge", "error");
  }
}

// ============ CONFIG MODE HANDLER ============

void handleConfigMode(NFCReader::CardInfo& cardInfo) {
  Serial.println(F("\n=== CONFIG MODE - READ ONLY ==="));
  Serial.println(F("⚠️  CONFIG MODE DOES NOT WRITE TO CARDS!"));
  Serial.println(F("⚠️  Use WRITE MODE via /write-cards to write cards!"));
  webServer.broadcastLog("📖 Config modus - Alleen lezen (gebruik /write-cards om te schrijven)", "info");
  
  // Reset any previous ISO-DEP session
  if (ntag424Handler != nullptr) {
    ntag424Handler->resetSession();
  }
  
  // Check if it's actually an NTAG424 DNA card
  if (cardInfo.cardType.indexOf("NTAG424") < 0 && 
      cardInfo.cardType.indexOf("DESFire") < 0 &&
      cardInfo.cardType.indexOf("SECURE") < 0) {
    webServer.broadcastLog("⚠️ Geen NTAG424 DNA kaart gedetecteerd", "warning");
    webServer.broadcastLog("Card type: " + cardInfo.cardType, "info");
    return;
  }
  
  // Initialize handler if needed
  if (ntag424Handler == nullptr) {
    Serial.println(F("❌ NTAG424 handler not initialized"));
    webServer.broadcastLog("❌ NTAG424 handler niet geïnitialiseerd", "error");
    return;
  }
  
  // Activate ISO14443-4 protocol for reading
  if (!ntag424Handler->activateCard()) {
    Serial.println(F("❌ Failed to activate card for ISO-DEP communication"));
    webServer.broadcastLog("❌ Kaart activatie mislukt", "error");
    return;
  }
  
  Serial.println(F("✅ Card activated for ISO-DEP"));
  
  // Read card version info
  uint8_t versionInfo[28];
  if (!ntag424Handler->getVersion(versionInfo)) {
    Serial.println(F("❌ Failed to read card version"));
    webServer.broadcastLog("❌ Kan versie info niet lezen", "error");
    return;
  }
  
  Serial.println(F("✅ Card version read successfully"));
  Serial.print(F("📇 Card UID: "));
  Serial.println(cardInfo.uidString);
  Serial.print(F("🏷️  Card Type: "));
  Serial.println(cardInfo.cardType);
  
  webServer.broadcastLog("✅ Kaart gelezen: " + cardInfo.uidString, "success");
  webServer.broadcastLog("Type: " + cardInfo.cardType, "info");
  webServer.broadcastLog("💡 Ga naar /write-cards om deze kaart te schrijven", "info");
  
  // Reset session for next card
  if (ntag424Handler != nullptr) {
    ntag424Handler->resetSession();
  }
}

// ============ WRITE MODE HANDLER (Standalone Key Derivation) ============

void handleWriteMode(NFCReader::CardInfo& cardInfo) {
  String uid = cardInfo.uidString;
  Serial.println(F("\n╔════════════════════════════════════════════╗"));
  Serial.println(F("║   WRITE MODE - AN12196 Personalization    ║"));
  Serial.println(F("╚════════════════════════════════════════════╝"));
  Serial.print(F("Processing card: "));
  Serial.println(uid);
  
  // Broadcast processing status
  webServer.broadcastWriteCardStatus(uid, "processing", "Kaart gedetecteerd - start schrijven...");
  
  // Reset any previous ISO-DEP session
  if (ntag424Handler != nullptr) {
    ntag424Handler->resetSession();
  }
  
  // Check if it's actually an NTAG424 DNA card
  if (cardInfo.cardType.indexOf("NTAG424") < 0 && 
      cardInfo.cardType.indexOf("DESFire") < 0 &&
      cardInfo.cardType.indexOf("SECURE") < 0) {
    String errorMsg = "Geen NTAG424 DNA kaart - type: " + cardInfo.cardType;
    Serial.print(F("❌ "));
    Serial.println(errorMsg);
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    return;
  }
  
  // ═══════════════════════════════════════════════════════════════
  // KEY DERIVATION / FETCH
  // ═══════════════════════════════════════════════════════════════
  String keySource = systemConfig.getKeySource();
  uint8_t derivedKey[16];
  
  Serial.println(F("\n[1] KEY SOURCE & DERIVATION"));
  Serial.print(F("    Key source: "));
  Serial.println(keySource);
  
  if (keySource == "esp32") {
    // ESP32 mode: derive key from local master secret
    String masterSecret = systemConfig.getMasterSecret();
    if (masterSecret.length() == 0) {
      String errorMsg = "Geen master secret beschikbaar (ESP32 mode)";
      Serial.println(F("\n❌❌❌ NO MASTER SECRET AVAILABLE! ❌❌❌"));
      Serial.println(F("Did you click 'Start Schrijven' with ESP32 mode?"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    Serial.print(F("🔑 Using master secret (first 8): "));
    Serial.println(masterSecret.substring(0, 8));
    
    // Derive master key from secret + UID
    webServer.broadcastWriteCardStatus(uid, "processing", "Bereken HMAC uit master secret + UID...");
    
    bool keyDerived = NTAG424Crypto::deriveMasterKey(masterSecret, uid, derivedKey, 1);
    
    if (!keyDerived) {
      String errorMsg = "Key derivation mislukt (HMAC-SHA256 fout)";
      Serial.println(F("❌ Key derivation failed"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    Serial.println(F("✅ Key derived successfully using HMAC-SHA256"));
    Serial.print(F("Derived K0: "));
    for (int i = 0; i < 16; i++) {
      char buf[3];
      sprintf(buf, "%02X", derivedKey[i]);
      Serial.print(buf);
    }
    Serial.println();
    
  } else if (keySource == "server") {
    // Server mode: fetch key from server by UID
    Serial.println(F("🌐 Fetching key from server..."));
    webServer.broadcastWriteCardStatus(uid, "processing", "Haal key op van server...");
    
    // Make HTTP request to server
    String serverUrl = systemConfig.getServerUrl();
    if (serverUrl.length() == 0) {
      String errorMsg = "Server URL niet geconfigureerd";
      Serial.println(F("❌ Server URL not configured"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    HTTPClient http;
    String requestUrl = serverUrl + "/api/get-key?uid=" + uid;
    Serial.print(F("📡 Request URL: "));
    Serial.println(requestUrl);
    
    http.begin(requestUrl);
    http.setTimeout(5000);  // 5 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode != 200) {
      String errorMsg = "Server request mislukt (HTTP " + String(httpCode) + ")";
      Serial.print(F("❌ HTTP request failed: "));
      Serial.println(httpCode);
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      http.end();
      return;
    }
    
    String response = http.getString();
    http.end();
    
    Serial.print(F("📥 Server response: "));
    Serial.println(response);
    
    // Parse JSON response: {"success":true,"key":"0123456789ABCDEF0123456789ABCDEF"}
    int keyStart = response.indexOf("\"key\":\"") + 7;
    int keyEnd = response.indexOf("\"", keyStart);
    
    if (keyStart < 7 || keyEnd < 0) {
      String errorMsg = "Server response bevat geen key";
      Serial.println(F("❌ Invalid server response (no key field)"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    String keyHex = response.substring(keyStart, keyEnd);
    
    if (keyHex.length() != 32) {
      String errorMsg = "Server key is niet 32 hex chars (16 bytes)";
      Serial.print(F("❌ Invalid key length from server: "));
      Serial.println(keyHex.length());
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    // Convert hex string to bytes
    size_t keyLen = NTAG424Crypto::hexStringToBytes(keyHex, derivedKey, 16);
    if (keyLen != 16) {
      String errorMsg = "Server key conversie mislukt";
      Serial.println(F("❌ Key conversion from hex failed"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    Serial.println(F("✅ Key received from server"));
    Serial.print(F("Server K0: "));
    for (int i = 0; i < 16; i++) {
      char buf[3];
      sprintf(buf, "%02X", derivedKey[i]);
      Serial.print(buf);
    }
    Serial.println();
    
  } else {
    String errorMsg = "Onbekende key source: " + keySource;
    Serial.println(F("❌ Unknown key source"));
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    return;
  }
  
  // Now derivedKey is available (from either ESP32 or Server mode)
  // Continue with AN12196 personalization flow
  
  Serial.println(F("\n[2] NTAG424 HANDLER INITIALIZATION"));
  if (ntag424Handler == nullptr) {
    String errorMsg = "NTAG424 handler niet geïnitialiseerd";
    Serial.println(F("    ❌ Handler not initialized"));
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    return;
  }
  Serial.println(F("    ✅ Handler ready"));
  
  // ═══════════════════════════════════════════════════════════════
  // AN12196 Section 6.1: ISO14443-4 PICC Activation
  // AN12196 Section 6.3: ISO SELECT NDEF Application
  // (Both handled by activateCard() function)
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[3] ISO-DEP ACTIVATION (AN12196 §6.1 & §6.3)"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Activeer ISO-DEP communicatie...");
  
  if (!ntag424Handler->activateCard()) {
    String errorMsg = "ISO-DEP activatie mislukt - geen DESFire support?";
    Serial.println(F("    ❌ Failed to activate card for ISO-DEP communication"));
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    return;
  }
  
  Serial.println(F("    ✅ ISO-DEP active"));
  Serial.println(F("    ✅ NDEF application selected"));
  
  // ═══════════════════════════════════════════════════════════════
  // AN12196 Section 6.5: GetVersion
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[4] GET VERSION (AN12196 §6.5)"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Verifieer kaart communicatie...");
  
  uint8_t versionInfo[28];
  if (!ntag424Handler->getVersion(versionInfo)) {
    String errorMsg = "Geen communicatie met kaart - controleer kaarttype";
    Serial.println(F("    ❌ Failed to communicate with card"));
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    return;
  }
  
  Serial.println(F("    ✅ Card communication OK"));
  
  // ═══════════════════════════════════════════════════════════════
  // DETERMINE OLD KEY (Factory or Previous)
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[5] DETERMINE OLD KEY"));
  bool isFactory = systemConfig.getIsFactory();
  uint8_t oldKey[16];
  
  if (isFactory) {
    // Factory kaart: gebruik default key (0x00...00)
    memcpy(oldKey, NTAG424Handler::DEFAULT_AES_KEY, 16);
    Serial.println(F("    Type: Factory card"));
    Serial.println(F("    Old Key: 00000000000000000000000000000000"));
    webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met factory key...");
  } else {
    // Reeds gepersonaliseerde kaart: gebruik previous key
    String prevKeyHex = systemConfig.getPreviousKey();
    if (prevKeyHex.length() != 32) {
      String errorMsg = "Vorige key ontbreekt of ongeldig";
      Serial.println(F("    ❌ Previous key missing or invalid"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    size_t keyLen = NTAG424Crypto::hexStringToBytes(prevKeyHex, oldKey, 16);
    if (keyLen != 16) {
      String errorMsg = "Vorige key conversie mislukt";
      Serial.println(F("    ❌ Previous key conversion failed"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    Serial.println(F("    Type: Personalized card"));
    Serial.print(F("    Old Key: "));
    Serial.print(prevKeyHex.substring(0, 8));
    Serial.println(F("..."));
    webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met vorige key...");
  }
  
  // ═══════════════════════════════════════════════════════════════
  // AN12196 Section 6.6: AuthenticateEV2First with Key 0x00
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[6] AUTHENTICATE EV2 FIRST (AN12196 §6.6)"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met oude key...");
  
  NTAG424Handler::AuthResult authResult;
  bool authenticated = ntag424Handler->authenticateEV2First(
    0,
    oldKey,
    authResult
  );
  
  if (!authenticated) {
    String errorMsg = "Authenticatie mislukt: " + authResult.errorMessage;
    Serial.print(F("    ❌ Authentication failed: "));
    Serial.println(authResult.errorMessage);
    if (!isFactory) {
      errorMsg += " (Controleer of vorige key correct is)";
    }
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    ntag424Handler->resetSession();
    return;
  }
  
  Serial.println(F("    ✅ Authentication successful"));
  Serial.println(F("    Session keys established"));
  
  // ═══════════════════════════════════════════════════════════════
  // AN12196 Section 6.16: ChangeKey (Master Key 0x00)
  // Case 2 (Table 27): KeyNo to be changed = AuthKey
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[7] CHANGE KEY (AN12196 §6.16.2)"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Schrijf nieuwe masterkey (K0)...");
  
  bool keyChanged = ntag424Handler->changeKey(
    0,  // Key number 0 (master key)
    oldKey,  // Old key (factory or previous)
    derivedKey  // New derived key
  );
  
  if (!keyChanged) {
    String errorMsg = "Schrijven masterkey mislukt";
    Serial.println(F("    ❌ ChangeKey failed"));
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    ntag424Handler->resetSession();
    return;
  }
  
  Serial.println(F("    ✅ Master key (K0) written successfully"));
  
  // ═══════════════════════════════════════════════════════════════
  // VERIFICATION: Re-authenticate with new key
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[8] VERIFICATION"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Verificeer nieuwe key...");
  
  NTAG424Handler::AuthResult verifyResult;
  bool verified = ntag424Handler->authenticateEV2First(
    0,
    derivedKey,
    verifyResult
  );
  
  if (verified) {
    Serial.println(F("    ✅ Verification successful"));
    Serial.println(F("\n╔═══════════════════════════════════════════╗"));
    Serial.println(F("║     ✅ CARD WRITE COMPLETE! ✅            ║"));
    Serial.println(F("╚═══════════════════════════════════════════╝"));
    webServer.broadcastWriteCardStatus(uid, "success", "Kaart succesvol geschreven en geverifieerd!");
    
    // Check if we're in single mode - if so, stop after one card
    if (systemConfig.isSingleWriteMode()) {
      Serial.println(F("Single mode - stopping after one card"));
      systemConfig.setWriteActive(false);
      systemConfig.clearMasterSecret();
      systemConfig.clearPreviousKey();
      systemConfig.setKeySource("esp32");  // Reset to default ESP32 mode
      webServer.broadcastLog("Single mode: schrijven gestopt na 1 kaart (key source reset naar ESP32)", "info");
    }
  } else {
    String errorMsg = "Verificatie mislukt: " + verifyResult.errorMessage;
    Serial.print(F("⚠️ Verification failed: "));
    Serial.println(verifyResult.errorMessage);
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
  }
  
  // Reset ISO-DEP session for next card
  if (ntag424Handler != nullptr) {
    ntag424Handler->resetSession();
  }
  
  // In continuous mode, wait for card removal before next
  if (systemConfig.isWriteActive() && systemConfig.isContinuousWriteMode()) {
    Serial.println(F("\n👉 Remove card and present next card to write"));
    webServer.broadcastLog("Verwijder kaart - klaar voor volgende kaart", "info");
  }
}

// ============ TEST COMMAND HANDLER ============

void handleSerialCommands() {
  // Check if serial data is available
  if (!Serial.available()) {
    return;
  }
  
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();
  
  // Ignore empty commands
  if (command.length() == 0) {
    return;
  }
  
  // Test commands: test0 through test5
  if (command.startsWith("test")) {
    if (command.length() != 5) {
      Serial.println(F("\n❌ Invalid test command format"));
      Serial.println(F("Usage: test0, test1, test2, test3, test4, or test5"));
      return;
    }
    
    uint8_t testMode = command.charAt(4) - '0';
    
    if (testMode > 5) {
      Serial.println(F("\n❌ Invalid test mode. Valid range: 0-5"));
      return;
    }
    
    Serial.println(F("\n════════════════════════════════════════════════════"));
    Serial.printf("  TEST MODE %d ACTIVATED\n", testMode);
    Serial.println(F("════════════════════════════════════════════════════"));
    Serial.println(F("\n⚠️  Prerequisites:"));
    Serial.println(F("  1. Place NTAG424 DNA card on reader"));
    Serial.println(F("  2. Card must have factory key (00...00)"));
    Serial.println(F("  3. Or specify custom keys in test code"));
    Serial.println(F("\n▶ Starting test in 3 seconds..."));
    Serial.println(F("  (Remove serial command to abort)"));
    Serial.println();
    
    delay(3000);
    
    // Check if NTAG424 handler is available
    if (ntag424Handler == nullptr) {
      Serial.println(F("\n❌ NTAG424 Handler not initialized!"));
      Serial.println(F("System must be fully initialized first."));
      webServer.broadcastLog("Test afgebroken: NTAG424 niet beschikbaar", "error");
      return;
    }
    
    // Check for card presence
    NFCReader::CardInfo cardInfo;
    if (!nfcReader->readCard(cardInfo)) {
      Serial.println(F("\n❌ No card detected!"));
      Serial.println(F("Place NTAG424 DNA card on reader and try again."));
      webServer.broadcastLog("Test afgebroken: geen kaart gedetecteerd", "error");
      return;
    }
    
    Serial.println(F("✅ Card detected!"));
    Serial.print(F("   UID: "));
    for (int i = 0; i < cardInfo.uidLength; i++) {
      Serial.printf("%02X", cardInfo.uid[i]);
    }
    Serial.println();
    
    // Activate card for ISO-DEP
    if (!ntag424Handler->activateCard()) {
      Serial.println(F("\n❌ Failed to activate ISO-DEP!"));
      webServer.broadcastLog("Test afgebroken: ISO-DEP activatie mislukt", "error");
      return;
    }
    
    Serial.println(F("✅ ISO-DEP activated"));
    
    // Select NDEF application
    if (!ntag424Handler->selectNdefApplication()) {
      Serial.println(F("\n❌ Failed to select NDEF application!"));
      webServer.broadcastLog("Test afgebroken: NDEF selectie mislukt", "error");
      return;
    }
    
    Serial.println(F("✅ NDEF application selected"));
    
    // Prepare keys for test (factory key → factory key, just to test the protocol)
    uint8_t oldKey[16] = {0};  // Factory default key: 00 00 ... 00
    uint8_t newKey[16] = {0};  // Same key (test purposes only)
    
    Serial.println(F("\n🔬 Starting ChangeKey test..."));
    Serial.println(F("   OldKey: 00000000000000000000000000000000 (factory)"));
    Serial.println(F("   NewKey: 00000000000000000000000000000000 (same for test)"));
    Serial.println(F("   KeyNo:  0 (master key)"));
    Serial.println();
    
    webServer.broadcastLog("ChangeKey test gestart...", "info");
    
    // Run the test (note: testMode parameter is no longer used)
    bool result = ntag424Handler->changeKey(0, oldKey, newKey);
    
    if (result) {
      Serial.println(F("\n╔═══════════════════════════════════════════════════════╗"));
      Serial.println(F("║                  🎉 TEST SUCCESS! 🎉                  ║"));
      Serial.println(F("╚═══════════════════════════════════════════════════════╝"));
      Serial.println(F("\nChangeKey test succeeded!"));
      Serial.println(F("Key change completed successfully."));
      Serial.println();
      
      webServer.broadcastLog("ChangeKey test GESLAAGD! 🎉", "success");
    } else {
      Serial.println(F("\n╔═══════════════════════════════════════════════════════╗"));
      Serial.println(F("║                    ❌ TEST FAILED                     ║"));
      Serial.println(F("╚═══════════════════════════════════════════════════════╝"));
      Serial.println(F("\nChangeKey test failed."));
      Serial.println(F("Check logs for details."));
      Serial.println();
      
      webServer.broadcastLog("ChangeKey test mislukt", "error");
    }
    
    // Reset session after test
    ntag424Handler->resetSession();
    Serial.println(F("Session reset - ready for next test\n"));
    
    return;
  }
  
  // Help command
  if (command == "help" || command == "?") {
    Serial.println(F("\n════════════════════════════════════════════════════"));
    Serial.println(F("  AVAILABLE TEST COMMANDS"));
    Serial.println(F("════════════════════════════════════════════════════"));
    Serial.println(F("  test0 - Current implementation (baseline)"));
    Serial.println(F("  test1 - P1=KeyNo (ISO-wrapped mode)"));
    Serial.println(F("  test2 - omitLe=true (Case 3 APDU)"));
    Serial.println(F("  test3 - CmdCtr=0 forced"));
    Serial.println(F("  test4 - Combo: P1=KeyNo + omitLe"));
    Serial.println(F("  test5 - Combo: omitLe + CmdCtr=0"));
    Serial.println(F("  help  - Show this help message"));
    Serial.println(F("════════════════════════════════════════════════════\n"));
    return;
  }
  
  // Unknown command
  Serial.print(F("\n❌ Unknown command: "));
  Serial.println(command);
  Serial.println(F("Type 'help' for available commands"));
}