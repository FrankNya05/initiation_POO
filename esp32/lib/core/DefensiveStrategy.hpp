#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  DefensiveStrategy.hpp — Stratégie défensive / contre-attaque
//
//  Comportement (priorité décroissante) :
//  1. ÉVADE  : bord détecté → reculer + tourner (survie)
//  2. ATTACK : adversaire très proche (<40 cm) → contre-attaque
//  3. EVADE  : adversaire moyen (40–80 cm) → recul latéral
//  4. SEARCH : aucune cible → patrouille lente
//
//  Différence avec AggressiveStrategy :
//  - N'attaque que si l'adversaire est très proche
//  - Préfère reculer et repositionner plutôt que de charger
//  - Rotation de patrouille plus lente
// ═══════════════════════════════════════════════════════════════

class DefensiveStrategy : public StrategyInterface {
public:

    const char* name() const override { return "Defensive"; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        // ── 1. Priorité absolue : bords de piste ─────────────
        SensorData lineL = ctx.getLineData(SensorPosition::LEFT);
        SensorData lineR = ctx.getLineData(SensorPosition::RIGHT);
        SensorData lineB = ctx.getLineData(SensorPosition::BACK);

        if (lineL.isValid && lineL.value.scalar > 0.0f) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ 200, -200 };
        }
        if (lineR.isValid && lineR.value.scalar > 0.0f) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -200, 200 };
        }
        if (lineB.isValid && lineB.value.scalar > 0.0f) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ 255, 255 };
        }

        // ── 2. Vérification Lidar ─────────────────────────────
        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid) {
            float dist  = lidar.value.vector.x;
            float angle = lidar.value.vector.y;

            // Très proche → contre-attaque immédiate
            if (dist < CLOSE_DIST_M) {
                ctx.setState(RobotConstants::State::ATTACK);

                if (angle <= FRONT_ANGLE || angle >= (360.0f - FRONT_ANGLE)) {
                    return AC{ 255, 255 };
                }
                if (angle < 180.0f) {
                    return AC{ 255, 120 };
                }
                return AC{ 120, 255 };
            }

            // Distance moyenne → recul + réorientation
            if (dist < MID_DIST_M) {
                ctx.setState(RobotConstants::State::EVADE);
                // Reculer en pivotant pour mettre l'adversaire de côté
                if (angle < 180.0f) {
                    return AC{ -150, -80 };  // recul en virant à gauche
                }
                return AC{ -80, -150 };      // recul en virant à droite
            }
        }

        // ── 3. Aucune menace → patrouille lente ──────────────
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ PATROL_SPEED, -PATROL_SPEED };
    }

private:
    static constexpr float CLOSE_DIST_M  = 0.40f;  // seuil contre-attaque (m)
    static constexpr float MID_DIST_M    = 0.80f;  // seuil recul (m)
    static constexpr float FRONT_ANGLE   = 15.0f;  // tolérance alignement (°)
    static constexpr int   PATROL_SPEED  = 120;    // vitesse patrouille
};
