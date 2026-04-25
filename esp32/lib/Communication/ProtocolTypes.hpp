#pragma once

#include <string>

/*
 * ProtocolTypes.hpp
 * -----------------
 * Plain data structures that represent the V1 protocol messages.
 *
 * TWO purposes:
 *   1. MotorCommand / RobotCommand — results of PARSING incoming JSON from the HMI.
 *   2. TelemetryData               — input struct for SERIALIZING outgoing telemetry.
 *
 * Design intent:
 *   - CommandParser    produces MotorCommand / RobotCommand (from JSON)
 *   - TelemetrySerializer  consumes TelemetryData (to JSON)
 *   - CommunicationManager exposes them via its API
 *   - The task layer (taskComm) fills TelemetryData from RobotContext
 *     and passes it to CommunicationManager
 *   - Nobody else ever touches ArduinoJson
 *
 * OOP concept: plain data structs (value types) with no behaviour.
 * They act as the boundary between robot data and protocol data.
 *
 * ── backLeft note ───────────────────────────────────────────────────────
 * PinConfig.hpp defines LINE_SENSOR_BACK_LEFT (pin 34), but RobotContext
 * currently stores line sensors under LEFT, RIGHT, and BACK only.
 * TelemetryData includes backLeft so the protocol field is always present
 * in the JSON, but the task that fills TelemetryData must set it to false
 * until pin 34 is wired into RobotContext.
 * This is an explicit, documented V1 limitation — not a hidden workaround.
 * ────────────────────────────────────────────────────────────────────────
 */

// ---------------------------------------------------------------------------
// TelemetryData
// ---------------------------------------------------------------------------
// Filled by the task layer (taskComm) from RobotContext, then passed to
// CommunicationManager::sendTelemetry(). Keeps RobotContext out of the
// communication layer.
// ---------------------------------------------------------------------------
struct TelemetryData {
    // Battery
    float battVoltage  = 0.0f;
    int   battPercent  = 0;
    bool  battCritical = false;

    // Line sensors (true = white border detected)
    bool lineFrontLeft  = false;
    bool lineFrontRight = false;
    bool lineBackLeft   = false;   // V1: pin 34 not yet in RobotContext → always false
    bool lineBack       = false;

    // Lidar (nearest detected object)
    float lidarDist  = 0.0f;
    float lidarAngle = 0.0f;
    bool  lidarValid = false;
};

// ---------------------------------------------------------------------------
// MotorCommand
// ---------------------------------------------------------------------------
// Result of parsing a CMD_MOTOR message from the HMI.
//
// action: "FORWARD" | "BACKWARD" | "LEFT" | "RIGHT" | "STOP"
// speed:  0–255
// ---------------------------------------------------------------------------
struct MotorCommand {
    std::string action;
    int         speed;

    MotorCommand() : action("STOP"), speed(0) {}
    MotorCommand(const std::string& a, int s) : action(a), speed(s) {}
};

// ---------------------------------------------------------------------------
// RobotCommand
// ---------------------------------------------------------------------------
// Result of parsing a CMD_ROBOT message from the HMI.
//
// command: "START" | "STOP" | "SET_STRATEGY" | "RESET"
// param:   optional (e.g. "AGGRESSIVE"), empty string when unused
// ---------------------------------------------------------------------------
struct RobotCommand {
    std::string command;
    std::string param;

    RobotCommand() : command(""), param("") {}
    RobotCommand(const std::string& c, const std::string& p) : command(c), param(p) {}
};
