<<<<<<< HEAD

/**
 * exemple_button.cpp
 * ------------------
 * Exemples d'utilisation de la classe Button dans un projet mini-sumo ESP32.
 *
 * Trois patterns courants :
 *   1. Utilisation simple dans loop()
 *   2. Utilisation dans une tâche FreeRTOS avec queue
 *   3. Machine d'états du robot pilotée par les événements bouton
 */
 
#include <Arduino.h>
#include "SensorSwitch.hpp"
#include "SensorTypes.hpp"
 
// ======================================================================
//  EXEMPLE 1 — Utilisation simple dans loop()
// ======================================================================
 
Button startBtn(RobotConfig::START_BUTTON, SensorPosition::FRONT);
 
void setup() {
    Serial.begin(115200);
    startBtn.init();
}
 
void loop() {
    if (startBtn.update()) {
        SensorData data = startBtn.getData();
 
        switch (startBtn.getEvent()) {
            case Event::SINGLE_PRESS:
                Serial.println("Appui simple → démarrer le robot");
                break;
 
            case Event::DOUBLE_PRESS:
                Serial.println("Double appui → changer de stratégie");
                break;
 
            case Event::LONG_PRESS:
                Serial.println("Appui long → arrêt d'urgence");
                break;
 
            default:
                break;
        }
 
        // On peut aussi utiliser la valeur numérique dans SensorData
        Serial.print("  timestamp = ");
        Serial.print(data.timestamp);
        Serial.print(" ms | event code = ");
        Serial.println((int)data.value.scalar);
    }
}
 
=======
#include <Arduino.h>
#include "WiFiComm.hpp"
#include "RTOSConfig.hpp"
#include "SensorManger.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "PinConfig.hpp"
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
    "S25Ultra",           // ssid
    "sylvain123",         // password
    "10.135.195.249",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);
SensorManager sensorManager;

void setup() {
    Serial.begin(115200);

    // ── Enregistre les callbacks AVANT begin() ────────────────────
    sensorManager.add(new BattSensor(SensorPosition::CENTER), true);

    if (!sensorManager.initAll()) {
        LOG("Erreur init capteurs !");
    }

    comm.onConnect([]() {
        LOG("✅ Connecté au broker Mosquitto !");
    });

    comm.onDisconnect([]() {
        LOG("❌ Connexion perdue !");
    });

    comm.onReceive([](const std::string& msg) {
        LOGF("📩 Message reçu : %s\n", msg.c_str());

        // Exemple — réagir à une commande
        if (msg == "STOP")  LOG("🛑 Robot arrêté !");
        if (msg == "START") LOG("🚀 Robot démarré !");
    });

    // ── Démarre la connexion ──────────────────────────────────────
    comm.begin();
}

void loop() {
    comm.loop();

    sensorManager.updateAll();  // ← ne pas oublier de mettre à jour les capteurs !

    SensorData batLevel = sensorManager.getDataByPosition(SensorPosition::CENTER);

    if (true) {
        LOGF("Niveau de la batterie : %.3f V\n", batLevel.value.scalar);

        if (comm.isConnected()) {
            comm.send(serializeSensorData(batLevel));
        }
    }else{
       LOGF("Niveau de la batterie : %.3f V\n", batLevel.value.scalar);
    }

    delay(500);
}
>>>>>>> dde70204bdf5f18da3ffdafd657b180c68878c88
