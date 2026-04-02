#pragma once
#include "IDriverMotor.hpp"
#include "PinConfig.hpp"


class DriverMotor : public IDriverMotor
{
private:
    uint8_t _pinL_PWM = RobotConfig::MOTOR_LEFT_PWM;
    uint8_t _pinL_DIR = RobotConfig::MOTOR_LEFT_DIR;
    uint8_t _pinR_DIR = RobotConfig::


public:
    DriverMotor(){}

    bool  init() override{


    }
    void  setSpeed() override{

    }
    void stop () override{

    }

};

