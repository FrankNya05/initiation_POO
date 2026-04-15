// ============================================
//  main.cpp — Point d'entrée du robot mini-sumo
//  Instancie tous les objets, initialise le
//  matériel et lance les tâches FreeRTOS.
// ============================================
#include <Arduino.h>

// ── Drivers ──────────────────────────────────
#include "DriverMotor.hpp"
#include "DriverManager.hpp"
#include "DriverLedRGB.hpp"

// ── Capteurs ─────────────────────────────────
#include "SensorManger.hpp"
#include "LidarSensor.hpp"
#include "LineSensor.hpp"
#include "IRSensor.hpp"
#include "IMUsensor.hpp"
#include "BattSensor.hpp"
#include "SensorSwitch.hpp"
#include "Encoder.hpp"

// ── Stratégies ───────────────────────────────
#include "StrategyManager.hpp"
#include "AdamantineStrategy.hpp"
#include "BerserkerStrategy.hpp"
#include "CircleDohyoStrategy.hpp"

// ── Communication ────────────────────────────
#include "CommunicationManager.hpp"

// ── Outils ───────────────────────────────────
#include "EKF.hpp"
#include "PID.hpp"

// ── RTOS ─────────────────────────────────────
#include "RobotContext.hpp"
#include "RTOSConfig.hpp"
#include "Queues.hpp"
#include "Tasks.hpp"

// ═══════════════════════════════════════════════════════════════
//  Mutex global Serial (requis par les macros LOG/LOGF)
// ═══════════════════════════════════════════════════════════════
SemaphoreHandle_t g_logMutex = nullptr;

// ═══════════════════════════════════════════════════════════════
//  Instances globales
// ═══════════════════════════════════════════════════════════════

// ── Moteurs ──────────────────────────────────
DriverManager  driverManager;
DriverLedRGB   led;

// ── Capteurs ─────────────────────────────────
SensorManager  sensorManager;
Encoder        encoderLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H);
Encoder        encoderRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H);

// ── Stratégies ───────────────────────────────
StrategyManager    strategyManager;
AdamantineStrategy stratAdamantine;
BerserkerStrategy  stratBerserker;
CircleDohyoStrategy stratCircle;

// ── Communication ────────────────────────────
CommunicationManager commManager;

// ── Localisation ─────────────────────────────
EKF ekf;

// ── PID vitesse moteurs ───────────────────────
PID pidLeft (1.2f, 0.05f, 0.02f);
PID pidRight(1.2f, 0.05f, 0.02f);

// ═══════════════════════════════════════════════════════════════
//  Callbacks pour taskCommand
// ═══════════════════════════════════════════════════════════════

void onStrategy(const char* name) {
    if      (strcmp(name, "ADAMANTINE") == 0) strategyManager.setStrategy(&stratAdamantine);
    else if (strcmp(name, "BERSERKER")  == 0) strategyManager.setStrategy(&stratBerserker);
    else if (strcmp(name, "CIRCLE")     == 0) strategyManager.setStrategy(&stratCircle);
    else LOGF("[main] Stratégie inconnue : %s\n", name);
}

void onLed(uint8_t r, uint8_t g, uint8_t b) {
    led.setColor(r, g, b);
}

// Valeurs PID courantes — modifiables individuellement depuis le HMI
static float g_kp = 1.2f, g_ki = 0.05f, g_kd = 0.02f;

void onPidKp(float v) {
    g_kp = v;
    pidLeft.setGains(g_kp, g_ki, g_kd);
    pidRight.setGains(g_kp, g_ki, g_kd);
}

void onPidKi(float v) {
    g_ki = v;
    pidLeft.setGains(g_kp, g_ki, g_kd);
    pidRight.setGains(g_kp, g_ki, g_kd);
}

void onPidKd(float v) {
    g_kd = v;
    pidLeft.setGains(g_kp, g_ki, g_kd);
    pidRight.setGains(g_kp, g_ki, g_kd);
}

void onPoseReset() {
    ekf.reset();
}

// ═══════════════════════════════════════════════════════════════
//  Paramètres de tâches (structs passées via pvParameters)
// ═══════════════════════════════════════════════════════════════
EKFParams         ekfParams = { &ekf, &encoderLeft, &encoderRight };
CommandTaskParams cmdParams = {
    &strategyManager,
    onStrategy,
    onLed,
    onPidKp,
    onPidKi,
    onPidKd,
    onPoseReset
};

// ═══════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    // ── Mutex Serial ──────────────────────────
    g_logMutex = xSemaphoreCreateMutex();

    LOG("[main] Démarrage robot mini-sumo");

    // ── LED ───────────────────────────────────
    led.init();
    led.setColor(LedColor::YELLOW);  // initialisation en cours

    // ── Moteurs ───────────────────────────────
    driverManager.add(new DriverMotor(), DriverRole::MAIN);
    if (!driverManager.initAll()) {
        LOG("[main] ERREUR init moteurs");
        led.setColor(LedColor::RED);
    }

    // ── Encodeurs ─────────────────────────────
    encoderLeft.init(true);   // true  = gauche
    encoderRight.init(false); // false = droite

    // ── Capteurs ──────────────────────────────
    sensorManager.add(new LidarSensor(),
                      true);

    sensorManager.add(new LineSensor(SensorPosition::FRONT_LEFT,  512),
                      true);
    sensorManager.add(new LineSensor(SensorPosition::FRONT_RIGHT, 512),
                      true);
    sensorManager.add(new LineSensor(SensorPosition::BACK,        512),
                      true);

    sensorManager.add(new TOFSensor(RobotConfig::TOF_IIC_ADDR,  SensorPosition::FRONT_LEFT),
                      true);
    sensorManager.add(new TOFSensor(RobotConfig::TOF_IIC_ADDR1, SensorPosition::FRONT_RIGHT),
                      true);

    sensorManager.add(new IMUSensor(SensorPosition::CENTER),
                      true);

    sensorManager.add(new BattSensor(SensorPosition::BATTERY),
                      true);

    sensorManager.add(new SensorSwitch(RobotConfig::START_BUTTON),
                      true);

    if (!sensorManager.initAll()) {
        LOG("[main] ERREUR init capteurs");
        led.setColor(LedColor::RED);
    }

    // ── EKF ───────────────────────────────────
    ekf.reset();

    // ── PID ───────────────────────────────────
    pidLeft.setLimits(-255, 255);
    pidRight.setLimits(-255, 255);

    // ── Stratégie par défaut ──────────────────
    strategyManager.setStrategy(&stratAdamantine);

    // ── Communication ─────────────────────────
    commManager.begin();
    commManager.setAutoSelectEnabled(true);

    // ── Queue commandes ───────────────────────
    RobotQueues::init();

    // ── Tâches FreeRTOS ───────────────────────
    xTaskCreatePinnedToCore(
        taskSensors, "Sensors",
        RTOSConfig::STACK_SENSORS, &sensorManager,
        RTOSConfig::PRIO_SENSORS,  nullptr,
        RTOSConfig::CORE_SENSORS);

    xTaskCreatePinnedToCore(
        taskEncoders, "Encoders",
        RTOSConfig::STACK_ENCODER, &ekfParams,
        RTOSConfig::PRIO_ENCODER,  nullptr,
        RTOSConfig::CORE_ENCODER);

    xTaskCreatePinnedToCore(
        taskStrategy, "Strategy",
        RTOSConfig::STACK_STRATEGY, &strategyManager,
        RTOSConfig::PRIO_STRATEGY,  nullptr,
        RTOSConfig::CORE_STRATEGY);

    xTaskCreatePinnedToCore(
        taskMotors, "Motors",
        RTOSConfig::STACK_MOTORS, &driverManager,
        RTOSConfig::PRIO_MOTORS,  nullptr,
        RTOSConfig::CORE_MOTORS);

    xTaskCreatePinnedToCore(
        taskComm, "Comm",
        RTOSConfig::STACK_COMM, &commManager,
        RTOSConfig::PRIO_COMM,  nullptr,
        RTOSConfig::CORE_COMM);

    xTaskCreatePinnedToCore(
        taskCommand, "Command",
        RTOSConfig::STACK_COMMAND, &cmdParams,
        RTOSConfig::PRIO_COMMAND,  nullptr,
        RTOSConfig::CORE_COMMAND);

    // ── Prêt ──────────────────────────────────
    led.setColor(LedColor::GREEN);
    LOG("[main] Toutes les tâches lancées — robot prêt");
}

// ═══════════════════════════════════════════════════════════════
//  loop() — vide : tout est géré par les tâches FreeRTOS
// ═══════════════════════════════════════════════════════════════
void loop() {
    // LED reflète l'état courant du robot
    switch (RobotContext::instance().getState()) {
        case RobotConstants::State::STANDBY: led.setColor(LedColor::YELLOW); break;
        case RobotConstants::State::SEARCH:  led.setColor(LedColor::BLUE);   break;
        case RobotConstants::State::ATTACK:  led.setColor(LedColor::RED);    break;
        case RobotConstants::State::EVADE:   led.setColor(LedColor::ORANGE); break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
