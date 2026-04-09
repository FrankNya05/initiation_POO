#pragma once

#include <ArduinoJson.h>
#include <string>
#include "ProtocolTypes.hpp"
#include "RobotConstants.hpp"

/*
 * TelemetrySerializer.hpp
 * ------------------------
 * Builds outgoing JSON message strings for the V1 protocol.
 *
 * This is the ONLY place on the ESP32 that writes outgoing JSON.
 * No other class needs to touch ArduinoJson to produce messages.
 *
 * Responsibilities:
 *   - buildTelemetry() → TELEMETRY message (battery + line + lidar)
 *   - buildState()     → STATE message
 *   - buildAck()       → ACK message
 *   - buildLog()       → LOG message
 *
 * Design decisions:
 *   - Static methods: no state, simple to call.
 *   - Takes plain types and ProtocolTypes structs — no RobotContext dependency.
 *   - StaticJsonDocument sizes:
 *       TELEMETRY: battery(3) + line(4) + lidar(3) + ts ≈ 200 bytes JSON
 *                  → 512-byte doc leaves comfortable headroom
 *       STATE / ACK / LOG: tiny → 128-byte doc
 *   - Returns std::string ready for CommunicationManager::sendRaw().
 *
 * OOP concept: static utility class — all serialization logic in one place,
 * away from managers and transport classes.
 *
 * ── State mapping ────────────────────────────────────────────────────────
 *   Firmware enum          Protocol string
 *   STANDBY           →   "IDLE"
 *   SEARCH            →   "SEARCH"
 *   ATTACK            →   "ATTACK"
 *   EVADE             →   "DEFENSE"
 * ─────────────────────────────────────────────────────────────────────────
 */
class TelemetrySerializer {
public:

    // -----------------------------------------------------------------------
    // buildTelemetry
    // -----------------------------------------------------------------------
    // Serializes a TelemetryData struct into a TELEMETRY JSON string.
    //
    // The TelemetryData is filled by the task layer (see example_task_comm.cpp).
    // This method knows nothing about RobotContext — it only formats data.
    // -----------------------------------------------------------------------
    static std::string buildTelemetry(const TelemetryData& d) {
        StaticJsonDocument<512> doc;

        doc["type"] = "TELEMETRY";
        JsonObject payload = doc.createNestedObject("payload");
        payload["ts"] = millis();

        // Battery
        JsonObject battery = payload.createNestedObject("battery");
        battery["voltage"]  = roundf(d.battVoltage * 100.0f) / 100.0f;
        battery["percent"]  = d.battPercent;
        battery["critical"] = d.battCritical;

        // Line sensors
        // Note: lineBackLeft is always false in V1 because pin 34 is not yet
        // wired into RobotContext. The field is present in the JSON so the
        // HMI parser does not need to handle a missing key. See ProtocolTypes.hpp.
        JsonObject line = payload.createNestedObject("line");
        line["frontLeft"]  = d.lineFrontLeft;
        line["frontRight"] = d.lineFrontRight;
        line["backLeft"]   = d.lineBackLeft;
        line["back"]       = d.lineBack;

        // Lidar
        JsonObject lidar = payload.createNestedObject("lidar");
        lidar["dist"]  = roundf(d.lidarDist  * 100.0f) / 100.0f;
        lidar["angle"] = roundf(d.lidarAngle * 10.0f)  / 10.0f;
        lidar["valid"] = d.lidarValid;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    // -----------------------------------------------------------------------
    // buildState
    // -----------------------------------------------------------------------
    // Maps the firmware State enum to the V1 protocol string and wraps it
    // in a STATE envelope.
    //
    // Example output:
    //   {"type":"STATE","payload":{"state":"ATTACK"}}
    // -----------------------------------------------------------------------
    static std::string buildState(RobotConstants::State state) {
        StaticJsonDocument<128> doc;
        doc["type"] = "STATE";
        JsonObject payload = doc.createNestedObject("payload");
        payload["state"] = _stateToString(state);

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    // -----------------------------------------------------------------------
    // buildAck
    // -----------------------------------------------------------------------
    // Wraps the echoed command string in an ACK envelope.
    //
    // Example output:
    //   {"type":"ACK","payload":{"command":"FORWARD"}}
    // -----------------------------------------------------------------------
    static std::string buildAck(const std::string& command) {
        StaticJsonDocument<128> doc;
        doc["type"] = "ACK";
        JsonObject payload = doc.createNestedObject("payload");
        payload["command"] = command;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    // -----------------------------------------------------------------------
    // buildLog
    // -----------------------------------------------------------------------
    // Wraps a freeform debug string in a LOG envelope.
    //
    // Example output:
    //   {"type":"LOG","payload":{"message":"Strategy switched to: Aggressive"}}
    // -----------------------------------------------------------------------
    static std::string buildLog(const std::string& message) {
        StaticJsonDocument<256> doc;
        doc["type"] = "LOG";
        JsonObject payload = doc.createNestedObject("payload");
        payload["message"] = message;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

private:

    static const char* _stateToString(RobotConstants::State state) {
        switch (state) {
            case RobotConstants::State::STANDBY: return "IDLE";
            case RobotConstants::State::SEARCH:  return "SEARCH";
            case RobotConstants::State::ATTACK:  return "ATTACK";
            case RobotConstants::State::EVADE:   return "DEFENSE";
            default:                             return "IDLE";
        }
    }
};
