#pragma once
<<<<<<< HEAD

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
=======
#include <string.h>
#include "RobotConstants.hpp"
#include "Sequences.hpp"

// ═══════════════════════════════════════════════════════════════
//  CommandParser.hpp — Parse une string HMI → RobotCommand
//
//  Commandes supportées :
//    START
//    STOP
//    RESET
//    POSE:RESET
//    STRATEGY:ADAMANTINE | BERSERKER | CIRCLE
//    MOTOR:L:<v>:R:<v>     ex. MOTOR:L:200:R:150
//    MOTOR:STOP
//    PID:KP:<v>            ex. PID:KP:1.2
//    PID:KI:<v>
//    PID:KD:<v>
//    LED:R:<v>:G:<v>:B:<v> ex. LED:R:255:G:0:B:0
//    SEQ:<name>            ex. SEQ:SPIN_LEFT_90
// ═══════════════════════════════════════════════════════════════

enum class CommandType : uint8_t {
    UNKNOWN = 0,
    START,
    STOP,
    RESET,
    POSE_RESET,
    STRATEGY,
    MOTOR_DIRECT,
    MOTOR_STOP,
    PID_KP,
    PID_KI,
    PID_KD,
    LED,
    SEQUENCE
};

struct RobotCommand {
    CommandType type = CommandType::UNKNOWN;

    // STRATEGY
    char strategyName[24] = {};

    // MOTOR_DIRECT
    int motorLeft  = 0;
    int motorRight = 0;

    // PID
    float pidValue = 0.0f;

    // LED
    uint8_t ledR = 0, ledG = 0, ledB = 0;

    // SEQUENCE
    const MoveStep* seqSteps  = nullptr;
    uint8_t         seqLength = 0;
};

// ─────────────────────────────────────────────────────────────
//  Fonction principale de parsing
// ─────────────────────────────────────────────────────────────
inline RobotCommand parseCommand(const char* raw) {

    RobotCommand cmd;

    if (strcmp(raw, "START") == 0) {
        cmd.type = CommandType::START;
    }
    else if (strcmp(raw, "STOP") == 0) {
        cmd.type = CommandType::STOP;
    }
    else if (strcmp(raw, "RESET") == 0) {
        cmd.type = CommandType::RESET;
    }
    else if (strcmp(raw, "POSE:RESET") == 0) {
        cmd.type = CommandType::POSE_RESET;
    }
    else if (strcmp(raw, "MOTOR:STOP") == 0) {
        cmd.type = CommandType::STOP;   // alias de STOP — met aussi l'état en STANDBY
    }
    else if (strncmp(raw, "STRATEGY:", 9) == 0) {
        cmd.type = CommandType::STRATEGY;
        strncpy(cmd.strategyName, raw + 9, sizeof(cmd.strategyName) - 1);
    }
    else if (strncmp(raw, "MOTOR:L:", 8) == 0) {
        // Format : MOTOR:L:<v>:R:<v>
        int l = 0, r = 0;
        const char* rPtr = strstr(raw, ":R:");
        if (rPtr) {
            l = atoi(raw + 8);
            r = atoi(rPtr + 3);
            cmd.type       = CommandType::MOTOR_DIRECT;
            cmd.motorLeft  = l;
            cmd.motorRight = r;
        }
    }
    else if (strncmp(raw, "PID:KP:", 7) == 0) {
        cmd.type     = CommandType::PID_KP;
        cmd.pidValue = atof(raw + 7);
    }
    else if (strncmp(raw, "PID:KI:", 7) == 0) {
        cmd.type     = CommandType::PID_KI;
        cmd.pidValue = atof(raw + 7);
    }
    else if (strncmp(raw, "PID:KD:", 7) == 0) {
        cmd.type     = CommandType::PID_KD;
        cmd.pidValue = atof(raw + 7);
    }
    else if (strncmp(raw, "LED:R:", 6) == 0) {
        // Format : LED:R:<v>:G:<v>:B:<v>
        const char* gPtr = strstr(raw, ":G:");
        const char* bPtr = strstr(raw, ":B:");
        if (gPtr && bPtr) {
            cmd.type = CommandType::LED;
            cmd.ledR = (uint8_t)atoi(raw + 6);
            cmd.ledG = (uint8_t)atoi(gPtr + 3);
            cmd.ledB = (uint8_t)atoi(bPtr + 3);
        }
    }
    else if (strncmp(raw, "SEQ:", 4) == 0) {
        const char* seqName = raw + 4;
        for (uint8_t i = 0; i < SEQUENCE_REGISTRY_LEN; i++) {
            if (strcmp(seqName, SEQUENCE_REGISTRY[i].name) == 0) {
                cmd.type      = CommandType::SEQUENCE;
                cmd.seqSteps  = SEQUENCE_REGISTRY[i].steps;
                cmd.seqLength = SEQUENCE_REGISTRY[i].length;
                break;
            }
        }
    }

    return cmd;
}
>>>>>>> origin/main
