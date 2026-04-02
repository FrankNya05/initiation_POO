#pragma once
#include "IDriverMotor.hpp"
#include "PinConfig.hpp"
#include <Arduino.h>
#include <cstdlib> // abs()

class DriverMotor : public IDriverMotor
{
private:
    uint8_t _pinL_PWM = RobotConfig::MOTOR_LEFT_PWM;
    uint8_t _pinL_DIR = RobotConfig::MOTOR_LEFT_DIR;
    uint8_t _pinR_PWM = RobotConfig::MOTOR_RIGHT_PWM;
    uint8_t _pinR_DIR = RobotConfig::MOTOR_RIGHT_DIR;

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

    // leftSpeed / rightSpeed : -255 (arrière max) à +255 (avant max)
    void setSpeed(int leftSpeed, int rightSpeed) override {
        digitalWrite(_pinL_DIR, leftSpeed  >= 0 ? HIGH : LOW);
        digitalWrite(_pinR_DIR, rightSpeed >= 0 ? HIGH : LOW);

        ledcWrite(RobotConfig::PWM_CHANNEL_LEFT,  abs(leftSpeed));
        ledcWrite(RobotConfig::PWM_CHANNEL_RIGHT, abs(rightSpeed));
    }

    void stop() override {
        setSpeed(0, 0);
    }
};

