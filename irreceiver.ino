#include <Arduino.h>
#include <IRremote.hpp>

#define IR_RECEIVE_PIN 26

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Alıcı Hazır. Kumanda tuşlarına basın...");
}

void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      Serial.print("Tuş Kodu (HEX): 0x");
      Serial.println(IrReceiver.decodedIRData.command, HEX);
    }
    IrReceiver.resume();
  }
}