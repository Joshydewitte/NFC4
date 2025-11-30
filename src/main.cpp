#include <Arduino.h>
#include <ReaderControl.h>

// Create ReaderControl instance
ReaderControl reader;

void setup() {
  Serial.begin(115200);
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("PN5180 ISO14443 Demo Sketch"));
  
  // Initialize the reader
  reader.begin();
}

void loop() {
  reader.loop();
}



