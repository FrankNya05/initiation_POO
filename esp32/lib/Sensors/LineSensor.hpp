#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#ifdef ARDUINO
#include <Arduino.h>
#include "PinConfig.hpp"
#endif

class LineSensor : public SensorsInterface {
private:
    SensorData     data;
    SensorPosition position;
    uint8_t        pin;
    int            seuil;

    static uint8_t positionToPin(SensorPosition pos) {
        switch (pos) {
            case SensorPosition::FRONT_LEFT:  return RobotConfig::LINE_SENSOR_FRONT_LEFT;
            case SensorPosition::FRONT_RIGHT: return RobotConfig::LINE_SENSOR_FRONT_RIGHT;
            case SensorPosition::BACK:        return RobotConfig::LINE_SENSOR_BACK;
            default:
                return 0xFF;  // Pin invalide
        }
    }

public:
    LineSensor(SensorPosition p, int s)
        : position(p), pin(positionToPin(p)), seuil(s) {}

    const char* typeId() const override { return "LINE"; }

    bool init() override {
        if (pin == 0xFF) return false;  // Pin invalide
        pinMode(pin, INPUT);
        return true;
    }

    bool update() override {
        int valeur = analogRead(pin);
        data.value     = SensorValue(valeur < seuil ? 1.0f : 0.0f);
        data.position  = position;
        data.isValid   = true;
        data.timestamp = millis();
        return true;
    }

    SensorData getData() const override {
        return data;
    }
};