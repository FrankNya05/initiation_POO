#include <Arduino.h>
#include "DriverMotor.hpp"
#include "Encoder.hpp"
DriverMotor motor;
Encoder encLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H);
Encoder encRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H);


void setup() {
    Serial.begin(115200);

    encLeft.init(true);   // isLeft = true
    encRight.init(false);

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

void loop() {
    int32_t pulsesL = encLeft.getAndReset();
    int32_t pulsesR = encRight.getAndReset();
    // utiliser pour PID...
    delay(10);
}
