/*
 * IMUSensor.cpp
 * -----------------------------------------------
 * Implémentation de IMUSensor (MPU6050)
 */

#include "IMUSensor.hpp"

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
    // Tente de démarrer la communication I2C avec le MPU6050.
    // begin() retourne false si le capteur ne répond pas.
    if (!_mpu.begin()) {
        Serial.println("[IMUSensor] MPU6050 introuvable sur le bus I2C.");
        return false;
    }

    // Configure la plage du gyroscope (voir IMUConfig dans le .hpp)
    _mpu.setGyroRange(IMUConfig::GYRO_RANGE);

    // Configure la plage de l'accéléromètre
    _mpu.setAccelerometerRange(IMUConfig::ACCEL_RANGE);

    // Active le filtre passe-bas pour réduire le bruit
    // causé par les vibrations des moteurs
    _mpu.setFilterBandwidth(IMUConfig::FILTER_BW);

    Serial.println("[IMUSensor] MPU6050 initialisé avec succès.");
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
    _data.value.imu.ax = accel.acceleration.x;
    _data.value.imu.ay = accel.acceleration.y;
    _data.value.imu.az = accel.acceleration.z;

    // Note : Adafruit retourne le gyroscope en rad/s.
    // On convertit en °/s pour une utilisation plus intuitive
    // dans le contexte de l'asservissement du robot.
    _data.value.imu.gx = gyro.gyro.x * RAD_TO_DEG;
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