#include <Arduino.h>
#include "IRSensor.hpp"

SemaphoreHandle_t g_logMutex = nullptr;

TOFSensor tofL(RobotConfig::TOF_IIC_ADDR,  SensorPosition::FRONT_LEFT);
TOFSensor tofR(RobotConfig::TOF_IIC_ADDR1, SensorPosition::FRONT_RIGHT);

void readTOF() {
    tofL.update();
    tofR.update();

    SensorData dL = tofL.getData();
    SensorData dR = tofR.getData();

    Serial.printf("FL: %.0f mm (valid:%d)  |  FR: %.0f mm (valid:%d)\n",
        dL.value.scalar, dL.isValid,
        dR.value.scalar, dR.isValid);
}

void setup() {
    Serial.begin(115200);
    g_logMutex = xSemaphoreCreateMutex();

    Serial.println(tofL.init() ? "[TOF FL] Init OK" : "[TOF FL] ERREUR");
    Serial.println(tofR.init() ? "[TOF FR] Init OK" : "[TOF FR] ERREUR");
}

void loop() {
    readTOF();
    delay(200);
}
