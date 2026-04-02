#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  AggressiveStrategy.hpp — Stratégie offensive
//
//  Comportement (priorité décroissante) :
//  1. ÉVADE  : bord détecté → reculer + tourner (survie)
//  2. ATTACK : adversaire détecté (<80 cm) → foncer dessus
//  3. SEARCH : aucune cible → rotation pour chercher
//
//  Seuils :
//  - Détection adversaire  : < ENEMY_DIST_M (0.80 m)
//  - Alignement front      : angle ±15°
//  - Vitesse attaque       : 255 (maximum)
//  - Vitesse recherche     : 180 (rotation lente)
// ═══════════════════════════════════════════════════════════════

class AggressiveStrategy : public StrategyInterface {
public:

    const char* name() const override { return "Aggressive"; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        // ── 1. Priorité absolue : bords de piste ─────────────
        //  Si un capteur de ligne détecte le bord blanc → manœuvre immédiate
        SensorData lineL = ctx.getLineData(SensorPosition::LEFT);
        SensorData lineR = ctx.getLineData(SensorPosition::RIGHT);
        SensorData lineB = ctx.getLineData(SensorPosition::BACK);

        if (lineL.isValid && lineL.value.scalar > 0.0f) {
            // Bord gauche → reculer en virant à droite
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ 200, -200 };
        }
        if (lineR.isValid && lineR.value.scalar > 0.0f) {
            // Bord droit → reculer en virant à gauche
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -200, 200 };
        }
        if (lineB.isValid && lineB.value.scalar > 0.0f) {
            // Bord arrière → avancer fort
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ 255, 255 };
        }

        // ── 2. Adversaire détecté par le Lidar ───────────────
        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid && lidar.value.vector.x < ENEMY_DIST_M) {
            ctx.setState(RobotConstants::State::ATTACK);

            float angle = lidar.value.vector.y;  // 0–360°

            // Adversaire devant (±15°) → charge directe
            if (angle <= FRONT_ANGLE || angle >= (360.0f - FRONT_ANGLE)) {
                return AC{ 255, 255 };
            }
            // Adversaire à droite → virer à droite
            if (angle < 180.0f) {
                return AC{ 255, 120 };
            }
            // Adversaire à gauche → virer à gauche
            return AC{ 120, 255 };
        }

        // ── 3. Aucune cible → rotation de recherche ──────────
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SEARCH_SPEED, -SEARCH_SPEED };  // rotation horaire
    }

private:
    // ── Seuils configurables ──────────────────────────────────
    static constexpr float ENEMY_DIST_M  = 0.80f;  // distance détection (m)
    static constexpr float FRONT_ANGLE   = 15.0f;  // tolérance alignement (°)
    static constexpr int   SEARCH_SPEED  = 180;    // vitesse rotation recherche
};
