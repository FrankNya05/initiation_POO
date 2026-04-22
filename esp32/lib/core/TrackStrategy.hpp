#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  TrackStrategy — Fonce vers tout obstacle entre 100 mm et 670 mm
//
//  Lidar donne angle en degrés (0° = arrière robot, 180° = avant).
//
//  Logique :
//  1. Bord détecté → évitement prioritaire
//  2. Obstacle dans [0.10 m – 0.67 m] → charge pleine puissance
//     avec steering proportionnel (diff/180 normalisé)
//  3. Cible perdue → se retourner vers dernière position connue (EKF)
//  4. Rien → attente immobile 300 ms, puis rotation 180° EKF
// ═══════════════════════════════════════════════════════════════

class TrackStrategy : public StrategyInterface {
public:
    const char* name()  const override { return "Track"; }
    LedColor    color() const override { return LedColor::MAGENTA; }

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
            if (b)        { _evadeCmd = AC{  SPEED_FWD,  SPEED_FWD  }; }
            else if (fl && fr) { _evadeCmd = AC{ -SPEED_FWD, -SPEED_FWD  }; }
            else if (fl)  { _evadeCmd = AC{ -SPEED_FWD, -SPEED_SLOW }; }
            else          { _evadeCmd = AC{ -SPEED_SLOW, -SPEED_FWD }; }
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 2. Lidar — charge immédiate ───────────────────────
        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid) {
            float dist  = lidar.value.vector.x;
            float angle = lidar.value.vector.y;

            if (dist >= MIN_DIST_M && dist <= MAX_DIST_M) {
                // Lidar 0° = arrière robot, 180° = avant robot
                float diff = angle - 180.0f;
                if (diff >  180.0f) diff -= 360.0f;
                if (diff < -180.0f) diff += 360.0f;

                // Cible trouvée → reset timer de recherche
                _searchWaitUntil = 0;
                _spinning        = false;

                // Mémoriser la position globale de l'ennemi (EKF)
                EKF::Pose pose = ctx.getPose();
                float offsetRad = (angle - 180.0f) * (M_PI / 180.0f);
                _lastEnemyTheta = pose.theta + offsetRad;
                _hasLastEnemy   = true;

                // Charge pleine puissance avec steering proportionnel
                float diffNorm = constrain(diff / 180.0f, -1.0f, 1.0f);
                ctx.setState(RobotConstants::State::ATTACK);
                int l = constrain((int)(SPEED_FWD * (1.0f + diffNorm)), -255, 255);
                int r = constrain((int)(SPEED_FWD * (1.0f - diffNorm)), -255, 255);
                return AC{ l, r };
            }
        }

        // ── 3. Rien → recherche intelligente ─────────────────
        ctx.setState(RobotConstants::State::SEARCH);

        // 3a. Mémoire ennemi — se réorienter vers dernière position connue
        if (_hasLastEnemy) {
            EKF::Pose pose = ctx.getPose();
            float diff = _lastEnemyTheta - pose.theta;
            while (diff >  M_PI) diff -= 2.0f * M_PI;
            while (diff < -M_PI) diff += 2.0f * M_PI;
            if (fabsf(diff) > 0.25f) {
                return diff > 0 ? AC{ SPEED_SEARCH, -SPEED_SEARCH }
                                : AC{ -SPEED_SEARCH, SPEED_SEARCH };
            }
            _hasLastEnemy = false;  // aligné → reprendre recherche normale
        }

        // 3b. Initialiser le timer au premier cycle sans cible
        if (_searchWaitUntil == 0) _searchWaitUntil = now + SEARCH_WAIT_MS;

        // Phase A : attente immobile (lidar scanne sans vibrations)
        if (now < _searchWaitUntil) {
            return AC{ 0, 0 };
        }

        // Phase B : rotation 180° guidée par EKF theta (plus précis que IMU)
        if (_spinning) {
            EKF::Pose pose = ctx.getPose();
            float turned = pose.theta - _spinStartTheta;
            while (turned >  M_PI) turned -= 2.0f * M_PI;
            while (turned < -M_PI) turned += 2.0f * M_PI;
            if (fabsf(turned) >= SPIN_TARGET_RAD) {
                _spinning        = false;
                _searchWaitUntil = now + SEARCH_WAIT_MS;
                return AC{ 0, 0 };
            }
            return AC{ SPEED_SEARCH, -SPEED_SEARCH };
        }

        // Attente expirée → démarrer le spin, mémoriser theta de départ
        _spinning       = true;
        _spinStartTheta = ctx.getPose().theta;
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 350;  // ms minimum pour quitter le bord

    // ── Recherche intelligente ────────────────────────────────
    uint32_t _searchWaitUntil = 0;       // fin de la phase d'attente immobile
    bool     _spinning        = false;   // true = rotation 180° en cours
    float    _spinStartTheta  = 0.0f;   // theta EKF au début du spin
    static constexpr uint32_t SEARCH_WAIT_MS  = 300;                // attente immobile (ms)
    static constexpr float    SPIN_TARGET_RAD = 0.95f * M_PI;       // ~171° en radians

    // ── Mémoire dernière position ennemi ─────────────────────
    bool  _hasLastEnemy    = false;
    float _lastEnemyTheta  = 0.0f;  // angle global (rad, EKF) de la dernière détection

    // ── Distances ─────────────────────────────────────────────
    static constexpr float MIN_DIST_M  = 0.10f;  // 100 mm
    static constexpr float MAX_DIST_M  = 0.67f;  // 670 mm

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_FWD    = 255;  // charge pleine puissance
    static constexpr int SPEED_SLOW   = 100;  // évitement latéral
    static constexpr int SPEED_SEARCH =  80;  // rotation surveillance
};
