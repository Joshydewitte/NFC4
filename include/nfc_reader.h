#ifndef NFC_READER_H
#define NFC_READER_H

#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

// Forward declaration
class NFCWebServer;

class NFCReader {
public:
    // Pin configuration structure
    struct PinConfig {
        uint8_t nss;
        uint8_t busy;
        uint8_t rst;
        uint8_t sck;
        uint8_t miso;
        uint8_t mosi;
    };
    
    // Card info structure
    struct CardInfo {
        uint8_t uid[8];
        uint8_t uidLength;
        String uidString;
        String cardType;
        bool isPresent;
        bool isSecure;
    };
    
    NFCReader(const PinConfig& pins);
    ~NFCReader();
    
    // Lifecycle
    bool begin();
    void loop();
    void reset();
    
    // Card operations
    bool isCardPresent();
    bool readCard(CardInfo& cardInfo);
    String getCardTypeDescription(const uint8_t* uid) const;
    
    // Health monitoring
    bool checkHealth();
    uint32_t getConsecutiveErrors() const { return consecutiveErrors; }
    void clearErrors() { consecutiveErrors = 0; }
    
    // WebServer integration
    void setWebServer(NFCWebServer* ws) { webServer = ws; }
    
    // Stats
    unsigned long getLastSuccessfulRead() const { return lastSuccessfulRead; }
    
private:
    // PN5180 instance
    PN5180ISO14443* nfc;
    PinConfig pins;
    
    // WebServer pointer for logging
    NFCWebServer* webServer;
    
    // State tracking
    bool initialized;
    bool cardWasPresent;
    bool cardShown;
    uint8_t consecutiveErrors;
    
    // Watchdog
    unsigned long lastSuccessfulRead;
    unsigned long lastPollCheck;
    static const unsigned long POLL_INTERVAL = 100;
    static const unsigned long MAX_NO_RESPONSE = 1000;
    
    // Error handling
    static const uint8_t MAX_ERRORS = 5;
    static const uint8_t REBOOT_ERRORS = 10;
    bool errorFlag;
    
    // Private methods
    void initializePins();
    void configureSPI();
    bool verifyDevice();
    void printDeviceInfo();
    
    // Health checks
    bool checkSPICommunication();
    bool checkEEPROM();
    bool checkIRQStatus();
    
    // Error handling
    void handleError(uint32_t irqStatus);
    void recoverFromError(uint32_t irqStatus);
    void showIRQStatus(uint32_t irqStatus);
    
    // Card detection
    bool detectCard7ByteUID(const uint8_t* uid) const;
    
    // Logging helpers
    void logToWeb(const String& message, const String& level = "info");
};

#endif // NFC_READER_H
