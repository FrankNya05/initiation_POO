// ============================================
//  Test moteurs - ESP32 + DRV8833 (avec PWM)
//  AIN1 (GPIO26) et BIN2 (GPIO14) = digital seulement
// ============================================
#include <Arduino.h>

#define AIN1  26   // digital seulement (DAC2)
#define AIN2  25   // gpio25 = in4
#define BIN1  27   // PWM
#define BIN2  14   // digital seulement

#define CH_AIN2  2
#define CH_BIN1  3
#define PWM_FREQ  20000
#define PWM_RES   8  // 0-255

void setup() {
  Serial.begin(115200);

  pinMode(AIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  ledcSetup(CH_AIN2, PWM_FREQ, PWM_RES);
  ledcSetup(CH_BIN1, PWM_FREQ, PWM_RES);
  ledcAttachPin(AIN2, CH_AIN2);
  ledcAttachPin(BIN1, CH_BIN1);

  Serial.println("Test moteurs pret");
}

// Avancer : moteur A slow decay, moteur B fast decay → même vitesse effective
// v : 0-255
void avancer(int v) {
  digitalWrite(AIN1, HIGH); ledcWrite(CH_AIN2, 255 - v); // slow decay : AIN1=H, AIN2=PWM(255-v)
  ledcWrite(CH_BIN1, v);    digitalWrite(BIN2, LOW);      // fast decay : BIN1=PWM(v), BIN2=L
}

// Reculer : moteur A fast decay, moteur B slow decay → même vitesse effective
// v : 0-255
void reculer(int v) {
  digitalWrite(AIN1, LOW);  ledcWrite(CH_AIN2, v);        // fast decay : AIN1=L, AIN2=PWM(v)
  ledcWrite(CH_BIN1, 255 - v); digitalWrite(BIN2, HIGH);  // slow decay : BIN1=PWM(255-v), BIN2=H
}

void stopMoteurs() {
  digitalWrite(AIN1, LOW); ledcWrite(CH_AIN2, 0);
  ledcWrite(CH_BIN1, 0);   digitalWrite(BIN2, LOW);
}

void loop() {
  Serial.println("Stop");
  stopMoteurs();
  delay(1000);

  Serial.println("Reculer (v=200)");
  reculer(200);
  delay(5000);
avancer(255);
 delay(5000);
}
