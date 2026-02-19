#include <Arduino.h>
#include "system_config.h"
#include "web_server.h"
#include "server_client.h"
#include "wifi_manager.h"
#include "nfc_reader.h"

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
  Serial.println(F("\n=== CONFIG MODE ==="));
  webServer.broadcastLog("Config modus - kaart configureren", "warning");
  
  String masterKey = "";
  bool useManualKey = systemConfig.hasMasterkey();
  
  if (useManualKey) {
    // Use manual masterkey (offline mode)
    masterKey = systemConfig.getSessionMasterkey();
    Serial.println(F("🔑 Using manual masterkey (offline)"));
    webServer.broadcastLog("Gebruik handmatige masterkey voor personalisatie", "info");
    
    // TODO: Write card with manual masterkey via DESFire
    webServer.broadcastLog("TODO: Kaart personaliseren met handmatige key", "warning");
    webServer.broadcastLog("✅ Offline personalisatie voltooid", "success");
    
  } else {
    // Use server keys (normal flow)
    Serial.println(F("🌐 Fetching keys from server..."));
    webServer.broadcastLog("Haal keys op van server...", "info");
    
    // Check if card exists
    String cardInfo_str = serverClient.getCardInfo(cardInfo.uidString);
    
    if (cardInfo_str.length() > 0) {
      Serial.println(F("ℹ️ Card exists in database"));
      webServer.broadcastLog("Kaart bestaat al - haal keys op...", "info");
      
      // Fetch masterkey from server
      masterKey = serverClient.getCardKey(cardInfo.uidString, "master");
      
      if (masterKey.length() > 0) {
        webServer.broadcastLog("Master key ontvangen: " + masterKey.substring(0, 8) + "...", "success");
        
        // TODO: Write card with PN5180 and masterkey via DESFire commands
        webServer.broadcastLog("TODO: Kaart personaliseren met DESFire commands", "warning");
        webServer.broadcastLog("✅ Kaart klaar voor gebruik", "success");
      } else {
        webServer.broadcastLog("❌ Kon keys niet ophalen van server", "error");
      }
      
    } else {
      // New card - register first
      Serial.println(F("📝 New card - registering..."));
      webServer.broadcastLog("Nieuwe kaart detecteerd - registreren...", "info");
      
      bool registered = serverClient.registerCard(cardInfo.uidString, 
                                                   "Card_" + cardInfo.uidString.substring(0, 8), 
                                                   "Auto-registered");
      
      if (registered) {
        webServer.broadcastLog("✅ Kaart geregistreerd in database", "success");
        
        // Fetch keys
        masterKey = serverClient.getCardKey(cardInfo.uidString, "master");
        
        if (masterKey.length() > 0) {
          webServer.broadcastLog("Keys ontvangen: " + masterKey.substring(0, 8) + "...", "success");
          
          // TODO: Write card with PN5180
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