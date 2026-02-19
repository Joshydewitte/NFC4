#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>

// Forward declaration
class NFCWebServer;

class ServerClient {
private:
    String serverUrl;
    HTTPClient http;
    bool serverOnline = false;
    unsigned long lastPing = 0;
    const unsigned long PING_INTERVAL = 30000; // 30 seconden
    NFCWebServer* webServer = nullptr;
    
public:
    ServerClient() {}
    
    void setWebServer(NFCWebServer* ws) {
        webServer = ws;
    }
    
    void logToWeb(const String& message, const String& level = "info") {
        if (webServer != nullptr) {
            // Forward declare the method we'll call
            extern void webServerBroadcastLog(NFCWebServer* ws, const String& msg, const String& lvl);
            webServerBroadcastLog(webServer, message, level);
        }
    }
    
    void setServerUrl(const String& url) {
        serverUrl = url;
        Serial.print(F("Server URL set to: "));
        Serial.println(serverUrl);
    }
    
    String getServerUrl() {
        return serverUrl;
    }
    
    bool isServerOnline() {
        return serverOnline;
    }
    
    bool testConnection() {
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server URL configured"));
            return false;
        }
        
        Serial.print(F("Testing connection to: "));
        Serial.println(serverUrl + "/api/ping");
        
        http.begin(serverUrl + "/api/ping");
        http.setTimeout(5000);
        http.addHeader("User-Agent", "ESP32-NFC-Reader");
        
        int httpCode = http.GET();
        
        Serial.print(F("HTTP Response Code: "));
        Serial.println(httpCode);
        
        if (httpCode > 0) {
            String payload = http.getString();
            Serial.print(F("Response: "));
            Serial.println(payload);
        } else {
            Serial.print(F("HTTP Error: "));
            Serial.println(http.errorToString(httpCode));
        }
        
        bool success = (httpCode == 200);
        serverOnline = success;
        http.end();
        
        Serial.print(F("Server test result: "));
        Serial.println(success ? "✅ OK" : "❌ FAILED");
        
        // Don't log every ping to avoid spam
        
        return success;
    }
    
    void periodicPing() {
        unsigned long now = millis();
        if (now - lastPing >= PING_INTERVAL) {
            lastPing = now;
            testConnection();
        }
    }
    
    // ============ NFC424 CHALLENGE/RESPONSE ============
    
    String requestChallenge(const String& cardUID) {
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server configured"));
            logToWeb("❌ Geen server geconfigureerd", "error");
            return "";
        }
        
        logToWeb("📤 Challenge aanvragen voor UID: " + cardUID, "info");
        
        // Server API: GET /api/challenge/:uid
        http.begin(serverUrl + "/api/challenge/" + cardUID);
        http.setTimeout(5000);
        
        int httpCode = http.GET();
        String challengeData = "";
        
        if (httpCode == 200) {
            String response = http.getString();
            
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc.containsKey("challenge")) {
                challengeData = responseDoc["challenge"].as<String>();
                Serial.println(F("✅ Challenge received from server"));
                Serial.print(F("   Challenge: "));
                Serial.println(challengeData);
                
                logToWeb("✅ Challenge ontvangen van server", "success");
                logToWeb("   Challenge: " + challengeData, "info");
            } else {
                Serial.println(F("❌ Invalid challenge response"));
                logToWeb("❌ Ongeldige challenge response", "error");
            }
        } else {
            Serial.print(F("❌ Challenge request failed: "));
            Serial.println(httpCode);
            logToWeb("❌ Challenge request mislukt (HTTP " + String(httpCode) + ")", "error");
        }
        
        http.end();
        return challengeData;
    }
    
    bool verifyResponse(const String& cardUID, const String& response) {
        if (serverUrl.isEmpty()) {
            logToWeb("❌ Geen server geconfigureerd voor verificatie", "error");
            return false;
        }
        
        logToWeb("🔐 Response naar server sturen voor verificatie", "info");
        logToWeb("   Response: " + response.substring(0, 16) + "...", "info");
        
        // Server API: POST /api/verify with body {uid, response}
        StaticJsonDocument<512> requestDoc;
        requestDoc["uid"] = cardUID;
        requestDoc["response"] = response;
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/verify");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        int httpCode = http.POST(requestBody);
        bool verified = false;
        
        if (httpCode == 200) {
            String responseStr = http.getString();
            
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, responseStr);
            
            if (!error && responseDoc.containsKey("valid")) {
                verified = responseDoc["valid"].as<bool>();
                Serial.print(F("✅ Server verification: "));
                Serial.println(verified ? "VALID" : "INVALID");
                
                if (verified) {
                    logToWeb("✅ Server verificatie: TOEGANG VERLEEND", "success");
                } else {
                    logToWeb("❌ Server verificatie: TOEGANG GEWEIGERD", "error");
                }
                
                if (responseDoc.containsKey("reason")) {
                    String reason = responseDoc["reason"].as<String>();
                    Serial.print(F("   Reason: "));
                    Serial.println(reason);
                    logToWeb("   Reden: " + reason, "info");
                }
            }
        } else {
            Serial.print(F("❌ Verification request failed: "));
            Serial.println(httpCode);
            logToWeb("❌ Verificatie request mislukt (HTTP " + String(httpCode) + ")", "error");
        }
        
        http.end();
        return verified;
    }
    
    // ============ KEY DERIVATION (for Config Mode) ============
    
    String getCardKey(const String& cardUID, const String& keyType) {
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server configured"));
            logToWeb("❌ Geen server geconfigureerd", "error");
            return "";
        }
        
        logToWeb("🔑 Key ophalen: " + keyType + " voor UID: " + cardUID, "info");
        
        // Server API: GET /api/key/:uid/:keytype
        // keyType: "master", "credit", "debit", "read"
        http.begin(serverUrl + "/api/key/" + cardUID + "/" + keyType);
        http.setTimeout(5000);
        
        int httpCode = http.GET();
        String key = "";
        
        if (httpCode == 200) {
            String response = http.getString();
            
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc.containsKey("key")) {
                key = responseDoc["key"].as<String>();
                Serial.print(F("✅ Key received: "));
                Serial.print(keyType);
                Serial.print(F(" for UID: "));
                Serial.println(cardUID);
                
                logToWeb("✅ " + keyType + " key ontvangen", "success");
            }
        } else {
            Serial.print(F("❌ Key request failed: "));
            Serial.println(httpCode);
            logToWeb("❌ Key request mislukt (HTTP " + String(httpCode) + ")", "error");
        }
        
        http.end();
        return key;
    }
    
    // ============ CARD REGISTRATION ============
    
    bool registerCard(const String& cardUID, const String& name = "", const String& notes = "") {
        if (serverUrl.isEmpty()) {
            logToWeb("❌ Geen server geconfigureerd", "error");
            return false;
        }
        
        logToWeb("📝 Kaart registreren: " + cardUID, "info");
        
        // Server API: POST /api/card/register
        StaticJsonDocument<512> requestDoc;
        requestDoc["uid"] = cardUID;
        if (name.length() > 0) requestDoc["name"] = name;
        if (notes.length() > 0) requestDoc["notes"] = notes;
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/card/register");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        int httpCode = http.POST(requestBody);
        bool success = false;
        
        if (httpCode == 200) {
            Serial.println(F("✅ Card registered successfully"));
            logToWeb("✅ Kaart succesvol geregistreerd", "success");
            
            String response = http.getString();
            StaticJsonDocument<1024> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc.containsKey("keys")) {
                Serial.println(F("   Keys received from server:"));
                Serial.print(F("   Master: "));
                Serial.println(responseDoc["keys"]["masterKey"].as<String>());
                
                logToWeb("Keys ontvangen van server", "info");
            }
            
            success = true;
        } else if (httpCode == 409) {
            Serial.println(F("⚠️ Card already registered"));
            logToWeb("⚠️ Kaart al geregistreerd", "warning");
        } else {
            Serial.print(F("❌ Card registration failed: "));
            Serial.println(httpCode);
            logToWeb("❌ Kaart registratie mislukt (HTTP " + String(httpCode) + ")", "error");
        }
        
        http.end();
        return success;
    }
    
    // ============ CARD WRITE LOGGING ============
    
    bool logCardWrite(const String& cardUID, const String& writeData) {
        // Note: Server doesn't have a specific card write log endpoint
        // Card writes are logged during authentication or registration
        Serial.println(F("ℹ️ Card write logged locally (no server endpoint)"));
        return true;
    }
    
    // ============ CARD INFO ============
    
    String getCardInfo(const String& cardUID) {
        if (serverUrl.isEmpty()) {
            logToWeb("❌ Geen server geconfigureerd", "error");
            return "";
        }
        
        logToWeb("ℹ️ Kaart info opvragen...", "info");
        
        // Server API: GET /api/card/:uid
        http.begin(serverUrl + "/api/card/" + cardUID);
        http.setTimeout(5000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String response = http.getString();
            Serial.println(F("✅ Card info retrieved"));
            logToWeb("✅ Kaart info ontvangen", "success");
            http.end();
            return response;
        } else if (httpCode == 404) {
            Serial.println(F("❌ Card not found in database"));
            logToWeb("ℹ️ Kaart niet gevonden in database", "info");
        } else {
            Serial.print(F("❌ Card info request failed: "));
            Serial.println(httpCode);
            logToWeb("❌ Kaart info request mislukt (HTTP " + String(httpCode) + ")", "error");
        }
        
        http.end();
        return "";
    }
    
    // ============ BALANCE OPERATIONS ============
    
    bool updateBalance(const String& cardUID, int amount, const String& operation = "credit") {
        if (serverUrl.isEmpty()) {
            return false;
        }
        
        // Server API: POST /api/transaction/:uid
        StaticJsonDocument<256> requestDoc;
        requestDoc["operation"] = operation;
        requestDoc["amount"] = amount;
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/transaction/" + cardUID);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        int httpCode = http.POST(requestBody);
        bool success = false;
        
        if (httpCode == 200) {
            String response = http.getString();
            
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc["success"]) {
                int newBalance = responseDoc["newBalance"];
                Serial.print(F("✅ Balance updated: "));
                Serial.println(newBalance);
                success = true;
            }
        } else {
            Serial.print(F("❌ Balance update failed: "));
            Serial.println(httpCode);
        }
        
        http.end();
        return success;
    }
    
    // ============ READER STATUS REPORTING ============
    
    void reportStatus(const String& status, const String& message) {
        if (serverUrl.isEmpty()) {
            return;
        }
        
        StaticJsonDocument<512> requestDoc;
        requestDoc["reader_id"] = WiFi.macAddress();
        requestDoc["status"] = status;
        requestDoc["message"] = message;
        requestDoc["timestamp"] = millis();
        requestDoc["uptime"] = millis() / 1000;
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/reader/status");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(3000);
        
        http.POST(requestBody);
        http.end();
    }
};

#endif // SERVER_CLIENT_H
