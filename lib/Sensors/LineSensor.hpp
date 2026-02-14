

#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#ifdef ARDUINO
#include <Arduino.h>
#endif

class LineSensor : public SensorsInterface {
private:
    bool state;
    SensorType type;

public:
    LineSensor(SensorType t) : state(false), type(t) {}

    bool init() override {
        //pinMode(static_cast<uint8_t>(type), INPUT);
        return true;
    }

    void update() override {
        //state = digitalRead(static_cast<uint8_t>(type));
    }

    float getValue() override {
        return state ? 1.0f : 0.0f;
    }

    SensorType getType() override {
        return type;
    }
};