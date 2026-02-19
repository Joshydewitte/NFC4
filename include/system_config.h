#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/md.h>

class SystemConfig {
private:
    Preferences prefs;
    
    // Runtime settings (niet opgeslagen)
    String sessionMasterkey;
    bool masterkeyActive;
    unsigned long startTime;
    uint32_t cardsRead;
    
public:
    SystemConfig();
    
    void begin();
    
    // ============ ADMIN MANAGEMENT ============
    bool hasAdminAccount();
    String hashPassword(const String& password);
    bool createAdminAccount(const String& username, const String& password);
    bool validateAdmin(const String& username, const String& password);
    
    // ============ SERVER SETTINGS ============
    void setServerUrl(const String& url);
    String getServerUrl();
    
    // ============ READER MODE ============
    void setReaderMode(const String& mode);
    String getReaderMode();
    bool isMachineMode();
    bool isConfigMode();
    
    // ============ MASTERKEY (SESSION ONLY) ============
    void setSessionMasterkey(const String& key);
    String getSessionMasterkey();
    bool hasMasterkey();
    void clearSessionMasterkey();
    
    // ============ NETWORK SETTINGS ============
    void setNetworkMode(const String& mode);
    String getNetworkMode();
    void setStaticIP(const String& ip, const String& gateway, const String& subnet);
    String getStaticIP();
    String getGateway();
    String getSubnet();
    
    // ============ STATISTICS ============
    void incrementCardsRead();
    uint32_t getCardsRead();
    unsigned long getUptime();
    
    // ============ RESET FUNCTIONS ============
    void resetNetwork();
    void factoryReset();
};

#endif // SYSTEM_CONFIG_H
