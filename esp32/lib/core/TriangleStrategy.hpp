#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"
#include <cmath>
class TriangleStrategy: public StrategyInterface
{
private:
    enum class Phase : uint8_t { IDLE, FORWARD, STOP_FWD, TURN, STOP_TURN, ORIENT_HOME, RETURN_HOME, DONE };
    Phase   _phase         = Phase::IDLE ; 
    int     _side          = 0;
    float   _startX        = 0.0f, _startY    = 0.0f;
    float  _t0             = 0.0f;
    float  _turnTarget     = 0.0f;
    int    _waitTicks      = 0;
    uint32_t _sideDeatline  = 0;

    static constexpr float    SIDE_MM          = 509.0f;
    static constexpr int      FWD_SPEED        = 150;
    static constexpr int      TURN_FAST        = 60;
    static constexpr int      TURN_SLOW        = 28;
    static constexpr float    TURN_SLOW_DEG    = 20.0f;
    static constexpr float    TURN_TOL_DEG     = 1.0f;
    static constexpr int      STOP_FWD_TICKS   = 10;     // 0.5 s à 20 Hz
    static constexpr int      STOP_TURN_TICKS  = 8;      // 0.4 s à 20 Hz
    static constexpr uint32_t SIDE_TIMEOUT_MS  = 6000;   // 6s max par côté (blocage/choc)

    static float _norm(float a) {
        while (a >  (float)M_PI) a -= 2.0f * (float)M_PI;
        while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
        return a;
    }

public:
    const char* name()  const override { return "Triangle"; }
    LedColor    color() const override { return LedColor::OFF;}
    void reset() override {
    _phase = Phase::IDLE;
    _sideDeatline = 0;
    }
    
    RobotConstants::ActionCommand execute( RobotContext &ctx) override{
        using AC = RobotConstants::ActionCommand;
        EKF::Pose pose = ctx.getPose();
        
        switch (_phase)
        {
            case Phase::IDLE:{
                _startX = pose.x;
                _startY = pose.y;
                _t0     = pose.theta;
                _side   = 0;
                ctx.setIdealHeading(_t0);
                _sideDeatline = millis() + SIDE_TIMEOUT_MS;
                _phase = Phase::FORWARD;
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{FWD_SPEED, FWD_SPEED};
            }
            case Phase::FORWARD:{
                float dx = pose.x - _startX;
                float dy = pose.y - _startY;
                float heading = _norm(_t0 + _side * (2.0f * (float)M_PI / 3.0f));  // 120°
                float progress = dx * cosf(heading) + dy * sinf(heading);
                bool targetReached = (progress >= SIDE_MM);
                bool timedOut      = (millis() > _sideDeatline);

                if(targetReached || timedOut){
                    if (timedOut) LOGF("[Traingle] Cote %d timeout\n", _side);
                    _phase     = Phase::STOP_FWD;
                    _waitTicks = STOP_FWD_TICKS;
                    return AC{ 0, 0 };
                }
                ctx.setState(RobotConstants::State::ATTACK);
                return AC{ FWD_SPEED, FWD_SPEED };
            }

            case Phase::STOP_FWD: {
                if (--_waitTicks <= 0) {
                    if (_side < 2) {
                        _turnTarget = _norm(_t0 + (_side + 1) * (2.0f * (float)M_PI / 3.0f));
                        _phase      = Phase::TURN;
                    } else {
                        _turnTarget = atan2f(-pose.y, -pose.x);
                        _phase      = Phase::ORIENT_HOME;
                    }
                }
                return AC{ 0, 0 };
            }
            case Phase::TURN:{
                float err = _norm(_turnTarget - pose.theta);
                float errDrg =  err * (180.0f/(float)M_PI);
                if(fabsf(errDrg) <= TURN_TOL_DEG){
                    _phase = Phase::STOP_TURN;
                    _waitTicks = STOP_TURN_TICKS;
                    return AC{ 0, 0 };
                }
                int spd = (fabsf(errDrg) > TURN_SLOW_DEG) ? TURN_FAST : TURN_SLOW;
                ctx.setState(RobotConstants::State::SEARCH);
                 if (errDrg > 0.0f)
                    return AC{ -spd,  spd };   // gauche
                else
                    return AC{  spd, -spd };   // droite (correction dépassement)
            }

            case Phase::STOP_TURN: {
                if (--_waitTicks <= 0) {
                    _side++;
                    _startX = pose.x;
                    _startY = pose.y;
                    ctx.setIdealHeading(_norm(_t0 + _side * ((float) M_PI - (float)M_PI / 3.0f)));
                    _sideDeatline = millis() + SIDE_TIMEOUT_MS;
                    _phase  = Phase::FORWARD;
                }
                return AC{ 0, 0 };
            }

            case Phase::ORIENT_HOME: {
                float err    = _norm(_turnTarget - pose.theta);
                float errDeg = err * (180.0f / (float)M_PI);
                if (fabsf(errDeg) <= TURN_TOL_DEG) {
                    _phase = Phase::RETURN_HOME;
                    return AC{ 0, 0 };
                }
                int spd = (fabsf(errDeg) > TURN_SLOW_DEG) ? TURN_FAST : TURN_SLOW;
                if (errDeg > 0.0f) return AC{ -spd,  spd };
                else               return AC{  spd, -spd };
            }

            case Phase::RETURN_HOME: {
                float dist = sqrtf(pose.x * pose.x + pose.y * pose.y);
                if (dist < 15.0f) {
                    _phase = Phase::DONE;
                    return AC{ 0, 0 };
                }
                ctx.setIdealHeading(atan2f(-pose.y, -pose.x));
                return AC{ FWD_SPEED, FWD_SPEED };
            }

            case Phase::DONE:
            default:
                ctx.setState(RobotConstants::State::STANDBY);
                return AC{ 0, 0 };
            }

    }
  
};

