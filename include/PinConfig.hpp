#pragma once
#include <cstdint>

namespace RobotConfig {
    // Configuration des pins GPIO ESP32
    constexpr uint8_t MOTOR_LEFT_PWM = 25;
    constexpr uint8_t MOTOR_LEFT_DIR = 26;
    constexpr uint8_t MOTOR_RIGHT_PWM = 27;
    constexpr uint8_t MOTOR_RIGHT_DIR = 14;
    
    constexpr uint8_t LINE_SENSOR_FRONT_LEFT = 32;
    constexpr uint8_t LINE_SENSOR_FRONT_RIGHT = 33;
    constexpr uint8_t LINE_SENSOR_BACK_LEFT = 34;
    constexpr uint8_t LINE_SENSOR_BACK_RIGHT = 35;
    
    constexpr uint8_t START_BUTTON = 15;
    
    // Paramètres PWM
    constexpr int PWM_FREQUENCY = 5000;
    constexpr int PWM_RESOLUTION = 8;  // 0-255
    constexpr int PWM_CHANNEL_LEFT = 0;
    constexpr int PWM_CHANNEL_RIGHT = 1;

    // paramtre UART
    constexpr int TX_LIDAR = -1;
    constexpr int RX_LIDAR = -1;
    
    // paramtre UART

}