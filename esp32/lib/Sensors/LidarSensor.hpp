#pragma once
#include <Arduino.h>
#include "SensorsInterface.hpp"
#include "PinConfig.hpp"
#include "RTOSConfig.hpp"

// ─────────────────────────────────────────────
//  YDLIDAR Tmini Plus — Ultra Stable FreeRTOS Ready
// ─────────────────────────────────────────────

namespace LidarProtocol {
    constexpr uint8_t  HEADER_1   = 0xAA;
    constexpr uint8_t  HEADER_2   = 0x55;
    constexpr uint8_t  CMD_PREFIX = 0xA5;
    constexpr uint8_t  CMD_START  = 0x60;
    constexpr uint8_t  CMD_STOP   = 0x65;

    constexpr uint8_t  MAX_POINTS = 40;

    constexpr float MIN_DIST_M = 0.05f;
    constexpr float MAX_DIST_M = 12.0f;

    constexpr uint8_t MIN_QUALITY = 8;

    constexpr uint32_t TOUR_MS = 150;
    constexpr uint32_t STARTUP_DELAY_MS = 100;

    // Correction d'orientation de montage (0° physique lidar → décalage mesuré)
    // Objet en face (devrait être 180°) apparaît à ~97° → offset = +83°
    constexpr float MOUNT_OFFSET_DEG = 83.0f;

    // Zone morte — châssis du robot bloque le scan autour de 0° (coordonnées corrigées)
    constexpr float BLIND_CENTER_DEG =   0.0f;  // centre de la zone masquée (arrière robot)
    constexpr float BLIND_HALF_DEG   =  80.0f;  // ±80° autour de 0° ignorés
}

class LidarSensor : public SensorsInterface {
public:

    LidarSensor()
        : _state(State::WAIT_HEADER_1)
        , _pktType(0)
        , _pktCount(0)
        , _pktIndex(0)
        , _byteInPt(0)
        , _angleRawStart(0)
        , _angleRawEnd(0)
        , _checksum(0)
        , _cs(0)
        , _packetReady(false)
        , _lastAngle(0)
        , _startupTime(0)
        , _started(false)
        , _nearestDist(LidarProtocol::MAX_DIST_M)
        , _nearestAngle(0)
        , _validPoints(0)
        , _lastResetMs(0)
    {}

    bool init() override {

        Serial2.setRxBufferSize(4096);

        Serial2.begin(
            230400,
            SERIAL_8N1,
            RobotConfig::RX_LIDAR,
            RobotConfig::TX_LIDAR
        );

        _startupTime = millis();
        _started = false;

        LOG("[LidarSensor] FreeRTOS Ready Init");

        return true;
    }

    bool update() override {

        _handleStartup();

        bool gotPacket = false;

        while (Serial2.available()) {

            _processByte(Serial2.read());

            if (_packetReady) {
                _parsePacket();
                _packetReady = false;
                gotPacket = true;
            }
        }

        _publishData();

        return gotPacket;
    }

    SensorData getData() const override {
        return _data;
    }

    void stop() {
        Serial2.write(LidarProtocol::CMD_PREFIX);
        Serial2.write(LidarProtocol::CMD_STOP);
    }

private:

    enum class State : uint8_t {
        WAIT_HEADER_1,
        WAIT_HEADER_2,
        READ_CT,
        READ_LSN,
        READ_FSA_L,
        READ_FSA_H,
        READ_LSA_L,
        READ_LSA_H,
        READ_CS_L,
        READ_CS_H,
        READ_POINTS
    };

    State _state;

    uint8_t _pktType;
    uint8_t _pktCount;
    uint8_t _pktIndex;
    uint8_t _byteInPt;

    uint16_t _angleRawStart;
    uint16_t _angleRawEnd;

    uint16_t _checksum;
    uint16_t _cs;

    bool _packetReady;

    float _lastAngle;

    uint32_t _startupTime;
    bool _started;

    struct RawPoint {
        uint8_t quality;
        uint16_t distRaw;
    } _points[LidarProtocol::MAX_POINTS];

    float _nearestDist;
    float _nearestAngle;

    int _validPoints;

    uint32_t _lastResetMs;

    SensorData _data;

    // ── Diagnostics checksum ──────────────────────
    uint32_t _csOkCount  = 0;
    uint32_t _csErrCount = 0;
    uint32_t _lastStatMs = 0;

    // ─────────────────────────────────────────────
    //  Non-blocking startup (FreeRTOS safe)
    // ─────────────────────────────────────────────
    void _handleStartup() {

        if (_started) return;

        if (millis() - _startupTime > LidarProtocol::STARTUP_DELAY_MS) {

            Serial2.write(LidarProtocol::CMD_PREFIX);
            Serial2.write(LidarProtocol::CMD_START);

            _started = true;

            LOG("[LidarSensor] Scan Started");
        }
    }

    // ─────────────────────────────────────────────
    void _publishData() {

        _data.position = SensorPosition::FRONT;
        _data.timestamp = millis();
        _data.dims = SensorDims::VEC3;

        if (_validPoints > 0) {
            _data.value = SensorValue(
                _nearestDist,
                _nearestAngle,
                (float)_validPoints
            );
            _data.isValid = true;
        }
        else {
            _data.isValid = false;
        }
    }

    // ─────────────────────────────────────────────
    void _processByte(uint8_t b) {

        switch (_state) {

        case State::WAIT_HEADER_1:
            if (b == LidarProtocol::HEADER_1)
                _state = State::WAIT_HEADER_2;
            break;

        case State::WAIT_HEADER_2:
            _state = (b == LidarProtocol::HEADER_2)
                ? State::READ_CT
                : State::WAIT_HEADER_1;
            break;

        case State::READ_CT:
            _pktType = b;
            _checksum = 0x55AA;
            _state = State::READ_LSN;
            break;

        case State::READ_LSN:
            _pktCount = b;
            _pktIndex = 0;
            _byteInPt = 0;
            _checksum ^= ((uint16_t)_pktCount << 8) | _pktType;
            _state = State::READ_FSA_L;
            break;

        case State::READ_FSA_L:
            _angleRawStart = b;
            _state = State::READ_FSA_H;
            break;

        case State::READ_FSA_H:
            _angleRawStart |= (uint16_t)b << 8;
            _checksum ^= _angleRawStart;
            _state = State::READ_LSA_L;
            break;

        case State::READ_LSA_L:
            _angleRawEnd = b;
            _state = State::READ_LSA_H;
            break;

        case State::READ_LSA_H:
            _angleRawEnd |= (uint16_t)b << 8;
            _checksum ^= _angleRawEnd;
            _state = State::READ_CS_L;
            break;

        case State::READ_CS_L:
            _cs = b;
            _state = State::READ_CS_H;
            break;

        case State::READ_CS_H:
            _cs |= (uint16_t)b << 8;
            _state = State::READ_POINTS;
            break;

        case State::READ_POINTS:
            _readPointByte(b);
            break;
        }
    }

    // ─────────────────────────────────────────────
    void _readPointByte(uint8_t b) {

        if (_pktIndex >= LidarProtocol::MAX_POINTS) {
            _state = State::WAIT_HEADER_1;
            return;
        }

        switch (_byteInPt) {

        case 0:
            _points[_pktIndex].quality = b;
            _checksum ^= b;
            _byteInPt = 1;
            break;

        case 1:
            _points[_pktIndex].distRaw = b;
            _byteInPt = 2;
            break;

        case 2:
            _points[_pktIndex].distRaw |= (uint16_t)b << 8;
            _checksum ^= _points[_pktIndex].distRaw;

            _byteInPt = 0;
            _pktIndex++;

            if (_pktIndex >= _pktCount) {
                _packetReady = true;
                _state = State::WAIT_HEADER_1;
            }

            break;
        }
    }

    // ─────────────────────────────────────────────
    void _resetScan() {

        _nearestDist  = LidarProtocol::MAX_DIST_M;
        _nearestAngle = 0;
        _validPoints  = 0;
    }

    // ─────────────────────────────────────────────
    void _parsePacket() {

        if (_checksum != _cs) {
            _csErrCount++;
            LOGF("[Lidar] CS FAIL  computed=0x%04X  received=0x%04X  delta=0x%04X\n",
                 _checksum, _cs, (uint16_t)(_checksum ^ _cs));
            _state = State::WAIT_HEADER_1;
            return;
        }

        _csOkCount++;

        // Rapport toutes les 5 secondes
        uint32_t nowStat = millis();
        if (nowStat - _lastStatMs >= 5000UL) {
            uint32_t total = _csOkCount + _csErrCount;
            LOGF("[Lidar] CS stats  OK=%lu  ERR=%lu  taux=%lu%%\n",
                 _csOkCount, _csErrCount,
                 total ? (_csErrCount * 100UL / total) : 0UL);
            _csOkCount  = 0;
            _csErrCount = 0;
            _lastStatMs = nowStat;
        }

        uint32_t now = millis();

        float angleStart = (_angleRawStart >> 1) / 64.0f;
        float angleEnd = (_angleRawEnd >> 1) / 64.0f;

        if (angleEnd < angleStart)
            angleEnd += 360.0f;

        if (angleStart < _lastAngle - 180.0f ||
            (now - _lastResetMs) > LidarProtocol::TOUR_MS)
        {
            _resetScan();
            _lastResetMs = now;
        }

        _lastAngle = angleStart;

        float angleStep = (_pktCount > 1)
            ? (angleEnd - angleStart) / (_pktCount - 1)
            : 0;

        for (uint8_t i = 0;
             i < _pktCount && i < LidarProtocol::MAX_POINTS;
             i++)
        {
            if (_points[i].quality < LidarProtocol::MIN_QUALITY)
                continue;

            float dist = (_points[i].distRaw / 4.0f) / 1000.0f;

            if (dist < LidarProtocol::MIN_DIST_M ||
                dist > LidarProtocol::MAX_DIST_M)
                continue;

            float angle = angleStart + i * angleStep;
            angle += LidarProtocol::MOUNT_OFFSET_DEG;
            if (angle >= 360.0f) angle -= 360.0f;

            // Rejeter les points dans la zone morte (châssis, coordonnées corrigées)
            float aDiff = angle - LidarProtocol::BLIND_CENTER_DEG;
            if (aDiff >  180.0f) aDiff -= 360.0f;
            if (aDiff < -180.0f) aDiff += 360.0f;
            if (fabsf(aDiff) <= LidarProtocol::BLIND_HALF_DEG) continue;

            _validPoints++;

            if (dist < _nearestDist) {
                _nearestDist  = dist;
                _nearestAngle = angle;
            }
        }

        _validPoints = min(_validPoints, 250);
    }
}; // end class LidarSensor
