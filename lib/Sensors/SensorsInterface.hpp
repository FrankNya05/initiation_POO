/*
* This header plement ahchitecture all sensor used

*/

#pragma once
#include "SensorTypes.hpp"
#include <vector>
class SensorsInterface {
public:
    virtual ~SensorsInterface() = default;
    virtual bool  init()              = 0;
    virtual bool  update()            = 0;
    virtual SensorData getData() const override = 0;
};
