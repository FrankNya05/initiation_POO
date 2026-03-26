#pragma once
#include <vector>
#include <cstring>
#include "SensorsInterface.hpp"
#include "RTOSConfig.hpp"
// ─────────────────────────────────────────────
//  SensorEntry — wrapper interne
// ─────────────────────────────────────────────
struct SensorEntry {
    SensorsInterface* sensor  = nullptr;
    bool              enabled = true;
};

// ─────────────────────────────────────────────
//  SensorManager
// ─────────────────────────────────────────────
class SensorManager {
public:

    // ── Cycle de vie ──────────────────────────

    ~SensorManager() {
        for (auto& entry : _sensors) {
            delete entry.sensor;
        }
    }

    // ── Enregistrement ────────────────────────

    /**
     * Ajoute un capteur.
     * @param sensor  Instance allouée dynamiquement — SensorManager en prend ownership.
     * @param enabled Actif dès l'ajout (false pour un capteur optionnel comme le LIDAR).
     */
    void add(SensorsInterface* sensor, bool enabled = true) {
        _sensors.push_back({sensor, enabled});
    }

    // ── Initialisation ────────────────────────

    /**
     * Initialise tous les capteurs actifs.
     * @return false dès le premier échec.
     */
    bool initAll() {
        for (auto& entry : _sensors) {
            if (entry.enabled && !entry.sensor->init()) {
                return false;
            }
        }
        return true;
    }


    /**
     * Active ou désactive un capteur à une position donnée.
     */
    void setEnabledByPosition(SensorPosition pos, bool state) {
        for (auto& entry : _sensors) {
            if (entry.sensor->getData().position == pos) {
                entry.enabled = state;
            }
        }
    }

    // ── Mise à jour ───────────────────────────

    /**
     * Met à jour tous les capteurs actifs.
     * @return false si au moins un capteur a échoué.
     */
    bool updateAll() {
        bool allOk = true;
        for (auto& entry : _sensors) {
            if (entry.enabled && !entry.sensor->update()) {
                allOk = false;
            }
        }
        return allOk;
    }

    /**
     * Met à jour uniquement le capteur à une position donnée.
     */
    bool updateByPosition(SensorPosition pos) {
        bool allOk = true;
        for (auto& entry : _sensors) {
            if (entry.enabled && entry.sensor->getData().position == pos) {
                if (!entry.sensor->update()) allOk = false;
            }
        }
        return allOk;
    }

    // ── Lecture ───────────────────────────────

    /**
     * Retourne un snapshot de toutes les données (capteurs actifs uniquement).
     */
    std::vector<SensorData> getAllData() const {
        std::vector<SensorData> all;
        for (const auto& entry : _sensors) {
            if (entry.enabled) {
                all.push_back(entry.sensor->getData());
            }
        }
        return all;
    }

    /**
     * Retourne la donnée d'un capteur à une position donnée.
     * @return nullptr si non trouvé ou inactif.
     */
// ✅ APRÈS
    SensorData getDataByPosition(SensorPosition pos) const {
        for (const auto& entry : _sensors) {
            if (entry.enabled && entry.sensor->getData().position == pos) {
                return entry.sensor->getData();
            }
        }
        return SensorData{};  // isValid = false par défaut
    }

    // ── Debug ─────────────────────────────────

    void printAll() const {
        for (const auto& entry : _sensors) {
            SensorData d = entry.sensor->getData();
            LOGF("[SensorManager] pos=%d enabled=%d valid=%d val=%.3f ts=%lu\n",
                (int)d.position,
                (int)entry.enabled,
                (int)d.isValid,
                d.value.scalar,
                d.timestamp);
        }
    }

private:
    std::vector<SensorEntry> _sensors;
};