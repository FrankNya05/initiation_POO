#pragma once
#include <cmath>
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  SquareStrategy — Parcours automatique d'un carré 50cm × 50cm
//
//  Machine à états non-bloquante appelée à 20Hz par taskStrategy.
//    IDLE → FORWARD (435mm) → STOP → TURN (90° gauche) → × 4 → DONE
//
//  Robustesse aux perturbations :
//  - Progression mesurée par projection sur le cap idéal (pas distance
//    euclidienne) : un choc latéral n'interrompt pas prématurément le côté
//  - Timeout par côté (SIDE_TIMEOUT_MS) : évite le blocage sur obstacle
//  - Cible angulaire absolue (t0 + k×π/2) : les erreurs de virage ne
//    s'accumulent pas d'un côté à l'autre
// ═══════════════════════════════════════════════════════════════

class SquareStrategy : public StrategyInterface {
public:
    const char* name()  const override { return "Square"; }
    LedColor    color() const override { return LedColor::CYAN; }
    void reset() override {
        _phase = Phase::IDLE;
        _sideDeadline = 0;
    }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {
        using AC = RobotConstants::ActionCommand;
        ctx.setLidarEnabled(false);

        EKF::Pose pose = ctx.getPose();

        switch (_phase) {

            case Phase::IDLE: {
                _startX = pose.x;
                _startY = pose.y;
                _t0     = pose.theta;
                _side   = 0;
                ctx.setIdealHeading(_t0);
                _sideDeadline = millis() + SIDE_TIMEOUT_MS;
                _phase  = Phase::FORWARD;
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{ FWD_SPEED, FWD_SPEED };
            }

            case Phase::FORWARD: {
                float dx = pose.x - _startX;
                float dy = pose.y - _startY;

                // Progression le long du cap idéal (robuste aux chocs latéraux)
                float heading  = _norm(_t0 + _side * (float)M_PI / 2.0f);
                float progress = dx * cosf(heading) + dy * sinf(heading);

                bool targetReached = (progress >= SIDE_MM);
                bool timedOut      = (millis() > _sideDeadline);

                if (targetReached || timedOut) {
                    if (timedOut) LOGF("[Square] Cote %d timeout\n", _side);
                    _phase     = Phase::STOP_FWD;
                    _waitTicks = STOP_FWD_TICKS;
                    return AC{ 0, 0 };
                }
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{ FWD_SPEED, FWD_SPEED };
            }

            case Phase::STOP_FWD: {
                if (--_waitTicks <= 0) {
                    if (_side < 3) {
                        _turnTarget = _norm(_t0 + (_side + 1) * (float)M_PI / 2.0f);
                        _phase      = Phase::TURN;
                    } else {
                        _phase = Phase::DONE;
                    }
                }
                return AC{ 0, 0 };
            }

            case Phase::TURN: {
                float err    = _norm(_turnTarget - pose.theta);
                float errDeg = err * (180.0f / (float)M_PI);

                if (fabsf(errDeg) <= TURN_TOL_DEG) {
                    _phase     = Phase::STOP_TURN;
                    _waitTicks = STOP_TURN_TICKS;
                    return AC{ 0, 0 };
                }

                int spd = (fabsf(errDeg) > TURN_SLOW_DEG) ? TURN_FAST : TURN_SLOW;
                ctx.setState(RobotConstants::State::SEARCH);
                if (errDeg > 0.0f)
                    return AC{ -spd,  spd };   // gauche
                else
                    return AC{  spd, -spd };   // droite (correction dépassement)
            }

            case Phase::STOP_TURN: {
                if (--_waitTicks <= 0) {
                    _side++;
                    _startX = pose.x;
                    _startY = pose.y;
                    ctx.setIdealHeading(_norm(_t0 + _side * (float)M_PI / 2.0f));
                    _sideDeadline = millis() + SIDE_TIMEOUT_MS;
                    _phase  = Phase::FORWARD;
                }
                return AC{ 0, 0 };
            }

            case Phase::DONE:
            default:
                ctx.setState(RobotConstants::State::STANDBY);
                return AC{ 0, 0 };
        }
    }

private:
    enum class Phase : uint8_t { IDLE, FORWARD, STOP_FWD, TURN, STOP_TURN, DONE };

    Phase    _phase        = Phase::IDLE;
    int      _side         = 0;
    float    _startX       = 0.0f, _startY = 0.0f;
    float    _t0           = 0.0f;
    float    _turnTarget   = 0.0f;
    int      _waitTicks    = 0;
    uint32_t _sideDeadline = 0;

    // Paramètres calibrés (voir square_test.py)
    static constexpr float    SIDE_MM          = 435.0f;
    static constexpr int      FWD_SPEED        = 150;
    static constexpr int      TURN_FAST        = 60;
    static constexpr int      TURN_SLOW        = 25;
    static constexpr float    TURN_SLOW_DEG    = 20.0f;
    static constexpr float    TURN_TOL_DEG     = 1.5f;
    static constexpr int      STOP_FWD_TICKS   = 10;     // 0.5 s à 20 Hz
    static constexpr int      STOP_TURN_TICKS  = 8;      // 0.4 s à 20 Hz
    static constexpr uint32_t SIDE_TIMEOUT_MS  = 6000;   // 6s max par côté (blocage/choc)

    static float _norm(float a) {
        while (a >  (float)M_PI) a -= 2.0f * (float)M_PI;
        while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
        return a;
    }
};
