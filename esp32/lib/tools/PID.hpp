#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  PID.hpp — Correcteur PID générique
//
//  Utilisation typique :
//  - Contrôle de vitesse moteur (consigne RPM vs RPM mesuré)
//  - Maintien d'un cap (consigne angle IMU vs angle mesuré)
//  - Suivi de distance (consigne TOF vs distance mesurée)
//
//  Usage minimal :
//      PID pid(1.0f, 0.1f, 0.05f);       // kp, ki, kd
//      pid.setLimits(-255, 255);          // sortie clampée
//      pid.setSetpoint(100.0f);           // consigne (ex. 100 RPM)
//      float out = pid.compute(measured); // appeler à période fixe
// ═══════════════════════════════════════════════════════════════

class PID {
public:

    // ── Constructeur ──────────────────────────────────────────
    PID(float kp, float ki, float kd)
        : _kp(kp), _ki(ki), _kd(kd) {}

    // ── Paramètres ────────────────────────────────────────────

    void setGains(float kp, float ki, float kd) {
        _kp = kp; _ki = ki; _kd = kd;
    }

    // Consigne cible
    void setSetpoint(float sp) { _setpoint = sp; }
    float getSetpoint() const  { return _setpoint; }

    // Limites de sortie (anti-windup + saturation)
    void setLimits(float minOut, float maxOut) {
        _minOut = minOut;
        _maxOut = maxOut;
    }

    // ── Calcul ────────────────────────────────────────────────
    //  Appeler à période fixe (même intervalle que la tâche appelante)
    //  @param measured  Valeur mesurée actuelle
    //  @return          Commande corrigée, clampée dans [minOut, maxOut]

    float compute(float measured) {
        const uint32_t now = millis();
        const float dt = (now - _lastTime) / 1000.0f;  // secondes

        // Première appel — initialiser sans pic dérivé
        if (_firstCall) {
            _lastTime   = now;
            _lastError  = _setpoint - measured;
            _firstCall  = false;
            return 0.0f;
        }

        if (dt <= 0.0f) return _lastOutput;

        const float error = _setpoint - measured;

        // Proportionnel
        const float P = _kp * error;

        // Intégral avec anti-windup (clamping)
        _integral += error * dt;
        _integral  = _clamp(_integral, _minOut / _ki, _maxOut / _ki);
        const float I = _ki * _integral;

        // Dérivé sur la mesure (évite le "derivative kick" sur changement de consigne)
        const float D = -_kd * (measured - _lastMeasured) / dt;

        const float output = _clamp(P + I + D, _minOut, _maxOut);

        _lastError    = error;
        _lastMeasured = measured;
        _lastOutput   = output;
        _lastTime     = now;

        return output;
    }

    // ── Utilitaires ───────────────────────────────────────────

    // Remet l'intégrateur à zéro (utile au changement d'état)
    void reset() {
        _integral     = 0.0f;
        _lastError    = 0.0f;
        _lastMeasured = 0.0f;
        _lastOutput   = 0.0f;
        _firstCall    = true;
    }

    float getError()    const { return _lastError; }
    float getOutput()   const { return _lastOutput; }
    float getIntegral() const { return _integral; }

private:

    float _kp = 0.0f;
    float _ki = 0.0f;
    float _kd = 0.0f;

    float _setpoint    = 0.0f;
    float _integral    = 0.0f;
    float _lastError   = 0.0f;
    float _lastMeasured = 0.0f;
    float _lastOutput  = 0.0f;

    float _minOut = -255.0f;
    float _maxOut =  255.0f;

    uint32_t _lastTime  = 0;
    bool     _firstCall = true;

    static float _clamp(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
};
