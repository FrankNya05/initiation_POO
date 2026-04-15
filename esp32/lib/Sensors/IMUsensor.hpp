/*
 * IMUSensor.hpp
 * -----------------------------------------------
 * Capteur IMU MPU6050 — accéléromètre + gyroscope
 *
 * Concepts OOP appliqués :
 *  - Héritage      : implémente SensorsInterface
 *  - Encapsulation : données et config privées,
 *                    accès uniquement via getData()
 *  - Polymorphisme : init(), update(), getData()
 *                    sont virtual dans SensorsInterface
 *  - RAII          : init() configure le matériel,
 *                    le destructeur de la lib Adafruit
 *                    gère la libération des ressources
 *
 * Matériel :
 *  - MPU6050 sur bus I2C (adresse par défaut : 0x68)
 *  - Librairie : Adafruit MPU6050
 *
 * Données produites (SensorDims::IMU6) :
 *  - value.imu.ax/ay/az : accélération en m/s²
 *  - value.imu.gx/gy/gz : vitesse angulaire en °/s
 *
 * Usage minimal :
 *  - L'axe gz (gyroscope Z) est le plus utile pour
 *    surveiller la rotation du robot en sumo.
 */

#pragma once
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"

// ─────────────────────────────────────────────
//  Constantes de configuration
// ─────────────────────────────────────────────
namespace IMUConfig {
    // Plage du gyroscope — options Adafruit :
    //   MPU6050_RANGE_250_DEG  → ±250 °/s  (plus précis, petit mouvement)
    //   MPU6050_RANGE_500_DEG  → ±500 °/s
    //   MPU6050_RANGE_1000_DEG → ±1000 °/s
    //   MPU6050_RANGE_2000_DEG → ±2000 °/s (rotation rapide)
    constexpr mpu6050_gyro_range_t  GYRO_RANGE  = MPU6050_RANGE_500_DEG;

    // Plage de l'accéléromètre — options Adafruit :
    //   MPU6050_RANGE_2_G  → ±2g  (plus précis)
    //   MPU6050_RANGE_4_G  → ±4g
    //   MPU6050_RANGE_8_G  → ±8g
    //   MPU6050_RANGE_16_G → ±16g (chocs importants)
    constexpr mpu6050_accel_range_t ACCEL_RANGE = MPU6050_RANGE_4_G;

    // Filtre passe-bas (réduit le bruit de vibration moteur)
    // Options : MPU6050_BAND_260_HZ → MPU6050_BAND_5_HZ
    constexpr mpu6050_bandwidth_t   FILTER_BW   = MPU6050_BAND_21_HZ;
}

// ─────────────────────────────────────────────
//  Classe IMUSensor
// ─────────────────────────────────────────────
class IMUSensor : public SensorsInterface {
public:

    // ── Constructeur ──────────────────────────
    // pos : position sur le robot (CENTER par défaut,
    //       à préciser lors de l'intégration finale)
    explicit IMUSensor(SensorPosition pos = SensorPosition::CENTER);

    // ── Interface SensorsInterface ────────────

    // Initialise le bus I2C et le MPU6050.
    // Retourne false si le capteur est introuvable.
    bool init() override;

    // Lit accéléromètre + gyroscope et remplit _data.
    // Retourne false si la lecture échoue.
    bool update() override;

    // Retourne le dernier SensorData lu (copie par valeur).
    SensorData getData() const override;

private:

    Adafruit_MPU6050 _mpu;       // objet de la librairie Adafruit
    SensorData       _data;      // données retournées par getData()
};