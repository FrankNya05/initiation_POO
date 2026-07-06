#include <Arduino.h>
#include "DriverMotor.hpp"
#include "Encoder.hpp"
#include "RobotConstants.hpp"
#include "PinConfig.hpp"

DriverMotor motor;
Encoder encLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H,
                 RobotConstants::PULSES_PER_REV);
Encoder encRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H,
                 RobotConstants::PULSES_PER_REV_RIGHT);

constexpr int   TEST_PWM     = 100;
constexpr float DEG_PER_PULSE = RobotConstants::DEG_PER_PULSE;  // ~0.257° @ 50:1

// Tourne la roue gauche de targetDeg degrés (encoder-based)
void rotateLeft(float targetDeg) {
    encLeft.resetAngle();
    motor.setSpeed(TEST_PWM, 0);
    Serial.printf("[G] cible=%.0f deg  (%.0f pulses)\n",
                  targetDeg, targetDeg / DEG_PER_PULSE);
    while (fabsf(encLeft.getRelativeAngleDeg()) < targetDeg) { delay(1); }
    motor.stop();
    Serial.printf("[G] atteint=%.2f deg\n\n", fabsf(encLeft.getRelativeAngleDeg()));
    delay(300);
}

// Tourne la roue droite de targetDeg degrés (encoder-based)
void rotateRight(float targetDeg) {
    encRight.resetAngle();
    motor.setSpeed(0, TEST_PWM);
    Serial.printf("[D] cible=%.0f deg  (%.0f pulses)\n",
                  targetDeg, targetDeg / DEG_PER_PULSE);
    while (fabsf(encRight.getRelativeAngleDeg()) < targetDeg) { delay(1); }
    motor.stop();
    Serial.printf("[D] atteint=%.2f deg\n\n", fabsf(encRight.getRelativeAngleDeg()));
    delay(300);
}

void printMenu() {
    Serial.println("─────────────────────────────────────────");
    Serial.printf(" PPR G=%d  PPR D=%d  PWM=%d  DEG/pulse=%.4f\n",
                  RobotConstants::PULSES_PER_REV,
                  RobotConstants::PULSES_PER_REV_RIGHT,
                  TEST_PWM, DEG_PER_PULSE);
    Serial.println("─────────────────────────────────────────");
    Serial.println(" 1 → G  90°   2 → D  90°");
    Serial.println(" 3 → G 180°   4 → D 180°");
    Serial.println(" 5 → G 360°   6 → D 360°");
    Serial.println(" 7 → G  45°   8 → D  45°");
    Serial.println("─────────────────────────────────────────");
}

void setup() {
    Serial.begin(115200);
    if (!motor.init()) {
        Serial.println("ERREUR init moteur");
        return;
    }
    encLeft.init(true);
    encRight.init(false);
    printMenu();
}

void loop() {
    if (!Serial.available()) return;

    char c = Serial.read();
    switch (c) {
        case '1': rotateLeft(90);   break;
        case '2': rotateRight(90);  break;
        case '3': rotateLeft(180);  break;
        case '4': rotateRight(180); break;
        case '5': rotateLeft(360);  break;
        case '6': rotateRight(360); break;
        case '7': rotateLeft(45);   break;
        case '8': rotateRight(45);  break;
        default:  printMenu();      break;
    }
}
