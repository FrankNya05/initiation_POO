#pragma once

#include <ArduinoJson.h>
#include <string>
#include "ProtocolTypes.hpp"
#include "RTOSConfig.hpp"

class CommandParser {
public:
    static std::string parseType(const std::string& raw) {
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] JSON error: %s\n", err.c_str());
            return "";
        }

        if (!doc["type"].is<const char*>()) {
            LOG("[CommandParser] Missing or invalid 'type'");
            return "";
        }

        return std::string(doc["type"].as<const char*>());
    }

    static bool parseCmdMotor(const std::string& raw, MotorCommand& out) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] CMD_MOTOR error: %s\n", err.c_str());
            return false;
        }

        JsonVariantConst payloadVar = doc["payload"];
        if (payloadVar.isNull() || !payloadVar.is<JsonObjectConst>()) {
            LOG("[CommandParser] CMD_MOTOR: missing or invalid 'payload'");
            return false;
        }

        JsonObjectConst payload = payloadVar.as<JsonObjectConst>();

        if (!payload["action"].is<const char*>()) {
            LOG("[CommandParser] CMD_MOTOR: missing 'action'");
            return false;
        }

        const char* action = payload["action"].as<const char*>();
        int speed = payload["speed"] | 0;

        if (speed < 0) speed = 0;
        if (speed > 255) speed = 255;

        out = MotorCommand(std::string(action), speed);
        return true;
    }

    static bool parseCmdRobot(const std::string& raw, RobotCommand& out) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            LOGF("[CommandParser] CMD_ROBOT error: %s\n", err.c_str());
            return false;
        }

        JsonVariantConst payloadVar = doc["payload"];
        if (payloadVar.isNull() || !payloadVar.is<JsonObjectConst>()) {
            LOG("[CommandParser] CMD_ROBOT: missing or invalid 'payload'");
            return false;
        }

        JsonObjectConst payload = payloadVar.as<JsonObjectConst>();

        if (!payload["command"].is<const char*>()) {
            LOG("[CommandParser] CMD_ROBOT: missing 'command'");
            return false;
        }

        const char* command = payload["command"].as<const char*>();
        const char* param   = payload["param"] | "";

        out = RobotCommand(std::string(command), std::string(param));
        return true;
    }
};