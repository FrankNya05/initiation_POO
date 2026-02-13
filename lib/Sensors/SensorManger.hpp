// SensorManager.hpp
#pragma once
#include <vector>
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"

class SensorManager {
private:
    std::vector<SensorsInterface*> sensors;
public:
    void addSensor(SensorsInterface* sensor);
    bool initAll();
    void updateAll();
    float getValue(SensorType type);
};
