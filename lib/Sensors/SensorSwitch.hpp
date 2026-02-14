#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#include "PinConfig.hpp"
#ifdef ARDUINO
#include <Arduino.h>
#endif
 
class SensorSwitch:public SensorsInterface
{
    private:
        SensorType type;
        bool state;
    public:
    SensorSwitch(SensorType t): state(false),type(t){}//constructeur avec initialisation liste

    bool  init() override{
       //pinMode(static_cast<uint8_t>(RobotConfig::START_BUTTON), INPUT);
        return RobotConfig::START_BUTTON;
    }

    // update sensor value
    void update() override{
        //state = digitalRead(static_cast<uint8_t>(type));
    
    }

     // get sensor value
     float getValue() override{
        return state ? 1.0f : 0.0f;
     }

    SensorType getType() override{
        return type;
    }
};