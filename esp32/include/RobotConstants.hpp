#pragma once
#include <cstdint>

// ═══════════════════════════════════════════════════════════════
//  RobotConstants.hpp — Constantes et types globaux du robot
//
//  Ce fichier définit les types partagés entre tous les modules :
//  stratégies, contexte, moteurs, communication.
//  Aucune dépendance externe — inclus partout sans risque.
// ═══════════════════════════════════════════════════════════════

namespace RobotConstants {

// ───────────────────────────────────────────────────────────────
//  State — état de la machine à états du robot
//  Utilisé par : RobotContext, StrategyManager, taskStrategy
// ───────────────────────────────────────────────────────────────
enum class State : uint8_t {
    STANDBY,    // Attente du signal de départ (5 secondes réglementaires)
    SEARCH,     // Rotation sur place — cherche l'adversaire
    ATTACK,     // Adversaire détecté — fonce dessus à pleine vitesse
    EVADE       // Bord de piste détecté — manœuvre de récupération
};

// ───────────────────────────────────────────────────────────────
//  ActionCommand — commande moteurs produite par une stratégie
//  Utilisé par : StrategyInterface::execute(), taskMotors
// ───────────────────────────────────────────────────────────────
struct ActionCommand {
    int leftSpeed  = 0;   // -255 (arrière max) → +255 (avant max)
    int rightSpeed = 0;

    // Helpers sémantiques
    bool isStop()    const { return leftSpeed == 0 && rightSpeed == 0; }
    bool isForward() const { return leftSpeed > 0 && rightSpeed > 0; }
    bool isTurning() const { return leftSpeed != rightSpeed; }
};

// ───────────────────────────────────────────────────────────────
//  Direction — directions prédéfinies pour DriverManager::move()
// ───────────────────────────────────────────────────────────────
enum class Direction : uint8_t {
    AVANT,
    ARRIERE,
    GAUCHE,
    DROITE
};

// ───────────────────────────────────────────────────────────────
//  MotorSide — côté moteur (usage interne DriverManager)
// ───────────────────────────────────────────────────────────────
enum class MotorSide : uint8_t {
    LEFT,
    RIGHT
};

    constexpr int   ENCODER_PPR        = 7;
    constexpr int   ENCODER_QUADRATURE = 4;
    constexpr int   GEAR_RATIO         = 100;  // JGA12-N20 100:1 (validé expérimentalement)
    constexpr int   PULSES_PER_REV     = ENCODER_PPR * ENCODER_QUADRATURE * GEAR_RATIO; // 2800

    constexpr float WHEEL_DIAMETER_MM      = 30.0f;
    constexpr float WHEEL_CIRCUMFERENCE_MM = 3.14159f * WHEEL_DIAMETER_MM;
    constexpr float WHEELBASE_MM           = 56.0f;   // entraxe centre roue gauche → droite

    // Angle par pulse (déduit de PULSES_PER_REV = 2800)
    constexpr float DEG_PER_PULSE = 360.0f / PULSES_PER_REV;   // ~0.4286°
    constexpr float RAD_PER_PULSE = 6.28318f / PULSES_PER_REV; // ~0.007480 rad


} // namespace RobotConstants
