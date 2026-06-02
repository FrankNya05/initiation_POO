# Mini Sumo Robot — ESP32

**Auteur :** Sylvain Ngacham

Projet d'initiation à la programmation orientée objet embarquée sur ESP32.  
Le robot exécute des stratégies autonomes (carré, triangle, recherche lidar) controlées via MQTT/WiFi.

---

## Matériel requis

| Composant | Détails |
|---|---|
| Microcontrôleur | ESP32 DevKit |
| IMU | MPU-6050 (I2C, addr 0x68) |
| Lidar | TF-Luna ou compatible UART |
| Moteurs | 2× N20 50:1, 1400 PPR |
| Capteurs de ligne | 3× IR analogique (avant-gauche, avant-droit, arrière) |
| TOF | 2× VL53L0X (avant-gauche, avant-droit) |
| Batterie | 2S LiPo (7.4V nominal) |

---

## Installation

### 1. Prérequis
- [PlatformIO](https://platformio.org/) (VS Code extension ou CLI)
- Broker MQTT (ex: Mosquitto via Docker)

### 2. Cloner le dépôt
```bash
git clone https://github.com/FrankNya05/initiation_POO.git
cd initiation_POO/esp32
```

### 3. Configurer le WiFi et MQTT
Dans `src/main.cpp`, modifier :
```cpp
static constexpr const char* WIFI_SSID      = "ton_reseau";
static constexpr const char* WIFI_PASS      = "ton_mot_de_passe";
static constexpr const char* MQTT_BROKER_IP = "192.168.x.x";
```

### 4. Flasher le robot
```bash
# Premier flash (USB)
pio run -e esp32dev --target upload

# Flash suivants (OTA WiFi)
pio run -e esp32_ota --target upload
```

---

## Démarrage

1. Allumer le robot — la LED fait rouge → vert → bleu (séquence de boot, ~5s)
2. Le robot se connecte au WiFi et au broker MQTT
3. **LED jaune** = STANDBY (en attente)

### Démarrer une stratégie

**Via bouton physique :**
- Appui court → délai réglementaire 5s → GO

**Via MQTT :**
```bash
# Choisir une stratégie
mosquitto_pub -h <broker_ip> -t "robot/cmd" -m "STRATEGY:Triangle"

# Démarrer
mosquitto_pub -h <broker_ip> -t "robot/cmd" -m "START"
```

---

## Commandes MQTT

Topic de commande : `robot/cmd`

| Commande | Description |
|---|---|
| `START` | Démarre le robot (délai 5s) |
| `STOP` | Arrêt d'urgence → STANDBY |
| `STRATEGY:<nom>` | Change de stratégie |
| `POSE:RESET` | Remet la position EKF à (0, 0, 0°) |
| `MOTOR:L:<v>:R:<v>` | Commande directe moteurs (-255 à 255) |
| `MOTOR:STOP` | Arrêt moteurs |
| `PID:KP:<v>` | Réglage gain proportionnel vitesse |
| `PID:KI:<v>` | Réglage gain intégral vitesse |
| `PID:YAW:KP:<v>` | Réglage PID cap |
| `LED:R:<v>:G:<v>:B:<v>` | Contrôle LED RGB (0-255) |

---

## Stratégies disponibles

| Nom | Description |
|---|---|
| `Triangle` | Trace un triangle équilatéral (~500mm de côté), oriente et revient à l'origine |
| `Square` | Trace un carré |
| `Seek` | Détecte et poursuit l'adversaire via lidar |
| `Track` | Suit une cible |
| `Circle` | Tourne en cercle sur le dohyo |
| `Adamantine` | Stratégie défensive |
| `Berserker` | Attaque agressive |

Exemple :
```bash
mosquitto_pub -h <broker_ip> -t "robot/cmd" -m "STRATEGY:Seek"
mosquitto_pub -h <broker_ip> -t "robot/cmd" -m "START"
```

---

## Télémétrie

Topic : `robot/telemetry` (JSON, ~10 Hz)

```json
{
  "type": "TELEMETRY",
  "payload": {
    "ts": 123456,
    "pose":     { "x": 12.3, "y": 4.5, "theta": 0.12 },
    "motors":   { "left": 150, "right": 150 },
    "imu":      { "ax": 9.8, "gx": 0.02 },
    "encoders": { "leftRpm": 85.0, "rightRpm": 85.0 },
    "lidar":    { "dist": 0.45, "angle": 355.0, "valid": true },
    "battery":  { "voltage": 7.4, "percent": 80 },
    "line":     { "frontLeft": false, "frontRight": false, "back": false },
    "tof":      { "frontLeft": 120, "frontRight": 115 }
  }
}
```

Pour écouter :
```bash
mosquitto_sub -h <broker_ip> -t "robot/telemetry"
```

---

## Contrôle du bouton start

| Appui | Action |
|---|---|
| Court (< 800ms) | Démarre la stratégie courante (délai 5s) |
| Long (≥ 800ms) | Arrêt d'urgence |
| Double appui rapide | Cycle vers la stratégie suivante |
