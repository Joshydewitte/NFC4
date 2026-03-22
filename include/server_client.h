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
    String readerId  = "";   // SHA-256 derived from MAC, set at startup
    String readerToken = ""; // API token from server after registration

    // ─── Server Discovery ────────────────────────────────────────────────────
    volatile bool _discoveryRunning = false;
    struct _DiscCtx { ServerClient* self; };
    static void _discoveryTask(void* pv) {
        _DiscCtx* ctx = static_cast<_DiscCtx*>(pv);
        ctx->self->_runDiscovery();
        delete ctx;
        vTaskDelete(nullptr);
    }

public:
    ServerClient() {}
    
    void setWebServer(NFCWebServer* ws) {
        webServer = ws;
    }

    void setReaderId(const String& id) {
        readerId = id;
    }

    void setReaderToken(const String& token) {
        readerToken = token;
    }

    String getReaderId() const {
        return readerId.length() > 0 ? readerId : WiFi.macAddress();
    }
    
    void logToWeb(const String& message, const String& level = "info") {
        if (webServer != nullptr) {
            // Forward declare the method we'll call
            extern void webServerBroadcastLog(NFCWebServer* ws, const String& msg, const String& lvl);
            webServerBroadcastLog(webServer, message, level);
        }
    }

    // Add reader authentication headers when a token is configured
    void addReaderAuthHeaders() {
        if (readerId.length() > 0) {
            http.addHeader("X-Reader-Id", readerId);
        }
        if (readerToken.length() > 0) {
            http.addHeader("X-Reader-Token", readerToken);
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
    
    // ─── Ping / health-check ────────────────────────────────────────────────
    // Result returned by every ping call so callers can act on new nonces.
    struct PingResult {
        bool    online;
        String  challenge;       // 32 hex chars when server issued a fresh nonce, else ""
        bool    challengeValid;  // server still has a valid nonce for this reader
    };

    /**
     * Single HTTP round-trip to /api/ping.
     * When requestChallenge=true, appends ?readerId=X&renew=1 so the server
     * generates and stores a fresh nonce AND returns it here — one request,
     * two jobs.
     */
    PingResult pingServer(bool requestChallenge = false) {
        PingResult result = { false, "" };
        if (serverUrl.isEmpty()) return result;

        String url = serverUrl + "/api/ping";
        if (requestChallenge) {
            url += "?readerId=" + getReaderId() + "&renew=1";
        }

        http.begin(url);
        http.setTimeout(3000);   // tight — this is just a heartbeat
        http.addHeader("User-Agent", "ESP32-NFC-Reader");
        addReaderAuthHeaders();

        int httpCode = http.GET();
        lastPing = millis();

        if (httpCode == 200) {
            result.online = true;
            serverOnline = true;

            {
                String body = http.getString();
                StaticJsonDocument<256> doc;
                if (!deserializeJson(doc, body)) {
                    // challengeValid: server tells us whether our cached nonce still exists
                    // Default true so older server versions don't trigger false renewals
                    result.challengeValid = doc["challengeValid"] | true;
                    if (doc.containsKey("challenge")) {
                        result.challenge = doc["challenge"].as<String>();
                        Serial.print(F("   [nonce] Fresh nonce via ping: "));
                        Serial.println(result.challenge.substring(0, 8) + "...");
                    }
                    if (!result.challengeValid) {
                        Serial.println(F("   \u26a0\ufe0f Ping: server nonce verdwenen (herstart?)"));
                    }
                }
            }
        } else {
            serverOnline = false;
            Serial.print(F("Ping failed HTTP "));
            Serial.println(httpCode);
        }

        http.end();
        return result;
    }

    /**
     * Backwards-compatible: returns true/false and runs pingServer(false).
     * Used at boot before the loop starts.
     */
    bool testConnection() {
        PingResult r = pingServer(false);
        Serial.print(F("Server test result: "));
        Serial.println(r.online ? "✅ OK" : "❌ FAILED");
        return r.online;
    }

    /**
     * Fetch the server's MAC address from /api/ping.
     * Returns the MAC string (e.g. "0C:9D:92:87:CB:6A") or "" on failure.
     * Uses a local HTTPClient to avoid conflicts with the member http instance.
     */
    String fetchServerMac() {
        if (serverUrl.isEmpty()) return "";
        HTTPClient localHttp;
        localHttp.begin(serverUrl + "/api/ping");
        localHttp.setTimeout(3000);
        String mac = "";
        int code = localHttp.GET();
        if (code == 200) {
            String body = localHttp.getString();
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, body) && doc["serverMac"].is<const char*>()) {
                mac = doc["serverMac"].as<String>();
            }
        }
        localHttp.end();
        return mac;
    }

    /**
     * Quick check whether the server still recognises this reader's token.
     * Sends GET /api/challenge/initial with reader-auth headers.
     * Returns the raw HTTP status code: 200=registered, 401=removed/unknown, <0=unreachable.
     */
    int checkReaderRegistration() {
        if (serverUrl.isEmpty() || readerToken.isEmpty()) return -1;
        HTTPClient hc;
        hc.begin(serverUrl + "/api/challenge/initial");
        hc.setTimeout(3000);
        if (readerId.length() > 0) hc.addHeader("X-Reader-Id", readerId);
        hc.addHeader("X-Reader-Token", readerToken);
        int code = hc.GET();
        hc.end();
        return code;
    }
    // ─── Server Discovery ────────────────────────────────────────────────────

    bool isDiscoveryRunning() const { return _discoveryRunning; }

    /**
     * Scan the local /24 subnet for NFC servers on port 3000.
     * Probes gateway + hosts .1-.20 + local-IP ±5 (up to 40 candidates, 350 ms timeout each).
     * Runs on a background FreeRTOS task; results are pushed to all WebSocket clients:
     *   {type:"discovery_start", data:{total:N}}
     *   {type:"discovery_found", data:{url:"http://..."}}
     *   {type:"discovery_done",  data:{count:N}}
     * Returns immediately.
     */
    void startServerDiscovery() {
        if (_discoveryRunning) return;
        _discoveryRunning = true;
        _DiscCtx* ctx = new _DiscCtx{this};
        if (xTaskCreate(_discoveryTask, "nfc_disc", 12288, ctx, 1, nullptr) != pdPASS) {
            delete ctx;
            _discoveryRunning = false;
            Serial.println(F("[DISC] \u274c Kon discovery task niet starten"));
        }
    }

    void _runDiscovery() {
        // Wait for the WebSocket handshake (opened by the browser just before
        // POSTing /api/discover-server) to fully register on the ESP side.
        // Without this delay the first broadcast messages are sent before the
        // WS client is connected and are silently dropped.
        vTaskDelay(800 / portTICK_PERIOD_MS);

        IPAddress localIP = WiFi.localIP();
        IPAddress gw      = WiFi.gatewayIP();

        const int MAX_CANDS = 40;
        IPAddress cands[MAX_CANDS];
        int cnt = 0;
        auto push = [&](IPAddress ip) {
            if (ip == localIP) return;
            for (int j = 0; j < cnt; j++) if (cands[j] == ip) return;
            if (cnt < MAX_CANDS) cands[cnt++] = ip;
        };
        push(gw);
        for (int h = 1; h <= 20; h++)
            push(IPAddress(localIP[0], localIP[1], localIP[2], (uint8_t)h));
        for (int d = -5; d <= 5; d++) {
            int h = (int)localIP[3] + d;
            if (h >= 1 && h <= 254)
                push(IPAddress(localIP[0], localIP[1], localIP[2], (uint8_t)h));
        }

        Serial.printf("[DISC] Zoeken op %d adressen (poort 3000)...\n", cnt);
        extern void webServerBroadcastRaw(NFCWebServer* ws, const String& json);
        if (webServer)
            webServerBroadcastRaw(webServer,
                "{\"type\":\"discovery_start\",\"data\":{\"total\":" + String(cnt) + "}}");

        int found = 0;
        for (int i = 0; i < cnt; i++) {
            String url = "http://" + cands[i].toString() + ":3000";
            HTTPClient hc;
            hc.begin(url + "/api/ping");
            hc.setTimeout(350);
            hc.addHeader("User-Agent", "ESP32-NFC-Discovery/1.0");
            int code = hc.GET();
            if (code == 200) {
                found++;
                Serial.print(F("[DISC] Gevonden: ")); Serial.println(url);
                if (webServer)
                    webServerBroadcastRaw(webServer,
                        "{\"type\":\"discovery_found\",\"data\":{\"url\":\"" + url + "\"}}");
            }
            hc.end();
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }

        Serial.printf("[DISC] Klaar. %d server(s) gevonden.\n", found);
        if (webServer)
            webServerBroadcastRaw(webServer,
                "{\"type\":\"discovery_done\",\"data\":{\"count\":" + String(found) + "}}");
        _discoveryRunning = false;
    }
    /**
     * Call from loop().  Only fires when PING_INTERVAL has elapsed.
     * Pass requestChallenge=true when the cached nonce is stale/absent so
     * the server issues a fresh one in the same response.
     * Returns PingResult so the caller can update its nonce cache.
     */
    PingResult periodicPing(bool requestChallenge = false) {
        unsigned long now = millis();
        if (lastPing == 0 || now - lastPing >= PING_INTERVAL) {
            return pingServer(requestChallenge);
        }
        return { serverOnline, "", true };   // interval not yet elapsed
    }

    // ─── NFC424 CHALLENGE/RESPONSE ───────────────────────────────────────────
    
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
        
        // Server API: GET /api/challenge/initial?readerId=<readerId>
        http.begin(serverUrl + "/api/challenge/initial?readerId=" + getReaderId());
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
        String derivedKeyHex;   // 32 hex chars = K0 for this card
        bool nonceExpired;      // true when server returned 410 (nonce gone/used)
        String nextChallenge;   // fresh nonce from server when nonceExpired=true
    };

    StartScanResult startScan(const String& uid) {
        StartScanResult result;
        result.success = false;
        result.cardKnown = false;
        result.nonceExpired = false;

        if (serverUrl.isEmpty()) {
            Serial.println(F("❌ No server configured"));
            return result;
        }

        StaticJsonDocument<256> requestDoc;
        requestDoc["uid"] = uid;
        requestDoc["readerId"] = getReaderId();
        String requestBody;
        serializeJson(requestDoc, requestBody);

        http.begin(serverUrl + "/api/scan/start");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        addReaderAuthHeaders();

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
        } else if (httpCode == 410) {
            // Nonce expired or already used — server issued a fresh one in the body
            result.nonceExpired = true;
            String responseStr = http.getString();
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, responseStr)) {
                result.nextChallenge = doc["nextChallenge"].as<String>();
                Serial.println(F("   ⚠️  scan/start 410 — fresh nonce received"));
            }
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
                             const String& encResponse, const String& transactionId) {        ScanResult result;
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
        requestDoc["readerId"] = getReaderId();
        
        String requestBody;
        serializeJson(requestDoc, requestBody);
        
        http.begin(serverUrl + "/api/scan-with-proof");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        addReaderAuthHeaders();
        
        int httpCode = http.POST(requestBody);
        
        if (httpCode == 200) {
            String responseStr = http.getString();
            
            StaticJsonDocument<1024> responseDoc;
            DeserializationError error = deserializeJson(responseDoc, responseStr);
            
            if (!error) {
                result.success = responseDoc["success"] | false;
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
            // Try to parse body anyway — server may have sent a fresh nonce
            String responseStr = http.getString();
            String httpErr = "HTTP ";
            httpErr += httpCode;
            StaticJsonDocument<512> responseDoc;
            if (!deserializeJson(responseDoc, responseStr)) {
                result.status = responseDoc["status"] | "error";
                const char* msg = responseDoc["message"];
                result.message = msg ? String(msg) : httpErr;
                result.nextChallenge = responseDoc["nextChallenge"] | "";
                if (result.nextChallenge.length() == 32) {
                    Serial.println(F("   🔄 Fresh nonce received in error response"));
                }
            } else {
                result.status = "error";
                result.message = httpErr;
            }
            logToWeb("❌ Scan request mislukt: HTTP " + String(httpCode), "error");
        }
        
        http.end();
        return result;
    }

    // ─── SDM FAST SCAN ───────────────────────────────────────────────────────────
    // POST /api/scan/sdm — single APDU authentication using the card's SDM mirrors.
    // UID, counter and CMAC are read back from the NDEF file; the server verifies
    // the CMAC with K0. No nonce/challenge is consumed.
    ScanResult sendSDMScan(const String& cardUID, const String& sdmUidHex,
                           const String& sdmCtrHex, const String& sdmMacHex) {
        ScanResult result;
        result.success = false;
        result.credits = 0;

        if (serverUrl.isEmpty()) {
            result.status = "error";
            return result;
        }

        StaticJsonDocument<384> requestDoc;
        requestDoc["uid"]     = cardUID;
        requestDoc["sdmUid"]  = sdmUidHex;
        requestDoc["sdmCtr"]  = sdmCtrHex;
        requestDoc["sdmMac"]  = sdmMacHex;
        requestDoc["readerId"] = getReaderId();

        String requestBody;
        serializeJson(requestDoc, requestBody);

        http.begin(serverUrl + "/api/scan/sdm");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(3000);
        addReaderAuthHeaders();

        int httpCode = http.POST(requestBody);

        if (httpCode == 200) {
            String responseStr = http.getString();
            StaticJsonDocument<512> doc;
            if (!deserializeJson(doc, responseStr)) {
                result.success = doc["success"] | false;
                result.status  = doc["status"]  | "";
                result.credits = doc["credits"] | 0;
                result.message = doc["message"] | "";
            }
        } else {
            // Non-200: SDM not set up or CMAC invalid — caller falls back to full auth
            String responseStr = http.getString();
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, responseStr)) {
                result.status = doc["status"] | "sdm_failed";
            } else {
                result.status = "sdm_failed";
            }
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
        requestDoc["reader_id"] = getReaderId();
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
    
    ScanResult sendScan(const String& cardUID, const String& cardStatus, const String& cardType = "") {
        ScanResult result;
        result.success = false;
        result.credits = 0;
        if (serverUrl.isEmpty()) {
            Serial.println(F("No server configured - scan not logged"));
            result.status = "error";
            return result;
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
            } else if (cardStatus == "empty_response") {
                isPersonalized = false;
                scanStatus = "empty_response";
            } else if (cardStatus == "timeout") {
                isPersonalized = false;
                scanStatus = "timeout";
            } else if (cardStatus == "rf_error") {
                isPersonalized = false;
                scanStatus = "rf_error";
            }
        }
        
        // Server API: POST /api/scan
        // Expected: { uid, readerId, readerName, isPersonalized, status, cardType }
        StaticJsonDocument<512> requestDoc;
        requestDoc["uid"] = cardUID;
        requestDoc["readerId"]   = getReaderId();
        requestDoc["readerName"] = getReaderId();
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
        addReaderAuthHeaders();
        
        int httpCode = http.POST(requestBody);
        
        if (httpCode == 200) {
            String responseStr = http.getString();
            Serial.println(F("✅ Scan logged to server"));
            StaticJsonDocument<512> responseDoc;
            if (!deserializeJson(responseDoc, responseStr)) {
                result.success = true;
                result.status = responseDoc["status"] | "";
                result.nextChallenge = responseDoc["nextChallenge"] | "";
                if (result.nextChallenge.length() == 32) {
                    Serial.println(F("   🔄 Fresh nonce received after challenge_failed"));
                }
            } else {
                result.success = true; // HTTP 200 is a success even if JSON parse fails
            }
        } else {
            Serial.print(F("❌ Scan logging failed: HTTP "));
            Serial.println(httpCode);
        }
        
        http.end();
        return result;
    }
};

#endif // SERVER_CLIENT_H
