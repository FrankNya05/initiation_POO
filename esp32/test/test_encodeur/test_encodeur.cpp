#include <Arduino.h>
#include "WiFiComm.hpp"
#include "RTOSConfig.hpp"
#include "SensorManger.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "PinConfig.hpp"
#include "Encoder.hpp"
#include "DriverMotor.hpp"
#include "DriverManager.hpp"
#include "BattSensor.hpp"
#include <ArduinoJson.h>


std::string serializeSensorData(const SensorData& data) {
    StaticJsonDocument<256> doc;

    doc["ts"]    = data.timestamp;
    doc["pos"]   = (int)data.position;
    doc["valid"] = data.isValid;

    // Scalar ou Vector selon le type de capteur
    if (data.dims == SensorDims::VEC3) {
        JsonObject val = doc.createNestedObject("val");
        val["x"] = round(data.value.vector.x * 1000) / 1000.0;  // distance
        val["y"] = round(data.value.vector.y * 10)   / 10.0;     // angle
        val["z"] = (int)data.value.vector.z;                      // nb points
    } else {
        doc["val"] = round(data.value.scalar * 1000) / 1000.0;
    }

    std::string output;
    serializeJson(doc, output);
    return output.c_str();
}

// ── Instanciation ─────────────────────────────────────────────────
WiFiComm comm(
    "S21",           // ssid
    "ftel7488",         // password
    "10.164.172.249",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);
SensorManager sensorManager;
Encoder encLeft;
Encoder encRight;
//=======
DriverMotor driver;
DriverManager divices;
//>>>>>>> b215584e08a6b66950a8b05360cafe94028d3953:src/main.cpp

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