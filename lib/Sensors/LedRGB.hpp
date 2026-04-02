#pragma once

/**
 * @file LedRGB.hpp
 * @brief Classe de contrôle d'une LED RGB (anode/cathode commune) sur ESP32.
 *
 * Caractéristiques :
 *  - Contrôle des 3 canaux R, G, B via PWM (ledcWrite / analogWrite)
 *  - Modes non bloquants : SOLID, BLINK, PULSE
 *  - Couleurs prédéfinies pour les états du robot mini-sumo
 *  - Compatible avec le même pattern que Button (update() dans loop/tâche)
 *
 * Branchement typique (cathode commune) :
 *   GPIO_R → R (+ résistance 100–220 Ω) → LED → GND
 *   GPIO_G → G (+ résistance 100–220 Ω) → LED → GND
 *   GPIO_B → B (+ résistance 100–220 Ω) → LED → GND
 */

#include <Arduino.h>

// ======================================================================
//  Structure couleur
// ======================================================================

struct Color {
    uint8_t r, g, b;

    constexpr Color() : r(0), g(0), b(0) {}
    constexpr Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    // Couleurs prédéfinies
    static constexpr Color OFF()     { return {  0,   0,   0}; }
    static constexpr Color RED()     { return {255,   0,   0}; }
    static constexpr Color GREEN()   { return {  0, 255,   0}; }
    static constexpr Color BLUE()    { return {  0,   0, 255}; }
    static constexpr Color YELLOW()  { return {255, 255,   0}; }
    static constexpr Color CYAN()    { return {  0, 255, 255}; }
    static constexpr Color MAGENTA() { return {255,   0, 255}; }
    static constexpr Color WHITE()   { return {255, 255, 255}; }
    static constexpr Color ORANGE()  { return {255, 100,   0}; }

    // Couleurs sémantiques mini-sumo
    static constexpr Color IDLE()    { return BLUE();   }   // en attente
    static constexpr Color RUNNING() { return GREEN();  }   // combat
    static constexpr Color PAUSED()  { return YELLOW(); }   // pause
    static constexpr Color STOPPED() { return RED();    }   // arrêt urgence

    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

// ======================================================================
//  Modes d'animation
// ======================================================================

enum class LedMode : uint8_t {
    SOLID,   ///< Couleur fixe
    BLINK,   ///< Clignotement on/off
    PULSE    ///< Fondu progressif (breathing)
};

// ======================================================================
//  Classe LedRGB
// ======================================================================

class LedRGB {
public:

    // ------------------------------------------------------------------ //
    //  Construction
    // ------------------------------------------------------------------ //

    /**
     * @param pinR        Broche GPIO canal rouge
     * @param pinG        Broche GPIO canal vert
     * @param pinB        Broche GPIO canal bleu
     * @param commonAnode true  → anode commune  (logique inversée)
     *                    false → cathode commune (logique directe)
     */
    LedRGB(uint8_t pinR, uint8_t pinG, uint8_t pinB,
           bool commonAnode = false)
        : _pinR(pinR), _pinG(pinG), _pinB(pinB)
        , _commonAnode(commonAnode)
    {}

    // ------------------------------------------------------------------ //
    //  Initialisation
    // ------------------------------------------------------------------ //

    /**
     * @brief Configure les PWM et éteint la LED.
     *        À appeler dans setup().
     */
    void begin() {
#if defined(ESP32)
        // ESP32 : utilise les canaux LEDC
        ledcSetup(_chR, PWM_FREQ, PWM_BITS);
        ledcSetup(_chG, PWM_FREQ, PWM_BITS);
        ledcSetup(_chB, PWM_FREQ, PWM_BITS);
        ledcAttachPin(_pinR, _chR);
        ledcAttachPin(_pinG, _chG);
        ledcAttachPin(_pinB, _chB);
#else
        pinMode(_pinR, OUTPUT);
        pinMode(_pinG, OUTPUT);
        pinMode(_pinB, OUTPUT);
#endif
        off();
    }

    // ------------------------------------------------------------------ //
    //  Interface principale
    // ------------------------------------------------------------------ //

    /** Couleur fixe, immédiatement. */
    void solid(Color c) {
        _color   = c;
        _mode    = LedMode::SOLID;
        _writeColor(c);
    }

    /**
     * @brief Clignotement non bloquant.
     * @param c          Couleur allumée
     * @param onMs       Durée allumée (ms)
     * @param offMs      Durée éteinte (ms), 0 = symétrique avec onMs
     */
    void blink(Color c, uint32_t onMs = 300, uint32_t offMs = 0) {
        _color   = c;
        _onMs    = onMs;
        _offMs   = (offMs == 0) ? onMs : offMs;
        _mode    = LedMode::BLINK;
        _blinkOn = true;
        _lastMs  = millis();
        _writeColor(c);
    }

    /**
     * @brief Pulse / breathing non bloquant.
     * @param c        Couleur cible
     * @param periodMs Durée d'un cycle complet (montée + descente)
     */
    void pulse(Color c, uint32_t periodMs = 1000) {
        _color    = c;
        _periodMs = periodMs;
        _mode     = LedMode::PULSE;
        _phaseMs  = 0;
        _lastMs   = millis();
    }

    /** Éteindre immédiatement. */
    void off() {
        _mode = LedMode::SOLID;
        _color = Color::OFF();
        _writeColor(Color::OFF());
    }

    /**
     * @brief Met à jour l'animation (BLINK / PULSE).
     *        À appeler aussi souvent que possible (loop() ou tâche FreeRTOS).
     */
    void update() {
        const uint32_t now = millis();
        const uint32_t dt  = now - _lastMs;
        _lastMs = now;

        switch (_mode) {
            case LedMode::SOLID:
                break;   // rien à faire

            case LedMode::BLINK:
                _updateBlink(now);
                break;

            case LedMode::PULSE:
                _updatePulse(dt);
                break;
        }
    }

    // ------------------------------------------------------------------ //
    //  Accesseurs
    // ------------------------------------------------------------------ //

    Color   color() const { return _color; }
    LedMode mode()  const { return _mode;  }

private:

    // ------------------------------------------------------------------ //
    //  Paramètres PWM
    // ------------------------------------------------------------------ //
    static constexpr uint32_t PWM_FREQ = 5000;   ///< Hz
    static constexpr uint8_t  PWM_BITS = 8;       ///< résolution (0–255)

    // Canaux LEDC ESP32 (3 canaux distincts)
    static constexpr uint8_t _chR = 0;
    static constexpr uint8_t _chG = 1;
    static constexpr uint8_t _chB = 2;

    // ------------------------------------------------------------------ //
    //  Configuration
    // ------------------------------------------------------------------ //
    uint8_t _pinR, _pinG, _pinB;
    bool    _commonAnode;

    // ------------------------------------------------------------------ //
    //  État interne
    // ------------------------------------------------------------------ //
    LedMode  _mode     = LedMode::SOLID;
    Color    _color    = Color::OFF();

    // Blink
    uint32_t _onMs     = 300;
    uint32_t _offMs    = 300;
    bool     _blinkOn  = false;
    uint32_t _blinkMs  = 0;   ///< timestamp du dernier changement

    // Pulse
    uint32_t _periodMs = 1000;
    uint32_t _phaseMs  = 0;   ///< position dans le cycle

    // Commun
    uint32_t _lastMs   = 0;

    // ------------------------------------------------------------------ //
    //  Helpers privés
    // ------------------------------------------------------------------ //

    void _writeColor(Color c) {
        uint8_t r = _commonAnode ? (255 - c.r) : c.r;
        uint8_t g = _commonAnode ? (255 - c.g) : c.g;
        uint8_t b = _commonAnode ? (255 - c.b) : c.b;

#if defined(ESP32)
        ledcWrite(_chR, r);
        ledcWrite(_chG, g);
        ledcWrite(_chB, b);
#else
        analogWrite(_pinR, r);
        analogWrite(_pinG, g);
        analogWrite(_pinB, b);
#endif
    }

    void _updateBlink(uint32_t now) {
        uint32_t duration = _blinkOn ? _onMs : _offMs;
        if ((now - _blinkMs) >= duration) {
            _blinkOn  = !_blinkOn;
            _blinkMs  = now;
            _writeColor(_blinkOn ? _color : Color::OFF());
        }
    }

    void _updatePulse(uint32_t dt) {
        _phaseMs = (_phaseMs + dt) % _periodMs;

        // Triangle : montée puis descente
        float ratio;
        uint32_t half = _periodMs / 2;
        if (_phaseMs < half) {
            ratio = (float)_phaseMs / half;          // 0.0 → 1.0
        } else {
            ratio = (float)(_periodMs - _phaseMs) / half;  // 1.0 → 0.0
        }

        Color c;
        c.r = (uint8_t)(_color.r * ratio);
        c.g = (uint8_t)(_color.g * ratio);
        c.b = (uint8_t)(_color.b * ratio);
        _writeColor(c);
    }
};