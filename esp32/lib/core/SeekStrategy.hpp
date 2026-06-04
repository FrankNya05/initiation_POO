#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  SeekStrategy — Approche l'objet le plus proche détecté par le lidar
//
//  États :
//  1. ATTACK : objet détecté dans l'arc avant (±70° autour de 10°)
//  2. SEARCH : rien détecté → machine d'états WAIT/ROTATE
//               WAIT   : robot immobile SEARCH_WAIT_MS (scan propre)
//               ROTATE : pivote de SEARCH_STEP_RAD (30°) via EKF θ
//              → couvre 360° en 12 pas sans distorsion lidar
// ═══════════════════════════════════════════════════════════════

class SeekStrategy : public StrategyInterface {
public:
    const char* name()  const override { return "Seek"; }
    LedColor    color() const override { return LedColor::YELLOW; }

    void reset() override {
        _searchPhase    = SearchPhase::WAIT;
        _searchDeadline = 0;
        _rotateStart    = 0.0f;
        _wasInAttack    = false;
    }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {
        using AC = RobotConstants::ActionCommand;

        SensorData lidar = ctx.getLidarData();

        if (lidar.isValid) {
            float dist  = lidar.value.vector.x;
            float angle = lidar.value.vector.y;

            if (dist >= MIN_DIST_M && dist <= MAX_DIST_M) {
                float diff = angle - FRONT_DEG;
                if (diff >  180.0f) diff -= 360.0f;
                if (diff < -180.0f) diff += 360.0f;

                if (fabsf(diff) <= FRONT_ARC_DEG) {
                    float diffNorm = constrain(diff / 90.0f, -1.0f, 1.0f);
                    int l = constrain((int)(SPEED_FWD * (1.0f + diffNorm)), 0, 255);
                    int r = constrain((int)(SPEED_FWD * (1.0f - diffNorm)), 0, 255);

                    if (fabsf(diffNorm) < 0.15f) {
                        auto encL = ctx.getEncoderData(SensorPosition::LEFT);
                        auto encR = ctx.getEncoderData(SensorPosition::RIGHT);
                        if (encL.isValid && encR.isValid) {
                            int corr = constrain((int)((encL.rpm - encR.rpm) * ENC_KP), -30, 30);
                            l = constrain(l - corr, 0, 255);
                            r = constrain(r + corr, 0, 255);
                        }
                    }

                    _wasInAttack = true;
                    ctx.setState(RobotConstants::State::ATTACK);
                    return AC{ l, r };
                }
            }
        }

        // ── SEARCH : rotation par pas de 30° ─────────────────────
        // Après perte de cible → attendre SEARCH_WAIT_MS (laisse le
        // temps au lidar de compléter un tour propre), puis pivoter
        // de SEARCH_STEP_RAD, répéter jusqu'à détection.
        if (_wasInAttack) {
            _wasInAttack    = false;
            _searchPhase    = SearchPhase::WAIT;
            _searchDeadline = millis() + SEARCH_WAIT_MS;
        }

        ctx.setState(RobotConstants::State::SEARCH);

        if (_searchPhase == SearchPhase::WAIT) {
            if (_searchDeadline == 0)
                _searchDeadline = millis() + SEARCH_WAIT_MS;

            if (millis() >= _searchDeadline) {
                _rotateStart = ctx.getPose().theta;
                _searchPhase = SearchPhase::ROTATE;
            }
            return AC{0, 0};
        }

        // ROTATE : pivote jusqu'à atteindre SEARCH_STEP_RAD
        float dTheta = ctx.getPose().theta - _rotateStart;
        while (dTheta >  (float)M_PI) dTheta -= 2.0f * (float)M_PI;
        while (dTheta < -(float)M_PI) dTheta += 2.0f * (float)M_PI;

        if (fabsf(dTheta) >= SEARCH_STEP_RAD) {
            _searchDeadline = millis() + SEARCH_WAIT_MS;
            _searchPhase    = SearchPhase::WAIT;
            return AC{0, 0};
        }

        return AC{SPEED_SEARCH, -SPEED_SEARCH};
    }

private:
    static constexpr float    FRONT_DEG        =  10.0f;
    static constexpr float    FRONT_ARC_DEG    =  70.0f;
    static constexpr float    MIN_DIST_M       =  0.15f;
    static constexpr float    MAX_DIST_M       =  0.8f;
    static constexpr int      SPEED_FWD        = 180;
    static constexpr float    ENC_KP           = 0.5f;

    static constexpr uint32_t SEARCH_WAIT_MS   = 500;
    static constexpr float    SEARCH_STEP_RAD  = (float)M_PI / 6.0f;  // 30°
    static constexpr int      SPEED_SEARCH     = 150;

    enum class SearchPhase : uint8_t { WAIT, ROTATE };

    SearchPhase _searchPhase    = SearchPhase::WAIT;
    uint32_t    _searchDeadline = 0;
    float       _rotateStart    = 0.0f;
    bool        _wasInAttack    = false;
};
