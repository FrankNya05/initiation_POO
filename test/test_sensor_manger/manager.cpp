#include <Arduino.h>
#include "RTOSConfig.hpp"    // ← LOG / LOGF
#include "SensorManger.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "PinConfig.hpp"
#include "BattSensor.hpp"
SensorManager sensorManager;

void setup() {
    Serial.begin(115200);

    // ── Capteurs de ligne ─────────────────────
    // LineSensor attend un SensorType (pin), pas une SensorPosition
    sensorManager.add(new LineSensor(SensorPosition::BACK), true);
    sensorManager.add(new LineSensor(SensorPosition::UNKNOWN), true);
    sensorManager.add(new LineSensor(SensorPosition::RIGHT), true);

    // ── TOF avant ────────────────────────────
    // Constructeur : (adresse I2C, position)
    sensorManager.add(new TOFSensor(RobotConfig::TOF_IIC_ADDR1, SensorPosition::FRONT), true);

    // 
    sensorManager.add(new BattSensor(SensorPosition::CENTER), true);

    // ── Initialisation ────────────────────────
    if (!sensorManager.initAll()) {
        LOG("Erreur init capteurs !");
    }
}

void loop() {

    sensorManager.updateAll();

    // ── Lire le TOF ───────────────────────────
    // getDataByPosition retourne par VALEUR — pas un pointeur !
  /* SensorData tofData = sensorManager.getDataByPosition(SensorPosition::CENTER);
    if (tofData.isValid) {
        LOGF("Distance adversaire : %.3f m\n", tofData.value.scalar);
    }
*/
    SensorData batLevel = sensorManager.getDataByPosition(SensorPosition::CENTER);
    if (batLevel.isValid) {
        LOGF("Miveau de la batterie : %.3f m\n", batLevel.value.scalar);
    } 
    // ── Debug ─────────────────────────────────
    sensorManager.printAll();

    delay(1000);
}