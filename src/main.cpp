#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include "WiFiComm.hpp"
#include "RTOSConfig.hpp"
#include "SensorManger.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "PinConfig.hpp"
#include "BattSensor.hpp"
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════════
//  Globals partagés entre tâches
// ═══════════════════════════════════════════════════════════════

// Mutex Serial — requis par les macros LOG / LOGF
SemaphoreHandle_t g_logMutex = nullptr;

// Objets principaux
WiFiComm comm(
    "S25Ultra",           // ssid
    "sylvain123",         // password
    "10.135.195.249",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);
SensorManager sensorManager;

// ═══════════════════════════════════════════════════════════════
//  Utilitaires
// ═══════════════════════════════════════════════════════════════

std::string serializeSensorData(const SensorData& data) {
    StaticJsonDocument<256> doc;
    doc["ts"]    = data.timestamp;
    doc["pos"]   = (int)data.position;
    doc["valid"] = data.isValid;
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
    return output;
}

// ═══════════════════════════════════════════════════════════════
//  Tâches FreeRTOS
// ═══════════════════════════════════════════════════════════════

// ── taskSensors ───────────────────────────────────────────────
//  Core 1 | Priorité 3 | Période 20 ms
//  Met à jour tous les capteurs enregistrés dans SensorManager.
void taskSensors(void* /*pv*/) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        sensorManager.updateAll();
        vTaskDelayUntil(&xLastWakeTime, RTOSConfig::PERIOD_SENSORS);
    }
}

// ── taskComm ──────────────────────────────────────────────────
//  Core 0 | Priorité 1 | Période 100 ms
//  Gère la boucle WiFi/MQTT et publie les données capteurs.
void taskComm(void* /*pv*/) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        comm.loop();

        // Publie les données batterie si connecté
        SensorData batt = sensorManager.getDataByPosition(SensorPosition::CENTER);
        if (batt.isValid && comm.isConnected()) {
            comm.send(serializeSensorData(batt));
        }

        vTaskDelayUntil(&xLastWakeTime, RTOSConfig::PERIOD_COMM);
    }
}

// ── taskStrategy — placeholder ────────────────────────────────
//  Core 1 | Priorité 2 | Période 50 ms
//  Logique sumo : sera implémentée à l'étape 5.
void taskStrategy(void* /*pv*/) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        // TODO étape 5 — chercher adversaire, foncer, éviter bord
        vTaskDelayUntil(&xLastWakeTime, RTOSConfig::PERIOD_STRATEGY);
    }
}

// ═══════════════════════════════════════════════════════════════
//  setup() — init + création des tâches
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    // Mutex Serial — DOIT être créé avant tout LOG
    g_logMutex = xSemaphoreCreateMutex();

    LOG("=== Mini Sumo — boot ===");

    // ── Capteurs ──────────────────────────────────────────────
    sensorManager.add(new BattSensor(SensorPosition::CENTER), true);

    if (!sensorManager.initAll()) {
        LOG("[setup] ERREUR : init capteurs échouée !");
    } else {
        LOG("[setup] Capteurs initialisés.");
    }

    // ── WiFi / MQTT callbacks ─────────────────────────────────
    comm.onConnect([]() {
        LOG("✅ Connecté au broker Mosquitto !");
    });

    comm.onDisconnect([]() {
        LOG("❌ Connexion perdue !");
    });

    comm.onReceive([](const std::string& msg) {
        LOGF("📩 Message reçu : %s\n", msg.c_str());
        if (msg == "STOP")  LOG("🛑 Robot arrêté !");
        if (msg == "START") LOG("🚀 Robot démarré !");
    });

    comm.begin();

    // ── Création des tâches ───────────────────────────────────
    xTaskCreatePinnedToCore(
        taskSensors, "Sensors",
        RTOSConfig::STACK_SENSORS, nullptr,
        RTOSConfig::PRIO_SENSORS,  nullptr,
        RTOSConfig::CORE_SENSORS
    );

    xTaskCreatePinnedToCore(
        taskComm, "Comm",
        RTOSConfig::STACK_COMM,  nullptr,
        RTOSConfig::PRIO_COMM,   nullptr,
        RTOSConfig::CORE_COMM
    );

    xTaskCreatePinnedToCore(
        taskStrategy, "Strategy",
        RTOSConfig::STACK_STRATEGY, nullptr,
        RTOSConfig::PRIO_STRATEGY,  nullptr,
        RTOSConfig::CORE_STRATEGY
    );

    LOG("[setup] Tâches démarrées.");
}

// ═══════════════════════════════════════════════════════════════
//  loop() — inutilisé (toute la logique est dans les tâches)
// ═══════════════════════════════════════════════════════════════
void loop() {
    vTaskDelete(nullptr);  // supprime la tâche loop() d'Arduino
}
