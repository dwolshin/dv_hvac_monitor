#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>



// The neatest way to access variables stored in EEPROM is using a structure
struct MyEEPROMStruct {
  String AIO_KEY;
  String AIO_USERNAME;
} eepromVar1;



void setup() {
  // put your setup code here, to run once:
  Serial.begin(74880);
  delay(1000);

  EEPROM.begin(sizeof(MyEEPROMStruct));

  eepromVar1.AIO_USERNAME = "dwolshin";
  eepromVar1.AIO_KEY = "aio_vYAG33QmsFoNvMvB8dwXpppru8VR";


  // set the EEPROM data ready for writing
  EEPROM.put(0, eepromVar1);

  // write the data to EEPROM - ignoring anything that might be there already (re-flash is guaranteed)
  boolean ok1 = EEPROM.commitReset();
  Serial.println((ok1) ? "Commit (Reset) OK" : "Commit failed");



}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Flash setup complete!");
  while (1) {
    delay (30000);
  }
}
