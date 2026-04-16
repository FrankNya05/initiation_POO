<<<<<<< HEAD
/*
 * test_comm_integration.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * End-to-end communication integration test.
 *
 * Purpose:
 *   Verify that the full V1 protocol works correctly with the real robot
 *   architecture: incoming HMI commands trigger real actions, outgoing
 *   telemetry carries real sensor data.
 *
 * What this file is:
 *   The integration layer between CommunicationManager and the robot managers.
 *   It is NOT a unit test and NOT a mock. Every API call in this file maps to
 *   a real method that exists in the repository.
 *
 * Architecture position:
 *   HMI ↔ WiFiComm ↔ CommunicationManager ↔ [this file] ↔ RobotContext
 *                                                         ↔ DriverManager
 *                                                         ↔ StrategyManager
 *
 * Logging:
 *   All output uses LOG / LOGF from RTOSConfig.hpp.
 *   Serial.begin() is the only direct Serial call permitted.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * V1 KNOWN LIMITATIONS (documented here, not hidden)
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 1. backLeft line sensor (pin 34):
 *    RobotContext stores line data under LEFT, RIGHT, and BACK only.
 *    LINE_SENSOR_BACK_LEFT (pin 34) has no matching SensorPosition entry.
 *    TelemetryData::lineBackLeft is therefore always set to false in V1.
 *    To fix: add a BACK_LEFT entry to RobotContext and update _buildTelemetry().
 *
 * 2. Battery helpers (getPercent, isCritical):
 *    These methods exist on BattSensor directly, not on RobotContext.
 *    RobotContext::getBattData() returns only the raw voltage as a scalar.
 *    Percent and critical flag are recomputed here using BattConfig constants,
 *    which mirror the exact logic in BattSensor::getPercent() and isCritical().
 *
 * 3. RESET command:
 *    No reset API exists on DriverManager or StrategyManager yet.
 *    The current implementation stops motors and returns to STANDBY.
 *    A future RESET implementation can replace the TODO block below.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ── Project infrastructure ────────────────────────────────────────────────────
#include "RTOSConfig.hpp"
#include "RobotContext.hpp"
#include "RobotConstants.hpp"

// ── Communication layer ───────────────────────────────────────────────────────
#include "CommunicationManager.hpp"
#include "ProtocolTypes.hpp"

// ── Actuators ─────────────────────────────────────────────────────────────────
#include "DriverManager.hpp"
#include "DriverMotor.hpp"

// ── Strategies ────────────────────────────────────────────────────────────────
#include "StrategyManager.hpp"
#include "AggressiveStrategy.hpp"
#include "DefensiveStrategy.hpp"

// ── Battery config constants (for percent / critical computation) ──────────────
#include "BattSensor.hpp"   // gives access to BattConfig namespace

// ═════════════════════════════════════════════════════════════════════════════
// Global definitions
// ═════════════════════════════════════════════════════════════════════════════

// g_logMutex must be defined exactly once in the file that contains setup().
// RTOSConfig.hpp declares it extern; we define it here.
SemaphoreHandle_t g_logMutex = nullptr;

// ── Network credentials — update before flashing ─────────────────────────────
static const char* WIFI_SSID     = "Frank";
static const char* WIFI_PASSWORD = "ftel7488";
static const char* BROKER_IP     = "172.17.45.34";  // Mosquitto broker IP

// ── Robot hardware instances ──────────────────────────────────────────────────
// DriverMotor is the concrete implementation of IDriverMotor.
// DriverManager takes ownership of the instance passed via add().
static DriverMotor   s_motorDriver;
static DriverManager s_drivers;

// ── Strategy instances ────────────────────────────────────────────────────────
// StrategyManager holds a raw observer pointer — it does NOT own these objects.
// Static storage duration ensures they outlive any task.
static AggressiveStrategy s_stratAggressive;
static DefensiveStrategy  s_stratDefensive;
static StrategyManager    s_strategyManager;

// ── Communication ─────────────────────────────────────────────────────────────
// No secondary channel for V1. The optional parameter defaults to nullptr.
static CommunicationManager s_comm(WIFI_SSID, WIFI_PASSWORD, BROKER_IP);

// ═════════════════════════════════════════════════════════════════════════════
// _applyMotorCommand
// ─────────────────────────────────────────────────────────────────────────────
// Routes a parsed CMD_MOTOR command to the real DriverManager.
//
// Uses the high-level directional methods on DriverManager:
//   forward(speed)   — both wheels forward
//   backward(speed)  — both wheels backward
//   turnLeft(speed)  — left wheel backward, right forward (pivot)
//   turnRight(speed) — right wheel backward, left forward (pivot)
//   stopAll()        — both wheels to 0
//
// Also writes the resulting ActionCommand into RobotContext so that other
// tasks (e.g. taskStrategy) can read the last commanded speeds.
// ═════════════════════════════════════════════════════════════════════════════
static void _applyMotorCommand(const MotorCommand& cmd) {
    RobotConstants::ActionCommand action;

    if (cmd.action == "FORWARD") {
        s_drivers.forward(cmd.speed);
        action = { cmd.speed, cmd.speed };
        LOGF("[taskComm] Motor FORWARD @ %d\n", cmd.speed);
    }
    else if (cmd.action == "BACKWARD") {
        s_drivers.backward(cmd.speed);
        action = { -cmd.speed, -cmd.speed };
        LOGF("[taskComm] Motor BACKWARD @ %d\n", cmd.speed);
    }
    else if (cmd.action == "LEFT") {
        s_drivers.turnLeft(cmd.speed);
        action = { -cmd.speed, cmd.speed };
        LOGF("[taskComm] Motor LEFT @ %d\n", cmd.speed);
    }
    else if (cmd.action == "RIGHT") {
        s_drivers.turnRight(cmd.speed);
        action = { cmd.speed, -cmd.speed };
        LOGF("[taskComm] Motor RIGHT @ %d\n", cmd.speed);
    }
    else if (cmd.action == "STOP") {
        s_drivers.stopAll();
        action = { 0, 0 };
        LOG("[taskComm] Motor STOP");
    }
    else {
        LOGF("[taskComm] Motor: unknown action '%s'\n", cmd.action.c_str());
        return;
    }

    // Keep RobotContext in sync so other tasks can read the last commanded speeds.
    RobotContext::instance().setMotorSpeeds(action);
}

// ═════════════════════════════════════════════════════════════════════════════
// _applyRobotCommand
// ─────────────────────────────────────────────────────────────────────────────
// Routes a parsed CMD_ROBOT command to the real robot logic.
//
// START        — transition to SEARCH, autonomous mode begins
// STOP         — stop motors immediately, return to STANDBY
// SET_STRATEGY — switch active strategy (param: "AGGRESSIVE" or "DEFENSIVE")
// RESET        — stop motors, return to STANDBY (no full reset API yet — see TODO)
// ═════════════════════════════════════════════════════════════════════════════
static void _applyRobotCommand(const RobotCommand& cmd) {
    RobotContext& ctx = RobotContext::instance();

    if (cmd.command == "START") {
        // Transition to SEARCH: the autonomous strategy loop takes over.
        // The HMI operator is responsible for starting the robot after the
        // mandatory 5-second countdown defined by competition rules.
        ctx.setState(RobotConstants::State::SEARCH);
        s_comm.sendState(RobotConstants::State::SEARCH);
        LOG("[taskComm] Robot START — state → SEARCH");
    }
    else if (cmd.command == "STOP") {
        // Stop motors immediately, then go back to STANDBY.
        // This is safe to call from the comm task because DriverManager
        // operations are reentrant (no shared state, direct PWM writes).
        s_drivers.stopAll();
        RobotConstants::ActionCommand stopped = { 0, 0 };
        ctx.setMotorSpeeds(stopped);
        ctx.setState(RobotConstants::State::STANDBY);
        s_comm.sendState(RobotConstants::State::STANDBY);
        LOG("[taskComm] Robot STOP — state → STANDBY, motors stopped");
    }
    else if (cmd.command == "SET_STRATEGY") {
        if (cmd.param == "AGGRESSIVE") {
            // setStrategy() takes a raw StrategyInterface* — no ownership transfer.
            // s_stratAggressive has static storage, so the pointer is always valid.
            s_strategyManager.setStrategy(&s_stratAggressive);
            s_comm.sendLog("Strategy set to: Aggressive");
            LOGF("[taskComm] Strategy → %s\n", s_strategyManager.currentName());
        }
        else if (cmd.param == "DEFENSIVE") {
            s_strategyManager.setStrategy(&s_stratDefensive);
            s_comm.sendLog("Strategy set to: Defensive");
            LOGF("[taskComm] Strategy → %s\n", s_strategyManager.currentName());
        }
        else {
            LOGF("[taskComm] SET_STRATEGY: unknown param '%s'\n", cmd.param.c_str());
        }
    }
    else if (cmd.command == "RESET") {
        // TODO: no full reset API exists on DriverManager or StrategyManager yet.
        // Current behaviour: stop motors and return to STANDBY.
        // A proper RESET would also reinitialize sensors — extend this block
        // when a reset API is added to the architecture.
        s_drivers.stopAll();
        RobotConstants::ActionCommand stopped = { 0, 0 };
        ctx.setMotorSpeeds(stopped);
        ctx.setState(RobotConstants::State::STANDBY);
        s_comm.sendState(RobotConstants::State::STANDBY);
        s_comm.sendLog("Reset: motors stopped, state STANDBY.");
        LOG("[taskComm] Robot RESET (partial — motors stopped, state → STANDBY)");
    }
    else {
        LOGF("[taskComm] Unknown robot command: '%s'\n", cmd.command.c_str());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// _buildTelemetry
// ─────────────────────────────────────────────────────────────────────────────
// Reads real sensor data from RobotContext and fills a TelemetryData struct.
//
// This is the only place in this file where RobotContext is read for outgoing
// telemetry. CommunicationManager never touches RobotContext directly.
//
// Battery:
//   RobotContext::getBattData() returns a SensorData with value.scalar = voltage.
//   getPercent() and isCritical() are computed using BattConfig constants,
//   matching BattSensor::getPercent() and isCritical() exactly.
//
// Line sensors:
//   RobotContext stores LEFT (→ FRONT_LEFT pin), RIGHT (→ FRONT_RIGHT pin),
//   BACK (→ rear center pin). lineBackLeft is always false in V1 — see note above.
//
// Lidar:
//   SensorData VEC3: vector.x = distance (m), vector.y = angle (°, 0–360).
// ═════════════════════════════════════════════════════════════════════════════
static TelemetryData _buildTelemetry() {
    RobotContext& ctx = RobotContext::instance();
    TelemetryData data;

    // ── Battery ──────────────────────────────────────────────────────────────
    SensorData batt = ctx.getBattData();
    if (batt.isValid) {
        float v = batt.value.scalar;   // voltage in Volts
        data.battVoltage = v;

        // Recompute percent using BattConfig constants (mirrors BattSensor::getPercent()).
        if      (v >= BattConfig::VOLT_MAX) data.battPercent = 100;
        else if (v <= BattConfig::VOLT_MIN) data.battPercent = 0;
        else                                data.battPercent = static_cast<int>(
                                                (v - BattConfig::VOLT_MIN) /
                                                (BattConfig::VOLT_MAX - BattConfig::VOLT_MIN) * 100.0f);

        // Recompute critical flag (mirrors BattSensor::isCritical()).
        data.battCritical = (v <= BattConfig::VOLT_ALERT);
    }

    // ── Line sensors ──────────────────────────────────────────────────────────
    // RobotContext positions map to physical pins as follows:
    //   LEFT  → LINE_SENSOR_FRONT_LEFT  (GPIO 32)
    //   RIGHT → LINE_SENSOR_FRONT_RIGHT (GPIO 33)
    //   BACK  → LINE_SENSOR_BACK        (GPIO 35)  ← single rear sensor
    //
    // V1 LIMITATION: lineBackLeft (LINE_SENSOR_BACK_LEFT, GPIO 34) has no
    // corresponding SensorPosition in RobotContext. It is hardcoded to false.
    // To fix: add BACK_LEFT to RobotContext and read it here.
    SensorData lineLeft  = ctx.getLineData(SensorPosition::LEFT);
    SensorData lineRight = ctx.getLineData(SensorPosition::RIGHT);
    SensorData lineBack  = ctx.getLineData(SensorPosition::BACK);

    data.lineFrontLeft  = lineLeft.isValid  && (lineLeft.value.scalar  > 0.0f);
    data.lineFrontRight = lineRight.isValid && (lineRight.value.scalar > 0.0f);
    data.lineBack       = lineBack.isValid  && (lineBack.value.scalar  > 0.0f);
    data.lineBackLeft   = false;  // V1 LIMITATION — see note above

    // ── Lidar ─────────────────────────────────────────────────────────────────
    // LidarSensor fills SensorData as VEC3:
    //   value.vector.x = distance to nearest object (metres, EMA-filtered)
    //   value.vector.y = angle to nearest object (degrees, 0–360°)
    SensorData lidar = ctx.getLidarData();
    data.lidarValid = lidar.isValid;
    if (lidar.isValid) {
        data.lidarDist  = lidar.value.vector.x;
        data.lidarAngle = lidar.value.vector.y;
    }

    return data;
}

// ═════════════════════════════════════════════════════════════════════════════
// taskComm — FreeRTOS task
// ─────────────────────────────────────────────────────────────────────────────
// Runs on Core 0 (required for WiFi/MQTT on ESP32).
// Stack: RTOSConfig::STACK_COMM (8192 bytes — WiFi + MQTT need headroom).
// Priority: RTOSConfig::PRIO_COMM (1 — low; comm is not time-critical).
// Period: RTOSConfig::PERIOD_COMM (100 ms = pdMS_TO_TICKS(100)).
//
// Each iteration:
//   1. comm.update() — keeps MQTT alive, dispatches incoming messages
//      (which trigger _applyMotorCommand / _applyRobotCommand via callbacks)
//   2. Sends TELEMETRY with real sensor data every 100 ms
//   3. Detects state machine transitions and sends STATE messages
// ═════════════════════════════════════════════════════════════════════════════
static void taskComm(void* /*params*/) {
    RobotConstants::State lastState = RobotConstants::State::STANDBY;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // ── Keep MQTT alive and dispatch received messages ────────────────────
        s_comm.update();

        if (s_comm.isConnected()) {
            // ── Periodic telemetry — every 100 ms ────────────────────────────
            TelemetryData data = _buildTelemetry();
            s_comm.sendTelemetry(data);

            // ── State change notification ─────────────────────────────────────
            // Read the current robot state from RobotContext.
            // If it changed since the last iteration, send a STATE message.
            // The strategy task and command callbacks both write to ctx.state,
            // so this correctly captures changes from both sources.
            RobotConstants::State currentState =
                RobotContext::instance().getState();

            if (currentState != lastState) {
                lastState = currentState;
                s_comm.sendState(currentState);
                LOGF("[taskComm] State changed → %d\n",
                     static_cast<int>(currentState));
            }
        }

        // Precise 100 ms period — uses vTaskDelayUntil to avoid drift.
        vTaskDelayUntil(&xLastWakeTime, RTOSConfig::PERIOD_COMM);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// setup()
// ═════════════════════════════════════════════════════════════════════════════
=======
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
>>>>>>> origin/main
void setup() {
    Serial.begin(115200);

<<<<<<< HEAD
    // ── Mutex must be created before any LOG / LOGF call ─────────────────────
    g_logMutex = xSemaphoreCreateMutex();
    LOG("[setup] Boot — communication integration test");

    // ── Motors ────────────────────────────────────────────────────────────────
    // DriverManager takes ownership of the DriverMotor pointer via add().
    // We pass the address of the static instance — lifetime is guaranteed.
    s_drivers.add(&s_motorDriver, DriverRole::MAIN, true);
    if (!s_drivers.initAll()) {
        LOG("[setup] ERROR: motor init failed");
    } else {
        LOG("[setup] Motors OK");
    }

    // ── Default strategy ──────────────────────────────────────────────────────
    // Start with Aggressive. The HMI can switch via SET_STRATEGY at runtime.
    s_strategyManager.setStrategy(&s_stratAggressive);
    LOGF("[setup] Strategy → %s\n", s_strategyManager.currentName());

    // ── Register typed command callbacks BEFORE comm.begin() ─────────────────
    // These lambdas capture by reference — safe because all captured objects
    // have static storage duration and outlive any FreeRTOS task.

    s_comm.onMotorCommand([](const MotorCommand& cmd) {
        _applyMotorCommand(cmd);
    });

    s_comm.onRobotCommand([](const RobotCommand& cmd) {
        _applyRobotCommand(cmd);
    });

    // ── Transport event callbacks ─────────────────────────────────────────────
    s_comm.onConnect([]() {
        LOG("[taskComm] Connected to MQTT broker.");

        // Push initial state so the HMI starts in sync.
        s_comm.sendState(RobotContext::instance().getState());
        s_comm.sendLog("Robot online. Strategy: " +
                       std::string(s_strategyManager.currentName()));
    });

    s_comm.onDisconnect([]() {
        LOG("[taskComm] Disconnected from MQTT broker.");
    });

    // ── Connect to WiFi and MQTT ──────────────────────────────────────────────
    s_comm.begin();

    // ── Start the comm FreeRTOS task ──────────────────────────────────────────
    // Core 0 is required for WiFi/MQTT on ESP32 (ESP-IDF constraint).
    xTaskCreatePinnedToCore(
        taskComm,
        "taskComm",
        RTOSConfig::STACK_COMM,      // 8192 bytes
        nullptr,
        RTOSConfig::PRIO_COMM,       // priority 1 (low)
        nullptr,
        RTOSConfig::CORE_COMM        // core 0
    );

    LOG("[setup] taskComm started");
}

// ═════════════════════════════════════════════════════════════════════════════
// loop()
// ─────────────────────────────────────────────────────────────────────────────
// All work is done in FreeRTOS tasks. loop() is intentionally empty.
// The Arduino scheduler still calls it but it yields immediately.
// ═════════════════════════════════════════════════════════════════════════════
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
=======
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

    // ── Communication WiFi/MQTT ──────────────────────────────
    commManager.configureWifi(
        "NomDeTonHotspot",      // SSID de ton téléphone
        "MotDePasse",           // mot de passe
        "192.168.x.x",          // IP fixe du Raspberry Pi
        1883,                   // port Mosquitto (défaut)
        "robot/data",           // topic ESP32 → Pi
        "robot/commande"        // topic Pi → ESP32
    );
    commManager.selectChannel(CommChannel::WIFI);
    // Note : commManager.begin() est appelé dans taskComm — ne pas le doubler ici
    
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
>>>>>>> origin/main
