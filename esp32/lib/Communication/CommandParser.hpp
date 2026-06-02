#pragma once
#include <string.h>
#include "RobotConstants.hpp"
#include "Sequences.hpp"

// ═══════════════════════════════════════════════════════════════
//  CommandParser.hpp — Parse une string HMI → RobotCommand
//
//  Commandes supportées (topic MQTT : robot/cmd) :
//
//  Contrôle général
//    START                         remet le robot en marche
//    STOP                          arrêt moteurs + état STANDBY
//    RESET                         alias STOP
//    POSE:RESET                    remet l'EKF à (0, 0, 0)
//
//  Stratégies
//    STRATEGY:<nom>                ex. STRATEGY:Square
//                                  noms : Track | Adamantine | Berserker
//                                         Circle | Seek | Square | ...
//
//  Moteurs (contrôle direct)
//    MOTOR:L:<v>:R:<v>             ex. MOTOR:L:150:R:150  (-255 à 255)
//    MOTOR:STOP                    alias STOP
//
//  PID vitesse roues (asservissement gauche/droite)
//    PID:KP:<v>                    ex. PID:KP:2.0   (défaut : 2.0)
//    PID:KI:<v>                    ex. PID:KI:0.15  (défaut : 0.15)
//    PID:KD:<v>                    ex. PID:KD:0.0   (défaut : 0.0)
//
//  PID cap / tenue de ligne droite (pidYaw)
//    PID:YAW:KP:<v>                ex. PID:YAW:KP:5.0   (défaut : 5.0)
//    PID:YAW:KI:<v>                ex. PID:YAW:KI:0.5   (défaut : 0.5)
//    PID:YAW:KD:<v>                ex. PID:YAW:KD:0.0   (défaut : 0.0)
//
//  LED
//    LED:R:<v>:G:<v>:B:<v>         ex. LED:R:255:G:0:B:0  (0-255)
//
//  Séquences prédéfinies
//    SEQ:<nom>                     ex. SEQ:SPIN_LEFT_90
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
    PID_YAW_KP,
    PID_YAW_KI,
    PID_YAW_KD,
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
    else if (strncmp(raw, "PID:YAW:KP:", 11) == 0) {
        cmd.type     = CommandType::PID_YAW_KP;
        cmd.pidValue = atof(raw + 11);
    }
    else if (strncmp(raw, "PID:YAW:KI:", 11) == 0) {
        cmd.type     = CommandType::PID_YAW_KI;
        cmd.pidValue = atof(raw + 11);
    }
    else if (strncmp(raw, "PID:YAW:KD:", 11) == 0) {
        cmd.type     = CommandType::PID_YAW_KD;
        cmd.pidValue = atof(raw + 11);
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
