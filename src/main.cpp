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

// ============ CACHED CHALLENGE (for fast card scanning) ============

uint8_t cachedChallenge[16] = {0};
bool hasCachedChallenge = false;

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
    
    // Ping server immediately and request first nonce if online
    Serial.println(F("\n=== Initial Server Ping ==="));
    if (serverClient.testConnection()) {
      Serial.println(F("\n=== Initial Challenge ==="));
      ServerClient::ChallengeData challengeData = serverClient.requestInitialChallenge();
      
      if (challengeData.success && challengeData.challenge.length() == 32) {  // 32 hex chars = 16 bytes
        // Convert hex to bytes
        for (int i = 0; i < 16; i++) {
          char hex[3] = {challengeData.challenge[i*2], challengeData.challenge[i*2+1], 0};
          cachedChallenge[i] = (uint8_t)strtol(hex, NULL, 16);
        }
        hasCachedChallenge = true;
        
        Serial.println(F("✅ Initial challenge cached for card scanning"));
        webServer.broadcastLog("✅ Challenge voorbereid voor snelle scans", "success");
      } else {
        Serial.println(F("⚠️ No initial challenge - will request per card"));
        webServer.broadcastLog("⚠️ Geen challenge - request per kaart", "warning");
      }
    } else {
      Serial.println(F("⚠️ Server offline at boot - will retry when card scanned"));
    }
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
  
  // Check write mode timeout (5 minutes)
  if (systemConfig.checkWriteTimeout()) {
    webServer.broadcastLog("Write mode timeout - secrets gewist", "warning");
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

    // After card handled: proactively request nonce for next scan
    if (serverClient.isServerOnline() && !hasCachedChallenge) {
      ServerClient::ChallengeData cd = serverClient.requestInitialChallenge();
      if (cd.success && cd.challenge.length() == 32) {
        for (int i = 0; i < 16; i++) {
          char h[3] = {cd.challenge[i*2], cd.challenge[i*2+1], 0};
          cachedChallenge[i] = (uint8_t)strtol(h, NULL, 16);
        }
        hasCachedChallenge = true;
        Serial.println(F("✅ Nonce klaar voor volgende scan"));
      }
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
  webServer.broadcastLog("Machine modus - challenge-response authenticatie", "info");
  
  // Check if it's a non-NTAG424 card
  if (cardInfo.cardType.indexOf("NTAG424") < 0 && 
      cardInfo.cardType.indexOf("DESFire") < 0 &&
      cardInfo.cardType.indexOf("SECURE") < 0) {
    Serial.println(F("⚪ Niet-NTAG424 kaart gedetecteerd"));
    Serial.print(F("   Type: "));
    Serial.println(cardInfo.cardType);
    webServer.broadcastLog("⚪ Niet-NTAG424: " + cardInfo.cardType, "info");
    
    // Send to server as non-NTAG424
    if (serverClient.isServerOnline()) {
      serverClient.sendScan(cardInfo.uidString, "non_ntag424", cardInfo.cardType);
    }
    return;
  }
  
  // Reset handler
  if (ntag424Handler == nullptr) {
    Serial.println(F("❌ NTAG424 handler niet geïnitialiseerd"));
    webServer.broadcastLog("❌ NTAG424 handler niet geïnitialiseerd", "error");
    return;
  }
  
  ntag424Handler->resetSession();
  
  // Step 1: Activate card
  webServer.broadcastLog("Stap 1: Kaart activeren...", "info");
  if (!ntag424Handler->activateCard()) {
    Serial.println(F("❌ Failed to activate card"));
    webServer.broadcastLog("❌ Kaart activatie mislukt", "error");
    return;
  }
  
  Serial.println(F("✅ Card activated"));
  
  // Step 2: Get key for this card from server
  webServer.broadcastLog("Stap 2: Key ophalen van server...", "info");
  String keyHex = serverClient.getCardKey(cardInfo.uidString, "master");
  
  if (keyHex.length() != 32) {
    Serial.println(F("❌ No key received from server"));
    webServer.broadcastLog("❌ Geen key van server ontvangen", "error");
    return;
  }
  
  Serial.print(F("✅ Key received: "));
  Serial.println(keyHex);
  
  // Convert key from hex to bytes
  uint8_t key[16];
  for (int i = 0; i < 16; i++) {
    char hex[3] = {keyHex[i*2], keyHex[i*2+1], 0};
    key[i] = (uint8_t)strtol(hex, NULL, 16);
  }
  
  // Step 3: Authenticate with card
  webServer.broadcastLog("Stap 3: Authenticeren met kaart...", "info");
  NTAG424Handler::AuthResult authResult;
  bool authenticated = ntag424Handler->authenticateEV2First(0, key, authResult);
  
  if (!authenticated) {
    Serial.print(F("❌ Authentication failed: "));
    Serial.println(authResult.errorMessage);
    webServer.broadcastLog("❌ Authenticatie mislukt: " + authResult.errorMessage, "error");
    return;
  }
  
  Serial.println(F("✅ Authentication successful"));
  
  // Step 4: Request challenge from server (this will be our RndA)
  webServer.broadcastLog("Stap 4: Challenge opvragen van server...", "info");
  String serverChallenge = serverClient.requestChallenge(cardInfo.uidString);
  
  if (serverChallenge.length() != 32) {  // Must be 32 hex chars = 16 bytes
    Serial.println(F("❌ Invalid challenge received from server"));
    webServer.broadcastLog("❌ Ongeldige challenge van server", "error");
    return;
  }
  
  Serial.print(F("✅ Challenge ontvangen: "));
  Serial.println(serverChallenge);
  webServer.broadcastLog("Challenge: " + serverChallenge.substring(0, 16) + "...", "success");
  
  // Convert challenge from hex to bytes (this will be our RndA)
  uint8_t externalRndA[16];
  for (int i = 0; i < 16; i++) {
    char hex[3] = {serverChallenge[i*2], serverChallenge[i*2+1], 0};
    externalRndA[i] = (uint8_t)strtol(hex, NULL, 16);
  }
  
  // Step 5: Re-authenticate with card using server's RndA
  webServer.broadcastLog("Stap 5: Re-authenticeren met server's RndA...", "info");
  ntag424Handler->resetSession();  // Reset before re-auth
  
  NTAG424Handler::AuthResult cryptoResult;
  authenticated = ntag424Handler->authenticateEV2First(0, key, externalRndA, cryptoResult);
  
  if (!authenticated) {
    Serial.print(F("❌ Crypto authentication failed: "));
    Serial.println(cryptoResult.errorMessage);
    webServer.broadcastLog("❌ Crypto authenticatie mislukt: " + cryptoResult.errorMessage, "error");
    return;
  }
  
  Serial.println(F("✅ Crypto authentication successful"));
  
  // Step 6: Extract crypto parameters for server verification
  webServer.broadcastLog("Stap 6: Crypto parameters extraheren...", "info");
  
  // Convert encrypted RndB to hex string
  String encRndBHex = "";
  for (int i = 0; i < 16; i++) {
    char hex[3];
    sprintf(hex, "%02X", cryptoResult.encryptedRndB[i]);
    encRndBHex += hex;
  }
  
  // Convert encrypted response (from card) to hex string
  String responseHex = "";
  for (int i = 0; i < 32; i++) {
    char hex[3];
    sprintf(hex, "%02X", cryptoResult.encryptedResponse[i]);
    responseHex += hex;
  }
  
  // Convert TI to hex string
  String tiHex = "";
  for (int i = 0; i < 4; i++) {
    char hex[3];
    sprintf(hex, "%02X", cryptoResult.transactionId[i]);
    tiHex += hex;
  }
  
  Serial.print(F("Encrypted RndB: "));
  Serial.println(encRndBHex);
  Serial.print(F("Encrypted Response (from card): "));
  Serial.println(responseHex);
  Serial.print(F("Transaction ID: "));
  Serial.println(tiHex);
  
  webServer.broadcastLog("RndB (enc): " + encRndBHex.substring(0, 16) + "...", "info");
  webServer.broadcastLog("Response: " + responseHex.substring(0, 16) + "...", "info");
  webServer.broadcastLog("TI: " + tiHex, "info");
  
  // Step 8: Verify with server using FULL CRYPTO MODE
  webServer.broadcastLog("Stap 8: Verificatie bij server (CRYPTO MODE)...", "info");
  bool verified = serverClient.verifyResponse(cardInfo.uidString, responseHex, encRndBHex, tiHex);
  
  if (verified) {
    Serial.println(F("✅ ACCESS GRANTED - Cryptographic verification successful"));
    webServer.broadcastLog("✅ TOEGANG VERLEEND - Cryptografische verificatie succesvol", "success");
  } else {
    Serial.println(F("❌ ACCESS DENIED - Cryptographic verification failed"));
    webServer.broadcastLog("❌ TOEGANG GEWEIGERD - Cryptografische verificatie mislukt", "error");
  }
  
  // Reset for next card
  ntag424Handler->resetSession();
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
    Serial.println(F("⚪ Niet-NTAG424 kaart gedetecteerd in config mode"));
    webServer.broadcastLog("⚠️ Geen NTAG424 DNA kaart gedetecteerd", "warning");
    webServer.broadcastLog("Card type: " + cardInfo.cardType, "info");
    
    // Send to server as non-NTAG424
    if (serverClient.isServerOnline()) {
      serverClient.sendScan(cardInfo.uidString, "non_ntag424", cardInfo.cardType);
      webServer.broadcastLog("✅ Scan verzonden naar server", "success");
    }
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
  
  // ═══════════════════════════════════════════════════════════════
  // ═══════════════════════════════════════════════════════════════
  // SCAN FLOW: nonce-based, single auth per card
  //   1. POST /api/scan/start {uid, readerId} → derivedKey
  //   2. Authenticate card once with derivedKey + nonce as RndA
  //   3. POST /api/scan/verify (scanWithProof) → card details + next nonce
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n🔍 Scanning card..."));
  String cardStatus = "unknown";
  bool cryptoProofSent = false;
  uint8_t derivedKey[16] = {0};
  
  if (hasCachedChallenge && serverClient.isServerOnline()) {
    // Step 1: POST /api/scan/start {uid, readerId}
    // Server looks up nonce by readerId and returns derived key K0
    Serial.println(F("   [1] scan/start → server..."));
    ServerClient::StartScanResult startResult = serverClient.startScan(
      cardInfo.uidString
    );

    if (startResult.success && startResult.cardKnown && startResult.derivedKeyHex.length() == 32) {
      // Parse derived key
      for (int i = 0; i < 16; i++) {
        char hex[3] = {startResult.derivedKeyHex[i*2], startResult.derivedKeyHex[i*2+1], 0};
        derivedKey[i] = (uint8_t)strtol(hex, NULL, 16);
      }
      Serial.print(F("   K0: "));
      Serial.println(startResult.derivedKeyHex);

      // Step 2: Authenticate directly – ISO-DEP already active from top of handleConfigMode
      {
        NTAG424Handler::AuthResult cryptoResult;
        bool cryptoAuth = ntag424Handler->authenticateEV2First(0, derivedKey, cachedChallenge, cryptoResult);

        if (cryptoAuth) {
          cardStatus = "personalized";
          Serial.println(F("   ✅ Auth OK → PERSONALIZED"));
          webServer.broadcastLog("✅ Gepersonaliseerde kaart", "success");

          // Step 3: POST /api/scan-with-proof → verify + get next nonce
          String encRndBHex = "", encResponseHex = "", tiHex = "";
          for (int i = 0; i < 16; i++) { char h[3]; sprintf(h, "%02X", cryptoResult.encryptedRndB[i]); encRndBHex += h; }
          for (int i = 0; i < 32; i++) { char h[3]; sprintf(h, "%02X", cryptoResult.encryptedResponse[i]); encResponseHex += h; }
          for (int i = 0; i < 4;  i++) { char h[3]; sprintf(h, "%02X", cryptoResult.transactionId[i]); tiHex += h; }

          ServerClient::ScanResult scanResult = serverClient.scanWithProof(
            cardInfo.uidString, encRndBHex, encResponseHex, tiHex
          );

          if (scanResult.success) {
            webServer.broadcastLog("✅ " + scanResult.message, "success");
            if (scanResult.credits > 0) webServer.broadcastLog("💰 Credits: " + String(scanResult.credits), "success");
            cryptoProofSent = true;

            // Cache next nonce for next card scan
            if (scanResult.nextChallenge.length() == 32) {
              for (int i = 0; i < 16; i++) {
                char h[3] = {scanResult.nextChallenge[i*2], scanResult.nextChallenge[i*2+1], 0};
                cachedChallenge[i] = (uint8_t)strtol(h, NULL, 16);
              }
              Serial.println(F("   ✅ Next nonce cached"));
            } else {
              // Server didn't return a next nonce - invalidate cache so next
              // scan requests a fresh one instead of reusing a spent nonce
              hasCachedChallenge = false;
              Serial.println(F("   ⚠️ No next nonce in response - will request fresh nonce next scan"));
            }
          } else {
            Serial.println(F("   ❌ Server verification failed"));
          }
        } else {
          Serial.println(F("   ❌ Card auth failed (wrong key?)"));
        }
      }

    } else if (startResult.success && !startResult.cardKnown) {
      // Card not registered on server - nonce not consumed, keep hasCachedChallenge = true
      // ISO-DEP already active, try factory key directly
      Serial.println(F("   Card unknown to server - trying factory key..."));
      {
        uint8_t factoryKey[16] = {0};
        NTAG424Handler::AuthResult authResult;
        if (ntag424Handler->authenticateEV2First(0, factoryKey, authResult)) {
          cardStatus = "factory";
          Serial.println(F("   ✅ Factory key works → FACTORY"));
          webServer.broadcastLog("✅ Factory kaart", "success");
        }
      }

    } else {
      // scan/start failed - invalidate nonce, it may be stale
      hasCachedChallenge = false;
      Serial.println(F("   ❌ scan/start failed - server error"));
    }

  } else {
    // No nonce cached - request a fresh one from server, then retry the full flow
    if (serverClient.isServerOnline()) {
      Serial.println(F("   [No nonce] Requesting fresh nonce from server..."));
      ServerClient::ChallengeData cd = serverClient.requestInitialChallenge();
      if (cd.success && cd.challenge.length() == 32) {
        for (int i = 0; i < 16; i++) {
          char h[3] = {cd.challenge[i*2], cd.challenge[i*2+1], 0};
          cachedChallenge[i] = (uint8_t)strtol(h, NULL, 16);
        }
        hasCachedChallenge = true;
        Serial.println(F("   ✅ Fresh nonce cached"));
        // Re-run scan immediately with fresh nonce
        // (handleMachineMode/handleConfigMode will be called again on next card present,
        //  but we can call ourselves recursively here since nonce is now ready)
        handleConfigMode(cardInfo);
        return;
      }
    }
    // Server offline or nonce request failed - try factory key only
    Serial.println(F("   [Offline/no-nonce] Trying factory key..."));
    uint8_t factoryKey[16] = {0};
    NTAG424Handler::AuthResult authResult;
    if (ntag424Handler->authenticateEV2First(0, factoryKey, authResult)) {
      cardStatus = "factory";
      webServer.broadcastLog("✅ Factory kaart (offline)", "success");
    }
  }

  if (cardStatus == "unknown") {
    Serial.println(F("   ⚠️  Status: UNKNOWN"));
    webServer.broadcastLog("⚠️ Onbekende kaart", "warning");
  }
  
  // Log final status
  Serial.print(F("\n📊 Final Status: "));
  Serial.println(cardStatus);
  webServer.broadcastLog("Status: " + cardStatus, "info");
  
  // ═══════════════════════════════════════════════════════════════
  // SEND SCAN TO SERVER (if crypto proof not already sent)
  // ═══════════════════════════════════════════════════════════════
  if (!cryptoProofSent && serverClient.isServerOnline()) {
    Serial.println(F("\n📤 Sending simple scan to server..."));
    bool sent = serverClient.sendScan(cardInfo.uidString, cardStatus, cardInfo.cardType);
    if (sent) {
      webServer.broadcastLog("✅ Scan verzonden naar server", "success");
    } else {
      webServer.broadcastLog("⚠️ Scan verzenden mislukt", "warning");
    }
  } else if (!cryptoProofSent) {
    Serial.println(F("⚠️  Server offline - scan not logged"));
    webServer.broadcastLog("⚠️ Server offline - scan niet gelogd", "warning");
  } else {
    Serial.println(F("✅ Crypto proof already sent - skip simple scan"));
  }
  
  // ═══════════════════════════════════════════════════════════════
  // DISPLAY INFO
  // ═══════════════════════════════════════════════════════════════
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
  bool isDirectKey = systemConfig.getIsDirectKey();
  uint8_t oldKey[16];
  
  if (isFactory) {
    // Factory kaart: gebruik default key (0x00...00)
    memcpy(oldKey, NTAG424Handler::DEFAULT_AES_KEY, 16);
    Serial.println(F("    Type: Factory card"));
    Serial.println(F("    Old Key: 00000000000000000000000000000000"));
    webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met factory key...");
  } else {
    // Reeds gepersonaliseerde kaart
    String prevKeyHex = systemConfig.getPreviousKey();
    if (prevKeyHex.length() != 32) {
      String errorMsg = "Vorige key ontbreekt of ongeldig";
      Serial.println(F("    ❌ Previous key missing or invalid"));
      webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
      return;
    }
    
    if (isDirectKey) {
      // Direct Key Mode: prevKeyHex is de directe AES key (geen derivation)
      Serial.println(F("    Type: Personalized card (Direct Key Mode)"));
      Serial.print(F("    Direct AES key (hex): "));
      Serial.println(prevKeyHex);
      
      // Convert hex string to bytes using robust method
      size_t keyLen = NTAG424Crypto::hexStringToBytes(prevKeyHex, oldKey, 16);
      if (keyLen != 16) {
        String errorMsg = "Direct key conversie mislukt";
        Serial.println(F("    ❌ Failed to convert direct key from hex"));
        webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
        return;
      }
      
      Serial.print(F("    Direct AES key (bytes): "));
      Serial.println(NTAG424Crypto::bytesToHexString(oldKey, 16));
      
      webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met directe AES key...");
      
    } else {
      // Normal Mode: prevKeyHex is master secret, derive K0
      Serial.println(F("    Type: Personalized card (Master Secret Mode)"));
      Serial.print(F("    Previous master secret: "));
      Serial.print(prevKeyHex.substring(0, 8));
      Serial.println(F("..."));
      
      // Derive K0 from previous master secret and UID
      if (!NTAG424Crypto::deriveMasterKey(prevKeyHex, uid, oldKey, 1)) {
        String errorMsg = "Kan key niet afleiden van master secret";
        Serial.println(F("    ❌ Failed to derive key from master secret"));
        webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
        return;
      }
      
      Serial.print(F("    Derived previous K0: "));
      Serial.println(NTAG424Crypto::bytesToHexString(oldKey, 16));
      webServer.broadcastWriteCardStatus(uid, "processing", "Authenticeer met afgeleide vorige key...");
    }
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
  // NOTE: GetKeySettings SKIPPED - sends plain command after auth
  // which can corrupt session state. Not critical for ChangeKey.
  // TODO: Implement proper authenticated GetKeySettings with MAC
  // ═══════════════════════════════════════════════════════════════
  
  // ═══════════════════════════════════════════════════════════════
  // AN12196 Section 6.16: ChangeKey (Master Key 0x00)
  // Case 2 (Table 27): KeyNo to be changed = AuthKey
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[7] CHANGE KEY (AN12196 §6.16.2)"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Schrijf nieuwe masterkey (K0)...");
  
  // ═══════════════════════════════════════════════════════════════
  // DEBUG: Show keys and master secret for verification
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n╔══════════════════════════════════════════════════════════════╗"));
  Serial.println(F("║             KEY DERIVATION VERIFICATION DATA                 ║"));
  Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
  
  Serial.print(F("\n[INPUT TO DERIVATION]"));
  Serial.println(F("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
  Serial.print(F("Master Secret:  "));
  if (keySource == "esp32") {
    // SECURITY: Only show preview, never full secret
    String secret = systemConfig.getMasterSecret();
    Serial.print(secret.substring(0, 8));
    Serial.println("... (preview only)");
  } else {
    Serial.println("(from server)");
  }
  Serial.print(F("Card UID:       "));
  Serial.println(uid);
  Serial.print(F("Derivation:     HMAC-SHA256(secret, UID + 'K0' + 0x01)"));
  Serial.println();
  
  Serial.println(F("\n[DERIVED KEYS]"));
  Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
  Serial.print(F("Old Key (authenticate): "));
  for (int i = 0; i < 16; i++) {
    char buf[3];
    sprintf(buf, "%02X", oldKey[i]);
    Serial.print(buf);
  }
  Serial.println();
  
  Serial.print(F("New Key (to write):     "));
  for (int i = 0; i < 16; i++) {
    char buf[3];
    sprintf(buf, "%02X", derivedKey[i]);
    Serial.print(buf);
  }
  Serial.println();
  Serial.println();
  
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
  // NOTE: CommitTransaction is NOT needed on NTAG424 DNA!
  // NTAG424 DNA has ONLY StandardData files which are NOT covered
  // by the transaction mechanism (NT4H2421Gx datasheet, Section 8.2.3.1):
  // "A StandardData file is not covered by the transaction mechanism"
  // ChangeKey on NTAG424 DNA is self-committing (AN12196 Table 27,
  // step 18: R-APDU=9100 with NO subsequent CommitTransaction).
  // Calling CommitTransaction after ChangeKey returns SW=911C
  // (no changes pending) and corrupts the session.
  // ═══════════════════════════════════════════════════════════════
  
  // ═══════════════════════════════════════════════════════════════
  // VERIFICATION: Re-authenticate with new key
  // Best practice: Full challenge-response test (same as server does)
  // ═══════════════════════════════════════════════════════════════
  Serial.println(F("\n[8] VERIFICATION - CRYPTO CHALLENGE-RESPONSE TEST"));
  Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
  webServer.broadcastWriteCardStatus(uid, "processing", "Verificeer nieuwe key (crypto test)...");
  
  // CRITICAL: Reset session before verifying with new key!
  // After ChangeKey, the session is still using OLD key's session keys
  // We need a fresh authentication with the NEW key
  ntag424Handler->resetSession();
  Serial.println(F("    🔄 Session reset for fresh authentication"));
  
  Serial.println(F("\n    [Test 1/2] First Challenge-Response Verification"));
  Serial.println(F("    ─────────────────────────────────────────────────"));
  NTAG424Handler::AuthResult verifyResult;
  bool verified = ntag424Handler->authenticateEV2First(
    0,
    derivedKey,
    verifyResult
  );
  
  if (!verified) {
    String errorMsg = "Verificatie mislukt - nieuwe key werkt niet!";
    Serial.println(F("    ❌ VERIFICATION FAILED!"));
    Serial.println(F("       Card did NOT accept the new key"));
    Serial.println(F("       This means ChangeKey did not work correctly"));
    
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    ntag424Handler->resetSession();
    return;
  }
  
  Serial.println(F("    ✅ First verification successful!"));
  Serial.println(F("       - Random challenge (RndA) generated"));
  Serial.println(F("       - Card responded with valid crypto"));
  Serial.println(F("       - RndA' verification passed (crypto correct!)"));
  Serial.print(F("       - Transaction ID: "));
  Serial.println(NTAG424Crypto::bytesToHexString(verifyResult.transactionId, 4));
  
  // SECOND VERIFICATION: Do it again with a different challenge for extra confidence
  Serial.println(F("\n    [Test 2/2] Second Challenge-Response Verification"));
  Serial.println(F("    ─────────────────────────────────────────────────"));
  Serial.println(F("    (Testing with fresh random challenge for extra confidence)"));
  
  ntag424Handler->resetSession();
  
  NTAG424Handler::AuthResult verifyResult2;
  bool verified2 = ntag424Handler->authenticateEV2First(
    0,
    derivedKey,
    verifyResult2
  );
  
  if (!verified2) {
    String errorMsg = "Tweede verificatie mislukt";
    Serial.println(F("    ❌ Second verification failed"));
    Serial.println(F("       Card was inconsistent - possible write issue"));
    
    webServer.broadcastWriteCardStatus(uid, "error", errorMsg);
    ntag424Handler->resetSession();
    return;
  }
  
  Serial.println(F("    ✅ Second verification successful!"));
  Serial.print(F("       - Transaction ID: "));
  Serial.println(NTAG424Crypto::bytesToHexString(verifyResult2.transactionId, 4));
  Serial.println(F("       - Card consistently responds to crypto challenges"));
  
  Serial.println(F("\n    ╔═══════════════════════════════════════════════════╗"));
  Serial.println(F("    ║  ✅ CARD FULLY VERIFIED - NEW KEY WORKS! ✅      ║"));
  Serial.println(F("    ╚═══════════════════════════════════════════════════╝"));
  Serial.println(F("    Both crypto challenge-response tests passed:"));
  Serial.println(F("    • ESP32 generated random challenges"));
  Serial.println(F("    • Card computed correct crypto responses"));
  Serial.println(F("    • RndA' verification confirmed key is correct"));
  Serial.println(F("    This is the SAME test the server will do!"));
  Serial.println();
  
  Serial.println(F("\n╔═══════════════════════════════════════════╗"));
  Serial.println(F("║     ✅ CARD WRITE COMPLETE! ✅            ║"));
  Serial.println(F("╚═══════════════════════════════════════════╝"));
  webServer.broadcastWriteCardStatus(uid, "success", "Kaart succesvol geschreven en volledig geverifieerd!");
  
  // Check if we're in single mode - if so, stop after one card
  if (systemConfig.isSingleWriteMode()) {
    Serial.println(F("Single mode - auto-cleanup na 1 kaart"));
    systemConfig.stopWriteMode();
    webServer.broadcastLog("Single mode: write mode gestopt en secrets gewist", "info");
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