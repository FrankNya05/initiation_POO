#include <Arduino.h>
#include "IRSensor.hpp"

// Capteur ToF frontal — adresse I2C définie dans PinConfig
TOFSensor tof(RobotConfig::TOF_IIC_ADDR, SensorPosition::FRONT);

void setup() {
    Serial.begin(115200);

    if (!tof.init()) {
        Serial.println("[main] Echec init TOFSensor");
        return;
    }
    Serial.println("[main] TOFSensor prêt");
}

void loop() {
    if (tof.update()) {
        SensorData d = tof.getData();
        Serial.printf("[TOF] distance = %.3f m  (t=%lu ms)\n",
                      d.value.scalar, d.timestamp);
    } else {
        Serial.println("[TOF] lecture invalide");
    }

    delay(100);
}
