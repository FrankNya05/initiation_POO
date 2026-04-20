#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  BerserkerStrategy.hpp — Stratégie offensive maximale
//
//  Philosophie : attaquer sans relâche, à pleine puissance
//
//  Priorités (décroissantes) :
//  1. ÉVADE   : bord détecté → récupération ultra-rapide (minimal)
//  2. IMPACT  : choc IMU > 3g → continuer à pousser pleine puissance
//  3. ATTAQUE : TOF < 20 cm → charge immédiate pleine puissance
//  4. ATTAQUE : Lidar détecte → fonce droit sur l'ennemi
//  5. SEARCH  : rien → rotation rapide pour trouver l'ennemi
//
//  Différences vs Adamantine :
//  - Impact détecté → maintenir la pression (ne jamais reculer)
//  - Jamais de recul latéral — toujours vers l'avant
//  - Seuil de détection plus large (1.20 m vs 0.90 m)
//  - Vitesse de rotation de recherche plus rapide
//  - Récupération bord plus agressive (recul court, repart vite)
//
//  Capteurs utilisés :
//  - LineSensors FRONT_LEFT / FRONT_RIGHT / BACK → bords
//  - IMU ax/ay                                  → détection d'impact
//  - TOF FRONT_LEFT / FRONT_RIGHT               → détection rapprochée
//  - Lidar FRONT                                → angle + distance ennemi
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
                // Choc ! On est en contact → pousser pleine puissance
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{ SPEED_MAX, SPEED_MAX };
            }
        }

        // ── 3. TOF — ennemi très proche → charge immédiate ────
        SensorData tofFL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
        SensorData tofFR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);

        bool tofCloseL = tofFL.isValid && tofFL.value.scalar < TOF_CLOSE_MM;
        bool tofCloseR = tofFR.isValid && tofFR.value.scalar < TOF_CLOSE_MM;

        if (tofCloseL || tofCloseR) {
            // Ennemi détecté de près → charge pleine puissance
            ctx.setState(RobotConstants::State::ATTACK);
            return AC{ SPEED_MAX, SPEED_MAX };
        }

        // ── 4. Lidar — traquer l'ennemi ───────────────────────
        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid && lidar.value.vector.x < LIDAR_DETECT_M) {
            ctx.setState(RobotConstants::State::ATTACK);

            // Lidar 0° = arrière robot, 180° = avant robot
            float diff = lidar.value.vector.y - 180.0f;
            if (diff >  180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;

            // Ennemi aligné devant → charge directe
            if (fabsf(diff) <= FRONT_ANGLE) {
                return AC{ SPEED_MAX, SPEED_MAX };
            }
            // Correction légère droite/gauche
            if (diff > 0.0f && diff <= SIDE_ANGLE) {
                return AC{ SPEED_MAX, SPEED_TURN };
            }
            if (diff < 0.0f && diff >= -SIDE_ANGLE) {
                return AC{ SPEED_TURN, SPEED_MAX };
            }
            // Ennemi très à droite/gauche → pivot agressif
            if (diff > 0.0f) return AC{  SPEED_MAX, -SPEED_PIVOT };
            return              AC{ -SPEED_PIVOT,  SPEED_MAX };
        }

        // ── 5. Rien détecté → rotation rapide pour chercher ───
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 250;  // Berserker : récupération plus courte

    // ── Seuils ────────────────────────────────────────────────
    static constexpr float TOF_CLOSE_MM   = 200.0f;  // TOF : charge immédiate (mm)
    static constexpr float LIDAR_DETECT_M =   1.20f; // Lidar : seuil de détection (m)
    static constexpr float IMPACT_THRESHOLD_G = 3.0f; // seuil choc IMU (m/s²  ≈ 3g)
    static constexpr float FRONT_ANGLE        = 15.0f; // alignement frontal (°)
    static constexpr float SIDE_ANGLE     =  60.0f;  // correction latérale (°)

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_MAX    = 255;  // charge pleine puissance
    static constexpr int SPEED_EVADE  = 220;  // récupération bord
    static constexpr int SPEED_TURN   = 140;  // correction de trajectoire
    static constexpr int SPEED_PIVOT  = 180;  // pivot pour réorientation
    static constexpr int SPEED_SEARCH = 200;  // rotation de recherche rapide
};
