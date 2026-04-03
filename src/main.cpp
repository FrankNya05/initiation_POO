#include "DriverMotor.hpp"
#include "Encoder.hpp"

// 1. Instanciation — pinA (phase), pinB (hall)
Encoder encLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H);
Encoder encRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H);

void setup() {
    Serial.begin(115200);

    // 2. Initialisation — attache les ISR de quadrature
    encLeft.init(true);   // isLeft = true  → utilise _isrLeft
    encRight.init(false); // isLeft = false → utilise _isrRight
}

void loop() {
    static uint32_t lastMs = millis();

    uint32_t now   = millis();
    float    delta = now - lastMs;   // intervalle en ms
    lastMs = now;

    // 3a. Lire et remettre à zéro atomiquement (usage typique PID)
    int32_t pulsesL = encLeft.getAndReset();
    int32_t pulsesR = encRight.getAndReset();

    // 3b. Ou lire sans réinitialiser (position cumulée)
    int32_t posL = encLeft.getCount();

    // 4. Conversion en RPM
    float rpmL = encLeft.getRPM(pulsesL, delta);
    float rpmR = encRight.getRPM(pulsesR, delta);

    Serial.printf("Gauche: %d pulses → %.1f RPM | Droite: %d pulses → %.1f RPM\n",
                  pulsesL, rpmL, pulsesR, rpmR);

    // 5. Remise à zéro manuelle (si besoin de reset sans lecture)
    // encLeft.reset();

    delay(100); // période de 100 ms
}
