#pragma once
#include <Arduino.h>
#include "PinConfig.hpp"
#include "RobotConstants.hpp"

class Encoder {
private:
    uint8_t  _pinA;
    uint8_t  _pinB;
    volatile int32_t _count      = 0;  // compteur relatif (remis à 0 sur demande)
    volatile int32_t _totalCount = 0;  // position absolue (jamais remise à 0)
    volatile int8_t  _lastA      = 0;

    // Pointeurs statiques pour les ISR (une par moteur)
    static Encoder* _instanceLeft;
    static Encoder* _instanceRight;

    void _handleISR() {
        int8_t a = digitalRead(_pinA);
        int8_t b = digitalRead(_pinB);
        int8_t delta = 0;

        if (a != _lastA) {
            delta = (a == b) ? +1 : -1;  // flanc sur A
        } else {
            delta = (a != b) ? +1 : -1;  // flanc sur B
        }
        _count      += delta;
        _totalCount += delta;
        _lastA = a;
    }

public:
    Encoder(uint8_t pinA, uint8_t pinB)
        : _pinA(pinA), _pinB(pinB) {}

    void init(bool isLeft) {
        pinMode(_pinA, INPUT_PULLUP);
        pinMode(_pinB, INPUT_PULLUP);
        _lastA = digitalRead(_pinA);

        if (isLeft) {
            _instanceLeft = this;
            attachInterrupt(digitalPinToInterrupt(_pinA), _isrLeft, CHANGE);
            attachInterrupt(digitalPinToInterrupt(_pinB), _isrLeft, CHANGE);
        } else {
            _instanceRight = this;
            attachInterrupt(digitalPinToInterrupt(_pinA), _isrRight, CHANGE);
            attachInterrupt(digitalPinToInterrupt(_pinB), _isrRight, CHANGE);
        }
    }

    // Lecture et remise à zéro atomique
    int32_t getAndReset() {
        portDISABLE_INTERRUPTS();
        int32_t val = _count;
        _count = 0;
        portENABLE_INTERRUPTS();
        return val;
    }

    int32_t getCount()      const { return _count; }
    int32_t getTotalCount() const { return _totalCount; }
    void    reset()               { _count = 0; }

    // Conversion pulses → RPM
    float getRPM(int32_t pulses, float deltaMs) {
        return (pulses / (float)RobotConstants::PULSES_PER_REV)
               * (60000.0f / deltaMs);
    }

    // ── Position angulaire absolue (depuis démarrage) ──────────
    float getAngleDeg() const {
        return _totalCount * RobotConstants::DEG_PER_PULSE;
    }

    float getAngleRad() const {
        return _totalCount * RobotConstants::RAD_PER_PULSE;
    }

    // ── Position angulaire relative (depuis dernier resetAngle) ─
    float getRelativeAngleDeg() {
        portDISABLE_INTERRUPTS();
        int32_t val = _count;
        portENABLE_INTERRUPTS();
        return val * RobotConstants::DEG_PER_PULSE;
    }

    float getRelativeAngleRad() {
        portDISABLE_INTERRUPTS();
        int32_t val = _count;
        portENABLE_INTERRUPTS();
        return val * RobotConstants::RAD_PER_PULSE;
    }

    // Remise à zéro de la position relative uniquement
    void resetAngle() {
        portDISABLE_INTERRUPTS();
        _count = 0;
        portENABLE_INTERRUPTS();
    }

    // ISR statiques — définies dans Encoder.cpp (IRAM_ATTR interdit dans un .hpp)
    static void IRAM_ATTR _isrLeft();
    static void IRAM_ATTR _isrRight();
};