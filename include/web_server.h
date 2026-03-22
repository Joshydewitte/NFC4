#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "system_config.h"

// Forward declaration
class ServerClient;

// Helper functions for ServerClient to access WebSocket
void webServerBroadcastLog(class NFCWebServer* ws, const String& message, const String& level);
void webServerBroadcastRaw(class NFCWebServer* ws, const String& json);

#include "server_client.h"
#include "login_page.h"
#include "admin_setup_page.h"
#include "settings_page.h"
#include "status_page.h"
#include "config_card_page.h"
#include "write_cards_page.h"

class NFCWebServer {
private:
    AsyncWebServer httpServer;
    AsyncWebSocket ws;
    SystemConfig* config;
    ServerClient* serverClient;
    
    // Session management
    String sessionToken = "";
    unsigned long sessionExpiry = 0;
    const unsigned long SESSION_TIMEOUT = 3600000; // 1 uur
    
public:
    NFCWebServer() : httpServer(80), ws("/ws") {}
    
    void begin(SystemConfig* cfg, ServerClient* srv) {
        config = cfg;
        serverClient = srv;
        
        Serial.println(F("Starting Web Server..."));
        
        setupRoutes();

        // WebSocket served at ws://<ip>/ws (port 80) — no separate port needed.
        // mathieucarbou ESPAsyncWebServer uses an internal mutex in textAll(),
        // making it safe to call broadcastLog() from Core 1 (loop) while
        // AsyncTCP processes events on Core 0.
        ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
            handleWebSocketEvent(server, client, type, arg, data, len);
        });
        httpServer.addHandler(&ws);

        httpServer.begin();

        Serial.println(F("✅ Web Server active on port 80"));
        Serial.println(F("✅ WebSocket active at ws://<ip>/ws"));
    }
    
    void loop() {
        // Release memory from disconnected WebSocket clients to prevent leaks.
        // HTTP + WS processing is handled by AsyncTCP on Core 0 — no handleClient() needed.
        ws.cleanupClients();
    }
    
    // ============ WEBSOCKET LOGGING ============
    
    void broadcastLog(const String& message, const String& level = "info") {
        String json = "{\"type\":\"log\",\"data\":{\"message\":\"" + message + "\",\"level\":\"" + level + "\"}}";
        ws.textAll(json);
    }

    void broadcastRaw(const String& json) {
        ws.textAll(json);
    }
    
    void broadcastStatus(const String& readerStatus, const String& serverStatus, const String& readerMode) {
        String json = "{\"type\":\"status\",\"data\":{";
        json += "\"readerStatus\":\"" + readerStatus + "\",";
        json += "\"serverStatus\":\"" + serverStatus + "\",";
        json += "\"readerMode\":\"" + readerMode + "\"";
        json += "}}";
        ws.textAll(json);
    }
    
    void broadcastStats(uint32_t cardsRead, unsigned long uptime) {
        String json = "{\"type\":\"stats\",\"data\":{";
        json += "\"cardsRead\":" + String(cardsRead) + ",";
        json += "\"uptime\":" + String(uptime);
        json += "}}";
        ws.textAll(json);
    }
    
    void broadcastWriteCardStatus(const String& uid, const String& status, const String& message) {
        String json = "{\"type\":\"write_card_status\",\"uid\":\"" + uid + "\",\"status\":\"" + status + "\",\"message\":\"" + message + "\"}";
        ws.textAll(json);
    }
    
private:
    void setupRoutes() {
        // In mathieucarbou ESPAsyncWebServer, onRequestBody only fires for the
        // catch-all handler (unmatched routes). For specific registered routes
        // the body callback MUST be passed inline as the 5th argument to on().
        // We define one shared lambda and reuse it for all body-reading POST routes.
        ArBodyHandlerFunction captureBody = [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                if (request->_tempObject) free(request->_tempObject);
                request->_tempObject = malloc(total + 1);
                ((uint8_t*)request->_tempObject)[total] = 0;
            }
            if (request->_tempObject) {
                memcpy((uint8_t*)request->_tempObject + index, data, len);
            }
        };

        // ============ PUBLIC ROUTES ============
        
        // Root - redirect based on admin setup status
        httpServer.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            Serial.println(F("[WEB] GET / requested"));
            
            if (!config->hasAdminAccount()) {
                Serial.println(F("[WEB] -> Showing admin setup page"));
                request->send(200, "text/html", ADMIN_SETUP_PAGE);
            } else if (!isAuthenticated(request)) {
                Serial.println(F("[WEB] -> Not authenticated, redirecting to /login"));
                request->redirect("/login");
            } else {
                Serial.println(F("[WEB] -> Authenticated, showing status page"));
                request->send(200, "text/html", STATUS_PAGE);
            }
        });
        
        // Login page
        httpServer.on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "text/html", LOGIN_PAGE);
        });
        
        // Admin setup (first time only)
        httpServer.on("/setup-admin", HTTP_POST,
            [this](AsyncWebServerRequest* request) { handleAdminSetup(request); },
            nullptr, captureBody);

        // Login API
        httpServer.on("/api/login", HTTP_POST,
            [this](AsyncWebServerRequest* request) { handleLogin(request); },
            nullptr, captureBody);
        
        // Logout API
        httpServer.on("/api/logout", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                sessionToken = "";
                sessionExpiry = 0;
                request->send(200, "application/json", "{\"success\":true}");
            },
            nullptr, captureBody);
        
        // ============ PROTECTED ROUTES ============
        
        // Settings page
        httpServer.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuthPage(request)) return;
            handleSettingsPage(request);
        });
        
        // Server settings API
        httpServer.on("/api/settings/server", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleSaveServerSettings(request); },
            nullptr, captureBody);

        // Reader mode API
        httpServer.on("/api/settings/mode", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleSaveReaderMode(request); },
            nullptr, captureBody);

        // Masterkey API
        httpServer.on("/api/settings/masterkey", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleSetMasterkey(request); },
            nullptr, captureBody);

        // Clear masterkey API
        httpServer.on("/api/settings/masterkey/clear", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                config->clearSessionMasterkey();
                request->send(200, "application/json", "{\"success\":true}");
            },
            nullptr, captureBody);

        // Network settings API
        httpServer.on("/api/settings/network", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleSaveNetworkSettings(request); },
            nullptr, captureBody);

        // NDEF / URL settings API
        httpServer.on("/api/settings/ndef", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleSaveNdefSettings(request); },
            nullptr, captureBody);

        httpServer.on("/api/settings/ndef", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuth(request)) return;
            String tpl  = config->getNdefUrlTemplate();
            bool   en   = config->isNdefEnabled();
            String mode = config->getNdefWriteMode();
            String json = "{\"urlTemplate\":\"" + tpl + "\",\"enabled\":" +
                          (en ? "true" : "false") + ",\"writeMode\":\"" + mode + "\"}";
            request->send(200, "application/json", json);
        });

        // Reader info: ID + whether token is configured + whether server still knows us
        httpServer.on("/api/reader/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuth(request)) return;
            String rid    = config->deriveReaderId();
            String mac    = WiFi.macAddress();
            bool   hasTok = config->hasReaderToken();

            // Check if the server still accepts our token (quick /api/ping with auth headers)
            String serverStatus = "unknown";
            if (hasTok && serverClient && serverClient->isServerOnline()) {
                int code = serverClient->checkReaderRegistration();
                if (code == 200)      serverStatus = "ok";
                else if (code == 401) serverStatus = "removed";
                else                  serverStatus = "unknown";
            }

            String json = "{\"readerId\":\"" + rid + "\",\"mac\":\"" + mac +
                          "\",\"hasToken\":" + (hasTok ? "true" : "false") +
                          ",\"serverStatus\":\"" + serverStatus + "\"}";
            request->send(200, "application/json", json);
        });

        // Save reader API token
        httpServer.on("/api/settings/reader-token", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                String body = request->_tempObject ? (char*)request->_tempObject : "";
                JsonDocument doc;
                if (deserializeJson(doc, body) || !doc["token"].is<const char*>()) {
                    request->send(400, "application/json", "{\"error\":\"Missing token\"}");
                    return;
                }
                String token = doc["token"].as<String>();
                if (token.length() == 0) {
                    request->send(400, "application/json", "{\"error\":\"Token cannot be empty\"}");
                    return;
                }
                config->setReaderToken(token);
                if (serverClient) {
                    serverClient->setReaderToken(token);
                    String serverMac = serverClient->fetchServerMac();
                    if (serverMac.length() > 0) {
                        config->setServerMac(serverMac);
                        Serial.println("✅ Server MAC auto-opgeslagen: " + serverMac);
                    } else {
                        Serial.println(F("⚠️  Server MAC kon niet automatisch worden opgehaald"));
                    }
                }
                request->send(200, "application/json", "{\"success\":true}");
            },
            nullptr, captureBody);

        // System control APIs
        httpServer.on("/api/reboot", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                request->send(200, "application/json", "{\"success\":true}");
                xTaskCreate([](void*){ vTaskDelay(600 / portTICK_PERIOD_MS); ESP.restart(); vTaskDelete(nullptr); }, "esp_rst", 2048, nullptr, 5, nullptr);
            },
            nullptr, captureBody);

        httpServer.on("/api/reset-network", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                config->resetNetwork();
                request->send(200, "application/json", "{\"success\":true,\"message\":\"Network reset\"}");
            },
            nullptr, captureBody);

        httpServer.on("/api/factory-reset", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                config->factoryReset();
                request->send(200, "application/json", "{\"success\":true,\"message\":\"Factory reset\"}");
                xTaskCreate([](void*){ vTaskDelay(600 / portTICK_PERIOD_MS); ESP.restart(); vTaskDelete(nullptr); }, "esp_rst", 2048, nullptr, 5, nullptr);
            },
            nullptr, captureBody);
        
        // Test server connection
        httpServer.on("/api/test-server", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuth(request)) return;
            unsigned long start = millis();
            bool success = serverClient->testConnection();
            unsigned long latency = millis() - start;
            
            String json = "{\"success\":" + String(success ? "true" : "false") + ",\"latency\":" + String(latency) + "}";
            request->send(200, "application/json", json);
        });

        // Server auto-discovery: scans the local subnet for NFC servers
        httpServer.on("/api/discover-server", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                if (serverClient->isDiscoveryRunning()) {
                    request->send(409, "application/json",
                        "{\"success\":false,\"message\":\"Zoeken al bezig\"}");
                    return;
                }
                serverClient->startServerDiscovery();
                request->send(200, "application/json", "{\"success\":true}");
            },
            nullptr, captureBody);
        
        // Stats API
        httpServer.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuth(request)) return;
            handleStatsAPI(request);
        });
        
        // Config card personalization page
        httpServer.on("/config-card", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuthPage(request)) return;
            request->send(200, "text/html", CONFIG_CARD_PAGE);
        });
        
        // Get current card API
        httpServer.on("/api/card/current", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuth(request)) return;
            handleGetCurrentCard(request);
        });
        
        // Start personalization API
        httpServer.on("/api/personalize/start", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleStartPersonalization(request); },
            nullptr, captureBody);

        // Write cards page
        httpServer.on("/write-cards", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!requireAuthPage(request)) return;
            request->send(200, "text/html", WRITE_CARDS_PAGE);
        });

        // Write cards API - Start
        httpServer.on("/api/write/start", HTTP_POST,
            [this](AsyncWebServerRequest* request) { if (!requireAuth(request)) return; handleWriteStart(request); },
            nullptr, captureBody);

        // Write cards API - Stop
        httpServer.on("/api/write/stop", HTTP_POST,
            [this](AsyncWebServerRequest* request) {
                if (!requireAuth(request)) return;
                handleWriteStop(request);
            },
            nullptr, captureBody);

        // Favicon — browsers always request this; serve a minimal response so
        // it never shows as an unhandled 404 in the serial log.
        httpServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(204); // No Content
        });

        // Catch-all 404 handler — replaces the noisy internal WebServer error log.
        httpServer.onNotFound([this](AsyncWebServerRequest* request) {
            String method = (request->method() == HTTP_GET) ? "GET" : "POST";
            String uri    = request->url();
            Serial.println("[WEB] 404 " + method + " " + uri);
            if (uri.startsWith("/api/")) {
                request->send(404, "application/json",
                    "{\"error\":\"Not found\",\"path\":\"" + uri + "\"}");
            } else {
                // Unknown page — redirect to root so the user lands somewhere useful
                request->redirect("/");
            }
        });
    }
    
    // ============ SESSION MANAGEMENT ============
    
    bool isAuthenticated(AsyncWebServerRequest* request) {
        // Check if session is valid
        if (sessionToken.isEmpty() || millis() > sessionExpiry) {
            Serial.println(F("[AUTH] No valid session token"));
            return false;
        }
        
        // Check cookie
        if (request->hasHeader("Cookie")) {
            String cookie = request->getHeader("Cookie")->value();
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
    
    bool requireAuth(AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return false;
        }
        // Refresh session
        sessionExpiry = millis() + SESSION_TIMEOUT;
        return true;
    }

    // For HTML page routes: redirect to /login instead of returning JSON 401
    bool requireAuthPage(AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) {
            request->redirect("/login");
            return false;
        }
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
    
    void handleAdminSetup(AsyncWebServerRequest* request) {
        if (config->hasAdminAccount()) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Admin already exists\"}");
            return;
        }
        
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, body)) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid request\"}");
            return;
        }
        
        String username = doc["username"] | "";
        String password = doc["password"] | "";
        
        if (username.length() < 4 || password.length() < 8) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
            return;
        }
        
        bool success = config->createAdminAccount(username, password);
        
        if (success) {
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to create admin\"}");
        }
    }
    
    void handleLogin(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        
        // Parse JSON with ArduinoJson (robust: handles special chars, empty body, etc.)
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, body)) {
            Serial.println(F("[LOGIN] JSON parse error — body: "));
            Serial.println(body);
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid request\"}");
            return;
        }
        
        String username = doc["username"] | "";
        String password = doc["password"] | "";
        
        if (config->validateAdmin(username, password)) {
            sessionToken = generateSessionToken();
            sessionExpiry = millis() + SESSION_TIMEOUT;
            
            // Note: HttpOnly removed so JavaScript can set the cookie
            String cookieValue = "session=" + sessionToken + "; Path=/; SameSite=Lax";
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
                "{\"success\":true,\"session\":\"" + sessionToken + "\"}");
            resp->addHeader("Set-Cookie", cookieValue);
            request->send(resp);
            
            Serial.print(F("✅ Admin logged in: "));
            Serial.println(username);
            Serial.print(F("[AUTH] Session token created: "));
            Serial.println(sessionToken);
        } else {
            Serial.println(F("❌ Login failed: Invalid credentials"));
            request->send(401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        }
    }
    
    void handleSettingsPage(AsyncWebServerRequest* request) {
        String page = SETTINGS_PAGE;
        
        // Replace placeholders
        page.replace("%SERVER_URL%", config->getServerUrl());
        page.replace("%SERVER_STATUS%", serverClient->isServerOnline() ? "Online" : "Offline");
        page.replace("%SERVER_STATUS_CLASS%", serverClient->isServerOnline() ? "status-online" : "status-offline");

        // Reader ID and MAC are always available locally — inject directly into HTML
        // so they're visible even if JS/fetch fails or the server removed this reader.
        page.replace("%READER_ID%", config->deriveReaderId());
        page.replace("%READER_MAC%", WiFi.macAddress());

        // Initial token status (refined by JS fetch after page load)
        bool hasTok = config->hasReaderToken();
        String tokenStatus;
        if (hasTok) {
            tokenStatus = "<span style=\"color:#4caf50;\">&#x2705; Token geconfigureerd &#x2014; controleer registratie...</span>";
        } else {
            tokenStatus = "<span style=\"color:#f57c00;\">&#x26A0;&#xFE0F; Geen token &#x2014; registreer deze reader op de server</span>";
        }
        page.replace("%READER_TOKEN_STATUS%", tokenStatus);
        
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
        
        request->send(200, "text/html", page);
    }
    
    void handleSaveServerSettings(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        int urlStart = body.indexOf("\"url\":\"") + 7;
        int urlEnd = body.indexOf("\"", urlStart);
        String url = body.substring(urlStart, urlEnd);
        
        config->setServerUrl(url);
        serverClient->setServerUrl(url);
        
        // Test de verbinding direct na het instellen
        bool connected = serverClient->testConnection();
        
        String json = "{\"success\":true,\"connected\":" + String(connected ? "true" : "false") + "}";
        request->send(200, "application/json", json);
        
        Serial.print(F("Server URL saved: "));
        Serial.println(url);
        Serial.print(F("Connection test: "));
        Serial.println(connected ? "OK" : "FAILED");
    }
    
    void handleSaveReaderMode(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        int modeStart = body.indexOf("\"mode\":\"") + 8;
        int modeEnd = body.indexOf("\"", modeStart);
        String mode = body.substring(modeStart, modeEnd);
        
        config->setReaderMode(mode);
        
        request->send(200, "application/json", "{\"success\":true}");
    }
    
    void handleSetMasterkey(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        int keyStart = body.indexOf("\"key\":\"") + 7;
        int keyEnd = body.indexOf("\"", keyStart);
        String key = body.substring(keyStart, keyEnd);
        
        config->setSessionMasterkey(key);
        
        request->send(200, "application/json", "{\"success\":true}");
    }
    
    void handleSaveNetworkSettings(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";

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

        request->send(200, "application/json", "{\"success\":true}");
    }

    void handleSaveNdefSettings(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";

        // urlTemplate
        int tplStart = body.indexOf("\"urlTemplate\":\"") + 15;
        int tplEnd   = body.indexOf("\"", tplStart);
        if (tplStart > 14 && tplEnd > tplStart) {
            String tpl = body.substring(tplStart, tplEnd);
            config->setNdefUrlTemplate(tpl);
        }

        // enabled boolean
        bool enabled = (body.indexOf("\"enabled\":true") >= 0);
        config->setNdefEnabled(enabled);

        // writeMode: "keys_and_ndef" | "keys_only" | "ndef_only"
        int wmStart = body.indexOf("\"writeMode\":\"") + 14;
        int wmEnd   = body.indexOf("\"", wmStart);
        if (wmStart > 13 && wmEnd > wmStart) {
            String wm = body.substring(wmStart, wmEnd);
            if (wm == "keys_and_ndef" || wm == "keys_only" || wm == "ndef_only") {
                config->setNdefWriteMode(wm);
            }
        }

        request->send(200, "application/json", "{\"success\":true}");
        Serial.println(F("[NDEF] Settings saved"));
    }
    
    void handleStatsAPI(AsyncWebServerRequest* request) {
        String json = "{";
        json += "\"readerStatus\":\"online\",";
        json += "\"serverStatus\":\"" + String(serverClient->isServerOnline() ? "online" : "offline") + "\",";
        json += "\"readerMode\":\"" + config->getReaderMode() + "\",";
        json += "\"cardsRead\":" + String(config->getCardsRead()) + ",";
        json += "\"uptime\":" + String(config->getUptime());
        json += "}";
        
        request->send(200, "application/json", json);
    }
    
    void handleGetCurrentCard(AsyncWebServerRequest* request) {
        // This will be implemented to return current detected card info
        // For now, return empty
        String json = "{\"present\":false}";
        request->send(200, "application/json", json);
    }
    
    void handleStartPersonalization(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        int uidStart = body.indexOf("\"uid\":\"") + 7;
        int uidEnd = body.indexOf("\"", uidStart);
        String uid = body.substring(uidStart, uidEnd);
        
        // Trigger personalization workflow
        // This will be implemented in the next step
        String json = "{\"success\":true,\"message\":\"Personalization started\"}";
        request->send(200, "application/json", json);
    }
    
    void handleWriteStart(AsyncWebServerRequest* request) {
        String body = request->_tempObject ? (char*)request->_tempObject : "";
        
        Serial.println(F("\n╔═══════════════════════════════════════════╗"));
        Serial.println(F("║       WRITE START REQUEST RECEIVED       ║"));
        Serial.println(F("╚═══════════════════════════════════════════╝"));
        Serial.print(F("Request body: "));
        Serial.println(body);
        Serial.println(F("───────────────────────────────────────────"));
        
        // Parse keySource first
        int sourceStart = body.indexOf("\"keySource\":\"") + 13;
        int sourceEnd = body.indexOf("\"", sourceStart);
        String keySource = body.substring(sourceStart, sourceEnd);
        Serial.print(F("✓ keySource: "));
        Serial.println(keySource);
        
        // Parse masterSecret (only required for esp32 mode)
        int secretStart = body.indexOf("\"masterSecret\":\"") + 16;
        int secretEnd = body.indexOf("\"", secretStart);
        String masterSecret = body.substring(secretStart, secretEnd);
        Serial.print(F("✓ masterSecret length: "));
        Serial.println(masterSecret.length());
        
        int modeStart = body.indexOf("\"mode\":\"") + 8;
        int modeEnd = body.indexOf("\"", modeStart);
        String mode = body.substring(modeStart, modeEnd);
        Serial.print(F("✓ mode: "));
        Serial.println(mode);
        
        // Parse isFactory (boolean)
        bool isFactory = body.indexOf("\"isFactory\":true") > 0;
        Serial.print(F("✓ isFactory: "));
        Serial.println(isFactory ? "true" : "false");
        
        // Parse isDirectKey (boolean)
        bool isDirectKey = body.indexOf("\"isDirectKey\":true") > 0;
        Serial.print(F("✓ isDirectKey: "));
        Serial.println(isDirectKey ? "true (previous key is AES key)" : "false (previous key is master secret)");
        
        // Parse resetToFactory (boolean)
        bool resetToFactory = body.indexOf("\"resetToFactory\":true") > 0;
        Serial.print(F("✓ resetToFactory: "));
        Serial.println(resetToFactory ? "true (reset K0 naar 0x00*16)" : "false");
        
        // Parse previousKey if not factory
        String previousKey = "";
        if (!isFactory) {
            // Check for null value first
            if (body.indexOf("\"previousKey\":null") > 0) {
                Serial.println(F("⚠️  previousKey is null in JSON (should have a value!)"));
                previousKey = "";  // Will fail validation below
            } else {
                int prevKeyPos = body.indexOf("\"previousKey\":\"");
                if (prevKeyPos >= 0) {  // Found
                    int prevKeyStart = prevKeyPos + 15;  // Start after "previousKey":"
                    int prevKeyEnd = body.indexOf("\"", prevKeyStart);
                    if (prevKeyEnd > prevKeyStart) {
                        previousKey = body.substring(prevKeyStart, prevKeyEnd);
                        Serial.print(F("✓ previousKey: "));
                        Serial.println(previousKey);
                        Serial.print(F("✓ previousKey length: "));
                        Serial.println(previousKey.length());
                    } else {
                        Serial.println(F("⚠️  Could not find previousKey closing quote!"));
                    }
                } else {
                    Serial.println(F("⚠️  previousKey field not found in JSON!"));
                }
            }
        }
        
        Serial.println(F("───────────────────────────────────────────"));
        Serial.println(F("───────────────────────────────────────────"));
        
        // Validate keySource
        if (keySource != "esp32" && keySource != "server") {
            Serial.println(F("❌ VALIDATION FAILED: Invalid keySource"));
            String json = "{\"success\":false,\"message\":\"Ongeldige key source (moet esp32 of server zijn)\"}";
            request->send(400, "application/json", json);
            return;
        }
        Serial.println(F("✓ keySource valid"));
        
        // For ESP32 mode, validate masterSecret (exact 32 hex characters = 16 bytes)
        if (keySource == "esp32") {
            if (masterSecret.length() != 32) {
                Serial.print(F("❌ VALIDATION FAILED: masterSecret length is "));
                Serial.print(masterSecret.length());
                Serial.println(F(", expected 32"));
                String json = "{\"success\":false,\"message\":\"Master secret moet exact 32 hex karakters zijn (16 bytes)\"}";
                request->send(400, "application/json", json);
                return;
            }
            // Validate hex characters
            for (int i = 0; i < masterSecret.length(); i++) {
                char c = masterSecret[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    Serial.print(F("❌ VALIDATION FAILED: Invalid hex char at position "));
                    Serial.print(i);
                    Serial.print(F(": '"));
                    Serial.print(c);
                    Serial.println(F("'"));
                    String json = "{\"success\":false,\"message\":\"Master secret moet alleen hex karakters bevatten (0-9, A-F)\"}";
                    request->send(400, "application/json", json);
                    return;
                }
            }
            Serial.println(F("✓ masterSecret valid (32 hex chars)"));
        }
        
        if (mode != "single" && mode != "continuous") {
            Serial.println(F("❌ VALIDATION FAILED: Invalid mode"));
            String json = "{\"success\":false,\"message\":\"Ongeldige mode (moet single of continuous zijn)\"}";
            request->send(400, "application/json", json);
            return;
        }
        Serial.println(F("✓ mode valid"));
        
        // Validate previous key for non-factory cards (MUST be exactly 32 hex characters)
        if (!isFactory && !resetToFactory) {
            Serial.println(F("Validating previousKey for non-factory card..."));
            
            if (previousKey.length() == 0) {
                Serial.println(F("❌ VALIDATION FAILED: previousKey is empty"));
                String json = "{\"success\":false,\"message\":\"Vorige key is verplicht voor niet-factory kaarten\"}";
                request->send(400, "application/json", json);
                return;
            }
            
            if (previousKey.length() != 32) {
                Serial.print(F("❌ VALIDATION FAILED: previousKey length is "));
                Serial.print(previousKey.length());
                Serial.println(F(", expected exactly 32"));
                String json = "{\"success\":false,\"message\":\"Vorige key moet exact 32 hex karakters zijn (16 bytes)\"}";
                request->send(400, "application/json", json);
                return;
            }
            
            // Validate hex characters
            for (int i = 0; i < previousKey.length(); i++) {
                char c = previousKey[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    Serial.print(F("❌ VALIDATION FAILED: Invalid hex char in previousKey at position "));
                    Serial.print(i);
                    Serial.print(F(": '"));
                    Serial.print(c);
                    Serial.println(F("'"));
                    String json = "{\"success\":false,\"message\":\"Vorige key moet alleen hex karakters bevatten (0-9, A-F)\"}";
                    request->send(400, "application/json", json);
                    return;
                }
            }
            
            Serial.println(F("✓ previousKey valid (32 hex chars)"));
        }
        
        // Store settings (runtime only, NOT persistent)
        config->setKeySource(keySource);
        if (keySource == "esp32") {
            config->setMasterSecret(masterSecret);
        }
        config->setIsFactory(isFactory);
        config->setIsDirectKey(isDirectKey);
        config->setResetToFactory(resetToFactory);
        if (!isFactory && !resetToFactory) {
            config->setPreviousKey(previousKey);
        }
        config->setWriteMode(mode);

        // Parse and store ndefMode from request body (overrides NVS default)
        int ndefModeStart = body.indexOf("\"ndefMode\":\"") + 12;
        int ndefModeEnd = body.indexOf("\"", ndefModeStart);
        if (ndefModeStart > 12 && ndefModeEnd > ndefModeStart) {
            String ndefMode = body.substring(ndefModeStart, ndefModeEnd);
            if (ndefMode == "keys_and_ndef" || ndefMode == "keys_only" || ndefMode == "ndef_only") {
                config->setNdefWriteMode(ndefMode);
                Serial.print(F("✓ ndefMode: "));
                Serial.println(ndefMode);
            }
        }

        config->setWriteActive(true);
        
        // Log for debugging
        Serial.println(F("\n╔═══════════════════════════════════════════╗"));
        Serial.println(F("║   WRITE MODE STARTED FROM WEB UI!       ║"));
        Serial.println(F("╚═══════════════════════════════════════════╝"));
        Serial.print(F("Key Source: "));
        Serial.println(keySource);
        if (keySource == "esp32") {
            Serial.print(F("Master Secret (first 8): "));
            Serial.println(masterSecret.substring(0, 8));
        } else {
            Serial.println(F("Keys will be fetched from server per UID"));
        }
        Serial.print(F("Mode: "));
        Serial.println(mode);
        Serial.print(F("Card Type: "));
        Serial.println(isFactory ? "Factory" : "Personalized");
        Serial.println(F("═══════════════════════════════════════════\n"));
        
        String json = "{\"success\":true,\"message\":\"Schrijfproces gestart in " + mode + " modus met " + keySource + " key source\"}";
        request->send(200, "application/json", json);
        
        String cardType = isFactory ? "factory" : (resetToFactory ? "factory reset" : "gepersonaliseerd");
        broadcastLog("🚀 Schrijfproces gestart (modus: " + mode + ", source: " + keySource + ", type: " + cardType + ")", "success");
    }
    
    void handleWriteStop(AsyncWebServerRequest* request) {
        Serial.println(F("\n╔═══════════════════════════════════════════╗"));
        Serial.println(F("║     WRITE MODE STOPPED FROM WEB UI!      ║"));
        Serial.println(F("╚═══════════════════════════════════════════╝"));
        Serial.println(F("Clearing master secret from RAM..."));
        Serial.println(F("Resetting key source to ESP32 default..."));
        Serial.println(F("═══════════════════════════════════════════\n"));
        
        config->setWriteActive(false);
        config->clearMasterSecret();
        config->clearPreviousKey();
        config->setKeySource("esp32");  // Reset to default ESP32 mode
        
        String json = "{\"success\":true,\"message\":\"Schrijfproces gestopt\"}";
        request->send(200, "application/json", json);
        
        broadcastLog("⏹️ Schrijfproces gestopt (key source reset naar ESP32)", "info");
    }
    
    // AsyncWebSocket event handler — runs on Core 0 (AsyncTCP task).
    // No delay() calls: AsyncWebSocket queues messages internally; sends are fire-and-forget.
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_DISCONNECT:
                Serial.printf("WebSocket [%u] Disconnected\n", client->id());
                break;

            case WS_EVT_ERROR:
                Serial.printf("WebSocket [%u] Error: %s\n", client->id(), (char*)data);
                break;
                
            case WS_EVT_CONNECT:
                {
                    IPAddress ip = client->remoteIP();
                    Serial.printf("WebSocket [%u] Connected from %s\n", client->id(), ip.toString().c_str());
                    
                    // Send welcome + initial state — no delay() needed, messages are queued
                    client->text("{\"type\":\"log\",\"data\":{\"message\":\"✅ WebSocket connected - reader online\",\"level\":\"success\"}}");
                    
                    String statusMsg = "{\"type\":\"status\",\"data\":{";
                    statusMsg += "\"readerStatus\":\"online\",";
                    statusMsg += "\"serverStatus\":\"" + String(serverClient->isServerOnline() ? "online" : "offline") + "\",";
                    statusMsg += "\"readerMode\":\"" + config->getReaderMode() + "\"";
                    statusMsg += "}}";
                    client->text(statusMsg);
                    
                    String statsMsg = "{\"type\":\"stats\",\"data\":{";
                    statsMsg += "\"cardsRead\":" + String(config->getCardsRead()) + ",";
                    statsMsg += "\"uptime\":" + String(config->getUptime());
                    statsMsg += "}}";
                    client->text(statsMsg);
                    
                    client->text("{\"type\":\"log\",\"data\":{\"message\":\"📡 Reader actief - wachtend op kaarten\",\"level\":\"info\"}}");
                }
                break;
                
            case WS_EVT_DATA:
                {
                    AwsFrameInfo* info = (AwsFrameInfo*)arg;
                    // Only handle complete, single-frame text messages
                    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                        String message = String((char*)data, len);
                        Serial.printf("WebSocket [%u] received: %s\n", client->id(), message.c_str());
                        
                        if (message.indexOf("\"type\":\"page_leave\"") > 0 || message.indexOf("\"type\":\"page_change\"") > 0) {
                            Serial.println(F("Page leave detected - checking write mode..."));
                            if (config->isWriteActive()) {
                                Serial.println(F("⚠️ User left write page - stopping write mode"));
                                config->stopWriteMode();
                                broadcastLog("Write mode gestopt: gebruiker verliet pagina", "warning");
                            }
                        }
                    }
                }
                break;
        }
    }
};

// Helper function declarations for ServerClient
void webServerBroadcastLog(NFCWebServer* ws, const String& message, const String& level);
void webServerBroadcastRaw(NFCWebServer* ws, const String& json);

#endif // WEB_SERVER_H
