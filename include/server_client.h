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
        lastPing = millis();  // reset timer regardless of result
        
        Serial.print(F("Server test result: "));
        Serial.println(success ? "✅ OK" : "❌ FAILED");
        
        return success;
    }
    
    void periodicPing() {
        unsigned long now = millis();
        if (lastPing == 0 || now - lastPing >= PING_INTERVAL) {
            testConnection();  // testConnection() sets lastPing internally
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
    
    // Request initial challenge (no UID required) - for pre-caching
    struct ChallengeData {
        String challenge;
        bool success;
    };
    
    ChallengeData requestInitialChallenge() {
        ChallengeData result;
        result.success = false;
        
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server configured"));
            return result;
        }
        
        Serial.println(F("📤 Requesting initial challenge..."));
        
        // Server API: GET /api/challenge/initial?readerId=<MAC>
        http.begin(serverUrl + "/api/challenge/initial?readerId=" + WiFi.macAddress());
        http.setTimeout(5000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String response = http.getString();
            
            StaticJsonDocument<512> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc.containsKey("challenge")) {
                result.challenge = responseDoc["challenge"].as<String>();
                result.success = true;
                
                Serial.println(F("✅ Initial challenge received"));
                Serial.print(F("   Challenge: "));
                Serial.println(result.challenge);
            } else {
                Serial.println(F("❌ Invalid challenge response"));
            }
        } else {
            Serial.print(F("❌ Challenge request failed: "));
            Serial.println(httpCode);
        }
        
        http.end();
        return result;
    }
    
    // Step 1 of scan flow: link nonce to UID, get derived key back
    struct StartScanResult {
        bool success;
        bool cardKnown;
        String derivedKeyHex;  // 32 hex chars = K0 for this card
    };

    StartScanResult startScan(const String& uid) {
        StartScanResult result;
        result.success = false;
        result.cardKnown = false;

        if (serverUrl.isEmpty()) {
            Serial.println(F("❌ No server configured"));
            return result;
        }

        StaticJsonDocument<256> requestDoc;
        requestDoc["uid"] = uid;
        requestDoc["readerId"] = WiFi.macAddress();
        String requestBody;
        serializeJson(requestDoc, requestBody);

        http.begin(serverUrl + "/api/scan/start");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);

        int httpCode = http.POST(requestBody);

        if (httpCode == 200) {
            String responseStr = http.getString();
            StaticJsonDocument<512> responseDoc;
            if (!deserializeJson(responseDoc, responseStr)) {
                result.success = true;
                result.cardKnown = responseDoc["cardKnown"] | false;
                result.derivedKeyHex = responseDoc["derivedKey"].as<String>();
                Serial.print(F("✅ Scan start OK, known: "));
                Serial.println(result.cardKnown ? "yes" : "no");
            }
        } else if (httpCode == 404) {
            result.success = true;   // not an error - card just not registered
            result.cardKnown = false;
            Serial.println(F("   Card not registered on server"));
        } else {
            Serial.print(F("❌ scan/start failed: HTTP "));
            Serial.println(httpCode);
        }

        http.end();
        return result;
    }

    // Send scan with crypto proof, receive result + next challenge
    struct ScanResult {
        bool success;
        String status;          // "known", "unknown", "challenge_failed"
        int credits;
        String message;
        String nextChallenge;   // For next card scan
    };
    
    ScanResult scanWithProof(const String& cardUID, const String& encRndB, 
                             const String& encResponse, const String& transactionId) {
        ScanResult result;
        result.success = false;
        result.credits = 0;
        
        if (serverUrl.isEmpty()) {
            Serial.println(F("❌ No server configured"));
            result.status = "error";
            result.message = "No server configured";
            return result;
        }
        
        Serial.println(F("📤 Sending scan with crypto proof..."));
        logToWeb("📤 Scan met crypto proof verzenden...", "info");
        
        // Server API: POST /api/scan-with-proof
        // Body: { uid, encRndB, encResponse, transactionId, readerId }
        StaticJsonDocument<1024> requestDoc;
        requestDoc["uid"] = cardUID;
        requestDoc["encRndB"] = encRndB;
        requestDoc["encResponse"] = encResponse;
        requestDoc["transactionId"] = transactionId;
        requestDoc["readerId"] = WiFi.macAddress();
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/scan-with-proof");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        int httpCode = http.POST(requestBody);
        
        if (httpCode == 200) {
            String responseStr = http.getString();
            
            StaticJsonDocument<1024> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, responseStr);
            
            if (!error) {
                result.success = true;
                result.status = responseDoc["status"].as<String>();
                result.credits = responseDoc["credits"] | 0;
                result.message = responseDoc["message"].as<String>();
                result.nextChallenge = responseDoc["nextChallenge"].as<String>();
                
                Serial.print(F("✅ Scan result: "));
                Serial.println(result.status);
                Serial.print(F("   Credits: "));
                Serial.println(result.credits);
                Serial.print(F("   Message: "));
                Serial.println(result.message);
                Serial.print(F("   Next challenge: "));
                Serial.println(result.nextChallenge);
                
                logToWeb("✅ Scan result: " + result.status, "success");
                if (result.credits > 0) {
                    logToWeb("💰 Credits: " + String(result.credits), "success");
                }
                logToWeb(result.message, "info");
            } else {
                Serial.println(F("❌ Invalid JSON response"));
                result.status = "error";
                result.message = "Invalid server response";
            }
        } else {
            Serial.print(F("❌ Scan request failed: HTTP "));
            Serial.println(httpCode);
            result.status = "error";
            result.message = "HTTP " + String(httpCode);
            logToWeb("❌ Scan request mislukt: HTTP " + String(httpCode), "error");
        }
        
        http.end();
        return result;
    }
    
    bool verifyResponse(const String& cardUID, const String& response, const String& rndB = "", const String& transactionId = "") {
        if (serverUrl.isEmpty()) {
            logToWeb("❌ Geen server geconfigureerd voor verificatie", "error");
            return false;
        }
        
        bool cryptoMode = (rndB.length() > 0 && transactionId.length() > 0);
        
        if (cryptoMode) {
            logToWeb("🔐 Full EV2 Crypto verificatie", "success");
        } else {
            logToWeb("🔐 Mock mode verificatie (geen crypto)", "warning");
        }
        logToWeb("   Response: " + response.substring(0, 16) + "...", "info");
        
        // Server API: POST /api/verify with body {uid, response, rndB, transactionId}
        StaticJsonDocument<512> requestDoc;
        requestDoc["uid"] = cardUID;
        requestDoc["response"] = response;
        
        // Add crypto fields if available
        if (cryptoMode) {
            requestDoc["rndB"] = rndB;
            requestDoc["transactionId"] = transactionId;
            logToWeb("   RndB: " + rndB.substring(0, 16) + "...", "info");
            logToWeb("   TI: " + transactionId, "info");
        }
        
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
    
    // ============ CARD SCAN LOGGING ============
    
    bool sendScan(const String& cardUID, const String& cardStatus, const String& cardType = "") {
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server configured - scan not logged"));
            return false;
        }
        
        // Determine status from cardType and cardStatus
        String scanStatus = "unknown";
        bool isPersonalized = false;
        
        // Check if it's a non-NTAG424 card
        if (cardType.length() > 0 && 
            cardType.indexOf("NTAG424") < 0 && 
            cardType.indexOf("DESFire") < 0 &&
            cardType.indexOf("SECURE") < 0) {
            scanStatus = "non_ntag424";
            isPersonalized = false;
        } else {
            // NTAG424 card - use cardStatus
            if (cardStatus == "personalized" || cardStatus == "gedoopt") {
                isPersonalized = true;
                scanStatus = "known";
            } else if (cardStatus == "personalized_unregistered") {
                isPersonalized = true;
                scanStatus = "personalized_unregistered";
            } else if (cardStatus == "factory") {
                isPersonalized = false;
                scanStatus = "factory";
            } else if (cardStatus == "unknown") {
                isPersonalized = false;
                scanStatus = "challenge_failed";
            }
        }
        
        // Server API: POST /api/scan
        // Expected: { uid, readerName, isPersonalized, status, cardType }
        StaticJsonDocument<512> requestDoc;
        requestDoc["uid"] = cardUID;
        requestDoc["readerName"] = WiFi.macAddress(); // Use MAC as reader identifier
        requestDoc["isPersonalized"] = isPersonalized;
        requestDoc["status"] = scanStatus;
        if (cardType.length() > 0) {
            requestDoc["cardType"] = cardType;
        }
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        Serial.println(F("📤 Sending scan to server..."));
        Serial.print(F("   UID: "));
        Serial.println(cardUID);
        Serial.print(F("   Status: "));
        Serial.println(scanStatus);
        Serial.print(F("   Card Type: "));
        Serial.println(cardType.length() > 0 ? cardType : "NTAG424");
        
        http.begin(serverUrl + "/api/scan");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        int httpCode = http.POST(requestBody);
        bool success = false;
        
        if (httpCode == 200) {
            String response = http.getString();
            Serial.println(F("✅ Scan logged to server"));
            Serial.print(F("   Response: "));
            Serial.println(response);
            success = true;
        } else {
            Serial.print(F("❌ Scan logging failed: HTTP "));
            Serial.println(httpCode);
        }
        
        http.end();
        return success;
    }
};

#endif // SERVER_CLIENT_H
