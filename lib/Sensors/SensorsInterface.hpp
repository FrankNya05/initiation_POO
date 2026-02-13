/*
* This header plement ahchitecture all sensor used

*/

#pragma once


class SensorsInterface
{
public:
    // That is my virtual destructor 
    virtual ~SensorsInterface() = default;

     // Initialize the sensor
    virtual bool  init() = 0;

    // update sensor value
    virtual void update() = 0;

     // get sensor value
     virtual float getValue() = 0;
};

