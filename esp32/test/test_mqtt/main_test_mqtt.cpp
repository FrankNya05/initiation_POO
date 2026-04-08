#include <Arduino.h>
#include "WiFiComm.hpp"
#include "RTOSConfig.hpp"
// ── Instanciation ─────────────────────────────────────────────────
WiFiComm comm(
    "S25Ultra",           // ssid
    "sylvain123",         // password
    "10.135.195.191",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);

void setup() {
    Serial.begin(115200);

    // ── Enregistre les callbacks AVANT begin() ────────────────────

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
    // Maintient la connexion MQTT + reçoit les messages
    comm.loop();

    // Envoie un message toutes les 5 secondes
    if (comm.isConnected()) {
        comm.send("{\"status\":\"online\", \"distance\":0.45}");
        delay(5000);
    }
}



//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "WiFiComm.hpp"
#include "RTOSConfig.hpp"
#include "SensorManger.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "PinConfig.hpp"
#include "BattSensor.hpp"
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

SemaphoreHandle_t g_logMutex = nullptr;

// ── Instanciation ─────────────────────────────────────────────────
WiFiComm comm(
    "Frank",           // ssid
    "ftel7488",         // password
    "10.127.53.148",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);

void setup() {
    Serial.begin(115200);

    // ── Enregistre les callbacks AVANT begin() ────────────────────
    g_logMutex = xSemaphoreCreateMutex();

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
    // Maintient la connexion MQTT + reçoit les messages
    comm.loop();

    // Envoie un message toutes les 5 secondes
    if (comm.isConnected()) {
        comm.send("{\"status\":\"online\", \"distance\":0.45}");
        delay(5000);
    }
}
