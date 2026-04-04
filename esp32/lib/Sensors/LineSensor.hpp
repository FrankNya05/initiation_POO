

#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#ifdef ARDUINO
#include <Arduino.h>
#include <stdexcept>
#include "PinConfig.hpp"
#endif

class LineSensor : public SensorsInterface {
private:
    SensorData     data;
    SensorPosition position;
    uint8_t        pin;
    static uint8_t positionToPin(SensorPosition pos){
        switch (pos) {
            case SensorPosition::LEFT:  return RobotConfig::LINE_SENSOR_FRONT_LEFT;
            case SensorPosition::RIGHT: return RobotConfig::LINE_SENSOR_FRONT_RIGHT;
            case SensorPosition::BACK:  return RobotConfig::LINE_SENSOR_BACK;
            default: return 0;
        }
    }

public:
    LineSensor(SensorPosition p) :position(p), pin(positionToPin(p)){}
    bool init() override {
        pinMode(pin, INPUT);
        return true;
    }

    bool update() override {
        data.value    = SensorValue(analogRead(pin) ? 2048.0f : 0.0f); 
        data.position = position;
        data.isValid  = true;
        data.timestamp = millis();
        return true;
    }

    SensorData getData() const override {
        return data;
    }
};