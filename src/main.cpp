#include "DriverMotor.hpp"
#include "Encoder.hpp"
#include "DriverManager.hpp"

Encoder encLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H);
Encoder encRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H);

DriverMotor driver;
DriverManager divices;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== SETUP START ===");

    encLeft.init(true);
    encRight.init(false);
    Serial.println("Encodeurs initialisés");

    divices.add(&driver, DriverRole::MAIN, true);
    if (!divices.initAll()) {
        Serial.println("ERREUR init moteurs!");
    } else {
        Serial.println("Moteurs initialisés");
    }

    encLeft.resetAngle();
    encRight.resetAngle();
    Serial.println("=== SETUP DONE ===");
}
static float nextTarget = 5.0f;
static bool  goingBack  = false;  // false = 0→360, true = 360→0
static uint32_t startMs = 0;
static bool started     = false;
static uint32_t lastPrintMs = 0;

void loop() {
    uint32_t now = millis();

    if (!started) {
        startMs = now;
        started = true;
        nextTarget = 5.0f;
        Serial.println("Moteur démarré ! (0° → 360°)");
    }

    float angleL = encLeft.getRelativeAngleDeg();
    float angleR = encRight.getRelativeAngleDeg();

    if (now - lastPrintMs >= 200) {
        Serial.printf("angle L = %6.1f° | angle R = %6.1f°\n", angleL, angleR);
        lastPrintMs = now;
    }

    if (!goingBack) {
        // Aller : 0 → 360
        divices.move(RobotConstants::Direction::AVANT, 80);

        if (angleL >= nextTarget) {
            Serial.printf(">>> Palier %5.1f° atteint !\n", nextTarget);
            nextTarget += 5.0f;
        }

        if (angleL >= 360.0f) {
            uint32_t elapsed = now - startMs;
            Serial.printf("--- 360° atteint en %lu ms (%.2f s) ---\n",
                          elapsed, elapsed / 1000.0f);

            // Préparer retour
            goingBack  = true;
            nextTarget = 360.0f - 5.0f; // paliers décroissants
            encLeft.resetAngle();
            encRight.resetAngle();
            startMs = now;
            Serial.println("Retour : 360° → 0°");
        }
    } else {
        // Retour : 360 → 0
        divices.move(RobotConstants::Direction::ARRIERE, 80);

        float absAngle = -angleL; // angle négatif en arrière

        if (absAngle >= (360.0f - nextTarget)) {
            Serial.printf(">>> Palier %5.1f° atteint !\n", nextTarget);
            nextTarget -= 5.0f;  // Décrémenter
        }

        if (absAngle >= 360.0f) {
            uint32_t elapsed = now - startMs;
            Serial.printf("--- Retour 0° atteint en %lu ms (%.2f s) ---\n",
                          elapsed, elapsed / 1000.0f);

            // Préparer prochain aller
            goingBack  = false;
            nextTarget = 5.0f;
            encLeft.resetAngle();
            encRight.resetAngle();
            startMs = now;
            Serial.println("Nouvel aller : 0° → 360°");
        }
    }
}