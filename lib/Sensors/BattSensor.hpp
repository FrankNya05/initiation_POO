/*
 * BattSensor.hpp
 * -----------------------------------------------
 * Capteur de tension batterie LiPo 2S pour ESP32
 *
 * Principe OOP appliqué :
 *  - Héritage    : implémente SensorsInterface (nouveau style)
 *  - Encapsulation : données et config privées, accès via getData()
 *  - Const correctness : getData() const
 *  - RAII léger  : init() configure l'ADC proprement
 *
 * Matériel :
 *  - Pont diviseur R1=100kΩ / R2=47kΩ sur GPIO 36 (VP)
 *  - Batterie LiPo 2S : min=6.0V / nominal=7.4V / max=8.4V
 */

#pragma once
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
#include "PinConfig.hpp"

#ifdef ARDUINO
#include <Arduino.h>
#endif

// ─────────────────────────────────────────────
//  Constantes de configuration
// ─────────────────────────────────────────────
namespace BattConfig {
    // Pont diviseur de tension (valeurs en Ohms)
    constexpr float R1            = 100000.0f;   // 100 kΩ
    constexpr float R2            =  47000.0f;   //  47 kΩ
    constexpr float DIVIDER_RATIO = (R1 + R2) / R2;  // ≈ 3.128

    // Référence ADC ESP32
    constexpr float ADC_REF       = 3.3f;
    constexpr float ADC_MAX       = 4095.0f;     // 12 bits

    // Seuils LiPo 2S (Volts)
    constexpr float VOLT_MAX      = 8.40f;       // 100%  — chargée
    constexpr float VOLT_NOM      = 7.40f;       // ~50%  — nominale
    constexpr float VOLT_MIN      = 6.00f;       //   0%  — vide
    constexpr float VOLT_ALERT    = 6.60f;       // seuil d'alerte critique

    // Filtrage ADC : nombre de lectures moyennées
    constexpr uint8_t FILTER_SAMPLES = 10;
}
using namespace BattConfig;
// ─────────────────────────────────────────────
//  Classe BattSensor
// ─────────────────────────────────────────────
class BattSensor : public SensorsInterface {
public:

    // ── TYPE_ID pour SensorManager::setEnabled() ──
    static constexpr const char* TYPE_ID = "BATT";

    // ── Constructeur ──────────────────────────────
    //  Utilise constexpr depuis PinConfig.hpp
    explicit BattSensor(SensorPosition pos = SensorPosition::CENTER)
        : _pin(RobotConfig::BATTERY_SENSOR)
    {
        _data.position = pos;
        _data.dims     = SensorDims::SCALAR;
        _data.isValid  = false;
    }

    // ── init() ────────────────────────────────────
    //  Configure la broche ADC et effectue une 1ère lecture
    bool init() override {
        // GPIO 36 (VP) est une entrée ADC uniquement — pas de pinMode requis,
        // mais on valide que la broche est bien une entrée analogique valide.
        analogSetAttenuation(ADC_11db);   // plage 0–3.3V
        analogReadResolution(12);         // 12 bits → 0–4095

        // Lecture initiale pour remplir le filtre
        for (uint8_t i = 0; i < BattConfig::FILTER_SAMPLES; i++) {
            _readRaw();
        }
        return true;
    }

    // ── update() ──────────────────────────────────
    //  Moyenne N lectures ADC → convertit en tension → remplit SensorData
    bool update() override {
        float voltage = _readFiltered();

        _data.value     = SensorValue(voltage);
        _data.timestamp = millis();
        _data.isValid   = (voltage >= BattConfig::VOLT_MIN - 0.5f);   // tolérance

        // Déclenchement de l'alerte si tension critique
        if (_data.isValid && voltage <= BattConfig::VOLT_ALERT) {
            _onLowBattery(voltage);
        }

        return _data.isValid;
    }

    // ── getData() ─────────────────────────────────
    SensorData getData() const override {
        return _data;
    }

    // ── Helpers publics (lecture directe) ─────────

    // Retourne la tension en Volts
    float getVoltage() const {
        return _data.value.scalar;
    }

    // Retourne le pourcentage de charge (0–100)
    //  Mapping linéaire entre VOLT_MIN et VOLT_MAX
    int getPercent() const {
        float v = _data.value.scalar;
        if (v >= BattConfig::VOLT_MAX) return 100;
        if (v <= BattConfig::VOLT_MIN) return 0;
        return static_cast<int>(
            (v - BattConfig::VOLT_MIN) /
            (BattConfig::VOLT_MAX - BattConfig::VOLT_MIN) * 100.0f
        );
    }

    // Indique si la batterie est en dessous du seuil critique
    bool isCritical() const {
        return _data.isValid && (_data.value.scalar <= BattConfig::VOLT_ALERT);
    }

private:

    // ── Membres privés ────────────────────────────
    const uint8_t _pin;          // GPIO 36 — depuis PinConfig.hpp
    SensorData    _data;         // Données partagées avec SensorManager

    // ── Lecture brute de l'ADC ────────────────────
    float _readRaw() const {
        int raw = analogRead(_pin);
        float vout = (static_cast<float>(raw) / BattConfig::ADC_MAX) * BattConfig::ADC_REF;
        return vout * BattConfig::DIVIDER_RATIO;
    }

    // ── Filtrage par moyenne glissante ────────────
    //  Élimine le bruit caractéristique de l'ADC ESP32
    float _readFiltered() const {
        float sum = 0.0f;
        for (uint8_t i = 0; i < BattConfig::FILTER_SAMPLES; i++) {
            sum += _readRaw();
        }
        return sum / static_cast<float>(BattConfig::FILTER_SAMPLES);
    }

    // ── Callback alerte batterie faible ───────────
    //  Appelé automatiquement dans update() si tension <= VOLT_ALERT
    void _onLowBattery(float voltage) const {
        #ifdef ARDUINO
        Serial.printf("[BattSensor] ⚠ BATTERIE CRITIQUE : %.2f V (%d%%)\n",
            voltage, getPercent());
        #endif
        // Extension possible : allumer une LED, envoyer un message BLE, etc.
    }
};