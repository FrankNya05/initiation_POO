#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "LidarSensor.hpp"

// Requis par les macros LOG dans RTOSConfig.hpp (nullptr = logs silencieux)
SemaphoreHandle_t g_logMutex = nullptr;

static LidarSensor lidar;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("# YDLIDAR Tmini Plus — donnees brutes");
    Serial.println("# Attente calibration zone morte (~2 s)...");

    lidar.init();

    // Attendre la fin de la calibration avant de logger
    while (!lidar.isCalibDone()) {
        lidar.waitAndProcess(pdMS_TO_TICKS(20));
    }

    Serial.printf("# Calibration OK  center=%.1f deg  half=+/-%.1f deg\n",
                  lidar.getBlindCenter(), lidar.getBlindHalf());

    // En-tete CSV
    Serial.println("timestamp_ms,dist_m,angle_deg,nb_points");
}

void loop()
{
    // Bloquant jusqu'a reception d'un paquet UART (max 20 ms)
    lidar.waitAndProcess(pdMS_TO_TICKS(20));

    SensorData d = lidar.getData();
    if (!d.isValid) return;

    // Une ligne CSV par mesure valide
    Serial.printf("%lu,%.4f,%.2f,%d\n",
                  d.timestamp,
                  d.value.vector.x,   // distance (m)
                  d.value.vector.y,   // angle    (deg)
                  (int)d.value.vector.z); // nb points valides
}
