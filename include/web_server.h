#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <WebSocketsServer.h>
#include "system_config.h"

// Forward declaration
class ServerClient;

// Helper function for ServerClient to access WebSocket logging
void webServerBroadcastLog(class NFCWebServer* ws, const String& message, const String& level);

#include "server_client.h"
#include "login_page.h"
#include "admin_setup_page.h"
#include "settings_page.h"
#include "status_page.h"
#include "config_card_page.h"
#include "write_cards_page.h"

class NFCWebServer {
private:
    WebServer httpServer;
    WebSocketsServer wsServer;
    SystemConfig* config;
    ServerClient* serverClient;
    
    // Session management
    String sessionToken = "";
    unsigned long sessionExpiry = 0;
    const unsigned long SESSION_TIMEOUT = 3600000; // 1 uur
    
public:
    NFCWebServer() : httpServer(80), wsServer(81) {}
    
    void begin(SystemConfig* cfg, ServerClient* srv) {
        config = cfg;
        serverClient = srv;
        
        Serial.println(F("Starting Web Server..."));
        
        // IMPORTANT: Tell WebServer to collect Cookie header
        const char* headerKeys[] = {"Cookie"};
        httpServer.collectHeaders(headerKeys, 1);
        
        setupRoutes();
        httpServer.begin();
        wsServer.begin();
        wsServer.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            this->handleWebSocketEvent(num, type, payload, length);
        });
        
        Serial.println(F("✅ Web Server active on port 80"));
        Serial.println(F("✅ WebSocket Server active on port 81"));
    }
    
    void loop() {
        httpServer.handleClient();
        wsServer.loop();
    }
    
    // ============ WEBSOCKET LOGGING ============
    
    void broadcastLog(const String& message, const String& level = "info") {
        String json = "{\"type\":\"log\",\"data\":{\"message\":\"" + message + "\",\"level\":\"" + level + "\"}}";
        wsServer.broadcastTXT(json);
    }
    
    void broadcastStatus(const String& readerStatus, const String& serverStatus, const String& readerMode) {
        String json = "{\"type\":\"status\",\"data\":{";
        json += "\"readerStatus\":\"" + readerStatus + "\",";
        json += "\"serverStatus\":\"" + serverStatus + "\",";
        json += "\"readerMode\":\"" + readerMode + "\"";
        json += "}}";
        wsServer.broadcastTXT(json);
    }
    
    void broadcastStats(uint32_t cardsRead, unsigned long uptime) {
        String json = "{\"type\":\"stats\",\"data\":{";
        json += "\"cardsRead\":" + String(cardsRead) + ",";
        json += "\"uptime\":" + String(uptime);
        json += "}}";
        wsServer.broadcastTXT(json);
    }
    
    void broadcastWriteCardStatus(const String& uid, const String& status, const String& message) {
        String json = "{\"type\":\"write_card_status\",\"uid\":\"" + uid + "\",\"status\":\"" + status + "\",\"message\":\"" + message + "\"}";
        wsServer.broadcastTXT(json);
    }
    
private:
    void setupRoutes() {
        // ============ PUBLIC ROUTES ============
        
        // Root - redirect based on admin setup status
        httpServer.on("/", HTTP_GET, [this]() {
            Serial.println(F("[WEB] GET / requested"));
            
            if (!config->hasAdminAccount()) {
                Serial.println(F("[WEB] -> Showing admin setup page"));
                httpServer.send(200, "text/html", ADMIN_SETUP_PAGE);
            } else if (!isAuthenticated()) {
                Serial.println(F("[WEB] -> Not authenticated, redirecting to /login"));
                httpServer.sendHeader("Location", "/login");
                httpServer.send(302);
            } else {
                Serial.println(F("[WEB] -> Authenticated, showing status page"));
                httpServer.send(200, "text/html", STATUS_PAGE);
            }
        });
        
        // Login page
        httpServer.on("/login", HTTP_GET, [this]() {
            httpServer.send(200, "text/html", LOGIN_PAGE);
        });
        
        // Admin setup (first time only)
        httpServer.on("/setup-admin", HTTP_POST, [this]() {
            handleAdminSetup();
        });
        
        // Login API
        httpServer.on("/api/login", HTTP_POST, [this]() {
            handleLogin();
        });
        
        // Logout API
        httpServer.on("/api/logout", HTTP_POST, [this]() {
            sessionToken = "";
            sessionExpiry = 0;
            httpServer.send(200, "application/json", "{\"success\":true}");
        });
        
        // ============ PROTECTED ROUTES ============
        
        // Settings page
        httpServer.on("/settings", HTTP_GET, [this]() {
            if (!requireAuth()) return;
            handleSettingsPage();
        });
        
        // Server settings API
        httpServer.on("/api/settings/server", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleSaveServerSettings();
        });
        
        // Reader mode API
        httpServer.on("/api/settings/mode", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleSaveReaderMode();
        });
        
        // Masterkey API
        httpServer.on("/api/settings/masterkey", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleSetMasterkey();
        });
        
        // Clear masterkey API
        httpServer.on("/api/settings/masterkey/clear", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            config->clearSessionMasterkey();
            httpServer.send(200, "application/json", "{\"success\":true}");
        });
        
        // Network settings API
        httpServer.on("/api/settings/network", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleSaveNetworkSettings();
        });
        
        // System control APIs
        httpServer.on("/api/reboot", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            httpServer.send(200, "application/json", "{\"success\":true}");
            delay(1000);
            ESP.restart();
        });
        
        httpServer.on("/api/reset-network", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            config->resetNetwork();
            httpServer.send(200, "application/json", "{\"success\":true,\"message\":\"Network reset\"}");
        });
        
        httpServer.on("/api/factory-reset", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            config->factoryReset();
            httpServer.send(200, "application/json", "{\"success\":true,\"message\":\"Factory reset\"}");
            delay(1000);
            ESP.restart();
        });
        
        // Test server connection
        httpServer.on("/api/test-server", HTTP_GET, [this]() {
            if (!requireAuth()) return;  
            unsigned long start = millis();
            bool success = serverClient->testConnection();
            unsigned long latency = millis() - start;
            
            String json = "{\"success\":" + String(success ? "true" : "false") + ",\"latency\":" + String(latency) + "}";
            httpServer.send(200, "application/json", json);
        });
        
        // Stats API
        httpServer.on("/api/stats", HTTP_GET, [this]() {
            if (!requireAuth()) return;
            handleStatsAPI();
        });
        
        // Config card personalization page
        httpServer.on("/config-card", HTTP_GET, [this]() {
            if (!requireAuth()) return;
            httpServer.send(200, "text/html", CONFIG_CARD_PAGE);
        });
        
        // Get current card API
        httpServer.on("/api/card/current", HTTP_GET, [this]() {
            if (!requireAuth()) return;
            handleGetCurrentCard();
        });
        
        // Start personalization API  
        httpServer.on("/api/personalize/start", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleStartPersonalization();
        });
        
        // Write cards page
        httpServer.on("/write-cards", HTTP_GET, [this]() {
            if (!requireAuth()) return;
            httpServer.send(200, "text/html", WRITE_CARDS_PAGE);
        });
        
        // Write cards API - Start
        httpServer.on("/api/write/start", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleWriteStart();
        });
        
        // Write cards API - Stop
        httpServer.on("/api/write/stop", HTTP_POST, [this]() {
            if (!requireAuth()) return;
            handleWriteStop();
        });
    }
    
    // ============ SESSION MANAGEMENT ============
    
    bool isAuthenticated() {
        // Check if session is valid
        if (sessionToken.isEmpty() || millis() > sessionExpiry) {
            Serial.println(F("[AUTH] No valid session token"));
            return false;
        }
        
        // Check cookie
        if (httpServer.hasHeader("Cookie")) {
            String cookie = httpServer.header("Cookie");
            Serial.print(F("[AUTH] Cookie received: "));
            Serial.println(cookie);
            Serial.print(F("[AUTH] Expected session: "));
            Serial.println(sessionToken);
            bool authenticated = cookie.indexOf("session=" + sessionToken) >= 0;
            Serial.print(F("[AUTH] Authenticated: "));
            Serial.println(authenticated ? "YES" : "NO");
            return authenticated;
        }
        
        Serial.println(F("[AUTH] No cookie header found"));
        return false;
    }
    
    bool requireAuth() {
        if (!isAuthenticated()) {
            httpServer.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return false;
        }
        // Refresh session
        sessionExpiry = millis() + SESSION_TIMEOUT;
        return true;
    }
    
    String generateSessionToken() {
        String token = "";
        for (int i = 0; i < 32; i++) {
            token += String(random(0, 16), HEX);
        }
        return token;
    }
    
    // ============ HANDLERS ============
    
    void handleAdminSetup() {
        if (config->hasAdminAccount()) {
            httpServer.send(400, "application/json", "{\"success\":false,\"message\":\"Admin already exists\"}");
            return;
        }
        
        String username = httpServer.arg("username");
        String password = httpServer.arg("password");
        
        if (username.length() < 4 || password.length() < 8) {
            httpServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
            return;
        }
        
        bool success = config->createAdminAccount(username, password);
        
        if (success) {
            httpServer.send(200, "application/json", "{\"success\":true}");
        } else {
            httpServer.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to create admin\"}");
        }
    }
    
    void handleLogin() {
        String body = httpServer.arg("plain");
        
        // Parse JSON
        int userStart = body.indexOf("\"username\":\"") + 12;
        int userEnd = body.indexOf("\"", userStart);
        String username = body.substring(userStart, userEnd);
        
        int passStart = body.indexOf("\"password\":\"") + 12;
        int passEnd = body.indexOf("\"", passStart);
        String password = body.substring(passStart, passEnd);
        
        if (config->validateAdmin(username, password)) {
            sessionToken = generateSessionToken();
            sessionExpiry = millis() + SESSION_TIMEOUT;
            
            // Note: HttpOnly removed so JavaScript can set the cookie
            String cookieValue = "session=" + sessionToken + "; Path=/; SameSite=Lax";
            httpServer.sendHeader("Set-Cookie", cookieValue);
            httpServer.send(200, "application/json", "{\"success\":true,\"session\":\"" + sessionToken + "\"}");
            
            Serial.print(F("✅ Admin logged in: "));
            Serial.println(username);
            Serial.print(F("[AUTH] Session token created: "));
            Serial.println(sessionToken);
        } else {
            Serial.println(F("❌ Login failed: Invalid credentials"));
            httpServer.send(401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        }
    }
    
    void handleSettingsPage() {
        String page = SETTINGS_PAGE;
        
        // Replace placeholders
        page.replace("%SERVER_URL%", config->getServerUrl());
        page.replace("%SERVER_STATUS%", serverClient->isServerOnline() ? "Online" : "Offline");
        page.replace("%SERVER_STATUS_CLASS%", serverClient->isServerOnline() ? "status-online" : "status-offline");
        
        String mode = config->getReaderMode();
        page.replace("%MACHINE_CHECKED%", mode == "machine" ? "checked" : "");
        page.replace("%CONFIG_CHECKED%", mode == "config" ? "checked" : "");
        page.replace("%CONFIG_DISPLAY%", mode == "config" ? "block" : "none");
        
        String netMode = config->getNetworkMode();
        page.replace("%DHCP_CHECKED%", netMode == "dhcp" ? "checked" : "");
        page.replace("%STATIC_CHECKED%", netMode == "static" ? "checked" : "");
        page.replace("%STATIC_DISPLAY%", netMode == "static" ? "block" : "none");
        page.replace("%STATIC_IP%", config->getStaticIP());
        page.replace("%GATEWAY%", config->getGateway());
        page.replace("%SUBNET%", config->getSubnet());
        
        httpServer.send(200, "text/html", page);
    }
    
    void handleSaveServerSettings() {
        String body = httpServer.arg("plain");
        int urlStart = body.indexOf("\"url\":\"") + 7;
        int urlEnd = body.indexOf("\"", urlStart);
        String url = body.substring(urlStart, urlEnd);
        
        config->setServerUrl(url);
        serverClient->setServerUrl(url);
        
        // Test de verbinding direct na het instellen
        bool connected = serverClient->testConnection();
        
        String json = "{\"success\":true,\"connected\":" + String(connected ? "true" : "false") + "}";
        httpServer.send(200, "application/json", json);
        
        Serial.print(F("Server URL saved: "));
        Serial.println(url);
        Serial.print(F("Connection test: "));
        Serial.println(connected ? "OK" : "FAILED");
    }
    
    void handleSaveReaderMode() {
        String body = httpServer.arg("plain");
        int modeStart = body.indexOf("\"mode\":\"") + 8;
        int modeEnd = body.indexOf("\"", modeStart);
        String mode = body.substring(modeStart, modeEnd);
        
        config->setReaderMode(mode);
        
        httpServer.send(200, "application/json", "{\"success\":true}");
    }
    
    void handleSetMasterkey() {
        String body = httpServer.arg("plain");
        int keyStart = body.indexOf("\"key\":\"") + 7;
        int keyEnd = body.indexOf("\"", keyStart);
        String key = body.substring(keyStart, keyEnd);
        
        config->setSessionMasterkey(key);
        
        httpServer.send(200, "application/json", "{\"success\":true}");
    }
    
    void handleSaveNetworkSettings() {
        String body = httpServer.arg("plain");
        
        int modeStart = body.indexOf("\"mode\":\"") + 8;
        int modeEnd = body.indexOf("\"", modeStart);
        String mode = body.substring(modeStart, modeEnd);
        
        config->setNetworkMode(mode);
        
        if (mode == "static") {
            int ipStart = body.indexOf("\"ip\":\"") + 6;
            int ipEnd = body.indexOf("\"", ipStart);
            String ip = body.substring(ipStart, ipEnd);
            
            int gwStart = body.indexOf("\"gateway\":\"") + 11;
            int gwEnd = body.indexOf("\"", gwStart);
            String gw = body.substring(gwStart, gwEnd);
            
            int snStart = body.indexOf("\"subnet\":\"") + 10;
            int snEnd = body.indexOf("\"", snStart);
            String sn = body.substring(snStart, snEnd);
            
            config->setStaticIP(ip, gw, sn);
        }
        
        httpServer.send(200, "application/json", "{\"success\":true}");
    }
    
    void handleStatsAPI() {
        String json = "{";
        json += "\"readerStatus\":\"online\",";
        json += "\"serverStatus\":\"" + String(serverClient->isServerOnline() ? "online" : "offline") + "\",";
        json += "\"readerMode\":\"" + config->getReaderMode() + "\",";
        json += "\"cardsRead\":" + String(config->getCardsRead()) + ",";
        json += "\"uptime\":" + String(config->getUptime());
        json += "}";
        
        httpServer.send(200, "application/json", json);
    }
    
    void handleGetCurrentCard() {
        // This will be implemented to return current detected card info
        // For now, return empty
        String json = "{\"present\":false}";
        httpServer.send(200, "application/json", json);
    }
    
    void handleStartPersonalization() {
        String body = httpServer.arg("plain");
        int uidStart = body.indexOf("\"uid\":\"") + 7;
        int uidEnd = body.indexOf("\"", uidStart);
        String uid = body.substring(uidStart, uidEnd);
        
        // Trigger personalization workflow
        // This will be implemented in the next step
        String json = "{\"success\":true,\"message\":\"Personalization started\"}";
        httpServer.send(200, "application/json", json);
    }
    
    void handleWriteStart() {
        String body = httpServer.arg("plain");
        
        // Parse JSON manually (simple approach)
        int secretStart = body.indexOf("\"masterSecret\":\"") + 16;
        int secretEnd = body.indexOf("\"", secretStart);
        String masterSecret = body.substring(secretStart, secretEnd);
        
        int modeStart = body.indexOf("\"mode\":\"") + 8;
        int modeEnd = body.indexOf("\"", modeStart);
        String mode = body.substring(modeStart, modeEnd);
        
        // Parse isFactory (boolean)
        bool isFactory = body.indexOf("\"isFactory\":true") > 0;
        
        // Parse previousKey if not factory
        String previousKey = "";
        if (!isFactory) {
            int prevKeyStart = body.indexOf("\"previousKey\":\"") + 16;
            if (prevKeyStart > 15) {  // Found
                int prevKeyEnd = body.indexOf("\"", prevKeyStart);
                previousKey = body.substring(prevKeyStart, prevKeyEnd);
            }
        }
        
        // Validate
        if (masterSecret.length() < 16) {
            String json = "{\"success\":false,\"message\":\"Master secret te kort (min 16 karakters)\"}";
            httpServer.send(400, "application/json", json);
            return;
        }
        
        if (mode != "single" && mode != "continuous") {
            String json = "{\"success\":false,\"message\":\"Ongeldige mode (moet single of continuous zijn)\"}";
            httpServer.send(400, "application/json", json);
            return;
        }
        
        if (!isFactory && previousKey.length() != 32) {
            String json = "{\"success\":false,\"message\":\"Vorige key moet 32 hex karakters zijn\"}";
            httpServer.send(400, "application/json", json);
            return;
        }
        
        // Store settings (runtime only, NOT persistent)
        config->setMasterSecret(masterSecret);
        config->setIsFactory(isFactory);
        if (!isFactory) {
            config->setPreviousKey(previousKey);
        }
        config->setWriteMode(mode);
        config->setWriteActive(true);
        
        String json = "{\"success\":true,\"message\":\"Schrijfproces gestart in " + mode + " modus\"}";
        httpServer.send(200, "application/json", json);
        
        String cardType = isFactory ? "factory" : "gepersonaliseerd";
        broadcastLog("🚀 Schrijfproces gestart (modus: " + mode + ", type: " + cardType + ")", "success");
    }
    
    void handleWriteStop() {
        config->setWriteActive(false);
        config->clearMasterSecret();
        config->clearPreviousKey();
        
        String json = "{\"success\":true,\"message\":\"Schrijfproces gestopt\"}";
        httpServer.send(200, "application/json", json);
        
        broadcastLog("⏹️ Schrijfproces gestopt", "info");
    }
    
    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("WebSocket [%u] Disconnected\n", num);
                break;
                
            case WStype_CONNECTED:
                {
                    IPAddress ip = wsServer.remoteIP(num);
                    Serial.printf("WebSocket [%u] Connected from %s\n", num, ip.toString().c_str());
                    
                    // Send welcome message
                    String welcome = "{\"type\":\"log\",\"data\":{\"message\":\"✅ WebSocket connected - reader online\",\"level\":\"success\"}}";
                    wsServer.sendTXT(num, welcome);
                    
                    // Send current status
                    delay(100); // Small delay to ensure client is ready
                    String statusMsg = "{\"type\":\"status\",\"data\":{";
                    statusMsg += "\"readerStatus\":\"online\",";
                    statusMsg += "\"serverStatus\":\"" + String(serverClient->isServerOnline() ? "online" : "offline") + "\",";
                    statusMsg += "\"readerMode\":\"" + config->getReaderMode() + "\"";
                    statusMsg += "}}";
                    wsServer.sendTXT(num, statusMsg);
                    
                    // Send initial stats
                    delay(100);
                    String statsMsg = "{\"type\":\"stats\",\"data\":{";
                    statsMsg += "\"cardsRead\":" + String(config->getCardsRead()) + ",";
                    statsMsg += "\"uptime\":" + String(config->getUptime());
                    statsMsg += "}}";
                    wsServer.sendTXT(num, statsMsg);
                    
                    // Send log message that system is ready
                    delay(100);
                    String readyMsg = "{\"type\":\"log\",\"data\":{\"message\":\"📡 Reader actief - wachtend op kaarten\",\"level\":\"info\"}}";
                    wsServer.sendTXT(num, readyMsg);
                }
                break;
                
            case WStype_TEXT:
                // Handle incoming text (if needed)
                break;
        }
    }
};

// Helper function declaration for ServerClient logging
void webServerBroadcastLog(NFCWebServer* ws, const String& message, const String& level);

#endif // WEB_SERVER_H
