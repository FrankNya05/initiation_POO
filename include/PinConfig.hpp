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
    constexpr uint8_t TX_LIDAR = 4;
    constexpr uint8_t RX_LIDAR = 5;
    
    // battery sensor
     constexpr uint8_t BATTERY_SENSOR = 12;

    // IMU_SENSOR_I2C,
     constexpr uint8_t SCL = 22;
     constexpr uint8_t SDA = 21;

    // ENCODER_SENSOR

     constexpr uint8_t ENCODER_MOTOR_LEFT_P = 16;
     constexpr uint8_t ENCODER_MOTOR_LEFT_H = 4;
     constexpr uint8_t ENCODER_MOTOR_RIGHT_P = 0;
     constexpr uint8_t ENCODER_MOTOR_RIGHT_H = 2;

     // IR_SENSOR
    constexpr uint8_t IR_SENSOR_FRONT_LEFT = 5;
    constexpr uint8_t IR_SENSOR_FRONT_RIGHT = 17;
    //  FRENAGE_CONTRE COURANT PAR BJT
    constexpr uint8_t BJT_COMMAND = 23;
    constexpr uint8_t BJT_COMMAND = 13;
     