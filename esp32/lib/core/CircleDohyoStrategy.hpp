#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  CircleDohyoStrategy.hpp — Suivi du bord blanc du dohyo
//
//  Comportement (boucle fermée sur LineSensors) :
//
//  1. ÉVADE  : les deux capteurs avant détectent la ligne
//              → reculer pour rentrer dans le ring
//  2. AJUSTE : un seul capteur avant détecte la ligne
//              → tourner pour rester tangent au bord
//  3. CHERCHE: aucun capteur ne voit la ligne
//              → avancer en courbe douce vers l'extérieur
//
//  Résultat : le robot tourne en cercle sur le bord du dohyo
//  Déclenchement HMI : STRATEGY:CIRCLE
// ═══════════════════════════════════════════════════════════════

class CircleDohyoStrategy : public StrategyInterface {
public:

    const char* name() const override { return "Circle"; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        // ── Bord arrière → avancer fort pour rentrer dans le ring ─
        if (b) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ SPEED_NORMAL, SPEED_NORMAL };
        }

        // ── Les deux capteurs avant voient la ligne ────────────────
        // Robot trop vers l'extérieur → reculer et pivoter vers l'intérieur
        if (fl && fr) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ -SPEED_SLOW, -SPEED_SLOW };
        }

        // ── Capteur avant-gauche seul → trop à gauche ─────────────
        // Virer à droite pour rester tangent
        if (fl && !fr) {
            ctx.setState(RobotConstants::State::SEARCH);
            return AC{ SPEED_NORMAL, SPEED_SLOW };
        }

        // ── Capteur avant-droit seul → trop à droite ──────────────
        // Virer à gauche pour rester tangent
        if (!fl && fr) {
            ctx.setState(RobotConstants::State::SEARCH);
            return AC{ SPEED_SLOW, SPEED_NORMAL };
        }

        // ── Aucun capteur → avancer en courbe vers l'extérieur ────
        // Courbe douce dans le sens de rotation choisi (ici : horaire)
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_NORMAL, SPEED_CURVE };
    }

private:
    // ── Vitesses configurables ─────────────────────────────────
    static constexpr int SPEED_NORMAL = 180;  // vitesse de croisière
    static constexpr int SPEED_SLOW   =  80;  // vitesse de correction
    static constexpr int SPEED_CURVE  = 140;  // roue intérieure pour la courbe
};
