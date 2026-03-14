#include "system_config.h"

SystemConfig::SystemConfig() 
    : sessionMasterkey(""),
      masterkeyActive(false),
      startTime(0),
      cardsRead(0),
      writeModeMasterSecret(""),
      writeModePreviousKey(""),
      writeMode("single"),
      writeActive(false),
      writeIsFactory(true),
      writeIsDirectKey(false),  // Default: previous key is master secret
      keySource("esp32") {  // Default to ESP32 mode
    startTime = millis();
}

void SystemConfig::begin() {
    prefs.begin("system", false);
}

// ============ ADMIN MANAGEMENT ============

bool SystemConfig::hasAdminAccount() {
    prefs.begin("system", true);
    bool exists = prefs.isKey("admin_user");
    prefs.end();
    return exists;
}

String SystemConfig::hashPassword(const String& password) {
    byte hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    String hashStr = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashStr += hex;
    }
    return hashStr;
}

bool SystemConfig::createAdminAccount(const String& username, const String& password) {
    if (hasAdminAccount()) {
        return false; // Already exists
    }
    
    prefs.begin("system", false);
    prefs.putString("admin_user", username);
    prefs.putString("admin_pass", hashPassword(password));
    prefs.end();
    
    Serial.print(F("Admin account created: "));
    Serial.println(username);
    return true;
}

bool SystemConfig::validateAdmin(const String& username, const String& password) {
    prefs.begin("system", true);
    String storedUser = prefs.getString("admin_user", "");
    String storedPass = prefs.getString("admin_pass", "");
    prefs.end();
    
    if (storedUser.isEmpty() || storedPass.isEmpty()) {
        return false;
    }
    
    return (username == storedUser && hashPassword(password) == storedPass);
}

// ============ SERVER SETTINGS ============

void SystemConfig::setServerUrl(const String& url) {
    prefs.begin("system", false);
    prefs.putString("server_url", url);
    prefs.end();
    Serial.print(F("Server URL saved: "));
    Serial.println(url);
}

String SystemConfig::getServerUrl() {
    prefs.begin("system", true);
    String url = prefs.getString("server_url", "");
    prefs.end();
    return url;
}

// ============ READER MODE ============

void SystemConfig::setReaderMode(const String& mode) {
    prefs.begin("system", false);
    prefs.putString("reader_mode", mode);
    prefs.end();
    Serial.print(F("Reader mode set to: "));
    Serial.println(mode);
}

String SystemConfig::getReaderMode() {
    prefs.begin("system", true);
    String mode = prefs.getString("reader_mode", "machine");
    prefs.end();
    return mode;
}

bool SystemConfig::isMachineMode() {
    return getReaderMode() == "machine";
}

bool SystemConfig::isConfigMode() {
    return getReaderMode() == "config";
}

// ============ MASTERKEY (SESSION ONLY) ============

void SystemConfig::setSessionMasterkey(const String& key) {
    sessionMasterkey = key;
    masterkeyActive = (key.length() == 32);
    if (masterkeyActive) {
        Serial.println(F("✅ Masterkey activated for session"));
    }
}

String SystemConfig::getSessionMasterkey() {
    return masterkeyActive ? sessionMasterkey : "";
}

bool SystemConfig::hasMasterkey() {
    return masterkeyActive;
}

void SystemConfig::clearSessionMasterkey() {
    // Overwrite sensitive data in memory before clearing
    if (sessionMasterkey.length() > 0) {
        char* buffer = const_cast<char*>(sessionMasterkey.c_str());
        memset(buffer, 0, sessionMasterkey.length());
    }
    sessionMasterkey = "";
    masterkeyActive = false;
    Serial.println(F("✅ Masterkey veilig gewist uit RAM"));
}

// ============ NETWORK SETTINGS ============

void SystemConfig::setNetworkMode(const String& mode) {
    prefs.begin("system", false);
    prefs.putString("net_mode", mode);
    prefs.end();
}

String SystemConfig::getNetworkMode() {
    prefs.begin("system", true);
    String mode = prefs.getString("net_mode", "dhcp");
    prefs.end();
    return mode;
}

void SystemConfig::setStaticIP(const String& ip, const String& gateway, const String& subnet) {
    prefs.begin("system", false);
    prefs.putString("static_ip", ip);
    prefs.putString("static_gw", gateway);
    prefs.putString("static_sn", subnet);
    prefs.end();
}

String SystemConfig::getStaticIP() {
    prefs.begin("system", true);
    String ip = prefs.getString("static_ip", "");
    prefs.end();
    return ip;
}

String SystemConfig::getGateway() {
    prefs.begin("system", true);
    String gw = prefs.getString("static_gw", "");
    prefs.end();
    return gw;
}

String SystemConfig::getSubnet() {
    prefs.begin("system", true);
    String sn = prefs.getString("static_sn", "");
    prefs.end();
    return sn;
}

// ============ STATISTICS ============

void SystemConfig::incrementCardsRead() {
    cardsRead++;
}

uint32_t SystemConfig::getCardsRead() {
    return cardsRead;
}

unsigned long SystemConfig::getUptime() {
    return (millis() - startTime) / 1000;
}

// ============ WRITE MODE (SESSION ONLY) ============

void SystemConfig::setMasterSecret(const String& secret) {
    writeModeMasterSecret = secret;
    Serial.println(F("✅ Master secret opgeslagen in RAM (niet persistent)"));
    Serial.print(F("   Secret preview (first 8): "));
    Serial.println(secret.substring(0, 8));
}

String SystemConfig::getMasterSecret() {
    return writeModeMasterSecret;
}

void SystemConfig::clearMasterSecret() {
    // Overwrite sensitive data in memory before clearing
    if (writeModeMasterSecret.length() > 0) {
        char* buffer = const_cast<char*>(writeModeMasterSecret.c_str());
        memset(buffer, 0, writeModeMasterSecret.length());
    }
    writeModeMasterSecret = "";
    Serial.println(F("✅ Master secret veilig gewist uit RAM"));
}

void SystemConfig::setPreviousKey(const String& key) {
    writeModePreviousKey = key;
    Serial.println(F("✅ Previous key opgeslagen in RAM (niet persistent)"));
}

String SystemConfig::getPreviousKey() {
    return writeModePreviousKey;
}

void SystemConfig::clearPreviousKey() {
    // Overwrite sensitive data in memory before clearing
    if (writeModePreviousKey.length() > 0) {
        char* buffer = const_cast<char*>(writeModePreviousKey.c_str());
        memset(buffer, 0, writeModePreviousKey.length());
    }
    writeModePreviousKey = "";
    Serial.println(F("✅ Previous key veilig gewist uit RAM"));
}

void SystemConfig::setIsFactory(bool factory) {
    writeIsFactory = factory;
    Serial.print(F("Factory kaart: "));
    Serial.println(factory ? "ja" : "nee");
}

void SystemConfig::stopWriteMode() {
    if (writeActive) {
        Serial.println(F("\n⏹️  Write mode gestopt - secrets worden gewist..."));
        writeActive = false;
        clearMasterSecret();
        clearPreviousKey();
        writeMode = "";
        writeIsFactory = false;
        writeIsDirectKey = false;
        keySource = "esp32";
        Serial.println(F("✅ Write mode volledig opgeschoond"));
    }
}

bool SystemConfig::checkWriteTimeout() {
    if (!writeActive) {
        return false;
    }
    
    unsigned long elapsed = millis() - writeActiveTimestamp;
    if (elapsed >= WRITE_MODE_TIMEOUT_MS) {
        Serial.println(F("\n⏱️  Write mode timeout bereikt (5 minuten)"));
        stopWriteMode();
        return true;
    }
    
    return false;
}

bool SystemConfig::getIsFactory() {
    return writeIsFactory;
}

void SystemConfig::setIsDirectKey(bool direct) {
    writeIsDirectKey = direct;
    Serial.print(F("Direct key mode: "));
    Serial.println(direct ? "ja (previous key is een afgeleide AES key)" : "nee (previous key is master secret)");
}

bool SystemConfig::getIsDirectKey() {
    return writeIsDirectKey;
}

void SystemConfig::setKeySource(const String& source) {
    keySource = source;
    Serial.print(F("Key source: "));
    Serial.println(source);
}

String SystemConfig::getKeySource() {
    return keySource;
}

void SystemConfig::setWriteMode(const String& mode) {
    writeMode = mode;
    Serial.print(F("Write mode: "));
    Serial.println(mode);
}

String SystemConfig::getWriteMode() {
    return writeMode;
}

void SystemConfig::setWriteActive(bool active) {
    writeActive = active;
    if (active) {
        writeActiveTimestamp = millis();
        Serial.println(F("✅ Write mode geactiveerd - timeout: 5 minuten"));
    } else {
        Serial.println(F("Write mode gedeactiveerd"));
    }
}

bool SystemConfig::isWriteActive() {
    return writeActive;
}

bool SystemConfig::isSingleWriteMode() {
    return writeMode == "single";
}

bool SystemConfig::isContinuousWriteMode() {
    return writeMode == "continuous";
}

// ============ NDEF / URL SETTINGS ============

void SystemConfig::setNdefUrlTemplate(const String& tpl) {
    prefs.begin("system", false);
    prefs.putString("ndef_url_tpl", tpl);
    prefs.end();
}

String SystemConfig::getNdefUrlTemplate() {
    prefs.begin("system", true);
    String v = prefs.getString("ndef_url_tpl", "");
    prefs.end();
    return v;
}

void SystemConfig::setNdefEnabled(bool enabled) {
    prefs.begin("system", false);
    prefs.putBool("ndef_en", enabled);
    prefs.end();
}

bool SystemConfig::isNdefEnabled() {
    prefs.begin("system", true);
    bool v = prefs.getBool("ndef_en", false);
    prefs.end();
    return v;
}

void SystemConfig::setNdefWriteMode(const String& mode) {
    prefs.begin("system", false);
    prefs.putString("ndef_mode", mode);
    prefs.end();
}

String SystemConfig::getNdefWriteMode() {
    prefs.begin("system", true);
    // Default: keys_and_ndef when NDEF is enabled
    String v = prefs.getString("ndef_mode", "keys_only");
    prefs.end();
    return v;
}

// ============ READER REGISTRATION ============

/**
 * Derive a stable reader ID from the WiFi MAC address.
 * Uses SHA-256("NFC-READER-ID:" + MAC_HEX) and takes the first 8 bytes (16 hex chars).
 * Result is cached in NVS so it survives restarts even if WiFi is not yet active.
 */
String SystemConfig::deriveReaderId() {
    // Return cached value if available
    prefs.begin("system", true);
    String cached = prefs.getString("reader_id", "");
    prefs.end();
    if (cached.length() == 16) return cached;

    // Build input: "NFC-READER-ID:" + MAC (no colons, e.g. "AABBCCDDEEFF")
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String input = "NFC-READER-ID:" + mac;

    // SHA-256
    uint8_t hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const uint8_t*)input.c_str(), input.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    // Take first 8 bytes → 16 uppercase hex chars
    String id = "";
    for (int i = 0; i < 8; i++) {
        char hex[3];
        sprintf(hex, "%02X", hash[i]);
        id += hex;
    }

    // Cache in NVS
    prefs.begin("system", false);
    prefs.putString("reader_id", id);
    prefs.end();

    Serial.print(F("📡 Reader ID derived: "));
    Serial.println(id);
    return id;
}

void SystemConfig::setReaderToken(const String& token) {
    prefs.begin("system", false);
    prefs.putString("reader_token", token);
    prefs.end();
    Serial.println(F("✅ Reader token opgeslagen"));
}

String SystemConfig::getReaderToken() {
    prefs.begin("system", true);
    String token = prefs.getString("reader_token", "");
    prefs.end();
    return token;
}

bool SystemConfig::hasReaderToken() {
    return getReaderToken().length() > 0;
}

// ============ RESET FUNCTIONS ============

void SystemConfig::resetNetwork() {
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-config", false);
    wifiPrefs.clear();
    wifiPrefs.end();
    
    prefs.begin("system", false);
    prefs.remove("net_mode");
    prefs.remove("static_ip");
    prefs.remove("static_gw");
    prefs.remove("static_sn");
    prefs.end();
    
    Serial.println(F("Network settings reset"));
}

void SystemConfig::factoryReset() {
    prefs.begin("system", false);
    prefs.clear();
    prefs.end();
    
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-config", false);
    wifiPrefs.clear();
    wifiPrefs.end();
    
    Serial.println(F("Factory reset completed"));
}
