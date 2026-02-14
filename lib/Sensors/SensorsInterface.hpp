/*
* This header plement ahchitecture all sensor used

*/

#pragma once
#include "SensorTypes.hpp"

class SensorsInterface
{
    private:
        SensorType type;
    public:
    // That is my virtual destructor 
    virtual ~SensorsInterface() = default;

     // Initialize the sensor
    virtual bool  init() = 0;

    // update sensor value
    virtual void update() = 0;

     // get sensor value
     virtual float getValue() = 0;

     virtual SensorType getType() = 0;
     
};

