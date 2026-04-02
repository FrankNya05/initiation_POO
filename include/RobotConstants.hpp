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

} // namespace RobotConstants
