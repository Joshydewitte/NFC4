#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config_page.h"
#include "connecting_page.h"
#include "success_page.h"

class WiFiConfigManager {
private:
    AsyncWebServer server;
    DNSServer dnsServer;
    Preferences preferences;
    
    const char* AP_SSID = "ESP32-NFC-Config";
    const char* AP_PASSWORD = "";  // Open AP voor gemakkelijke toegang
    const byte DNS_PORT = 53;
    
    bool configMode = false;
    String connectedSSID = "";
    IPAddress stationIP;

    // Pending WiFi connect (scheduled from handleConnect, executed in loop)
    bool pendingConnect = false;
    String pendingSSID = "";
    String pendingPassword = "";
    
    // Callback functie wanneer verbinding succesvol is
    void (*onConnectedCallback)() = nullptr;
    
public:
    WiFiConfigManager() : server(80) {}
    
    void begin() {
        // Laad opgeslagen credentials
        preferences.begin("wifi-config", false);
        String savedSSID = preferences.getString("ssid", "");
        String savedPassword = preferences.getString("password", "");
        preferences.end();
        
        Serial.println(F("\n=== WiFi Manager ==="));
        
        // Probeer eerst verbinding te maken met opgeslagen credentials
        if (savedSSID.length() > 0) {
            Serial.print(F("Probeer verbinding met opgeslagen netwerk: "));
            Serial.println(savedSSID);
            
            if (connectToWiFi(savedSSID, savedPassword)) {
                Serial.println(F("✅ Verbonden met opgeslagen netwerk"));
                stationIP = WiFi.localIP();
                Serial.print(F("IP Adres: "));
                Serial.println(stationIP);
                configMode = false;
                return;
            } else {
                Serial.println(F("❌ Verbinding mislukt met opgeslagen netwerk"));
            }
        }
        
        // Start configuratie modus
        startConfigMode();
    }
    
    void startConfigMode() {
        Serial.println(F("\n📡 Start Configuratie Modus"));
        configMode = true;
        
        // Start Access Point
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        
        delay(100);
        
        IPAddress IP = WiFi.softAPIP();
        Serial.print(F("AP IP adres: "));
        Serial.println(IP);
        Serial.print(F("AP SSID: "));
        Serial.println(AP_SSID);
        
        // Start DNS server voor captive portal
        dnsServer.start(DNS_PORT, "*", IP);
        
        // Setup web server routes
        setupWebServer();
        server.begin();
        
        Serial.println(F("✅ Configuratie portal actief"));
        Serial.print(F("Verbind met WiFi netwerk: "));
        Serial.println(AP_SSID);
    }
    
    void setupWebServer() {
        // Root en captive portal redirects
        server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleRoot(request);
        });
        
        server.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleRoot(request);  // Android captive portal
        });
        
        server.on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleRoot(request);  // iOS captive portal
        });
        
        server.on("/canonical.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleRoot(request);  // Firefox captive portal
        });
        
        server.on("/success.txt", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleRoot(request);  // Windows captive portal
        });
        
        // WiFi scan endpoint
        server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleScan(request);
        });
        
        // Connect endpoint
        server.on("/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
            this->handleConnect(request);
        });
        
        // Status endpoint
        server.on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
            this->handleStatus(request);
        });
        
        // Catch-all voor captive portal
        server.onNotFound([this](AsyncWebServerRequest* request) {
            this->handleRoot(request);
        });
    }
    
    void handleRoot(AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse(200, "text/html", CONFIG_PAGE);
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        resp->addHeader("Pragma", "no-cache");
        resp->addHeader("Expires", "-1");
        request->send(resp);
    }
    
    void handleScan(AsyncWebServerRequest* request) {
        Serial.println(F("WiFi scan gestart..."));
        int n = WiFi.scanNetworks();
        
        String json = "{\"networks\":[";
        
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"encryption\":\"";
            
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN:
                    json += "Open";
                    break;
                case WIFI_AUTH_WEP:
                    json += "WEP";
                    break;
                case WIFI_AUTH_WPA_PSK:
                    json += "WPA";
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    json += "WPA2";
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    json += "WPA/WPA2";
                    break;
                case WIFI_AUTH_WPA2_ENTERPRISE:
                    json += "WPA2-Enterprise";
                    break;
                default:
                    json += "Unknown";
            }
            json += "\"";
            json += "}";
        }
        
        json += "]}";
        
        Serial.print(F("Gevonden netwerken: "));
        Serial.println(n);
        
        request->send(200, "application/json", json);
        WiFi.scanDelete();
    }
    
    void handleConnect(AsyncWebServerRequest* request) {
        String ssid = request->arg("ssid");
        String password = request->arg("password");
        
        Serial.println(F("\n=== Verbinding maken ==="));
        Serial.print(F("SSID: "));
        Serial.println(ssid);
        
        // Stuur connecting pagina direct terug
        String connectingPage = CONNECTING_PAGE;
        connectingPage.replace("%SSID%", ssid);
        request->send(200, "text/html", connectingPage);
        
        // Sla credentials op
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        
        // Schedule verbinding vanuit de main loop (blocking connect niet in async handler)
        pendingSSID = ssid;
        pendingPassword = password;
        pendingConnect = true;
    }
    
    void handleStatus(AsyncWebServerRequest* request) {
        if (WiFi.status() == WL_CONNECTED && stationIP[0] != 0) {
            // Succespagina tonen
            String successPage = SUCCESS_PAGE;
            successPage.replace("%IP_ADDRESS%", stationIP.toString());
            successPage.replace("%SSID%", connectedSSID);
            request->send(200, "text/html", successPage);
            
            // Stop configuratie modus (server blijft actief maar AP wordt afgebroken)
            stopConfigMode();
            
            // Roep callback aan
            if (onConnectedCallback != nullptr) {
                onConnectedCallback();
            }
        } else {
            // Nog aan het verbinden
            request->send(200, "text/html", "<html><body><h1>Nog aan het verbinden...</h1><script>setTimeout(function(){window.location.href='/status';}, 2000);</script></body></html>");
        }
    }
    
    bool connectToWiFi(String ssid, String password) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        Serial.print(F("Verbinden"));
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        return (WiFi.status() == WL_CONNECTED);
    }
    
    void stopConfigMode() {
        if (configMode) {
            Serial.println(F("Stop configuratie modus"));
            dnsServer.stop();
            // AsyncWebServer has no stop() — AP teardown makes it unreachable
            WiFi.softAPdisconnect(true);
            configMode = false;
        }
    }
    
    void loop() {
        if (configMode) {
            dnsServer.processNextRequest();
            // Execute pending WiFi connection from main loop (avoids blocking async task)
            if (pendingConnect) {
                pendingConnect = false;
                if (connectToWiFi(pendingSSID, pendingPassword)) {
                    connectedSSID = pendingSSID;
                    stationIP = WiFi.localIP();
                    Serial.println(F("✅ Verbinding succesvol!"));
                    Serial.print(F("IP: "));
                    Serial.println(stationIP);
                } else {
                    Serial.println(F("❌ Verbinding mislukt"));
                }
            }
        }
    }
    
    bool isConfigMode() {
        return configMode;
    }
    
    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }
    
    IPAddress getLocalIP() {
        return WiFi.localIP();
    }
    
    void setOnConnectedCallback(void (*callback)()) {
        onConnectedCallback = callback;
    }
    
    // Functie om configuratie te resetten (voor debugging)
    void resetConfig() {
        preferences.begin("wifi-config", false);
        preferences.clear();
        preferences.end();
        Serial.println(F("WiFi configuratie gereset"));
    }
};

#endif // WIFI_MANAGER_H
