

/* 
All enumerated sensors for this allpication

*/

#pragma once

enum class SensorType {
    LIDAR,
    LINE_LEFT,
    LINE_RIGHT,
    ULTRASONIC,
    TOTAL  // toujours en dernier pour connaître le nombre de capteurs
};

// Structure envoyée via la queue

struct SensorData {
    float lidar;
    float lineLeft;
    float lineRight;
    float ultrasonic;
};