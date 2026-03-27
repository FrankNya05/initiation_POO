#pragma once
#include <cstdint>

namespace RobotConfig {
    // --- Moteurs (PWM & Direction) ---
    // GPIO numériques standards pour les ponts en H
    constexpr uint8_t MOTOR_LEFT_PWM = 25;
    constexpr uint8_t MOTOR_LEFT_DIR = 26;
    constexpr uint8_t MOTOR_RIGHT_PWM = 27;
    constexpr uint8_t MOTOR_RIGHT_DIR = 14;
    
    // --- Capteurs de ligne (Digitaux ou Analogiques sur ADC1) ---
    constexpr uint8_t LINE_SENSOR_FRONT_LEFT = 32;
    constexpr uint8_t LINE_SENSOR_FRONT_RIGHT = 33;
    constexpr uint8_t LINE_SENSOR_BACK_LEFT = 34;
    constexpr uint8_t LINE_SENSOR_BACK = 35;
    
    // --- Bouton de démarrage ---
    constexpr uint8_t START_BUTTON = 15;
    
    // --- Paramètres PWM ---
    constexpr int PWM_FREQUENCY = 5000;
    constexpr int PWM_RESOLUTION = 8;  // Plage 0-255
    constexpr int PWM_CHANNEL_LEFT = 0;
    constexpr int PWM_CHANNEL_RIGHT = 1;

    // --- UART pour le LIDAR ---
    // Utilise les ports série matériels stables (UART2)
    constexpr uint8_t RX_LIDAR = 17;
    constexpr uint8_t TX_LIDAR = 16;
    
    // --- Capteur Batterie (ADC1) ---
    // INDISPENSABLE pour fonctionner avec le Bluetooth/Wi-Fi
    constexpr uint8_t BATTERY_SENSOR = 36; // Broche VP

    // --- Bus I2C (IMU / Écran) ---
    constexpr uint8_t SCL = 22;
    constexpr uint8_t SDA = 21;

    // TOF Laser Range Sensor Mini
    constexpr uint8_t TOF_SCL = SCL;  // Broche VN
    constexpr uint8_t TOF_SDA = SDA; // Pin digitale/analogue safe

    constexpr uint8_t TOF_IIC_ADDR  = 0x09;  // addresse i2c du premier capteur de distance
    constexpr uint8_t TOF_IIC_ADDR1 = 0x0A;
    
    // --- Encodeurs Moteurs (Entrées numériques rapides) ---
    // Déplacés sur des pins "safe" pour éviter les erreurs de boot
    constexpr uint8_t ENCODER_MOTOR_LEFT_P = 18;
    constexpr uint8_t ENCODER_MOTOR_LEFT_H = 19;
    constexpr uint8_t ENCODER_MOTOR_RIGHT_P = 4;
    constexpr uint8_t ENCODER_MOTOR_RIGHT_H = 5;

    // --- Capteurs Infrarouges SHARP (Analogiques sur ADC1) ---

   

    // --- Commande Freinage BJT ---
    // Pin de sortie stable pour le transistor
    constexpr uint8_t BJT_COMMAND = 23;
}
