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
            if (b) { _evadeCmd = AC{  SPEED_MAX,    SPEED_MAX   }; }  // bord arrière → avance
            else   { _evadeCmd = AC{ -SPEED_EVADE, -SPEED_EVADE }; }  // bord avant → recul droit
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

        bool detL = tofFL.isValid && tofFL.value.scalar > TOF_MIN_MM && tofFL.value.scalar < TOF_DETECT_MM;
        bool detR = tofFR.isValid && tofFR.value.scalar > TOF_MIN_MM && tofFR.value.scalar < TOF_DETECT_MM;

        if (detL || detR) {
            ctx.setState(RobotConstants::State::ATTACK);
            _wasAttacking  = true;
            _searchStarted = false;  // reset 360° pour la prochaine recherche
            if (detL && detR) return AC{ SPEED_MAX, SPEED_MAX };
            if (detL)         return AC{ SPEED_TURN, SPEED_MAX };
            return                   AC{ SPEED_MAX, SPEED_TURN };
        }

        // ── 4. Cible perdue pendant ATTACK → stop + rotation ──
        if (_wasAttacking) {
            _wasAttacking  = false;
            _lostTargetMs  = now;   // mémoriser l'instant de perte
            _searchStarted = false;
        }

        // ── 5. Rien détecté → arrêt bref puis rotation 360° ──
        ctx.setState(RobotConstants::State::SEARCH);

        // Arrêt bref après perte de cible (laisser l'inertie se dissiper)
        if (now - _lostTargetMs < LOST_STOP_MS) {
            return AC{ 0, 0 };
        }

        // Mémoriser le theta de départ du scan
        if (!_searchStarted) {
            _searchStartTheta = ctx.getPose().theta;
            _searchStarted    = true;
        }

        // 360° complet sans détection → avancer légèrement et relancer
        EKF::Pose pose = ctx.getPose();
        float turned = pose.theta - _searchStartTheta;
        while (turned >  M_PI) turned -= 2.0f * M_PI;
        while (turned < -M_PI) turned += 2.0f * M_PI;
        if (fabsf(turned) >= FULL_TURN_RAD) {
            _searchStarted = false;
            return AC{ SPEED_SEARCH, SPEED_SEARCH };  // avance 1 cycle puis relance
        }

        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 450;

    // ── Perte de cible ────────────────────────────────────────
    bool     _wasAttacking = false;
    uint32_t _lostTargetMs = 0;
    static constexpr uint32_t LOST_STOP_MS = 150;  // arrêt 150ms après perte cible

    // ── Recherche 360° EKF ───────────────────────────────────
    bool  _searchStarted    = false;
    float _searchStartTheta = 0.0f;
    static constexpr float FULL_TURN_RAD = 1.9f * M_PI;  // ~342° (marge friction)

    // ── Seuils ────────────────────────────────────────────────
    static constexpr float TOF_MIN_MM          = 350.0f; // ignore les lectures < 350mm (sol/structure)
    static constexpr float TOF_DETECT_MM       = 600.0f; // portée de détection TOF (mm)
    static constexpr float IMPACT_THRESHOLD_G  =   3.0f; // seuil choc IMU (m/s² ≈ 3g)

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_MAX    = 255;  // charge pleine puissance
    static constexpr int SPEED_EVADE  = 220;  // récupération bord
    static constexpr int SPEED_TURN   = 100;  // correction de trajectoire
    static constexpr int SPEED_SEARCH = 200;  // rotation de recherche rapide
};
