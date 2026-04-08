/*
 * example_task_comm.cpp
 * ---------------------
 * Reference example: how to wire CommunicationManager into a FreeRTOS task
 * (or the Arduino loop if you are not using tasks yet).
 *
 * This file is NOT meant to be dropped into the project as-is. Adapt the
 * credentials, driver calls, and strategy manager calls to your actual code.
 *
 * Responsibilities of this layer:
 *   - Fill TelemetryData from RobotContext (this is the task's job)
 *   - Call comm.sendTelemetry(data) every 100 ms
 *   - Call comm.sendState() on every state machine transition
 *   - Register typed command callbacks that update RobotContext / managers
 *   - Call comm.update() every iteration to keep MQTT alive
 *
 * This is the ONLY layer that touches both RobotContext and CommunicationManager.
 * That boundary is intentional: neither class depends on the other.
 *
 * Telemetry period: 100 ms (10 Hz) — matches RTOSConfig::PERIOD_COMM.
 *
 * Logging: all output uses LOG / LOGF from RTOSConfig.hpp.
 * No raw Serial calls are present in this file.
 */

#include <Arduino.h>
#include "CommunicationManager.hpp"
#include "RobotContext.hpp"
#include "RobotConstants.hpp"
#include "ProtocolTypes.hpp"
#include "RTOSConfig.hpp"

// ── Network credentials — replace with your actual values ─────────────────
static const char* WIFI_SSID     = "YourSSID";
static const char* WIFI_PASSWORD = "YourPassword";
static const char* BROKER_IP     = "192.168.1.100";

// ── Global instance ───────────────────────────────────────────────────────
// No secondary channel for the V1 sprint — the optional parameter defaults
// to nullptr, so it can be omitted entirely.
CommunicationManager comm(WIFI_SSID, WIFI_PASSWORD, BROKER_IP);

// ============================================================================
// buildTelemetryFromContext
// ============================================================================
// Reads sensor data from RobotContext and fills a TelemetryData struct.
//
// This is a free function — it is the meeting point between the robot data
// layer (RobotContext) and the communication layer (CommunicationManager).
// Keeping it here, rather than inside CommunicationManager, is what preserves
// the decoupling between the two systems.
//
// ── backLeft note ───────────────────────────────────────────────────────────
// TelemetryData::lineBackLeft corresponds to LINE_SENSOR_BACK_LEFT (pin 34).
// RobotContext currently stores line sensors under LEFT, RIGHT, and BACK only.
// lineBackLeft is explicitly set to false until RobotContext is extended with
// a BACK_LEFT position. This is a known V1 limitation, not a hidden workaround.
// When it is wired in, replace the `data.lineBackLeft = false` line with:
//   SensorData backLeft = ctx.getLineData(SensorPosition::BACK_LEFT);
//   data.lineBackLeft = backLeft.isValid && (backLeft.value.scalar > 0.0f);
// ────────────────────────────────────────────────────────────────────────────
static TelemetryData buildTelemetryFromContext() {
    RobotContext& ctx = RobotContext::instance();
    TelemetryData data;

    // ── Battery ──────────────────────────────────────────────────────────
    SensorData batt = ctx.getBattData();
    if (batt.isValid) {
        float v = batt.value.scalar;
        data.battVoltage = v;

        // Linear mapping: 6.0 V = 0 %, 8.4 V = 100 %
        // Mirrors BattSensor::getPercent() logic.
        if      (v >= 8.4f) data.battPercent = 100;
        else if (v <= 6.0f) data.battPercent = 0;
        else                data.battPercent = static_cast<int>(
                                (v - 6.0f) / (8.4f - 6.0f) * 100.0f);

        data.battCritical = (v <= 6.6f);
    }

    // ── Line sensors ─────────────────────────────────────────────────────
    // RobotContext positions:  LEFT → FRONT_LEFT pin (32)
    //                          RIGHT → FRONT_RIGHT pin (33)
    //                          BACK  → BACK pin (35)
    SensorData lineLeft  = ctx.getLineData(SensorPosition::LEFT);
    SensorData lineRight = ctx.getLineData(SensorPosition::RIGHT);
    SensorData lineBack  = ctx.getLineData(SensorPosition::BACK);

    data.lineFrontLeft  = lineLeft.isValid  && (lineLeft.value.scalar  > 0.0f);
    data.lineFrontRight = lineRight.isValid && (lineRight.value.scalar > 0.0f);
    data.lineBack       = lineBack.isValid  && (lineBack.value.scalar  > 0.0f);

    // V1 LIMITATION — see note above.
    data.lineBackLeft = false;

    // ── Lidar ─────────────────────────────────────────────────────────────
    // SensorData VEC3 layout: vector.x = distance (m), vector.y = angle (°)
    SensorData lidar = ctx.getLidarData();
    data.lidarValid = lidar.isValid;
    if (lidar.isValid) {
        data.lidarDist  = lidar.value.vector.x;
        data.lidarAngle = lidar.value.vector.y;
    }

    return data;
}

// ============================================================================
// setup()
// ============================================================================
void setup() {
    Serial.begin(115200);
    g_logMutex = xSemaphoreCreateMutex();   // required before any LOG / LOGF call

    // ------------------------------------------------------------------
    // 1. Register typed command callbacks BEFORE begin()
    // ------------------------------------------------------------------

    comm.onMotorCommand([](const MotorCommand& cmd) {
        LOGF("[taskComm] Motor: %s @ speed %d\n", cmd.action.c_str(), cmd.speed);

        // Route to your DriverManager instance:
        //
        // if      (cmd.action == "FORWARD")  driver.setSpeed( cmd.speed,  cmd.speed);
        // else if (cmd.action == "BACKWARD") driver.setSpeed(-cmd.speed, -cmd.speed);
        // else if (cmd.action == "LEFT")     driver.setSpeed(-cmd.speed,  cmd.speed);
        // else if (cmd.action == "RIGHT")    driver.setSpeed( cmd.speed, -cmd.speed);
        // else if (cmd.action == "STOP")     driver.stop();
    });

    comm.onRobotCommand([](const RobotCommand& cmd) {
        LOGF("[taskComm] Robot cmd: %s (%s)\n", cmd.command.c_str(), cmd.param.c_str());

        RobotContext& ctx = RobotContext::instance();

        if (cmd.command == "START") {
            ctx.setState(RobotConstants::State::SEARCH);
        }
        else if (cmd.command == "STOP") {
            ctx.setState(RobotConstants::State::STANDBY);
            // driver.stop();  ← also stop motors immediately
        }
        else if (cmd.command == "SET_STRATEGY") {
            // strategyManager.select(cmd.param);
            comm.sendLog("Strategy set to: " + cmd.param);
        }
        else if (cmd.command == "RESET") {
            // Add reset logic here when needed
        }
    });

    // ------------------------------------------------------------------
    // 2. Transport event callbacks
    // ------------------------------------------------------------------

    comm.onConnect([]() {
        LOG("[taskComm] Connected to MQTT broker.");
        // Immediately push current state so the HMI starts in sync.
        comm.sendState(RobotContext::instance().getState());
        comm.sendLog("Robot online.");
    });

    comm.onDisconnect([]() {
        LOG("[taskComm] Disconnected from MQTT broker.");
    });

    // ------------------------------------------------------------------
    // 3. Connect
    // ------------------------------------------------------------------
    comm.begin();
}

// ============================================================================
// loop() — or the body of a FreeRTOS task
// ============================================================================
void loop() {
    // Always call update() — keeps MQTT alive and dispatches incoming messages.
    comm.update();

    // ── Periodic telemetry — every 100 ms ──────────────────────────────────
    // 100 ms matches RTOSConfig::PERIOD_COMM (pdMS_TO_TICKS(100)).
    // If this runs inside a FreeRTOS task, use vTaskDelayUntil instead
    // of the millis() guard below (see the task version at the bottom).
    static uint32_t lastTelemetry = 0;
    if (comm.isConnected() && (millis() - lastTelemetry >= 100)) {
        lastTelemetry = millis();

        // Collect data (this layer's responsibility)
        TelemetryData data = buildTelemetryFromContext();

        // Hand to CommunicationManager — no JSON work here
        comm.sendTelemetry(data);
    }

    // ── State change notification ────────────────────────────────────────
    static RobotConstants::State lastState = RobotConstants::State::STANDBY;
    RobotConstants::State currentState = RobotContext::instance().getState();

    if (currentState != lastState) {
        lastState = currentState;
        if (comm.isConnected()) {
            comm.sendState(currentState);
        }
    }
}

// ============================================================================
// FreeRTOS task version
// ============================================================================
// Use this instead of loop() when you have a dedicated comm task.
// Stack size: RTOSConfig::STACK_COMM (8192 bytes) — WiFi + MQTT need headroom.
// Core:       RTOSConfig::CORE_COMM  (core 0)    — required for WiFi on ESP32.
// Priority:   RTOSConfig::PRIO_COMM  (1)          — low; comm is not time-critical.
//
// void taskComm(void* /*params*/) {
//     RobotConstants::State lastState = RobotConstants::State::STANDBY;
//     TickType_t xLastWakeTime = xTaskGetTickCount();
//
//     for (;;) {
//         comm.update();
//
//         if (comm.isConnected()) {
//             TelemetryData data = buildTelemetryFromContext();
//             comm.sendTelemetry(data);
//
//             RobotConstants::State s = RobotContext::instance().getState();
//             if (s != lastState) {
//                 lastState = s;
//                 comm.sendState(s);
//             }
//         }
//
//         // Precise 100 ms period — matches RTOSConfig::PERIOD_COMM
//         vTaskDelayUntil(&xLastWakeTime, RTOSConfig::PERIOD_COMM);
//     }
// }
