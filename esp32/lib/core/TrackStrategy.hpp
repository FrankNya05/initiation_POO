#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  TrackStrategy — Fonce vers tout obstacle entre 100 mm et 700 mm
//
//  Lidar donne angle en degrés (0° = arrière robot, 180° = avant).
//
//  Confirmation adversaire (anti-faux-positifs) :
//  - Lidar doit détecter la cible CONFIRM_CYCLES fois consécutives
//    (3 cycles × 50 ms = 150 ms minimum de présence stable)
//  - Si dist < TOF_GATE_M, au moins un TOF avant doit aussi voir
//    quelque chose (< TOF_MAX_MM) pour valider la charge
//
//  Recherche intelligente (aucune cible détectée) :
//  - Robot immobile pendant SEARCH_WAIT_MS (3 s) → lidar scanne
//  - Si toujours rien → rotation 180° guidée par IMU (gx = lacet)
//  - Puis nouvelle attente de 3 s
//
//  Logique :
//  1. Bord détecté → évitement prioritaire (reset confirmation)
//  2. Obstacle dans [0.10 m – 0.70 m] :
//     - Confirmer N cycles → puis charger / virer
//     - Sinon → orienter en douceur sans charger
//  3. Rien → attente immobile 3 s, puis rotation 180° IMU
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
            _confirmCount = 0;
            _evadeUntil = now + EVADE_MIN_MS;
            if (b)        { _evadeCmd = AC{  SPEED_FWD,  SPEED_FWD  }; }
            else if (fl && fr) { _evadeCmd = AC{ -SPEED_FWD, -SPEED_FWD  }; }
            else if (fl)  { _evadeCmd = AC{ -SPEED_FWD, -SPEED_SLOW }; }
            else          { _evadeCmd = AC{ -SPEED_SLOW, -SPEED_FWD }; }
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 2. Lidar — confirmation adversaire ────────────────
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
                _spinAccumDeg    = 0.0f;

                // Incrémenter compteur de confirmation
                if (_confirmCount < CONFIRM_CYCLES) ++_confirmCount;

                // Gate TOF : si cible très proche, exiger confirmation TOF
                bool tofOk = true;
                if (dist < TOF_GATE_M) {
                    SensorData tofL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
                    SensorData tofR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);
                    bool tofLok = tofL.isValid && tofL.value.scalar < TOF_MAX_MM;
                    bool tofRok = tofR.isValid && tofR.value.scalar < TOF_MAX_MM;
                    tofOk = tofLok || tofRok;
                }

                // Adversaire confirmé → charger
                if (_confirmCount >= CONFIRM_CYCLES && tofOk) {
                    ctx.setState(RobotConstants::State::ATTACK);
                    if (fabsf(diff) <= ALIGN_DEG) {
                        return AC{ SPEED_FWD, SPEED_FWD };
                    }
                    if (diff > 0.0f) return AC{ SPEED_FWD,  SPEED_TURN };
                    return             AC{ SPEED_TURN, SPEED_FWD  };
                }

                // En cours de confirmation → s'orienter sans foncer
                ctx.setState(RobotConstants::State::SEARCH);
                if (fabsf(diff) <= ALIGN_DEG) return AC{ SPEED_APPROACH, SPEED_APPROACH };
                if (diff > 0.0f)              return AC{ SPEED_APPROACH, SPEED_TURN };
                return                               AC{ SPEED_TURN, SPEED_APPROACH };
            }
        }

        // ── 3. Rien → recherche intelligente ─────────────────
        _confirmCount = 0;
        ctx.setState(RobotConstants::State::SEARCH);

        // Initialiser le timer au premier cycle sans cible
        if (_searchWaitUntil == 0) _searchWaitUntil = now + SEARCH_WAIT_MS;

        // Phase A : attente immobile (lidar scanne sans vibrations)
        if (now < _searchWaitUntil) {
            return AC{ 0, 0 };
        }

        // Phase B : rotation 180° guidée par IMU (gx = lacet)
        if (_spinning) {
            SensorData imu = ctx.getIMUData();
            if (imu.isValid) {
                // Intégrer gx (°/s) × dt (s) = degrés tournés
                float dt = (now - _spinLastMs) / 1000.0f;
                _spinLastMs   = now;
                _spinAccumDeg += fabsf(imu.value.imu.gx) * dt;
            }
            if (_spinAccumDeg >= SPIN_TARGET_DEG) {
                // 180° atteint → arrêter et lancer une nouvelle attente
                _spinning        = false;
                _spinAccumDeg    = 0.0f;
                _searchWaitUntil = now + SEARCH_WAIT_MS;
                return AC{ 0, 0 };
            }
            return AC{ SPEED_SEARCH, -SPEED_SEARCH };  // tourner
        }

        // Attente expirée, pas encore en rotation → démarrer le spin
        _spinning     = true;
        _spinAccumDeg = 0.0f;
        _spinLastMs   = now;
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE verrouillé ─────────────────────────────────────
    uint32_t                       _evadeUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    static constexpr uint32_t      EVADE_MIN_MS = 350;  // ms minimum pour quitter le bord

    // ── Recherche intelligente ────────────────────────────────
    uint32_t _searchWaitUntil = 0;      // fin de la phase d'attente immobile
    bool     _spinning        = false;  // true = rotation 180° en cours
    float    _spinAccumDeg    = 0.0f;   // degrés accumulés depuis le début du spin
    uint32_t _spinLastMs      = 0;      // timestamp du dernier cycle (intégration gx)
    static constexpr uint32_t SEARCH_WAIT_MS  = 3000;  // attente immobile (ms)
    static constexpr float    SPIN_TARGET_DEG = 170.0f; // 170° ≈ 180° (marge friction)

    // ── Confirmation adversaire ───────────────────────────────
    int _confirmCount = 0;
    static constexpr int   CONFIRM_CYCLES = 3;      // 3 × 50 ms = 150 ms
    static constexpr float TOF_GATE_M     = 0.35f;  // en dessous, exiger TOF
    static constexpr float TOF_MAX_MM     = 400.0f; // TOF valide si < 400 mm

    // ── Distances ─────────────────────────────────────────────
    static constexpr float MIN_DIST_M  = 0.10f;  // 100 mm
    static constexpr float MAX_DIST_M  = 0.70f;  // 700 mm
    static constexpr float ALIGN_DEG   = 20.0f;  // tolérance alignement

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_FWD      = 255;  // charge confirmée
    static constexpr int SPEED_APPROACH = 120;  // approche pendant confirmation
    static constexpr int SPEED_TURN     =  80;  // roue intérieure virage
    static constexpr int SPEED_SLOW     = 100;  // évitement latéral
    static constexpr int SPEED_SEARCH   =  80;  // rotation surveillance
};
