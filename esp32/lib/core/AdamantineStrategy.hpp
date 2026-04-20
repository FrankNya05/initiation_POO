#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  AdamantineStrategy.hpp — Stratégie défensive
//
//  Philosophie : survivre d'abord, contre-attaquer ensuite
//
//  Priorités (décroissantes) :
//  1. ÉVADE   : bord détecté → récupération immédiate
//  2. IMPACT  : choc IMU > 2g + robot repoussé → reculer et repositionner
//  3. RECUL   : ennemi très proche (TOF < 15 cm) → reculer + pivoter
//  4. ATTAQUE : ennemi aligné et proche (Lidar < 50 cm) → contre-attaque
//  5. RECUL   : ennemi détecté (Lidar 50–90 cm) → recul latéral
//  6. SEARCH  : rien détecté → rotation lente de surveillance
//
//  Capteurs utilisés :
//  - LineSensors FRONT_LEFT / FRONT_RIGHT / BACK → bords
//  - IMU ax/ay                                  → détection d'impact
//  - TOF FRONT_LEFT / FRONT_RIGHT               → détection rapprochée
//  - Lidar FRONT                                → distance + angle ennemi
// ═══════════════════════════════════════════════════════════════

class AdamantineStrategy : public StrategyInterface {
public:

    const char* name()  const override { return "Adamantine"; }
    LedColor    color() const override { return LedColor::CYAN; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        // ── 0. EVADE verrouillé — durée minimale garantie ─────
        uint32_t now = millis();
        if (now < _evadeUntil) {
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 1. Bords — priorité absolue ───────────────────────
        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        if (b || fl || fr) {
            _evadeUntil = now + EVADE_MIN_MS;
            if (b)             { _evadeCmd = AC{  SPEED_CHARGE,  SPEED_CHARGE }; }
            else if (fl && fr) { _evadeCmd = AC{ -SPEED_EVADE,  -SPEED_EVADE  }; }
            else if (fl)       { _evadeCmd = AC{ -SPEED_EVADE,  -SPEED_SLOW   }; }
            else               { _evadeCmd = AC{ -SPEED_SLOW,   -SPEED_EVADE  }; }
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 2. Impact IMU — choc détecté → repositionnement ───
        SensorData imu = ctx.getIMUData();
        if (imu.isValid) {
            float impact = sqrtf(imu.value.imu.ax * imu.value.imu.ax
                               + imu.value.imu.ay * imu.value.imu.ay);
            if (impact > IMPACT_THRESHOLD_G) {
                // Choc ! Le robot est en train d'être repoussé
                // → reculer et pivoter pour changer d'angle d'approche
                ctx.setState(RobotConstants::State::EVADE);
                // Pivoter du côté opposé à la force latérale
                if (imu.value.imu.ay > 0.0f) {
                    return AC{ -SPEED_EVADE, -SPEED_SLOW };  // recul virant gauche
                }
                return AC{ -SPEED_SLOW, -SPEED_EVADE };      // recul virant droite
            }
        }

        // ── 3. TOF — ennemi très proche (< 15 cm) ─────────────
        // Réaction rapide avant que le Lidar puisse traiter
        SensorData tofFL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
        SensorData tofFR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);

        bool tofCloseL = tofFL.isValid && tofFL.value.scalar < TOF_CLOSE_MM;
        bool tofCloseR = tofFR.isValid && tofFR.value.scalar < TOF_CLOSE_MM;

        // ── 4. (anciennement 2.) TOF
        if (tofCloseL && tofCloseR) {
            // Ennemi droit devant très près → reculer puis pivoter
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_EVADE, -SPEED_EVADE };
        }
        if (tofCloseL) {
            // Ennemi très proche à gauche → reculer en virant à droite
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_SLOW, -SPEED_EVADE };
        }
        if (tofCloseR) {
            // Ennemi très proche à droite → reculer en virant à gauche
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_EVADE, -SPEED_SLOW };
        }

        // ── 3. Lidar — analyse de la distance ennemi ──────────
        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid) {
            float dist  = lidar.value.vector.x;
            float angle = lidar.value.vector.y;

            // Contre-attaque si ennemi aligné et proche
            if (dist < LIDAR_CLOSE_M) {
                ctx.setState(RobotConstants::State::ATTACK);

                // Lidar 0° = arrière robot, 180° = avant robot
                float diff = angle - 180.0f;
                if (diff >  180.0f) diff -= 360.0f;
                if (diff < -180.0f) diff += 360.0f;

                if (fabsf(diff) <= FRONT_ANGLE) {
                    return AC{ SPEED_CHARGE, SPEED_CHARGE };  // aligné → fonce
                }
                if (diff > 0.0f) {
                    return AC{ SPEED_CHARGE, SPEED_TURN };    // droite
                }
                return AC{ SPEED_TURN, SPEED_CHARGE };        // gauche
            }

            // Ennemi à distance moyenne → recul latéral pour esquiver
            if (dist < LIDAR_MID_M) {
                ctx.setState(RobotConstants::State::EVADE);
                if (angle < 180.0f) {
                    return AC{ -SPEED_SLOW, -SPEED_TURN };  // recul virant gauche
                }
                return AC{ -SPEED_TURN, -SPEED_SLOW };      // recul virant droite
            }
        }

        // ── 4. Rien détecté → rotation lente de surveillance ──
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_PATROL, -SPEED_PATROL };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 350;

    // ── Seuils ────────────────────────────────────────────────
    static constexpr float IMPACT_THRESHOLD_G = 2.0f;  // seuil choc IMU (m/s² ≈ 2g)
    static constexpr float TOF_CLOSE_MM      = 150.0f; // TOF : très proche (mm)
    static constexpr float LIDAR_CLOSE_M =   0.35f; // Lidar : contre-attaque (m) — dohyo ∅0.7 m
    static constexpr float LIDAR_MID_M   =   0.65f; // Lidar : orienter vers ennemi (m)
    static constexpr float FRONT_ANGLE   =  20.0f;  // tolérance alignement (°)

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_CHARGE =  255;  // contre-attaque pleine puissance
    static constexpr int SPEED_EVADE  =  200;  // récupération bord
    static constexpr int SPEED_TURN   =  120;  // virage correction
    static constexpr int SPEED_SLOW   =   80;  // correction douce
    static constexpr int SPEED_PATROL =  100;  // rotation surveillance
};
