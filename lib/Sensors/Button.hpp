// Button.hpp
#pragma once

#include <Arduino.h>
#include "SensorsInterface.hpp"
#include "PinConfig.hpp"

class Button : public SensorsInterface {
public:

    enum class Event : uint8_t {
        NONE = 0,
        SINGLE_PRESS,
        DOUBLE_PRESS,
        LONG_PRESS
    };

    explicit Button(uint8_t pin = RobotConfig::START_BUTTON) : _pin(pin) {}

    bool init() override {
        pinMode(_pin, INPUT_PULLUP);
        _reset();
        return true;
    }

    bool update() override {
        const uint32_t now     = millis();
        const bool     pressed = (digitalRead(_pin) == LOW);
        _currentEvent          = Event::NONE;

        // --- Front montant : début d'appui ---
        if (pressed && !_isPressed) {
            _isPressed      = true;
            _pressStartTime = now;
        }

        // --- Front descendant : relâchement ---
        if (!pressed && _isPressed) {

            // Rebond ignoré
            if ((now - _pressStartTime) < DEBOUNCE_MS) {
                _isPressed = false;
                return false;
            }

            _isPressed = false;
            const uint32_t duration = now - _pressStartTime;

            if (duration >= LONG_PRESS_MS) {
                _currentEvent = Event::LONG_PRESS;
                _reset();
            } else {
                if (_pressCount < 2) _pressCount++;
                _lastReleaseTime = now;
                _waitingSecond   = true;
            }
        }

        // --- Fenêtre double appui expirée ---
        if (_waitingSecond && (now - _lastReleaseTime) > DOUBLE_PRESS_MS) {
            _currentEvent = (_pressCount == 2)
                            ? Event::DOUBLE_PRESS
                            : Event::SINGLE_PRESS;
            _reset();
        }

        return (_currentEvent != Event::NONE);
    }

    [[nodiscard]] SensorData getData() const override {
        SensorData data{};
        data.buttonEvent = _currentEvent;
        return data;
    }

    [[nodiscard]] Event getEvent() const { return _currentEvent; }

private:

    void _reset() {
        _pressCount    = 0;
        _waitingSecond = false;
        _isPressed     = false;
    }

    // -- Configuration --
    uint8_t _pin;

    // -- Timing --
    uint32_t _pressStartTime  = 0;
    uint32_t _lastReleaseTime = 0;

    // -- Flags --
    bool    _isPressed     = false;
    bool    _waitingSecond = false;
    uint8_t _pressCount    = 0;

    // -- Résultat --
    Event _currentEvent = Event::NONE;

    // -- Seuils (ms) --
    static constexpr uint32_t DEBOUNCE_MS     = 50;
    static constexpr uint32_t LONG_PRESS_MS   = 800;
    static constexpr uint32_t DOUBLE_PRESS_MS = 400;
};