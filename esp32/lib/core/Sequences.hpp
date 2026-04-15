#pragma once
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
//  Sequences.hpp — Séquences de mouvement prédéfinies
//
//  Une séquence = tableau de MoveStep exécutés en ordre
//  Chaque step : vitesse gauche/droite + durée en ms
//
//  Déclenchement via commande HMI : SEQ:SPIN_LEFT_90
// ═══════════════════════════════════════════════════════════════

struct MoveStep {
    int      leftSpeed;   // -255 → +255
    int      rightSpeed;  // -255 → +255
    uint32_t durationMs;  // durée du step en ms
};

// ─────────────────────────────────────────────────────────────
//  Séquences disponibles
// ─────────────────────────────────────────────────────────────

// Rotation 90° gauche sur place
constexpr MoveStep SEQ_SPIN_LEFT_90[] = {
    { -180, 180, 400 }
};
constexpr uint8_t SEQ_SPIN_LEFT_90_LEN = 1;

// Rotation 90° droite sur place
constexpr MoveStep SEQ_SPIN_RIGHT_90[] = {
    { 180, -180, 400 }
};
constexpr uint8_t SEQ_SPIN_RIGHT_90_LEN = 1;

// Rotation 360° sur place
constexpr MoveStep SEQ_SPIN_360[] = {
    { 180, -180, 1600 }
};
constexpr uint8_t SEQ_SPIN_360_LEN = 1;

// Avancer ~30 cm
constexpr MoveStep SEQ_FORWARD_30CM[] = {
    { 200, 200, 1000 }
};
constexpr uint8_t SEQ_FORWARD_30CM_LEN = 1;

// Reculer + tourner (récupération bord)
constexpr MoveStep SEQ_EVADE_BACK[] = {
    { -200, -200, 400 },   // reculer
    {  180, -180, 300 }    // pivoter à droite
};
constexpr uint8_t SEQ_EVADE_BACK_LEN = 2;

// Charge : accélération progressive puis pleine vitesse
constexpr MoveStep SEQ_CHARGE[] = {
    { 150, 150, 200 },   // montée en puissance
    { 255, 255, 800 }    // charge pleine vitesse
};
constexpr uint8_t SEQ_CHARGE_LEN = 2;

// ─────────────────────────────────────────────────────────────
//  Registre — pour accès par nom depuis CommandParser
// ─────────────────────────────────────────────────────────────

struct SequenceEntry {
    const char*      name;
    const MoveStep*  steps;
    uint8_t          length;
};

constexpr SequenceEntry SEQUENCE_REGISTRY[] = {
    { "SPIN_LEFT_90",  SEQ_SPIN_LEFT_90,  SEQ_SPIN_LEFT_90_LEN  },
    { "SPIN_RIGHT_90", SEQ_SPIN_RIGHT_90, SEQ_SPIN_RIGHT_90_LEN },
    { "SPIN_360",      SEQ_SPIN_360,      SEQ_SPIN_360_LEN      },
    { "FORWARD_30CM",  SEQ_FORWARD_30CM,  SEQ_FORWARD_30CM_LEN  },
    { "EVADE_BACK",    SEQ_EVADE_BACK,    SEQ_EVADE_BACK_LEN    },
    { "CHARGE",        SEQ_CHARGE,        SEQ_CHARGE_LEN        },
};
constexpr uint8_t SEQUENCE_REGISTRY_LEN =
    sizeof(SEQUENCE_REGISTRY) / sizeof(SEQUENCE_REGISTRY[0]);
