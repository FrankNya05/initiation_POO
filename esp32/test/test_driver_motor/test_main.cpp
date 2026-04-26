#include <Arduino.h>
#include "DriverMotor.hpp"

DriverMotor motor;

void setup() {
    Serial.begin(115200);

    if (!motor.init()) {
        Serial.println("Erreur init moteur !");
        return;
    }
    for (size_t i = 0; i < 254; i++)
    {
       motor.setSpeed(200,200);
       delay(100);
    }
    motor.stop();
}

void loop() {}
