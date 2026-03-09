#include <Wire.h>
#include <Arduino.h>
#define TOF_ADDR 0x09  // 0x08 + ID(1)

float readDistance() {
  Wire.beginTransmission(TOF_ADDR);
  Wire.write(0x24);  // ← registre correct pour la distance
  Wire.endTransmission(false);
  Wire.requestFrom(TOF_ADDR, (uint8_t)4);

  if (Wire.available() < 4) return -1.0f;

  uint8_t d[4];
  for (int i = 0; i < 4; i++) d[i] = Wire.read();

  // Little-endian, 24 bits signés, unité = mm
  int32_t raw = (int32_t)(d[0] | (d[1] << 8) | (d[2] << 16));
  if (raw & 0x800000) raw |= 0xFF000000;
  return raw / 1000.0f;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz max supporté
}

void loop() {
  float dist = readDistance();
  if (dist < 0)
    Serial.println("Erreur lecture");
  else
    Serial.printf("Distance: %.3f m\n", dist);
  delay(20);
}