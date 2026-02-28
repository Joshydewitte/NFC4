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
      writeIsFactory(true) {
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
    sessionMasterkey = "";
    masterkeyActive = false;
    Serial.println(F("Masterkey cleared"));
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
}

String SystemConfig::getMasterSecret() {
    return writeModeMasterSecret;
}

void SystemConfig::clearMasterSecret() {
    writeModeMasterSecret = "";
    Serial.println(F("Master secret gewist uit RAM"));
}

void SystemConfig::setPreviousKey(const String& key) {
    writeModePreviousKey = key;
    Serial.println(F("✅ Previous key opgeslagen in RAM (niet persistent)"));
}

String SystemConfig::getPreviousKey() {
    return writeModePreviousKey;
}

void SystemConfig::clearPreviousKey() {
    writeModePreviousKey = "";
    Serial.println(F("Previous key gewist uit RAM"));
}

void SystemConfig::setIsFactory(bool factory) {
    writeIsFactory = factory;
    Serial.print(F("Factory kaart: "));
    Serial.println(factory ? "ja" : "nee");
}

bool SystemConfig::getIsFactory() {
    return writeIsFactory;
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
    Serial.print(F("Write active: "));
    Serial.println(active ? "YES" : "NO");
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
