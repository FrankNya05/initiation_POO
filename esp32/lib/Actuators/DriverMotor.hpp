#pragma once
#include "IDriverMotor.hpp"
#include "PinConfig.hpp"
#include <Arduino.h>
#include <cstdlib> // abs()

class DriverMotor : public IDriverMotor
{
private:
    uint8_t _pinL_DIR = RobotConfig::MOTOR_LEFT_DIR;
    uint8_t _pinL_PWM = RobotConfig::MOTOR_LEFT_PWM;
    uint8_t _pinR_DIR = RobotConfig::MOTOR_RIGHT_DIR;
    uint8_t _pinR_PWM = RobotConfig::MOTOR_RIGHT_PWM;

    void _setMotor(uint8_t pinDir, uint8_t pinPWM,
                   int channelDir, int channelPWM, int speed) {
        speed = constrain(speed, -255, 255);

        if (speed == 0) {
            // Coast : xIN1=0, xIN2=0
            ledcWrite(channelDir, 0);
            ledcWrite(channelPWM, 0);
        } else if (speed > 0) {
            // Avant slow decay : xIN1=1, xIN2=PWM
            ledcWrite(channelDir, 255);
            ledcWrite(channelPWM, 255 - speed);
        } else {
            // Arrière slow decay : xIN1=PWM, xIN2=1
            ledcWrite(channelDir, 255 - abs(speed));
            ledcWrite(channelPWM, 255);
        }
    }

public:
    DriverMotor() {}

    bool init() override {
        ledcSetup(RobotConfig::PWM_CHANNEL_LEFT_DIR,  RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_LEFT,      RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_RIGHT_DIR, RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_RIGHT,     RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);

        ledcAttachPin(_pinL_DIR, RobotConfig::PWM_CHANNEL_LEFT_DIR);
        ledcAttachPin(_pinL_PWM, RobotConfig::PWM_CHANNEL_LEFT);
        ledcAttachPin(_pinR_DIR, RobotConfig::PWM_CHANNEL_RIGHT_DIR);
        ledcAttachPin(_pinR_PWM, RobotConfig::PWM_CHANNEL_RIGHT);

        stop();
        return true;
    }

    void 
    setSpeed(int leftSpeed, int rightSpeed) override {
        _setMotor(_pinL_DIR, _pinL_PWM,
                  RobotConfig::PWM_CHANNEL_LEFT_DIR,  RobotConfig::PWM_CHANNEL_LEFT,  leftSpeed);
        _setMotor(_pinR_DIR, _pinR_PWM,
                  RobotConfig::PWM_CHANNEL_RIGHT_DIR, RobotConfig::PWM_CHANNEL_RIGHT, rightSpeed);
    }

    void setSpeedPercent(int leftPct, int rightPct) {
        int l = map(constrain(leftPct,  -100, 100), -100, 100, -255, 255);
        int r = map(constrain(rightPct, -100, 100), -100, 100, -255, 255);
        setSpeed(l, r);
    }

    void stop() override {
        _setMotor(_pinL_DIR, _pinL_PWM,
                  RobotConfig::PWM_CHANNEL_LEFT_DIR,  RobotConfig::PWM_CHANNEL_LEFT,  0);
        _setMotor(_pinR_DIR, _pinR_PWM,
                  RobotConfig::PWM_CHANNEL_RIGHT_DIR, RobotConfig::PWM_CHANNEL_RIGHT, 0);
    }
};
