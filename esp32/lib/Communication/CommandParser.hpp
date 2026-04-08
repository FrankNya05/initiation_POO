#pragma once

#include <ArduinoJson.h>
#include <string>
#include "ProtocolTypes.hpp"
#include "RTOSConfig.hpp"

/*
 * CommandParser.hpp
 * -----------------
 * Parses raw JSON strings received from the HMI into clean C++ structs.
 *
 * This is the ONLY place on the ESP32 that reads incoming JSON.
 * No other class needs to touch ArduinoJson to handle commands.
 *
 * Responsibilities:
 *   - parseType()      → read the "type" field from any V1 envelope
 *   - parseCmdMotor()  → fill a MotorCommand struct from a CMD_MOTOR message
 *   - parseCmdRobot()  → fill a RobotCommand struct from a CMD_ROBOT message
 *
 * Design decisions:
 *   - Static methods: no state, no dependencies, simple to call anywhere.
 *   - Output via reference parameter (out-param): avoids heap allocation.
 *   - Returns bool so the caller can handle parse failure gracefully.
 *   - Uses StaticJsonDocument with fixed sizes that comfortably fit V1
 *     messages (largest V1 command is ~80 bytes; 256-byte doc is generous).
 *   - All logging uses LOG / LOGF from RTOSConfig.hpp (no raw Serial calls).
 *
 * OOP concept: static utility class — a group of related functions sharing
 * a single well-defined responsibility (parsing the incoming protocol).
 */
class CommandParser {
public:

    // -----------------------------------------------------------------------
    // parseType
    // -----------------------------------------------------------------------
    // Extracts the "type" field from any V1 envelope.
    // Returns an empty string if the JSON is malformed or "type" is missing.
    //
    // Call this first to decide which parse function to use next.
    // -----------------------------------------------------------------------
    static std::string parseType(const std::string& raw) {
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] JSON error: %s\n", err.c_str());
            return "";
        }
        const char* t = doc["type"] | "";
        return std::string(t);
    }

    // -----------------------------------------------------------------------
    // parseCmdMotor
    // -----------------------------------------------------------------------
    // Parses a CMD_MOTOR message into a MotorCommand struct.
    // Returns true on success, false if required fields are missing.
    //
    // Expected JSON:
    //   { "type": "CMD_MOTOR", "payload": { "action": "FORWARD", "speed": 150 } }
    // -----------------------------------------------------------------------
    static bool parseCmdMotor(const std::string& raw, MotorCommand& out) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] CMD_MOTOR error: %s\n", err.c_str());
            return false;
        }

        JsonObject payload = doc["payload"];
        if (payload.isNull()) {
            LOG("[CommandParser] CMD_MOTOR: missing 'payload'");
            return false;
        }

        const char* action = payload["action"] | nullptr;
        if (action == nullptr) {
            LOG("[CommandParser] CMD_MOTOR: missing 'action'");
            return false;
        }

        int speed = payload["speed"] | 0;
        // Clamp to valid PWM range — defensive against HMI bugs
        if (speed < 0)   speed = 0;
        if (speed > 255) speed = 255;

        out = MotorCommand(std::string(action), speed);
        return true;
    }

    // -----------------------------------------------------------------------
    // parseCmdRobot
    // -----------------------------------------------------------------------
    // Parses a CMD_ROBOT message into a RobotCommand struct.
    // Returns true on success, false if required fields are missing.
    //
    // Expected JSON:
    //   { "type": "CMD_ROBOT", "payload": { "command": "START", "param": "" } }
    // -----------------------------------------------------------------------
    static bool parseCmdRobot(const std::string& raw, RobotCommand& out) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] CMD_ROBOT error: %s\n", err.c_str());
            return false;
        }

        JsonObject payload = doc["payload"];
        if (payload.isNull()) {
            LOG("[CommandParser] CMD_ROBOT: missing 'payload'");
            return false;
        }

        const char* command = payload["command"] | nullptr;
        if (command == nullptr) {
            LOG("[CommandParser] CMD_ROBOT: missing 'command'");
            return false;
        }

        const char* param = payload["param"] | "";   // param is optional

        out = RobotCommand(std::string(command), std::string(param));
        return true;
    }
};
