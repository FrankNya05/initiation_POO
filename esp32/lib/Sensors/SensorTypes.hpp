

/* 
All enumerated sensors for this allpication

*/

#pragma once

// Structure envoyée via la queue


enum class SensorType
{
    LIDAR,
    LINE,
    ULTRASONIC,
    TOFSensor
};

enum status{ RUN,NO_RUN};

enum class SensorPosition
{
    LEFT,
    RIGHT,
    FRONT,
    BACK,
    CENTER,
    UNKNOWN
};

 struct TOFConfig{
  uint8_t addr;
  SensorPosition position;
};

enum class SensorDims : uint8_t {
    SCALAR = 1,
    VEC3 = 3
};

union SensorValue {
    float scalar;
    struct { float x, y, z; } vector;

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



