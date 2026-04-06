#pragma once
#include <vector>
#include "IDriverMotor.hpp"
#include "RobotConstants.hpp"
#include "RTOSConfig.hpp"

// ─────────────────────────────────────────────
//  DriverRole — rôle fonctionnel du driver
// ─────────────────────────────────────────────
enum class DriverRole {
    MAIN,       // driver principal (locomotion)
    SECONDARY,  // driver auxiliaire (bras, mécanisme...)
    UNKNOWN
};

// ─────────────────────────────────────────────
//  DriverEntry — wrapper interne
// ─────────────────────────────────────────────
struct DriverEntry {
    IDriverMotor* driver  = nullptr;
    DriverRole    role    = DriverRole::UNKNOWN;
    bool          enabled = true;
};

// ─────────────────────────────────────────────
//  DriverManager
// ─────────────────────────────────────────────
class DriverManager {
public:

    // ── Cycle de vie ──────────────────────────

    ~DriverManager() {
        for (auto& entry : _drivers) {
            delete entry.driver;
        }
    }

    // ── Enregistrement ────────────────────────

    /**
     * Ajoute un driver.
     * @param driver  Instance allouée dynamiquement — DriverManager en prend ownership.
     * @param role    Rôle fonctionnel du driver (MAIN par défaut).
     * @param enabled Actif dès l'ajout.
     */
    void add(IDriverMotor* driver,
             DriverRole role    = DriverRole::MAIN,
             bool       enabled = true)
    {
        _drivers.push_back({driver, role, enabled});
    }

    // ── Initialisation ────────────────────────

    /**
     * Initialise tous les drivers actifs.
     * @return false dès le premier échec.
     */
    bool initAll() {
        for (auto& entry : _drivers) {
            if (entry.enabled && !entry.driver->init()) {
                return false;
            }
        }
        return true;
    }

    // ── Commandes ─────────────────────────────

    /**
     * Applique une vitesse à tous les drivers actifs.
     * @param leftSpeed  -255 (arrière) → +255 (avant)
     * @param rightSpeed -255 (arrière) → +255 (avant)
     */
    void setSpeedAll(int leftSpeed, int rightSpeed) {
        for (auto& entry : _drivers) {
            if (entry.enabled) {
                entry.driver->setSpeed(leftSpeed, rightSpeed);
            }
        }
    }

    /**
     * Applique une vitesse au driver d'un rôle donné uniquement.
     */
    void setSpeedByRole(DriverRole role, int leftSpeed, int rightSpeed) {
        for (auto& entry : _drivers) {
            if (entry.enabled && entry.role == role) {
                entry.driver->setSpeed(leftSpeed, rightSpeed);
            }
        }
    }

    /**
     * Arrête tous les drivers actifs.
     */
    void stopAll() {
        for (auto& entry : _drivers) {
            if (entry.enabled) {
                entry.driver->stop();
            }
        }
    }

    /**
     * Arrête uniquement les drivers d'un rôle donné.
     */
    void stopByRole(DriverRole role) {
        for (auto& entry : _drivers) {
            if (entry.enabled && entry.role == role) {
                entry.driver->stop();
            }
        }
    }

    // ── Activation ────────────────────────────

    /**
     * Active ou désactive tous les drivers d'un rôle donné.
     */
    void setEnabledByRole(DriverRole role, bool state) {
        for (auto& entry : _drivers) {
            if (entry.role == role) {
                entry.enabled = state;
            }
        }
    }

    // ── Commandes haut niveau ─────────────────

    /**
     * Avance à la vitesse donnée (0–255).
     */
    void forward(int speed) {
        setSpeedByRole(DriverRole::MAIN, speed, speed);
    }

    /**
     * Recule à la vitesse donnée (0–255).
     */
    void backward(int speed) {
        setSpeedByRole(DriverRole::MAIN, -speed, -speed);
    }

    /**
     * Tourne à gauche sur place à la vitesse donnée (0–255).
     */
    void turnLeft(int speed) {
        setSpeedByRole(DriverRole::MAIN, -speed, speed);
    }

    /**
     * Tourne à droite sur place à la vitesse donnée (0–255).
     */
    void turnRight(int speed) {
        setSpeedByRole(DriverRole::MAIN, speed, -speed);
    }

    /**
     * Exécute une direction prédéfinie.
     */
    void move(RobotConstants::Direction dir, int speed = 200) {
        switch (dir) {
            case RobotConstants::Direction::AVANT:   forward(speed);   break;
            case RobotConstants::Direction::ARRIERE: backward(speed);  break;
            case RobotConstants::Direction::GAUCHE:  turnLeft(speed);  break;
            case RobotConstants::Direction::DROITE:  turnRight(speed); break;
        }
    }

    // ── Debug ─────────────────────────────────

    void printAll() const {
        for (const auto& entry : _drivers) {
            LOGF("[DriverManager] role=%d enabled=%d\n",
                (int)entry.role,
                (int)entry.enabled);
        }
    }

private:
    std::vector<DriverEntry> _drivers;
};
