#pragma once
#include <Arduino.h>
#include "PinConfig.hpp"
#include "RTOSConfig.hpp"

// ─────────────────────────────────────────────────────────────
//  Couleurs prédéfinies (format 0xRRGGBB) — états du robot
// ─────────────────────────────────────────────────────────────
enum class LedColor : uint32_t {
    OFF     = 0x000000,
    RED     = 0xFF0000,  // attaque / erreur
    GREEN   = 0x00FF00,  // prêt / succès
    BLUE    = 0x0000FF,  // recherche
    YELLOW  = 0xFFFF00,  // attente
    CYAN    = 0x00FFFF,
    MAGENTA = 0xFF00FF,
    WHITE   = 0xFFFFFF,
    ORANGE  = 0xFF6000,  // calibration
};

// ═══════════════════════════════════════════════════════════════
//  DriverLedRGB — Contrôle LED RGB par PWM (ledc ESP32)
//  Canaux ledc 4-6 (0-3 réservés aux moteurs)
//  Supporte anode commune (logique inversée) et cathode commune.
// ═══════════════════════════════════════════════════════════════
class DriverLedRGB
{
private:
    uint8_t _pinR;
    uint8_t _pinG;
    uint8_t _pinB;
    bool    _commonAnode;

    uint8_t _r = 0;
    uint8_t _g = 0;
    uint8_t _b = 0;
    uint8_t _brightness = 100;  // %

    void _writeChannel(int channel, uint8_t value) {
        uint8_t scaled = (uint16_t)value * _brightness / 100;
        ledcWrite(channel, _commonAnode ? (255 - scaled) : scaled);
    }

public:
    explicit DriverLedRGB(uint8_t pinR       = RobotConfig::LED_PIN_R,
                          uint8_t pinG       = RobotConfig::LED_PIN_G,
                          uint8_t pinB       = RobotConfig::LED_PIN_B,
                          bool    commonAnode = true)
        : _pinR(pinR), _pinG(pinG), _pinB(pinB), _commonAnode(commonAnode) {}

    // ── Cycle de vie ─────────────────────────────────────────

    bool init() {
        ledcSetup(RobotConfig::PWM_CHANNEL_LED_R, RobotConfig::PWM_FREQUENCY_LED, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_LED_G, RobotConfig::PWM_FREQUENCY_LED, RobotConfig::PWM_RESOLUTION);
        ledcSetup(RobotConfig::PWM_CHANNEL_LED_B, RobotConfig::PWM_FREQUENCY_LED, RobotConfig::PWM_RESOLUTION);

        ledcAttachPin(_pinR, RobotConfig::PWM_CHANNEL_LED_R);
        ledcAttachPin(_pinG, RobotConfig::PWM_CHANNEL_LED_G);
        ledcAttachPin(_pinB, RobotConfig::PWM_CHANNEL_LED_B);

        off();
        LOGF("[LedRGB] init OK — R:%d G:%d B:%d commonAnode:%d\n",
             _pinR, _pinG, _pinB, _commonAnode);
        return true;
    }

    // ── Commandes couleur ────────────────────────────────────

    /** Couleur par composantes R, G, B (0–255). */
    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        _r = r; _g = g; _b = b;
        _writeChannel(RobotConfig::PWM_CHANNEL_LED_R, _r);
        _writeChannel(RobotConfig::PWM_CHANNEL_LED_G, _g);
        _writeChannel(RobotConfig::PWM_CHANNEL_LED_B, _b);
    }

    /** Couleur via un entier 0xRRGGBB. */
    void setColor(uint32_t rgb) {
        setColor((rgb >> 16) & 0xFF,
                 (rgb >>  8) & 0xFF,
                  rgb        & 0xFF);
    }

    /** Couleur via l'enum LedColor. */
    void setColor(LedColor color) {
        setColor(static_cast<uint32_t>(color));
    }

    // ── Raccourcis ───────────────────────────────────────────

    void off()    { setColor(0, 0, 0); }
    void red()    { setColor(255,   0,   0); }
    void green()  { setColor(  0, 255,   0); }
    void blue()   { setColor(  0,   0, 255); }
    void white()  { setColor(255, 255, 255); }
    void yellow() { setColor(255, 255,   0); }
    void orange() { setColor(255,  96,   0); }

    // ── Luminosité ───────────────────────────────────────────

    /**
     * Règle la luminosité globale (0–100 %).
     * La couleur courante est immédiatement mise à jour.
     */
    void setBrightness(uint8_t percent) {
        _brightness = constrain(percent, 0, 100);
        setColor(_r, _g, _b);  // réapplique avec nouvelle luminosité
    }

    // ── Accesseurs ───────────────────────────────────────────

    uint8_t getR()          const { return _r; }
    uint8_t getG()          const { return _g; }
    uint8_t getB()          const { return _b; }
    uint8_t getBrightness() const { return _brightness; }
};
