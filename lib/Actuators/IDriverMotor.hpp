
#pragma once
#include "SensorTypes.hpp"

class IDriverMotor {
public:
    virtual ~IDriverMotor() = default;
    virtual bool  init() = 0;
    virtual void  setSpeed() = 0;
    virtual void stop () = 0;
};
