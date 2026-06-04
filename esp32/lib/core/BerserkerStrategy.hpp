#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  BerserkerStrategy.hpp — Stratégie push-out TOF
//
//  Priorités (décroissantes) :
//  1. ÉVADE   : bord détecté → 2 phases :
//                 Phase 1 (EVADE_BACKUP_MS) : recul droit (temps)
//                 Phase 2 (EVADE_PIVOT_ANGLE EKF) : demi-tour 180°
//               Exception : bord arrière → avancer (pas de pivot)
//  2. IMPACT  : choc IMU ay/az > seuil → pousser sans relâche
//  3. TOF     : adversaire [350–600 mm] → charge avec correction
//  4. TRACK   : TOF vient de perdre l'adversaire → rotation vers
//                 le dernier côté vu (TRACK_TARGET_ANGLE EKF)
//  5. SEARCH  : rien → rotation 360° sur place (scan TOF continu)
//
//  Toutes les consignes de rotation s'appuient sur EKF θ.
//  Seul le recul linéaire (EVADE_BACKUP_MS) reste temporel.
// ═══════════════════════════════════════════════════════════════

class BerserkerStrategy : public StrategyInterface {
public:

    const char* name()  const override { return "Berserker"; }
    LedColor    color() const override { return LedColor::MAGENTA; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {

        using AC = RobotConstants::ActionCommand;

        uint32_t now = millis();

        // ── 0. EVADE phase 1 : recul linéaire (temps) ─────────
        if (now < _evadeUntil) {
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── 0b. EVADE phase 2 : demi-tour (EKF θ) ─────────────
        // Angle sauvegardé au premier cycle après la fin du recul.
        if (_pivoting) {
            if (!_pivotAngleSaved) {
                _pivotStartTheta = ctx.getPose().theta;
                _pivotAngleSaved = true;
            }
            float dTheta = ctx.getPose().theta - _pivotStartTheta;
            while (dTheta >  (float)M_PI) dTheta -= 2.0f * (float)M_PI;
            while (dTheta < -(float)M_PI) dTheta += 2.0f * (float)M_PI;
            if (fabsf(dTheta) < EVADE_PIVOT_ANGLE) {
                ctx.setState(RobotConstants::State::EVADE);
                return _pivotCmd;
            }
            _pivoting        = false;
            _pivotAngleSaved = false;
            _graceUntil      = millis() + SEARCH_GRACE_MS;
        }

        // ── 1. Bords — récupération en 2 phases ───────────────
        SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
        SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
        SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

        bool fl = lineFL.isValid && lineFL.value.scalar > 0.0f;
        bool fr = lineFR.isValid && lineFR.value.scalar > 0.0f;
        bool b  = lineB.isValid  && lineB.value.scalar  > 0.0f;

        if (b || fl || fr) {
            _tofDetCount     = 0;
            _tracking        = false;
            _trackAngleSaved = false;
            if (b) {
                _evadeUntil      = now + EVADE_BACKUP_MS;
                _pivoting        = false;
                _pivotAngleSaved = false;
                _graceUntil      = now + EVADE_BACKUP_MS + SEARCH_GRACE_MS;
                _evadeCmd        = AC{ SPEED_MAX, SPEED_MAX };
            } else {
                _evadeUntil      = now + EVADE_BACKUP_MS;
                _pivoting        = true;
                _pivotAngleSaved = false;
                _evadeCmd        = AC{ -SPEED_EVADE, -SPEED_EVADE };
                _pivotCmd = (fl && !fr) ? AC{  SPEED_EVADE, -SPEED_EVADE }
                                        : AC{ -SPEED_EVADE,  SPEED_EVADE };
            }
            ctx.setState(RobotConstants::State::EVADE);
            return _evadeCmd;
        }

        // ── Grace period post-EVADE ────────────────────────────
        // Bloque IMU/TOF pendant SEARCH_GRACE_MS après le pivot.
        bool inGrace = (now < _graceUntil);

        // ── 2. Impact IMU — contact → pousser sans relâche ────
        if (!inGrace) {
            SensorData imu = ctx.getIMUData();
            if (imu.isValid) {
                float impact = sqrtf(imu.value.imu.ay * imu.value.imu.ay
                                   + imu.value.imu.az * imu.value.imu.az);
                if (impact > IMPACT_THRESHOLD_G) {
                    ctx.setState(RobotConstants::State::ATTACK);
                    return AC{ SPEED_MAX, SPEED_MAX };
                }
            }
        }

        // ── 3. TOF — détection avec hysteresis (2 frames) ─────
        if (!inGrace) {
            SensorData tofFL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
            SensorData tofFR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);

            bool detL = tofFL.isValid && tofFL.value.scalar > TOF_MIN_MM && tofFL.value.scalar < TOF_DETECT_MM;
            bool detR = tofFR.isValid && tofFR.value.scalar > TOF_MIN_MM && tofFR.value.scalar < TOF_DETECT_MM;

            if (detL || detR) {
                if (detL && !detR) _lastDetSide = 1;
                else if (detR && !detL) _lastDetSide = 2;
                _tracking        = false;
                _trackAngleSaved = false;
                _tofDetCount++;
                if (_tofDetCount >= TOF_MIN_FRAMES) {
                    ctx.setState(RobotConstants::State::ATTACK);
                    if (detL && detR) return AC{ SPEED_MAX,  SPEED_MAX  };
                    if (detL)         return AC{ SPEED_TURN, SPEED_MAX  };
                    return                   AC{ SPEED_MAX,  SPEED_TURN };
                }
            } else {
                if (_tofDetCount > 0 && !_tracking) {
                    _tracking        = true;
                    _trackAngleSaved = false;
                }
                _tofDetCount = 0;
            }
        }

        // ── 4. Recherche ──────────────────────────────────────
        ctx.setState(RobotConstants::State::SEARCH);

        // Rattrapage : rotation vers le dernier côté vu (EKF θ)
        if (_tracking) {
            if (!_trackAngleSaved) {
                _trackStartTheta = ctx.getPose().theta;
                _trackAngleSaved = true;
            }
            float dTheta = ctx.getPose().theta - _trackStartTheta;
            while (dTheta >  (float)M_PI) dTheta -= 2.0f * (float)M_PI;
            while (dTheta < -(float)M_PI) dTheta += 2.0f * (float)M_PI;
            if (fabsf(dTheta) < TRACK_TARGET_ANGLE) {
                return (_lastDetSide == 1) ? AC{ -SPEED_TRACK,  SPEED_TRACK }
                                           : AC{  SPEED_TRACK, -SPEED_TRACK };
            }
            _tracking        = false;
            _trackAngleSaved = false;
        }

        // Scan 360° sur place — TOF lit à chaque cycle de 50 ms
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    // ── EVADE phase 1 (recul linéaire, temps) ────────────────
    uint32_t                       _evadeUntil = 0;
    uint32_t                       _graceUntil = 0;
    RobotConstants::ActionCommand  _evadeCmd   = {0, 0};
    RobotConstants::ActionCommand  _pivotCmd   = {0, 0};
    static constexpr uint32_t      EVADE_BACKUP_MS = 220;   // recul ~31 mm à SPEED_EVADE
    static constexpr uint32_t      SEARCH_GRACE_MS = 400;   // stabilisation post-pivot

    // ── EVADE phase 2 — pivot 180° (EKF θ) ───────────────────
    bool  _pivoting        = false;
    bool  _pivotAngleSaved = false;
    float _pivotStartTheta = 0.0f;
    static constexpr float EVADE_PIVOT_ANGLE = (float)M_PI;         // 180°

    // ── Rattrapage EKF θ (après perte détection TOF) ─────────
    bool    _tracking        = false;
    bool    _trackAngleSaved = false;
    float   _trackStartTheta = 0.0f;
    uint8_t _lastDetSide     = 2;   // 1=gauche, 2=droite (défaut droite)
    static constexpr float TRACK_TARGET_ANGLE = (float)M_PI / 4.0f; // 45°

    // ── Hysterèse TOF ─────────────────────────────────────────
    uint8_t                        _tofDetCount  = 0;
    static constexpr uint8_t       TOF_MIN_FRAMES = 2;  // 2 × 50 ms = 100 ms

    // ── Seuils ────────────────────────────────────────────────
    static constexpr float TOF_MIN_MM         = 350.0f;
    static constexpr float TOF_DETECT_MM      = 600.0f;
    static constexpr float IMPACT_THRESHOLD_G =   3.0f;

    // ── Vitesses ──────────────────────────────────────────────
    static constexpr int SPEED_MAX    = 255;
    static constexpr int SPEED_EVADE  = 220;
    static constexpr int SPEED_TURN   = 150;  // correction cap pendant la charge
    static constexpr int SPEED_TRACK  = 120;  // rotation rattrapage
    static constexpr int SPEED_SEARCH = 110;  // scan 360° sur place
};
