#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO14443.h>

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_NANO)

#define PN5180_NSS  10
#define PN5180_BUSY 9
#define PN5180_RST  7

#elif defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)

// Seeed Xiao ESP32S3 pinout: Dx != GPIOx!
// D10=GPIO9, D9=GPIO8, D8=GPIO7, D7=GPIO44, D6=GPIO43, D5=GPIO6
#define PN5180_NSS  9   // D10
#define PN5180_BUSY 6   // D5
#define PN5180_RST  43  // D6
#define PN5180_SCK  44  // D7
#define PN5180_MISO 7   // D8
#define PN5180_MOSI 8   // D9

#elif defined(ARDUINO_ARCH_ESP32)

// Standaard ESP32 pinout
#define PN5180_NSS  5
#define PN5180_BUSY 16
#define PN5180_RST  17

#else
#error Please define your pinout here!
#endif

uint32_t loopCnt = 0;
bool errorFlag = false;
uint8_t consecutiveErrors = 0;
const uint8_t MAX_ERRORS = 5;
const uint8_t REBOOT_ERRORS = 10; // Reboot na 10 fouten

// PN5180 watchdog variabelen
unsigned long lastSuccessfulRead = 0;
unsigned long lastPollCheck = 0;
const unsigned long POLL_INTERVAL = 100; // Check elke 0.1 seconden
const unsigned long MAX_NO_RESPONSE = 1000; // Reset na 1 seconden geen respons

PN5180ISO14443 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);

void showIRQStatus(uint32_t irqStatus) {
  Serial.print(F("IRQ-Status 0x"));
  Serial.print(irqStatus, HEX);
  Serial.print(": [ ");
  if (irqStatus & (1<< 0)) Serial.print(F("RQ "));
  if (irqStatus & (1<< 1)) Serial.print(F("TX "));
  if (irqStatus & (1<< 2)) Serial.print(F("IDLE "));
  if (irqStatus & (1<< 3)) Serial.print(F("MODE_DETECTED "));
  if (irqStatus & (1<< 4)) Serial.print(F("CARD_ACTIVATED "));
  if (irqStatus & (1<< 5)) Serial.print(F("STATE_CHANGE "));
  if (irqStatus & (1<< 6)) Serial.print(F("RFOFF_DET "));
  if (irqStatus & (1<< 7)) Serial.print(F("RFON_DET "));
  if (irqStatus & (1<< 8)) Serial.print(F("TX_RFOFF "));
  if (irqStatus & (1<< 9)) Serial.print(F("TX_RFON "));
  if (irqStatus & (1<<10)) Serial.print(F("RF_ACTIVE_ERROR "));
  if (irqStatus & (1<<11)) Serial.print(F("TIMER0 "));
  if (irqStatus & (1<<12)) Serial.print(F("TIMER1 "));
  if (irqStatus & (1<<13)) Serial.print(F("TIMER2 "));
  if (irqStatus & (1<<14)) Serial.print(F("RX_SOF_DET "));
  if (irqStatus & (1<<15)) Serial.print(F("RX_SC_DET "));
  if (irqStatus & (1<<16)) Serial.print(F("TEMPSENS_ERROR "));
  if (irqStatus & (1<<17)) Serial.print(F("GENERAL_ERROR "));
  if (irqStatus & (1<<18)) Serial.print(F("HV_ERROR "));
  if (irqStatus & (1<<19)) Serial.print(F("LPCD "));
  Serial.println("]");
}

void recoverFromError(uint32_t irqStatus) {
  // Clear IRQ status first
  nfc.clearIRQStatus(0xFFFFFFFF);
  delay(10);
  
  // Check for collision/multi-card error
  if (irqStatus & (1<<10)) { // RF_ACTIVE_ERROR
    Serial.println(F("*** Collision detected - multiple cards?"));
  }
  
  // Soft reset and reinitialize
  nfc.reset();
  delay(50);
  nfc.setupRF();
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("PN5180 ISO14443 Demo Sketch"));

#if defined(ARDUINO_XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3)
  Serial.println(F("Configuring SPI: D7(SCK)=GPIO44 D8(MISO)=GPIO7 D9(MOSI)=GPIO8 D10(NSS)=GPIO9"));
  // ESP32 SPI.begin() zonder NSS - NSS wordt handmatig beheerd
  SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI);
#elif defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Configuring SPI with default ESP32 pins"));
  SPI.begin();
#endif

  pinMode(PN5180_NSS, OUTPUT);
  digitalWrite(PN5180_NSS, HIGH);
  pinMode(PN5180_BUSY, INPUT);
  pinMode(PN5180_RST, OUTPUT);
  digitalWrite(PN5180_RST, HIGH);
  
  delay(100); // Geef PN5180 tijd om op te starten
  
  Serial.println(F("Before nfc.begin()"));
  nfc.begin();
  Serial.println(F("XIAO_ESP32S3) || defined(SEEED_XIAO_ESP32S3begin()"));
  
  // Herconfigureer SPI na nfc.begin()
#if defined(ARDUINO_ARCH_ESP32)
  SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI);
  Serial.println(F("SPI reconfigured after nfc.begin()"));
#endif

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  
  nfc.reset();
  
  Serial.println(F("Reset complete"));

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  if (0xff == productVersion[1]) {
    Serial.println(F("Initialization failed!?"));
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    exit(-1);
  }

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading firmware version..."));
  uint8_t firmwareVersion[2];
  nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
  Serial.print(F("Firmware version="));
  Serial.print(firmwareVersion[1]);
  Serial.print(".");
  Serial.println(firmwareVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading EEPROM version..."));
  uint8_t eepromVersion[2];
  nfc.readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
  Serial.print(F("EEPROM version="));
  Serial.print(eepromVersion[1]);
  Serial.print(".");
  Serial.println(eepromVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Enable RF field..."));
  nfc.setupRF();
  
  // Initialiseer watchdog timers
  lastSuccessfulRead = millis();
  lastPollCheck = millis();
}

bool checkPN5180Health() {
  // Test 1: IRQ status moet leesbaar zijn
  uint32_t irqStatus = nfc.getIRQStatus();
  
  if (irqStatus == 0xFFFFFFFF) {
    Serial.println(F("*** PN5180 SPI not responding!"));
    return false;
  }
  
  // Test 1b: Check voor specifiek bekende problematische waarden
  if (irqStatus == 0xE4FB44CA) {
    Serial.println(F("*** PN5180 problematic IRQ status detected!"));
    return false;
  }
  
  // Test 1c: Check voor gezonde/normale status EERST (voor critical error check)
  if (irqStatus == 0x24006 || irqStatus == 0x24007 || irqStatus == 0x4006 || irqStatus == 0x4007) {
    // Dit zijn normale waarden, alles OK
    return true;
  }
  
  // Test 1d: Check voor kritieke error flags (alleen als het GEEN bekende goede waarde is)
  // Check alleen HV_ERROR en TEMPSENS_ERROR (GENERAL_ERROR is te breed)
  if (irqStatus & ((1<<16) | (1<<18))) { // TEMPSENS_ERROR | HV_ERROR
    Serial.print(F("*** PN5180 critical error flags: 0x"));
    Serial.println(irqStatus, HEX);
    showIRQStatus(irqStatus);
    return false;
  }
  
  // GENERAL_ERROR alleen checken in combinatie met andere slechte tekens
  if ((irqStatus & (1<<17)) && (irqStatus & (1<<10))) { // GENERAL_ERROR + RF_ACTIVE_ERROR
    Serial.print(F("*** PN5180 general + RF error: 0x"));
    Serial.println(irqStatus, HEX);
    showIRQStatus(irqStatus);
    return false;
  }
  
  // Test 2: Constant zeros detecteren
  static uint8_t zeroCount = 0;
  static uint32_t lastIrqStatus = 0;
  
  if (irqStatus == 0x00000000 && lastIrqStatus == 0x00000000) {
    zeroCount++;
    if (zeroCount > 3) {
      Serial.println(F("*** PN5180 stuck at zero!"));
      zeroCount = 0;
      return false;
    }
  } else {
    zeroCount = 0;
  }
  lastIrqStatus = irqStatus;
  
  // Test 3: EEPROM moet leesbaar zijn (elke 5e check om overhead te beperken)
  static uint8_t eepromCheckCounter = 0;
  if (++eepromCheckCounter >= 5) {
    eepromCheckCounter = 0;
    
    uint8_t testByte[1];
    if (!nfc.readEEprom(PRODUCT_VERSION, testByte, 1)) {
      Serial.println(F("*** PN5180 EEPROM read failed!"));
      return false;
    }
    
    if (testByte[0] == 0xFF) {
      Serial.println(F("*** PN5180 EEPROM corrupted!"));
      return false;
    }
  }
  
  return true;
}

void resetPN5180() {
  
  nfc.reset();
  delay(100);
  
  // Verifieer reset door product versie te lezen
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  
  if (0xff == productVersion[1]) {
    Serial.println(F("*** PN5180 reset failed - rebooting ESP32..."));
    Serial.flush();
    delay(1000);
    ESP.restart();
  }
  
  nfc.setupRF();
  
  lastSuccessfulRead = millis();
  consecutiveErrors = 0;
}

// ISO 14443 loop
void loop() {
  unsigned long currentMillis = millis();
  
  // Periodieke health check
  if (currentMillis - lastPollCheck >= POLL_INTERVAL) {
    lastPollCheck = currentMillis;
    
    if (!checkPN5180Health()) {
      Serial.println(F("*** PN5180 health check failed"));
      resetPN5180();
      return;
    }
    else{
      lastSuccessfulRead = currentMillis; 
    }
  }
  
  // Watchdog: te lang geen succesvolle operatie
  if (lastSuccessfulRead > 0 && 
      (currentMillis - lastSuccessfulRead) >= MAX_NO_RESPONSE) {
    Serial.println(F("*** PN5180 watchdog timeout - no response for 1s"));
    resetPN5180();
    return;
  }
  
  if (errorFlag) {
    uint32_t irqStatus = nfc.getIRQStatus();
    showIRQStatus(irqStatus);

    if (0 == (RX_SOF_DET_IRQ_STAT & irqStatus)) {
      Serial.println(F("*** No card detected!"));
    }

    recoverFromError(irqStatus);
    
    consecutiveErrors++;
    
    if (consecutiveErrors >= REBOOT_ERRORS) {
      Serial.println(F("*** Too many consecutive errors - rebooting ESP32..."));
      Serial.flush();
      delay(1000);
      ESP.restart();
    }
    
    if (consecutiveErrors >= MAX_ERRORS) {
      Serial.println(F("*** Too many errors, waiting 2 seconds..."));
      delay(2000);
    }

    errorFlag = false;
    delay(10);
    return;
  }

  static bool wasPresent = false;
  static bool shown = false;
  
  bool present = nfc.isCardPresent();
  
  if (!present) {
    if (wasPresent) {
      Serial.println(F("Card removed"));
      shown = false;
      consecutiveErrors = 0; // Reset error counter when card removed
      lastSuccessfulRead = millis(); // Update watchdog
    }
    wasPresent = false;
    delay(100); // Small delay to prevent tight loop
    return;
  }
  
  wasPresent = true;
  
  uint8_t uid[8];
  if (!nfc.readCardSerial(uid)) {
    Serial.println(F("Error in readCardSerial"));
    errorFlag = true;
    return;
  }
  
  // Successful read - reset error counter and update watchdog
  consecutiveErrors = 0;
  lastSuccessfulRead = millis();
  
  if (!shown) {
    Serial.print(F("Card serial successful, UID="));
    for (int i=0; i<8; i++) {
      if (uid[i] < 0x10) Serial.print(F("0")); // Leading zero voor single digits
      Serial.print(uid[i], HEX);
      if (i < 7) Serial.print(F(":"));
    }
    Serial.println();
    shown = true;
  }

  delay(1000);  
}