#include <Arduino.h>
#include "RTOSConfig.hpp"
#include "LidarSensor.hpp"

LidarSensor lidar;

void setup() {
    Serial.begin(115200);
    delay(10000);

    Serial.println("=== Test LidarSensor ===");

    if (lidar.init()) {
        Serial.println("✅ Init OK");
    } else {
        Serial.println("❌ Init FAILED");
    }
}

void loop() {
    lidar.update();

    SensorData data = lidar.getData();

    if (data.isValid) {
        Serial.printf("✅ Proche: %.2fm @ %.1f deg | %d pts\n",
            data.value.vector.x,
            data.value.vector.y,
            (int)data.value.vector.z);
    } else {
        Serial.println("⏳ Pas encore de données valides...");
    }

    delay(500);
}