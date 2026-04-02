#include <Arduino.h>
#include "DriverMotor.hpp"

DriverMotor motor;

void setup() {
    Serial.begin(115200);

    if (!motor.init()) {
        Serial.println("Erreur init moteur !");
        return;
    }

    // Avance 2 secondes
    motor.setSpeed(200, 200);
    delay(2000);

    // Tourne à gauche 1 seconde
    motor.setSpeed(-150, 150);
    delay(1000);

    // Recule 1 seconde
    motor.setSpeed(-200, -200);
    delay(1000);

    // Arrêt
    motor.stop();
}

void loop() {}
