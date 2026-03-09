/*
 * Mini Sumo Robot
 * Application principale de contrôle du véhicule
 * Nombre maximal de lignes: 100
 */

#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  delay(1000);

  Serial.println("Scan I2C...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Capteur trouvé à l'adresse: 0x%02X\n", addr);
    }
  }
  Serial.println("Scan terminé.");
}


void loop() {
  // put your main code here, to run repeatedly:
}

