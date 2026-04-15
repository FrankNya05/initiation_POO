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

    const char* name() const override { return "Berserker"; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        // ── 1. Bords — récupération ultra-rapide ──────────────
        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        if (b) {
            // Bord arrière → charger pleine puissance
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ SPEED_MAX, SPEED_MAX };
        }
        if (fl && fr) {
            // Les deux capteurs avant → reculer court et pivoter vite
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_EVADE, -SPEED_EVADE };
        }
        if (fl) {
            // Bord avant-gauche → reculer et pivoter droite agressivement
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ SPEED_MAX, -SPEED_EVADE };
        }
        if (fr) {
            // Bord avant-droit → reculer et pivoter gauche agressivement
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_EVADE, SPEED_MAX };
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

            float angle = lidar.value.vector.y;

            // Ennemi devant → charge directe
            if (angle <= FRONT_ANGLE || angle >= (360.0f - FRONT_ANGLE)) {
                return AC{ SPEED_MAX, SPEED_MAX };
            }
            // Ennemi légèrement à droite → correction légère
            if (angle < SIDE_ANGLE) {
                return AC{ SPEED_MAX, SPEED_TURN };
            }
            // Ennemi légèrement à gauche → correction légère
            if (angle > (360.0f - SIDE_ANGLE)) {
                return AC{ SPEED_TURN, SPEED_MAX };
            }
            // Ennemi très à droite → pivot agressif sur place
            if (angle < 180.0f) {
                return AC{ SPEED_MAX, -SPEED_PIVOT };
            }
            // Ennemi très à gauche → pivot agressif sur place
            return AC{ -SPEED_PIVOT, SPEED_MAX };
        }

        // ── 5. Rien détecté → rotation rapide pour chercher ───
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
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
