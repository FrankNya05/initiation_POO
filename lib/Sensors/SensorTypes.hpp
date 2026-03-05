

/* 
All enumerated sensors for this allpication

*/

#pragma once


// Structure envoyée via la queue

struct SensorData {
    float lidar;
    float lineLeft;
    float lineRight;
    float ultrasonic;
};

enum class SensorType
{
    LIDAR,
    LINE,
    ULTRASONIC
};

enum class SensorPosition
{
    LEFT,
    RIGHT,
    FRONT,
    BACK,
    CENTER
};