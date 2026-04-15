#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#include "PinConfig.hpp"
#ifdef ARDUINO
#include <Arduino.h>
#endif

// ═══════════════════════════════════════════════════════════════
//  SensorSwitch — Détection de 3 types d'appui sur un bouton
//
//  COURT  : relâché avant LONG_PRESS_MS, sans 2e appui dans DOUBLE_MS
//  LONG   : maintenu >= LONG_PRESS_MS
//  RAPID  : 2 appuis courts dans DOUBLE_MS (double-clic)
//
//  Machine à états :
//
//   IDLE ──(press)──► PRESSED ──(release, dur >= LONG)──► IDLE [emit LONG]
//                         │
//                    (release, dur < LONG)
//                         │
//                         ▼
//                    WAIT_DOUBLE ──(press)──► IDLE [emit RAPID]
//                         │
//                    (timeout DOUBLE_MS)
//                         │
//                         ▼
//                      IDLE [emit SHORT]
// ═══════════════════════════════════════════════════════════════

class SensorSwitch : public SensorsInterface
{
public:
    // ── Seuils temporels (ms) ────────────────────────────────
    static constexpr uint32_t DEBOUNCE_MS    =  50;
    static constexpr uint32_t LONG_PRESS_MS  = 800;
    static constexpr uint32_t DOUBLE_MS      = 350;

    explicit SensorSwitch(uint8_t pin = RobotConfig::START_BUTTON,
                          bool    activeLow = true)
        : _pin(pin), _activeLow(activeLow) {}

    // ── Cycle de vie ─────────────────────────────────────────

    bool init() override {
        pinMode(_pin, _activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
        _state      = State::IDLE;
        _lastData.value     = SensorValue(static_cast<float>(PressType::NONE));
        _lastData.isValid   = false;
        _lastData.dims      = SensorDims::SCALAR;
        return true;
    }

    // ── Mise à jour (appeler en boucle ou depuis une tâche) ──

    bool update() override {
        const uint32_t now     = millis();
        const bool     pressed = _readPressed();

        // Debounce : ignore les changements trop rapides
        if (pressed != _lastRaw) {
            _debounceStart = now;
            _lastRaw       = pressed;
        }
        if ((now - _debounceStart) < DEBOUNCE_MS) {
            _emit(PressType::NONE);   // pas encore stable
            return true;
        }

        const bool stable = pressed;  // valeur deboncée

        switch (_state) {

            case State::IDLE:
                if (stable) {
                    _pressStart = now;
                    _state      = State::PRESSED;
                }
                _emit(PressType::NONE);
                break;

            case State::PRESSED:
                if (!stable) {
                    // Bouton relâché
                    uint32_t held = now - _pressStart;
                    if (held >= LONG_PRESS_MS) {
                        _emit(PressType::LONG);
                        _state = State::IDLE;
                    } else {
                        // Appui court — attendre éventuel 2e clic
                        _releaseTime = now;
                        _state       = State::WAIT_DOUBLE;
                        _emit(PressType::NONE);
                    }
                } else {
                    _emit(PressType::NONE);
                }
                break;

            case State::WAIT_DOUBLE:
                if (stable) {
                    // 2e appui détecté dans la fenêtre → RAPID
                    _emit(PressType::RAPID);
                    _state = State::IDLE;
                } else if ((now - _releaseTime) >= DOUBLE_MS) {
                    // Fenêtre expirée → SHORT confirmé
                    _emit(PressType::SHORT);
                    _state = State::IDLE;
                } else {
                    _emit(PressType::NONE);
                }
                break;
        }

        return true;
    }

    // ── Accesseurs ───────────────────────────────────────────

    SensorData getData() const override { return _lastData; }

    /** Retourne le dernier type d'appui détecté. */
    PressType getPressType() const {
        return static_cast<PressType>(static_cast<uint8_t>(_lastData.value.scalar));
    }

    /** Vrai uniquement le cycle où un type d'appui a été émis. */
    bool hasEvent() const { return _lastData.isValid; }

private:
    enum class State : uint8_t { IDLE, PRESSED, WAIT_DOUBLE };

    uint8_t  _pin;
    bool     _activeLow;

    State    _state        = State::IDLE;
    bool     _lastRaw      = false;
    uint32_t _debounceStart = 0;
    uint32_t _pressStart    = 0;
    uint32_t _releaseTime   = 0;

    SensorData _lastData;

    bool _readPressed() const {
        bool raw = digitalRead(_pin);
        return _activeLow ? !raw : raw;
    }

    void _emit(PressType p) {
        _lastData.value     = SensorValue(static_cast<float>(static_cast<uint8_t>(p)));
        _lastData.isValid   = (p != PressType::NONE);
        _lastData.timestamp = millis();
        _lastData.dims      = SensorDims::SCALAR;
    }
};
