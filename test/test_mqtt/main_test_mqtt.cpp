#include <Arduino.h>
#include "WiFiComm.hpp"

// ── Instanciation ─────────────────────────────────────────────────
WiFiComm comm(
    "S25Ultra",           // ssid
    "sylvain123",         // password
    "10.135.195.249",     // broker IP
    1883,                 // port
    "robot/data",         // topic publication
    "robot/commande"      // topic abonnement
);

void setup() {
    Serial.begin(115200);

    // ── Enregistre les callbacks AVANT begin() ────────────────────

    comm.onConnect([]() {
        Serial.println("✅ Connecté au broker Mosquitto !");
    });

    comm.onDisconnect([]() {
        Serial.println("❌ Connexion perdue !");
    });

    comm.onReceive([](const std::string& msg) {
        Serial.printf("📩 Message reçu : %s\n", msg.c_str());

        // Exemple — réagir à une commande
        if (msg == "STOP")  Serial.println("🛑 Robot arrêté !");
        if (msg == "START") Serial.println("🚀 Robot démarré !");
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
