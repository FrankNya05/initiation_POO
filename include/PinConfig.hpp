#pragma once
#include <cstdint>

namespace RobotConfig {
    // --- Moteurs (PWM & Direction) ---
    // Note: Utilisation de ADC2 ok ici car ce sont des sorties (PWM)
    constexpr uint8_t MOTOR_LEFT_PWM = 25;
    constexpr uint8_t MOTOR_LEFT_DIR = 26;
    constexpr uint8_t MOTOR_RIGHT_PWM = 27;
    constexpr uint8_t MOTOR_RIGHT_DIR = 14;
    
    // --- Capteurs de ligne (ADC1 - Safe) ---
    constexpr uint8_t LINE_SENSOR_FRONT_LEFT = 32;
    constexpr uint8_t LINE_SENSOR_FRONT_RIGHT = 33;
    constexpr uint8_t LINE_SENSOR_BACK_LEFT = 34;
    constexpr uint8_t LINE_SENSOR_BACK_RIGHT = 35;
    
    // --- Bouton de démarrage ---
    constexpr uint8_t START_BUTTON = 15;
    
    // --- Paramètres PWM ---
    constexpr int PWM_FREQUENCY = 5000;
    constexpr int PWM_RESOLUTION = 8;  // 0-255
    constexpr int PWM_CHANNEL_LEFT = 0;
    constexpr int PWM_CHANNEL_RIGHT = 1;

    // --- UART pour le LIDAR ---
    // Utilisation des pins UART2 matérielles (plus stables)
    constexpr uint8_t RX_LIDAR = 16;
    constexpr uint8_t TX_LIDAR = 17;
    
    // --- Capteur Batterie ---
    // Déplacé sur ADC1 (GPIO 36) pour fonctionner même avec le Wi-Fi activé
    constexpr uint8_t BATTERY_SENSOR = 36;

    // --- IMU (I2C) ---
    constexpr uint8_t SCL = 22;
    constexpr uint8_t SDA = 21;

    // --- Encodeurs Moteurs ---
    // Déplacés pour éviter les broches de boot (0, 2) et les conflits UART
    constexpr uint8_t ENCODER_MOTOR_LEFT_P = 18;
    constexpr uint8_t ENCODER_MOTOR_LEFT_H = 19;
    constexpr uint8_t ENCODER_MOTOR_RIGHT_P = 4;
    constexpr uint8_t ENCODER_MOTOR_RIGHT_H = 5;

    // --- Capteurs IR (Infrarouge) ---
    // Pins GPIO standard disponibles
    constexpr uint8_t IR_SENSOR_FRONT_LEFT = 36;  
    constexpr uint8_t IR_SENSOR_FRONT_RIGHT = 39; 
    // --- Commande Freinage BJT ---
    // Une seule définition sur une pin stable
    constexpr uint8_t BJT_COMMAND = 23;
}
