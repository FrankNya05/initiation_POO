

/* 
All enumerated sensors for this allpication

*/

#pragma once
#include <stdint.h>

// Structure envoyée via la queue


enum class SensorType
{
    LIDAR,
    LINE,
    ULTRASONIC,
    TOFSensor,
    IMU,
    SWITCH
};

enum class PressType : uint8_t {
    NONE  = 0,
    SHORT = 1,  // appui court  (< LONG_PRESS_MS, hors double-clic)
    LONG  = 2,  // appui long   (>= LONG_PRESS_MS)
    RAPID = 3,  // double appui rapide
};

enum status{ RUN,NO_RUN};

enum class SensorPosition
{
    LEFT,         // Encodeur gauche
    RIGHT,        // Encodeur droit
    FRONT,        // Lidar (obstacle le plus proche — distance + angle)
    BACK,         // LineSensor arrière
    CENTER,       // IMU (MPU6050)
    FRONT_LEFT,   // TOF avant-gauche, LineSensor avant-gauche
    FRONT_RIGHT,  // TOF avant-droit,  LineSensor avant-droit
    BATTERY,      // Capteur de batterie
    UNKNOWN       // Valeur par défaut / non assigné
};

 struct TOFConfig{
  uint8_t addr;
  SensorPosition position;
};

enum class SensorDims : uint8_t {
    SCALAR = 1,
    VEC3 = 3,
    IMU6 = 6
};

union SensorValue {
    float scalar;
    struct { float x, y, z; } vector;
    struct {
        float ax, ay, az;   // accéléromètre
        float gx, gy, gz;   // gyroscope
    } imu;
    SensorValue() : scalar(0.0f) {}
    explicit SensorValue(float s) : scalar(s) {}
    SensorValue(float x, float y, float z) : vector{x, y, z} {}
};

struct SensorData {
    SensorPosition position  = SensorPosition::UNKNOWN;
    SensorValue    value     = {};
    SensorDims     dims      = SensorDims::SCALAR;
    uint32_t       timestamp = 0;
    bool           isValid   = false;
};