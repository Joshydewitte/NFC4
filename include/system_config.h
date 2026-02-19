#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Preferences.h>
#include <mbedtls/md.h>

class SystemConfig {
private:
    Preferences prefs;
    
    // Runtime settings (niet opgeslagen)
    String sessionMasterkey = "";
    bool masterkeyActive = false;
    unsigned long startTime = 0;
    uint32_t cardsRead = 0;
    
public:
    SystemConfig() {
        startTime = millis();
    }
    
    void begin() {
        prefs.begin("system", false);
    }
    
    // ============ ADMIN MANAGEMENT ============
    
    bool hasAdminAccount() {
        prefs.begin("system", true);
        bool exists = prefs.isKey("admin_user");
        prefs.end();
        return exists;
    }
    
    String hashPassword(const String& password) {
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
    
    bool createAdminAccount(const String& username, const String& password) {
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
    
    bool validateAdmin(const String& username, const String& password) {
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
    
    void setServerUrl(const String& url) {
        prefs.begin("system", false);
        prefs.putString("server_url", url);
        prefs.end();
        Serial.print(F("Server URL saved: "));
        Serial.println(url);
    }
    
    String getServerUrl() {
        prefs.begin("system", true);
        String url = prefs.getString("server_url", "");
        prefs.end();
        return url;
    }
    
    // ============ READER MODE ============
    
    void setReaderMode(const String& mode) {
        prefs.begin("system", false);
        prefs.putString("reader_mode", mode);
        prefs.end();
        Serial.print(F("Reader mode set to: "));
        Serial.println(mode);
    }
    
    String getReaderMode() {
        prefs.begin("system", true);
        String mode = prefs.getString("reader_mode", "machine");
        prefs.end();
        return mode;
    }
    
    bool isMachineMode() {
        return getReaderMode() == "machine";
    }
    
    bool isConfigMode() {
        return getReaderMode() == "config";
    }
    
    // ============ MASTERKEY (SESSION ONLY) ============
    
    void setSessionMasterkey(const String& key) {
        sessionMasterkey = key;
        masterkeyActive = (key.length() == 32);
        if (masterkeyActive) {
            Serial.println(F("✅ Masterkey activated for session"));
        }
    }
    
    String getSessionMasterkey() {
        return masterkeyActive ? sessionMasterkey : "";
    }
    
    bool hasMasterkey() {
        return masterkeyActive;
    }
    
    void clearSessionMasterkey() {
        sessionMasterkey = "";
        masterkeyActive = false;
        Serial.println(F("Masterkey cleared"));
    }
    
    // ============ NETWORK SETTINGS ============
    
    void setNetworkMode(const String& mode) {
        prefs.begin("system", false);
        prefs.putString("net_mode", mode);
        prefs.end();
    }
    
    String getNetworkMode() {
        prefs.begin("system", true);
        String mode = prefs.getString("net_mode", "dhcp");
        prefs.end();
        return mode;
    }
    
    void setStaticIP(const String& ip, const String& gateway, const String& subnet) {
        prefs.begin("system", false);
        prefs.putString("static_ip", ip);
        prefs.putString("static_gw", gateway);
        prefs.putString("static_sn", subnet);
        prefs.end();
    }
    
    String getStaticIP() {
        prefs.begin("system", true);
        String ip = prefs.getString("static_ip", "");
        prefs.end();
        return ip;
    }
    
    String getGateway() {
        prefs.begin("system", true);
        String gw = prefs.getString("static_gw", "");
        prefs.end();
        return gw;
    }
    
    String getSubnet() {
        prefs.begin("system", true);
        String sn = prefs.getString("static_sn", "");
        prefs.end();
        return sn;
    }
    
    // ============ STATISTICS ============
    
    void incrementCardsRead() {
        cardsRead++;
    }
    
    uint32_t getCardsRead() {
        return cardsRead;
    }
    
    unsigned long getUptime() {
        return (millis() - startTime) / 1000;
    }
    
    // ============ RESET FUNCTIONS ============
    
    void resetNetwork() {
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
    
    void factoryReset() {
        prefs.begin("system", false);
        prefs.clear();
        prefs.end();
        
        Preferences wifiPrefs;
        wifiPrefs.begin("wifi-config", false);
        wifiPrefs.clear();
        wifiPrefs.end();
        
        Serial.println(F("Factory reset completed"));
    }
};

#endif // SYSTEM_CONFIG_H
