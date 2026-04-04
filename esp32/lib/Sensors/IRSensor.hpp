#pragma once
#include <Wire.h>
#include <Arduino.h>
#include "SensorsInterface.hpp"
#include "PinConfig.hpp"
using namespace RobotConfig;

class TOFSensor : public SensorsInterface {
public:
    static constexpr const char* TYPE_ID = "TOF";

    TOFSensor(uint8_t addr, SensorPosition position)
        : _config{addr, position} {}

    bool init() override {
        Wire.begin(TOF_SDA, TOF_SCL);
        Wire.setClock(400000);

        Wire.beginTransmission(_config.addr);
        if (Wire.endTransmission() != 0) {
            Serial.printf("[TOFSensor] Capteur introuvable à 0x%02X\n", _config.addr);
            return false;
        }
        Serial.printf("[TOFSensor] Capteur 0x%02X initialisé.\n", _config.addr);
        return true;
    }

    bool update() override {
        float value = _readDistance(_config.addr);

        _data.position  = _config.position;
        _data.timestamp = millis();

        if (value > -1.0f) {
            _data.value   = SensorValue(value);
            _data.isValid = true;
            return true;
        }

        _data.isValid = false;
        return false;
    }

    SensorData getData() const override {
        return _data;
    }

private:
    const TOFConfig _config;
    SensorData      _data;

    float _readDistance(uint8_t addr) {
        Wire.beginTransmission(addr);
        Wire.write(0x24);
        Wire.endTransmission(false);
        Wire.requestFrom(addr, (uint8_t)4);

        if (Wire.available() < 4) return -1.0f;

        uint8_t d[4];
        for (int i = 0; i < 4; i++) d[i] = Wire.read();

        int32_t raw = (int32_t)(d[0] | (d[1] << 8) | (d[2] << 16));
        if (raw & 0x800000) raw |= 0xFF000000;
        return raw / 1000.0f;
    }
};