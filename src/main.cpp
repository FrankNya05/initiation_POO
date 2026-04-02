#include <Arduino.h>
#include "DriverMotor.hpp"

DriverMotor motor;

void setup() {
    Serial.begin(115200);

    if (!motor.init()) {
        Serial.println("Erreur init moteur !");
        return;
    }

     for (int i = 0; i < 100; i++)
    {
       motor.setSpeedPercent(200, 0);
       delay(100);
    }
    motor.stop();
    delay(100);
     for (int i = 100; i > 0; i--)
    {
       motor.setSpeedPercent(200, 0);
       delay(100);
    }
    motor.stop();
}

void loop() {}
