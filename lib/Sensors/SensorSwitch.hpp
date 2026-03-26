// Button.hpp
#pragma once

#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"      // SensorData, SensorValue, SensorDims, Event, SensorPosition
#include "PinConfig.hpp"
#include <cstdint>
#include <Arduino.h>

/**
 * @brief Capteur bouton-poussoir avec détection d'appui simple, double et long.
 *
 * Hérite de SensorsInterface et utilise les types définis dans SensorTypes.hpp.
 *
 * Encodage dans SensorData :
 *   - value.scalar = static_cast<float>(Event)
 *   - dims         = SensorDims::SCALAR
 *   - isValid      = true uniquement quand un événement vient de se produire
 *   - timestamp    = millis() au moment de l'événement
 */
class Button : public SensorsInterface {
public:

    /**
     * @param pin      Broche GPIO (active-low avec INPUT_PULLUP)
     * @param position Position logique du bouton sur le robot
     */
    explicit Button(uint8_t        pin      = RobotConfig::START_BUTTON,
                    SensorPosition position = SensorPosition::UNKNOWN)
        : _pin(pin), _position(position)
    {}

    // ------------------------------------------------------------------ //
    //  Initialisation
    // ------------------------------------------------------------------ //

    bool init() override {
        pinMode(_pin, INPUT_PULLUP);
        _reset();
        _currentEvent = Event::NONE;
        return true;
    }

    // ------------------------------------------------------------------ //
    //  Mise à jour (à appeler dans loop() ou une tâche FreeRTOS)
    // ------------------------------------------------------------------ //

    bool update() override {
        const uint32_t now = millis();
        _currentEvent      = Event::NONE;
        // Note : _data.isValid reste true jusqu'au prochain fireEvent()
        // afin que getData() puisse le lire même après plusieurs update().

        // --- Lecture avec anti-rebond (debounce) ----------------------
        const bool rawPressed = (digitalRead(_pin) == LOW);

        if (rawPressed != _rawPrev) {
            _debounceStart = now;
            _rawPrev       = rawPressed;
        }

        if ((now - _debounceStart) < DEBOUNCE_MS) {
            return false;   // transitoire, on ignore
        }

        const bool pressed = rawPressed;   // état stable

        // --- Effacer le flag longPressFired dès que le bouton est relâché --
        if (!pressed && _longPressFired) {
            _longPressFired = false;
        }

        // --- Front montant : début d'appui ----------------------------
        // Ignoré si un LONG_PRESS vient juste d'être émis (bouton encore tenu)
        if (pressed && !_isPressed && !_longPressFired) {
            _isPressed      = true;
            _pressStartTime = now;
        }

        // --- Appui maintenu → LONG_PRESS (émis sans attendre relâchement)
        if (_isPressed && pressed) {
            if ((now - _pressStartTime) >= LONG_PRESS_MS) {
                _isPressed      = false;    // évite la détection au relâchement
                _longPressFired = true;     // bloque le front montant fantôme
                _fireEvent(Event::LONG_PRESS, now);
                _reset();
                return true;
            }
        }

        // --- Front descendant : relâchement ---------------------------
        if (!pressed && _isPressed) {
            _isPressed       = false;
            _pressCount++;
            _lastReleaseTime = now;
            _waitingSecond   = true;
        }

        // --- Fenêtre double appui expirée → SINGLE ou DOUBLE ----------
        if (_waitingSecond && (now - _lastReleaseTime) > DOUBLE_PRESS_MS) {
            const Event e = (_pressCount >= 2) ? Event::DOUBLE_PRESS
                                               : Event::SINGLE_PRESS;
            _fireEvent(e, now);
            _reset();
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------------ //
    //  Accesseurs
    // ------------------------------------------------------------------ //

    /**
     * @brief Retourne le SensorData rempli.
     *
     * isValid est true uniquement juste après un appel à update() qui a émis
     * un événement. Il repasse à false au prochain appel de update().
     */
    [[nodiscard]] SensorData getData() const override {
        return _data;
    }

    /** Dernier événement détecté (Event::NONE si aucun depuis le dernier update). */
    [[nodiscard]] Event getEvent() const { return _currentEvent; }

    /** Retourne true si le bouton est physiquement pressé (état stable). */
    [[nodiscard]] bool isPhysicallyPressed() const { return _isPressed; }

private:

    // ------------------------------------------------------------------ //
    //  Helpers privés
    // ------------------------------------------------------------------ //

    void _reset() {
        _pressCount    = 0;
        _waitingSecond = false;
    }

    void _fireEvent(Event e, uint32_t timestamp) {
        _currentEvent     = e;
        _data.position    = _position;
        _data.value       = SensorValue(static_cast<float>(e));
        _data.dims        = SensorDims::SCALAR;
        _data.timestamp   = timestamp;
        _data.isValid     = true;   // reste true jusqu'au prochain _fireEvent()
    }

    // ------------------------------------------------------------------ //
    //  Configuration
    // ------------------------------------------------------------------ //
    uint8_t        _pin;
    SensorPosition _position;

    // ------------------------------------------------------------------ //
    //  Timing
    // ------------------------------------------------------------------ //
    uint32_t _pressStartTime  = 0;
    uint32_t _lastReleaseTime = 0;
    uint32_t _debounceStart   = 0;

    // ------------------------------------------------------------------ //
    //  Flags d'état
    // ------------------------------------------------------------------ //
    bool    _isPressed      = false;
    bool    _waitingSecond  = false;
    bool    _rawPrev        = false;   ///< état brut précédent (pour debounce)
    bool    _longPressFired = false;   ///< bloque le front montant fantôme après LONG_PRESS
    uint8_t _pressCount     = 0;

    // ------------------------------------------------------------------ //
    //  Données de sortie
    // ------------------------------------------------------------------ //
    Event      _currentEvent = Event::NONE;
    SensorData _data         = {};

    // ------------------------------------------------------------------ //
    //  Seuils de timing (ms)
    // ------------------------------------------------------------------ //
    static constexpr uint32_t DEBOUNCE_MS   =  20;   ///< Anti-rebond
    static constexpr uint32_t LONG_PRESS_MS = 800;   ///< Durée appui long
    static constexpr uint32_t DOUBLE_PRESS_MS = 400; ///< Fenêtre double appui
};