#pragma once
#include <Arduino.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════
//  EKF.hpp — Filtre de Kalman Étendu pour localisation 2D
//
//  État estimé : [x (mm), y (mm), θ (rad)]
//
//  Sources de mesure fusionnées :
//  - Encodeurs (odométrie différentielle) → prédiction
//  - IMU gz (gyroscope Z)                → correction du cap
//
//  Paramètres robot :
//  - Diamètre roue    : 50 mm
//  - Entraxe          : 56 mm
//  - Pulses/tour      : 2800 (7 * 4 * 100)
//
//  Usage :
//      EKF ekf;
//      ekf.reset();
//      ekf.predict(pulsesLeft, pulsesRight);   // depuis encodeurs
//      ekf.updateIMU(gz, dtSec);               // depuis IMU
//      EKF::Pose pose = ekf.getPose();
// ═══════════════════════════════════════════════════════════════

class EKF {
public:

    struct Pose {
        float x     = 0.0f;  // mm
        float y     = 0.0f;  // mm
        float theta = 0.0f;  // rad
    };

    EKF() { reset(); }

    // ── Remise à zéro ─────────────────────────────────────────
    void reset() {
        _x = _y = _theta = _thetaPrev = 0.0f;
        _cov[0][0] = 1.0f;  _cov[0][1] = 0.0f;  _cov[0][2] = 0.0f;
        _cov[1][0] = 0.0f;  _cov[1][1] = 1.0f;  _cov[1][2] = 0.0f;
        _cov[2][0] = 0.0f;  _cov[2][1] = 0.0f;  _cov[2][2] = 0.1f;
    }

    // ── Prédiction — odométrie encodeurs ─────────────────────
    //  Appeler à chaque cycle taskEncoders (200 Hz)
    //  @param pulsesLeft   impulsions encodeur gauche (signées, remises à 0 après)
    //  @param pulsesRight  impulsions encodeur droit  (signées, remises à 0 après)
    void predict(int32_t pulsesLeft, int32_t pulsesRight) {

        const float dl     = pulsesLeft  * MM_PER_PULSE;
        const float dr     = pulsesRight * MM_PER_PULSE;
        const float d      = (dl + dr) * 0.5f;
        const float dTheta = (dr - dl) / WHEELBASE;
        const float midTheta = _theta + dTheta * 0.5f;

        _thetaPrev = _theta;
        _x        += d * cosf(midTheta);
        _y        += d * sinf(midTheta);
        _theta     = _wrapAngle(_theta + dTheta);

        // Jacobien F = linearisation du modele de mouvement
        float F[3][3] = {
            { 1.0f, 0.0f, -d * sinf(midTheta) },
            { 0.0f, 1.0f,  d * cosf(midTheta) },
            { 0.0f, 0.0f,  1.0f               }
        };

        // Bruit de processus Q proportionnel au mouvement
        const float absD = fabsf(d) + fabsf(dTheta) * WHEELBASE * 0.5f;
        float Q[3][3] = {
            { _qXY * absD, 0.0f,        0.0f                             },
            { 0.0f,        _qXY * absD, 0.0f                             },
            { 0.0f,        0.0f,        _qTheta * fabsf(dTheta) + 1e-6f }
        };

        // P = F * P * F^T + Q
        float FP[3][3], FPFT[3][3];
        _mat3Mul(F, _cov, FP);
        _mat3MulT(FP, F, FPFT);
        _mat3Add(FPFT, Q, _cov);
    }

    // ── Mise à jour — gyroscope IMU ───────────────────────────
    //  Appeler à chaque cycle taskSensors (50 Hz)
    //  @param gz    vitesse angulaire Z en rad/s
    //  @param dtSec intervalle de temps en secondes
    void updateIMU(float gz, float dtSec) {

        const float zMeasured  = gz * dtSec;
        const float innovation = zMeasured - (_theta - _thetaPrev);

        // H = [0, 0, 1] → S = cov[2][2] + R
        const float S = _cov[2][2] + _rIMU;
        float K[3] = { _cov[0][2] / S, _cov[1][2] / S, _cov[2][2] / S };

        _x     += K[0] * innovation;
        _y     += K[1] * innovation;
        _theta  = _wrapAngle(_theta + K[2] * innovation);

        // P = (I - K*H) * P
        for (int i = 0; i < 3; i++) {
            _cov[i][0] -= K[i] * _cov[2][0];
            _cov[i][1] -= K[i] * _cov[2][1];
            _cov[i][2] -= K[i] * _cov[2][2];
        }
    }

    // ── Accesseurs ────────────────────────────────────────────
    Pose  getPose()             const { return { _x, _y, _theta }; }
    float getX()                const { return _x; }
    float getY()                const { return _y; }
    float getTheta()            const { return _theta; }
    float getUncertaintyX()     const { return _cov[0][0]; }
    float getUncertaintyY()     const { return _cov[1][1]; }
    float getUncertaintyTheta() const { return _cov[2][2]; }

    // ── Réglage des bruits (à calibrer selon le robot réel) ───
    void setProcessNoise(float qXY, float qTheta) { _qXY = qXY; _qTheta = qTheta; }
    void setMeasurementNoiseIMU(float rIMU)        { _rIMU = rIMU; }

private:

    float _x = 0.0f, _y = 0.0f, _theta = 0.0f, _thetaPrev = 0.0f;
    float _cov[3][3] = {};   // matrice de covariance (evite le conflit avec le macro _P de newlib)

    float _qXY    = 0.5f;   // bruit processus position  (mm²/mm parcouru)
    float _qTheta = 0.1f;   // bruit processus cap       (rad²/rad tourné)
    float _rIMU   = 0.01f;  // bruit mesure gyroscope    (rad²)

    static constexpr float WHEEL_DIAMETER = 50.0f;
    static constexpr float WHEELBASE      = 56.0f;
    static constexpr float PULSES_PER_REV = 2800.0f;
    static constexpr float MM_PER_PULSE   = (3.14159f * WHEEL_DIAMETER) / PULSES_PER_REV;

    static float _wrapAngle(float a) {
        while (a >  3.14159f) a -= 2.0f * 3.14159f;
        while (a < -3.14159f) a += 2.0f * 3.14159f;
        return a;
    }

    static void _mat3Mul(const float A[3][3], const float B[3][3], float C[3][3]) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                C[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) C[i][j] += A[i][k] * B[k][j];
            }
    }

    static void _mat3MulT(const float A[3][3], const float B[3][3], float C[3][3]) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                C[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) C[i][j] += A[i][k] * B[j][k];
            }
    }

    static void _mat3Add(const float A[3][3], const float B[3][3], float C[3][3]) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) C[i][j] = A[i][j] + B[i][j];
    }
};
