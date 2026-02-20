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
void checkConfigButton();

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
    
    // Handle based on reader mode
    if (systemConfig.isMachineMode()) {
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
  Serial.println(F("\n=== CONFIG MODE - NTAG424 PERSONALIZATION ==="));
  webServer.broadcastLog("🔧 Config modus - Start NTAG424 personalisatie", "info");
  
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
  
  // Step 3: Ensure card is still present and activated
  // NOTE: This is the LAST isCardPresent() check before entering ISO-DEP mode.
  // After activateCard(), we must NOT call isCardPresent() again as it would
  // reset the ISO-DEP session!
  if (!nfcReader->isCardPresent()) {
    Serial.println(F("❌ Card removed"));
    webServer.broadcastLog("❌ Kaart verwijderd", "error");
    return;
  }
  
  // Step 4: Authenticate and change key
  Serial.println(F("🔐 Authenticating with factory key..."));
  webServer.broadcastLog("Stap 2/4: Authenticeer met factory key...", "info");
  
  // Check if handler is initialized
  if (ntag424Handler == nullptr) {
    Serial.println(F("❌ NTAG424 handler not initialized"));
    webServer.broadcastLog("❌ NTAG424 handler niet geïnitialiseerd", "error");
    return;
  }
  
  // Test basic communication with GetVersion
  Serial.println(F("🔍 Testing card communication..."));
  
  // First activate ISO14443-4 protocol
  if (!ntag424Handler->activateCard()) {
    Serial.println(F("❌ Failed to activate card for ISO-DEP communication"));
    webServer.broadcastLog("❌ Kaart activatie mislukt - geen ISO-DEP support?", "error");
    return;
  }
  
  Serial.println(F("✅ Card activated for ISO-DEP"));
  webServer.broadcastLog("✅ ISO-DEP activatie succesvol", "success");
  
  // Now try GetVersion
  uint8_t versionInfo[28];
  if (!ntag424Handler->getVersion(versionInfo)) {
    Serial.println(F("❌ Failed to communicate with card - not a valid NTAG424 DNA?"));
    webServer.broadcastLog("❌ Geen communicatie met kaart - geen geldige NTAG424 DNA?", "error");
    webServer.broadcastLog("Tip: Controleer of het echt een NTAG424 DNA kaart is", "warning");
    return;
  }
  
  Serial.println(F("✅ Card communication OK"));
  webServer.broadcastLog("✅ Kaart communicatie OK", "success");
  
  // First authenticate to verify card communication
  NTAG424Handler::AuthResult authResult;
  bool authenticated = ntag424Handler->authenticateEV2First(
    0, 
    NTAG424Handler::DEFAULT_AES_KEY, 
    authResult
  );
  
  if (!authenticated) {
    Serial.print(F("❌ Authentication failed: "));
    Serial.println(authResult.errorMessage);
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
    Serial.print(F("⚠️ Verification failed: "));
    Serial.println(verifyResult.errorMessage);
    webServer.broadcastLog("⚠️ Verificatie mislukt - key mogelijk niet correct!", "warning");
    webServer.broadcastLog("Error: " + verifyResult.errorMessage, "warning");
  }
  
  // Reset ISO-DEP session for next card
  if (ntag424Handler != nullptr) {
    ntag424Handler->resetSession();
  }
  
  // Wait for card removal
  Serial.println(F("\n👉 Remove card and present next card to personalize"));
  webServer.broadcastLog("Verwijder kaart - klaar voor volgende kaart", "info");
}