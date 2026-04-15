
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <IRSensor.hpp>
// ============================================
//              CONFIGURATION
// ============================================
const char* ssid        = "S25Ultra";
const char* password    = "sylvain123";

const char* mqtt_server = "10.135.195.249";
const int   mqtt_port   = 1883;

const char* topic_pub = "Sylvain/capteur";
const char* topic_sub = "Sylvain/commande";

// Deep Sleep — durée en secondes
#define SLEEP_SEC 5
#define LED_BUILTIN 2
// ============================================

WiFiClient espClient;
PubSubClient client(espClient);

// Compteur qui survit au deep sleep (stocké en RAM RTC)
RTC_DATA_ATTR int nbEnvois = 0;

// ============================================
//         CONNEXION WIFI
// ============================================
bool connectWifi() {
  WiFi.begin(ssid, password);
  Serial.print("🔄 WiFi");
  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++tentatives > 20) {
      Serial.println("\n❌ WiFi échoué !");
      return false;
    }
  }
  Serial.println("\n✅ WiFi connecté : " + WiFi.localIP().toString());
  return true;
}

// ============================================
//         CONNEXION MQTT
// ============================================
bool connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  Serial.print("🔄 MQTT");
  int tentatives = 0;
  while (!client.connected()) {
    if (client.connect("ESP32-Sylva")) {
      Serial.println("\n✅ MQTT connecté !");
      client.subscribe(topic_sub);
      return true;
    }
    delay(1000);
    Serial.print(".");
    if (++tentatives > 5) {
      Serial.println("\n❌ MQTT échoué !");
      return false;
    }
  }
  return true;
}

// ============================================
//         CRÉER ET ENVOYER LE JSON
// ============================================
void envoyerJSON(SensorData DATA) {
  // --- Créer le JSON ---
  StaticJsonDocument<256> doc;

  // Informations de l'appareil
  doc["appareil"]    = "ESP32-Sylva";
  doc["nb_envois"]   = nbEnvois;

  // Données capteurs (remplacez par vos vrais capteurs)
  doc["DISTANCE"] = DATA.value.scalar;
  doc["humidite"]    = 60.0;
  doc["luminosite"]  = 850;

  // Infos réseau
  doc["ip"]          = WiFi.localIP().toString();
  doc["wifi_rssi"]   = WiFi.RSSI();
  doc["uptime_ms"]   = millis();

  // --- Convertir en char[] ---
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  // --- Publier ---
  if (client.publish(topic_pub, jsonBuffer)) {
    Serial.println("📤 JSON envoyé !");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("❌ Échec envoi JSON !");
  }
  pinMode(LED_BUILTIN, OUTPUT);
}

void led2(String commande )
{
  if (String(commande) == "ON") {
  digitalWrite(LED_BUILTIN, HIGH);  // ← Allume
  Serial.println("💡 LED allumée !");
} else if (String(commande) == "OFF") {
  digitalWrite(LED_BUILTIN, LOW);   // ← Éteint
  Serial.println("💡 LED éteinte !");
}
}
// ============================================
//         RECEVOIR UN JSON (callback)
// ============================================
void callback(char* topic, byte* payload, unsigned int length) {
  // Reconstruire le message
  String raw = "";
  for (int i = 0; i < length; i++) raw += (char)payload[i];

  Serial.println("\n📩 Message reçu sur : " + String(topic));
  Serial.println("   Contenu brut : " + raw);

  // Parser le JSON
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, raw);

  if (err) {
    Serial.println("❌ JSON invalide : " + String(err.c_str()));
    return;
  }

  // Lire les valeurs
  const char* commande = doc["commande"] | "inconnu";
  int         valeur   = doc["valeur"]   | 0;

  Serial.println("   Commande : " + String(commande));
  Serial.println("   Valeur   : " + String(valeur));

  // Réagir à la commande
  if (String(commande) == "ON") {
    Serial.println("💡 LED allumée !");
  } else if (String(commande) == "OFF") {
    Serial.println("💡 LED éteinte !");
  } else if (String(commande) == "SLEEP") {
    Serial.println("😴 Mise en veille forcée !");
    ESP.deepSleep(valeur * 1000000);
  }
  led2(commande );
}

// ============================================
//                  SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(500);

  TOFSensor VAR = TOFSensor(0x08,SensorPosition::FRONT);
  if(VAR.init()){

  }
 
  nbEnvois++;
  Serial.println("\n==============================");
  Serial.println("   ESP32 — Réveil #" + String(nbEnvois));
  Serial.println("==============================");

  // Étape 1 — WiFi
  if (!connectWifi()) {
    Serial.println("😴 Dodo " + String(SLEEP_SEC) + "s...");
    ESP.deepSleep(SLEEP_SEC * 1000000ULL);
  }

  // Étape 2 — MQTT
  client.setCallback(callback);
  if (!connectMQTT()) {
    Serial.println("😴 Dodo " + String(SLEEP_SEC) + "s...");
    ESP.deepSleep(SLEEP_SEC * 1000000ULL);
  }

  // Étape 3 — Envoyer le JSON
  VAR.update();
  envoyerJSON(VAR.getData());

  // Étape 4 — Attendre les messages entrants (2 secondes)
  Serial.println("👂 Écoute pendant 2s...");
  long debut = millis();
  while (millis() - debut < 10000) {
    client.loop();
    delay(10);
    VAR.update();
  }

  // Étape 5 — Deep Sleep
  Serial.println("😴 Dodo " + String(SLEEP_SEC) + "s...\n");
  WiFi.disconnect();
  ESP.deepSleep(SLEEP_SEC * 1000000ULL);
}

void loop() {
  // Vide — tout se passe dans setup() avec deep sleep
}

