






#ifndef READERCONTROL_H
#define READERCONTROL_H

#include <PN5180ISO14443.h>

class ReaderControl {
public:
    ReaderControl();
    void begin();
    void loop();
    
private:
    bool checkPN5180Health();
    void resetPN5180();
    void showIRQStatus(uint32_t irqStatus);
    void recoverFromError(uint32_t irqStatus);

    static constexpr unsigned long POLL_INTERVAL = 800; // 0.8 seconds
    static constexpr unsigned long MAX_NO_RESPONSE = 5000; // 5 seconds
    static constexpr uint8_t MAX_ERRORS = 5;
    static constexpr uint8_t REBOOT_ERRORS = 10;

    unsigned long lastPollCheck;
    unsigned long lastSuccessfulRead;
    uint8_t consecutiveErrors;
    bool errorFlag;
    bool wasPresent;
    bool shown;
    
    PN5180ISO14443 nfc;
};

#endif // READERCONTROL_H