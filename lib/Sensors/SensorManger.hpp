#pragma once
#include <vector>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
//  Thread-safe : mutex protège _sensors sur
//  toutes les lectures / écritures concurrentes
// ─────────────────────────────────────────────
class SensorManager {
public:

    // ── Cycle de vie ──────────────────────────

    SensorManager() {
        _mutex = xSemaphoreCreateMutex();
    }

    ~SensorManager() {
        for (auto& entry : _sensors) {
            delete entry.sensor;
        }
        if (_mutex) vSemaphoreDelete(_mutex);
    }

    // ── Enregistrement ────────────────────────

    /**
     * Ajoute un capteur (appelé avant les tâches — pas de mutex nécessaire).
     * @param sensor  Instance allouée dynamiquement — SensorManager en prend ownership.
     * @param enabled Actif dès l'ajout.
     */
    void add(SensorsInterface* sensor, bool enabled = true) {
        _sensors.push_back({sensor, enabled});
    }

    // ── Initialisation ────────────────────────

    /**
     * Initialise tous les capteurs actifs (appelé dans setup(), avant les tâches).
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

    // ── Activation ────────────────────────────

    /**
     * Active ou désactive un capteur à une position donnée.
     */
    void setEnabledByPosition(SensorPosition pos, bool state) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (auto& entry : _sensors) {
                if (entry.sensor->getData().position == pos) {
                    entry.enabled = state;
                }
            }
            xSemaphoreGive(_mutex);
        }
    }

    // ── Mise à jour ───────────────────────────

    /**
     * Met à jour tous les capteurs actifs.
     * Appelé depuis taskSensors (Core 1).
     * @return false si au moins un capteur a échoué.
     */
    bool updateAll() {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
        bool allOk = true;
        for (auto& entry : _sensors) {
            if (entry.enabled && !entry.sensor->update()) {
                allOk = false;
            }
        }
        xSemaphoreGive(_mutex);
        return allOk;
    }

    /**
     * Met à jour uniquement le capteur à une position donnée.
     */
    bool updateByPosition(SensorPosition pos) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
        bool allOk = true;
        for (auto& entry : _sensors) {
            if (entry.enabled && entry.sensor->getData().position == pos) {
                if (!entry.sensor->update()) allOk = false;
            }
        }
        xSemaphoreGive(_mutex);
        return allOk;
    }

    // ── Lecture ───────────────────────────────

    /**
     * Retourne un snapshot de toutes les données (capteurs actifs uniquement).
     * Thread-safe — snapshot copié sous mutex, rendu hors mutex.
     */
    std::vector<SensorData> getAllData() const {
        std::vector<SensorData> snapshot;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (const auto& entry : _sensors) {
                if (entry.enabled) snapshot.push_back(entry.sensor->getData());
            }
            xSemaphoreGive(_mutex);
        }
        return snapshot;
    }

    /**
     * Retourne la donnée d'un capteur à une position donnée.
     * @return SensorData avec isValid=false si non trouvé ou inactif.
     */
    SensorData getDataByPosition(SensorPosition pos) const {
        SensorData result{};   // isValid = false par défaut
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (const auto& entry : _sensors) {
                if (entry.enabled && entry.sensor->getData().position == pos) {
                    result = entry.sensor->getData();
                    break;
                }
            }
            xSemaphoreGive(_mutex);
        }
        return result;
    }

    // ── Debug ─────────────────────────────────

    void printAll() const {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
        for (const auto& entry : _sensors) {
            SensorData d = entry.sensor->getData();
            LOGF("[SensorManager] pos=%d enabled=%d valid=%d val=%.3f ts=%lu\n",
                (int)d.position,
                (int)entry.enabled,
                (int)d.isValid,
                d.value.scalar,
                d.timestamp);
        }
        xSemaphoreGive(_mutex);
    }

private:
    std::vector<SensorEntry>  _sensors;
    mutable SemaphoreHandle_t _mutex = nullptr;  // mutable → utilisable dans const
};
