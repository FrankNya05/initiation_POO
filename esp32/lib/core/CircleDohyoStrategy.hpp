#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  CircleDohyoStrategy.hpp — Suivi du bord blanc du dohyo
//
//  Le robot tourne en sens anti-horaire (CCW) en gardant
//  le bord blanc sur sa DROITE (capteur FR = capteur de suivi).
//
//  Priorités (décroissantes) :
//  1. BORD ARRIÈRE (b) : avancer immédiatement — sans verrou,
//     priorité absolue pour ne pas quitter le ring par l'arrière
//  2. fl && fr (bord droit devant) : pivot gauche (CCW) pour
//     se réaligner le long du bord — jamais de recul
//  3. FL seul : trop à gauche → virer à droite
//  4. FR seul : capteur de suivi → corriger à gauche
//  5. Aucun capteur : courbe douce vers l'extérieur (droite)
//
//  Mesure du diamètre : après chaque tour complet (EKF θ ≥ 2π),
//  le diamètre estimé est imprimé sur Serial.
//
//  Déclenchement HMI : STRATEGY:CIRCLE
// ═══════════════════════════════════════════════════════════════

class CircleDohyoStrategy : public StrategyInterface {
public:
    const char* name()  const override { return "Circle"; }
    LedColor    color() const override { return LedColor::WHITE; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {
        using AC = RobotConstants::ActionCommand;

        // ── 0. EVADE lock — pivot fl&&fr (EKF θ) ──────────────
        // Angle sauvegardé au premier cycle après le déclenchement.
        if (_pivoting) {
            EKF::Pose pose = ctx.getPose();
            if (!_pivotAngleSaved) {
                _pivotStartTheta = pose.theta;
                _pivotAngleSaved = true;
            }
            float dTheta = pose.theta - _pivotStartTheta;
            while (dTheta >  (float)M_PI) dTheta -= 2.0f * (float)M_PI;
            while (dTheta < -(float)M_PI) dTheta += 2.0f * (float)M_PI;
            if (fabsf(dTheta) < EVADE_PIVOT_ANGLE) {
                ctx.setState(RobotConstants::State::EVADE);
                return _evadeCmd;
            }
            _pivoting        = false;
            _pivotAngleSaved = false;
        }

        // ── Lecture capteurs ──────────────────────────────────
        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        // ── Mesure du diamètre du cercle (EKF) ────────────────
        _measureCircle(ctx.getPose());

        // ── 1. Bord arrière → avancer (priorité absolue, sans verrou)
        // Pas de lock : le robot avance TANT QUE le capteur détecte,
        // ce qui garantit qu'il ne quitte pas le ring par l'arrière.
        if (b) {
            ctx.setState(RobotConstants::State::EVADE);
            return AC{ SPEED_NORMAL, SPEED_NORMAL };
        }

        // ── 2. fl && fr → pivot gauche (CCW) ─────────────────
        // Recul interdit : risque de déclencher le bord arrière.
        // Pivot CCW : ramène le robot face au bord en sens de suivi.
        if (fl && fr) {
            _pivoting        = true;
            _pivotAngleSaved = false;
            _evadeCmd        = AC{ -SPEED_NORMAL, SPEED_NORMAL };
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 3. FL seul → trop à gauche → virer à droite ──────
        if (fl) {
            ctx.setState(RobotConstants::State::SEARCH);
            return AC{ SPEED_NORMAL, SPEED_SLOW };
        }

        // ── 4. FR seul → capteur de suivi → corriger à gauche ─
        if (fr) {
            ctx.setState(RobotConstants::State::SEARCH);
            return AC{ SPEED_SLOW, SPEED_NORMAL };
        }

        // ── 5. Aucun capteur → courbe douce vers le bord (droite)
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_NORMAL, SPEED_CURVE };
    }

private:
    // ─────────────────────────────────────────────────────────
    //  Mesure du diamètre du cercle parcouru
    //
    //  Méthode :
    //  - Arc accumulé (somme des pas EKF √(dx²+dy²)) → longueur
    //    du trajet réel du centre du robot
    //  - Angle accumulé (somme |dθ| EKF) → rotation totale
    //  - À 2π (tour complet) : D = arc / π  (car arc = π × D)
    // ─────────────────────────────────────────────────────────
    void _measureCircle(const EKF::Pose& pose) {
        if (!_measuring) {
            _measuring  = true;
            _prevTheta  = pose.theta;
            _prevX      = pose.x;
            _prevY      = pose.y;
            _totalAngle = 0.0f;
            _totalDist  = 0.0f;
            return;
        }

        // Angle incrémental (wrappé → valeur absolue)
        float dTheta = pose.theta - _prevTheta;
        while (dTheta >  (float)M_PI) dTheta -= 2.0f * (float)M_PI;
        while (dTheta < -(float)M_PI) dTheta += 2.0f * (float)M_PI;
        _totalAngle += fabsf(dTheta);

        // Arc incrémental (déplacement euclidien EKF)
        float dx = pose.x - _prevX;
        float dy = pose.y - _prevY;
        _totalDist += sqrtf(dx * dx + dy * dy);

        _prevTheta = pose.theta;
        _prevX     = pose.x;
        _prevY     = pose.y;

        // Tour complet détecté → calculer et imprimer le diamètre
        if (_totalAngle >= 2.0f * (float)M_PI) {
            float diameter = _totalDist / (float)M_PI;
            Serial.printf(
                "[Circle] Tour complet — diametre estime : %.0f mm"
                "  (arc=%.0f mm, angle=%.2f rad)\n",
                diameter, _totalDist, _totalAngle);
            _totalAngle = 0.0f;
            _totalDist  = 0.0f;
        }
    }

    // ── EVADE lock — pivot fl&&fr (EKF θ) ────────────────────
    RobotConstants::ActionCommand  _evadeCmd        = {0, 0};
    bool                           _pivoting        = false;
    bool                           _pivotAngleSaved = false;
    float                          _pivotStartTheta = 0.0f;
    static constexpr float         EVADE_PIVOT_ANGLE = 50.0f * (float)M_PI / 180.0f;  // 50° CCW

    // ── Mesure du cercle ──────────────────────────────────────
    bool  _measuring   = false;
    float _prevTheta   = 0.0f;
    float _prevX       = 0.0f;
    float _prevY       = 0.0f;
    float _totalAngle  = 0.0f;  // rad absolu (non wrappé)
    float _totalDist   = 0.0f;  // mm (arc EKF cumulé)

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_NORMAL = 180;  // vitesse de croisière
    static constexpr int SPEED_SLOW   =  80;  // correction douce
    static constexpr int SPEED_CURVE  = 140;  // roue droite (intérieure) pour courbe CCW
};
