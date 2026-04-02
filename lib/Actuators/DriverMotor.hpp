#pragma once
#include "IDriverMotor.hpp"
#include "PinConfig.hpp"
#include <Arduino.h>
#include <cstdlib> // abs()

class DriverMotor : public IDriverMotor
{
private:
    uint8_t _pinR_PWM = RobotConfig::MOTOR_RIGHT_PWM;
    uint8_t _pinL_DIR = RobotConfig::MOTOR_LEFT_DIR;
    uint8_t _pinL_PWM = RobotConfig::MOTOR_LEFT_PWM;
    uint8_t _pinR_DIR = RobotConfig::MOTOR_RIGHT_DIR;

    void _setMotor(uint8_t pinDir, int channel, int speed) {
    if (speed == 0) {
        // Coast propre : LOW + PWM=0
        digitalWrite(pinDir, LOW);
        ledcWrite(channel, 0);
    } else {
        digitalWrite(pinDir, speed > 0 ? HIGH : LOW);
        ledcWrite(channel, 255 - abs(speed));
    }
}

public:
    DriverMotor() {}

    bool init() override {
        pinMode(_pinL_DIR, OUTPUT);
        pinMode(_pinR_DIR, OUTPUT);

        ledcSetup(RobotConfig::PWM_CHANNEL_LEFT,  RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_RIGHT, RobotConfig::PWM_FREQUENCY, RobotConfig::PWM_RESOLUTION);

        ledcAttachPin(_pinL_PWM, RobotConfig::PWM_CHANNEL_LEFT);
        ledcAttachPin(_pinR_PWM, RobotConfig::PWM_CHANNEL_RIGHT);

        stop();
        return true;
    }

    void setSpeed(int leftSpeed, int rightSpeed) override {
        
        _setMotor(_pinL_DIR, RobotConfig::PWM_CHANNEL_LEFT,  leftSpeed);
        _setMotor(_pinR_DIR, RobotConfig::PWM_CHANNEL_RIGHT, rightSpeed);
    }
    
    void setSpeedPercent(int leftPct, int rightPct) {
    int l = map(constrain(leftPct,  -100, 100), -100, 100, -255, 255);
    int r = map(constrain(rightPct, -100, 100), -100, 100, -255, 255);
    setSpeed(l, r);
  }
  
    void stop() override {
        _setMotor(_pinL_DIR, RobotConfig::PWM_CHANNEL_LEFT,  0);
        _setMotor(_pinR_DIR, RobotConfig::PWM_CHANNEL_RIGHT, 0);
    }

};

