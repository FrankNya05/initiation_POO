#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "SensorTypes.hpp"
#include "RobotConstants.hpp"

// ═══════════════════════════════════════════════════════════════
//  RobotContext.hpp — État centralisé et thread-safe du robot
//
//  OOP appliqué :
//  - Singleton    : une seule instance accessible partout
//  - Encapsulation: données privées, accès via getters/setters
//  - Thread-safe  : mutex FreeRTOS sur chaque accès
//
//  Rôle dans l'architecture :
//  - taskSensors  → setSensorData (écrit)
//  - taskStrategy → getLidarData, getLineData, setState (lit + écrit)
//  - taskMotors   → getMotorSpeeds (lit)
//  - taskComm     → getBattData (lit)
// ═══════════════════════════════════════════════════════════════

class RobotContext {
public:

    // ── Singleton (Meyer's) ────────────────────────────────────
    //  Thread-safe en C++11 : l'initialisation statique locale
    //  est garantie atomique par le standard.
    static RobotContext& instance() {
        static RobotContext inst;
        return inst;
    }

    // Interdit la copie — un seul contexte robot
    RobotContext(const RobotContext&)            = delete;
    RobotContext& operator=(const RobotContext&) = delete;

    // ───────────────────────────────────────────────────────────
    //  État de la machine à états
    // ───────────────────────────────────────────────────────────

    void setState(RobotConstants::State s) {
        _lock();
        _state = s;
        _unlock();
    }

    RobotConstants::State getState() {
        _lock();
        auto s = _state;
        _unlock();
        return s;
    }

    // ───────────────────────────────────────────────────────────
    //  Données capteurs
    // ───────────────────────────────────────────────────────────

    // Lidar — distance + angle de l'adversaire le plus proche
    void setLidarData(const SensorData& d) {
        _lock();
        _lidarData = d;
        _unlock();
    }

    SensorData getLidarData() {
        _lock();
        auto d = _lidarData;
        _unlock();
        return d;
    }

    // Capteurs de ligne — par position (LEFT, RIGHT, BACK)
    void setLineData(SensorPosition pos, const SensorData& d) {
        _lock();
        _lineData[_posToIndex(pos)] = d;
        _unlock();
    }

    SensorData getLineData(SensorPosition pos) {
        _lock();
        auto d = _lineData[_posToIndex(pos)];
        _unlock();
        return d;
    }

    // Batterie
    void setBattData(const SensorData& d) {
        _lock();
        _battData = d;
        _unlock();
    }

    SensorData getBattData() {
        _lock();
        auto d = _battData;
        _unlock();
        return d;
    }

    // ───────────────────────────────────────────────────────────
    //  Commande moteurs (dernière commande envoyée)
    // ───────────────────────────────────────────────────────────

    void setMotorSpeeds(const RobotConstants::ActionCommand& cmd) {
        _lock();
        _motorSpeeds = cmd;
        _unlock();
    }

    RobotConstants::ActionCommand getMotorSpeeds() {
        _lock();
        auto cmd = _motorSpeeds;
        _unlock();
        return cmd;
    }

private:

    // ── Constructeur privé — crée le mutex ────────────────────
    RobotContext() {
        _mutex = xSemaphoreCreateMutex();
    }

    // ── Données internes ──────────────────────────────────────
    RobotConstants::State         _state       = RobotConstants::State::STANDBY;
    SensorData                    _lidarData;
    SensorData                    _lineData[3]; // [0]=LEFT [1]=RIGHT [2]=BACK
    SensorData                    _battData;
    RobotConstants::ActionCommand _motorSpeeds;

    // ── Mutex FreeRTOS ────────────────────────────────────────
    SemaphoreHandle_t _mutex = nullptr;

    // ── Helpers verrou ────────────────────────────────────────
    void _lock()   { if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY); }
    void _unlock() { if (_mutex) xSemaphoreGive(_mutex); }

    // ── Mapping position → index tableau ─────────────────────
    static uint8_t _posToIndex(SensorPosition pos) {
        switch (pos) {
            case SensorPosition::LEFT:  return 0;
            case SensorPosition::RIGHT: return 1;
            case SensorPosition::BACK:  return 2;
            default:                    return 0;
        }
    }
};
