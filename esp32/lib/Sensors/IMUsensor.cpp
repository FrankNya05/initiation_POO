/*
 * IMUSensor.cpp
 * -----------------------------------------------
 * Implémentation de IMUSensor (MPU6050)
 */

#include "IMUSensor.hpp"
#include "RTOSConfig.hpp"
// ─────────────────────────────────────────────
//  Constructeur
// ─────────────────────────────────────────────
IMUSensor::IMUSensor(SensorPosition pos) {
    // Initialise les métadonnées de _data.
    // Les valeurs mesurées seront remplies dans update().
    _data.position = pos;
    _data.dims     = SensorDims::IMU6;
    _data.isValid  = false;
}

// ─────────────────────────────────────────────
//  init()
//  Appelé une fois dans setup() ou initAll().
// ─────────────────────────────────────────────
bool IMUSensor::init() {
    // Étape 1 : sonde brute pour trouver l'adresse (0x68 AD0=LOW ou 0x69 AD0=HIGH)
    uint8_t foundAddr = 0;
    for (int i = 0; i < 5 && foundAddr == 0; i++) {
        Wire.setClock(100000);
        for (uint8_t addr : {(uint8_t)0x68, (uint8_t)0x69}) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) { foundAddr = addr; break; }
        }
        if (foundAddr == 0) delay(200);
    }
    if (foundAddr == 0) {
        Serial.println("[IMUSensor] MPU6050 absent (0x68 et 0x69)");
        return false;
    }
    Serial.printf("[IMUSensor] MPU6050 detecte a 0x%02X\n", foundAddr);

    // Étape 2 : init Adafruit — configure i2c_dev (ignoré si WHO_AM_I clone)
    // Note : begin() réessaie 5× en interne avec delay(10). Même en cas d'échec,
    //        i2c_dev reste valide et getEvent() fonctionne directement via i2c_dev.
    bool adaOk = false;
    for (int i = 0; i < 3 && !adaOk; i++) {
        Wire.setClock(100000);
        adaOk = _mpu.begin(foundAddr);
        if (!adaOk) delay(200);
    }

    if (!adaOk) {
        // begin() échoue (souvent WHO_AM_I clone ≠ 0x68) — config manuelle via Wire brut
        Serial.printf("[IMUSensor] begin() echoue — init manuelle sur 0x%02X\n", foundAddr);
        Wire.beginTransmission(foundAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(); // wake
        delay(100);
        Wire.beginTransmission(foundAddr); Wire.write(0x1A); Wire.write(0x04); Wire.endTransmission(); // DLPF 21Hz
        Wire.beginTransmission(foundAddr); Wire.write(0x1B); Wire.write(0x08); Wire.endTransmission(); // gyro ±500°/s
        Wire.beginTransmission(foundAddr); Wire.write(0x1C); Wire.write(0x08); Wire.endTransmission(); // accel ±4g
        delay(10);
    }

    // Configure la plage du gyroscope (voir IMUConfig dans le .hpp)
    _mpu.setGyroRange(IMUConfig::GYRO_RANGE);

    // Configure la plage de l'accéléromètre
    _mpu.setAccelerometerRange(IMUConfig::ACCEL_RANGE);

    // Active le filtre passe-bas pour réduire le bruit
    // causé par les vibrations des moteurs
    _mpu.setFilterBandwidth(IMUConfig::FILTER_BW);

    // Mesure du biais statique du gyroscope axe X (axe lacet du robot).
    // Le MPU6050 n'a pas d'auto-calibration — on mesure la dérive à
    // l'arrêt et on la soustrait dans update(). Robot immobile, moteurs
    // éteints pendant les ~200 ms de cette mesure.
    {
        constexpr int    N   = 200;
        constexpr int    DLY = 1;   // ms entre échantillons → ~200 ms total
        double sumGx = 0.0, sumAy = 0.0, sumAz = 0.0;
        sensors_event_t a, g, t;
        for (int i = 0; i < N; i++) {
            _mpu.getEvent(&a, &g, &t);
            sumGx += g.gyro.x * RAD_TO_DEG;
            sumAy += a.acceleration.y;
            sumAz += a.acceleration.z;
            delay(DLY);
        }
        _gxBias = (float)(sumGx / N);
        _ayBias = (float)(sumAy / N);
        _azBias = (float)(sumAz / N);
        Serial.printf("[IMUSensor] Biais gx=%+.3f deg/s  ay=%+.3f m/s²  az=%+.3f m/s²\n",
                      _gxBias, _ayBias, _azBias);
    }

    LOG("[IMUSensor] MPU6050 initialisé avec succès.");
    return true;
}

// ─────────────────────────────────────────────
//  update()
//  Appelé périodiquement par la tâche capteurs.
// ─────────────────────────────────────────────
bool IMUSensor::update() {
    // sensors_event_t est un type Adafruit qui regroupe
    // les données d'un capteur à un instant donné.
    sensors_event_t accel, gyro, temp;

    // Lecture simultanée des trois sources du MPU6050.
    // getEvent() retourne void — on ne peut pas détecter
    // une erreur ici, mais le capteur est validé dans init().
    _mpu.getEvent(&accel, &gyro, &temp);

    // Remplit le membre imu de l'union SensorValue.
    // ax/ay/az en m/s², gx/gy/gz en °/s (rad/s natif Adafruit → °/s).
    _data.value.imu.ax = accel.acceleration.x;           // axe gravité — non corrigé
    _data.value.imu.ay = accel.acceleration.y - _ayBias; // biais statique retiré
    _data.value.imu.az = accel.acceleration.z - _azBias; // biais statique retiré

    // Note : Adafruit retourne le gyroscope en rad/s.
    // On convertit en °/s pour une utilisation plus intuitive
    // dans le contexte de l'asservissement du robot.
    _data.value.imu.gx = gyro.gyro.x * RAD_TO_DEG - _gxBias;
    _data.value.imu.gy = gyro.gyro.y * RAD_TO_DEG;
    _data.value.imu.gz = gyro.gyro.z * RAD_TO_DEG;

    _data.timestamp = millis();
    _data.isValid   = true;

    return true;
}

// ─────────────────────────────────────────────
//  getData()
//  Retourne une copie du dernier SensorData lu.
//  const : ne modifie pas l'état de l'objet.
// ─────────────────────────────────────────────
SensorData IMUSensor::getData() const {
    return _data;
}