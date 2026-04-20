#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  BerserkerStrategy.hpp — Stratégie offensive maximale (TOF only)
//
//  Philosophie : attaquer sans relâche, à pleine puissance
//
//  Priorités (décroissantes) :
//  1. ÉVADE   : bord détecté → récupération ultra-rapide
//  2. IMPACT  : choc IMU > 3g → continuer à pousser pleine puissance
//  3. ATTAQUE : TOF détecte ennemi (<500mm) → charge avec correction
//  4. SEARCH  : rien → rotation rapide pour trouver l'ennemi
//
//  Capteurs utilisés :
//  - LineSensors FRONT_LEFT / FRONT_RIGHT / BACK → bords
//  - IMU ax/ay                                  → détection d'impact
//  - TOF FRONT_LEFT / FRONT_RIGHT               → détection + correction
//  (Lidar non utilisé dans cette stratégie)
// ═══════════════════════════════════════════════════════════════

class BerserkerStrategy : public StrategyInterface {
public:

    const char* name()  const override { return "Berserker"; }
    LedColor    color() const override { return LedColor::MAGENTA; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        // ── 0. EVADE verrouillé — durée minimale garantie ─────
        uint32_t now = millis();
        if (now < _evadeUntil) {
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 1. Bords — récupération ultra-rapide ──────────────
        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        if (b || fl || fr) {
            _evadeUntil = now + EVADE_MIN_MS;
            if (b)             { _evadeCmd = AC{  SPEED_MAX,   SPEED_MAX   }; }
            else if (fl && fr) { _evadeCmd = AC{ -SPEED_EVADE, -SPEED_EVADE }; }
            else if (fl)       { _evadeCmd = AC{  SPEED_MAX,   -SPEED_EVADE }; }
            else               { _evadeCmd = AC{ -SPEED_EVADE,  SPEED_MAX   }; }
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 2. Impact IMU — choc détecté → maintenir la pression
        SensorData imu = ctx.getIMUData();
        if (imu.isValid) {
            float impact = sqrtf(imu.value.imu.ax * imu.value.imu.ax
                               + imu.value.imu.ay * imu.value.imu.ay);
            if (impact > IMPACT_THRESHOLD_G) {
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{ SPEED_MAX, SPEED_MAX };
            }
        }

        // ── 3. TOF — détection et charge avec correction ──────
        SensorData tofFL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
        SensorData tofFR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);

        bool detL = tofFL.isValid && tofFL.value.scalar > 0.0f && tofFL.value.scalar < TOF_DETECT_MM;
        bool detR = tofFR.isValid && tofFR.value.scalar > 0.0f && tofFR.value.scalar < TOF_DETECT_MM;

        if (detL || detR) {
            ctx.setState(RobotConstants::State::ATTACK);
            if (detL && detR) {
                // Ennemi centré → charge pleine puissance
                return AC{ SPEED_MAX, SPEED_MAX };
            }
            if (detL) {
                // Ennemi à gauche → corriger vers gauche
                return AC{ SPEED_TURN, SPEED_MAX };
            }
            // Ennemi à droite → corriger vers droite
            return AC{ SPEED_MAX, SPEED_TURN };
        }

        // ── 4. Rien détecté → rotation rapide pour chercher ───
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 250;

    // ── Seuils ────────────────────────────────────────────────
    static constexpr float TOF_DETECT_MM       = 500.0f; // portée de détection TOF (mm)
    static constexpr float IMPACT_THRESHOLD_G  =   3.0f; // seuil choc IMU (m/s² ≈ 3g)

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_MAX    = 255;  // charge pleine puissance
    static constexpr int SPEED_EVADE  = 220;  // récupération bord
    static constexpr int SPEED_TURN   = 100;  // correction de trajectoire
    static constexpr int SPEED_SEARCH = 200;  // rotation de recherche rapide
};
