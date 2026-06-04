#pragma once
#include <Arduino.h>
#include "driver/uart.h"
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

    constexpr float MIN_DIST_M = 0.15f;
    constexpr float MAX_DIST_M = 12.0f;

    constexpr uint8_t MIN_QUALITY = 8;

    constexpr uint32_t TOUR_MS = 150;
    constexpr uint32_t STARTUP_DELAY_MS = 100;

    // Correction d'orientation de montage (0° physique lidar → décalage mesuré)
    // Objet en face (devrait être 180°) apparaît à ~97° → offset = +83°
    constexpr float MOUNT_OFFSET_DEG = 83.0f;

    // Zone morte par défaut (repli si calibration échoue)
    // Mesuré sur CSV : bruit chassis concentré à 90-100° et 180-230°
    constexpr float BLIND_CENTER_DEG = 160.0f;  // mesuré sur scan brut : châssis 105°-225°
    constexpr float BLIND_HALF_DEG   =  65.0f;  // couvre 95°-225°

    // Calibration automatique de la zone morte
    constexpr uint8_t  CALIB_BINS        = 72;                    // 5° par bin
    constexpr float    CALIB_BIN_DEG     = 360.0f / CALIB_BINS;
    constexpr uint32_t CALIB_DURATION_MS = 2000;                  // durée de la phase de calibration
    constexpr float    CALIB_DEAD_RATIO  = 0.30f;                 // bin mort si < 30% du max
    constexpr float    BLIND_MIN_HALF    = 10.0f;                 // demi-largeur minimum
    constexpr float    BLIND_MAX_HALF    = 120.0f;                // demi-largeur maximum
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
        , _uartQueue(nullptr)
        , _calibDone(false)
        , _calibStartMs(0)
        , _blindCenter(LidarProtocol::BLIND_CENTER_DEG)
        , _blindHalf(LidarProtocol::BLIND_HALF_DEG)
    {}

    bool init() override {
        const uart_config_t cfg = {
            .baud_rate  = 230400,
            .data_bits  = UART_DATA_8_BITS,
            .parity     = UART_PARITY_DISABLE,
            .stop_bits  = UART_STOP_BITS_1,
            .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_APB,
        };
        uart_param_config(UART_NUM_2, &cfg);
        uart_set_pin(UART_NUM_2,
                     RobotConfig::TX_LIDAR, RobotConfig::RX_LIDAR,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(UART_NUM_2, 4096, 0, 20, &_uartQueue, 0);

        memset(_calibHits, 0, sizeof(_calibHits));
        _calibDone   = true;   // zone morte fixe mesurée empiriquement
        _blindCenter = LidarProtocol::BLIND_CENTER_DEG;
        _blindHalf   = LidarProtocol::BLIND_HALF_DEG;

        _startupTime = millis();
        _started = false;

        LOG("[LidarSensor] ESP-IDF UART DMA Init");
        return true;
    }

    // Non-blocking: vide le ring buffer sans attendre (garde la compatibilité SensorsInterface).
    bool update() override {
        _handleStartup();
        _checkCalibDone();
        bool gotPacket = false;
        uint8_t buf[128];
        int len = uart_read_bytes(UART_NUM_2, buf, sizeof(buf), 0);
        for (int i = 0; i < len; i++) {
            _processByte(buf[i]);
            if (_packetReady) {
                _parsePacket();
                _packetReady = false;
                gotPacket = true;
            }
        }
        _publishData();
        return gotPacket;
    }

    // Bloquant : réveille la tâche dès qu'un événement UART arrive (interrupt/DMA → queue).
    bool waitAndProcess(TickType_t timeout = pdMS_TO_TICKS(20)) {
        _handleStartup();
        _checkCalibDone();
        uart_event_t event;
        if (!xQueueReceive(_uartQueue, &event, timeout))
            return false;
        if (event.type != UART_DATA)
            return false;
        uint8_t buf[256];
        int len = uart_read_bytes(UART_NUM_2, buf,
                                  min((size_t)event.size, sizeof(buf)), 0);
        for (int i = 0; i < len; i++) {
            _processByte(buf[i]);
            if (_packetReady) {
                _parsePacket();
                _packetReady = false;
            }
        }
        _publishData();
        return true;
    }

    SensorData getData() const override {
        return _data;
    }

    void stop() {
        const uint8_t cmd[] = { LidarProtocol::CMD_PREFIX, LidarProtocol::CMD_STOP };
        uart_write_bytes(UART_NUM_2, cmd, sizeof(cmd));
        _started = false;
    }
    void start() {
    // Si déjà démarré, on ne fait rien
    if (_started) return;

        // Réinitialisation de la machine d'état de lecture des octets
        _state = State::WAIT_HEADER_1;
        _packetReady = false;

        // Réinitialisation des buffers de scan
        _resetScan(false);

        // Déclenche le mécanisme automatique de démarrage dans _handleStartup()
        _startupTime = millis();
        _started = false; 

    LOG("[LidarSensor] Requesting Start...");
    }
    bool  isCalibDone()     const { return _calibDone;   }
    float getBlindCenter()  const { return _blindCenter; }
    float getBlindHalf()    const { return _blindHalf;   }

    struct ScanPoint {
        float   angle;
        float   dist;
        uint8_t quality;
    };

    const ScanPoint* getRawScan()      const { return _rawScanPub;      }
    uint16_t         getRawScanCount() const { return _rawScanPubCount; }

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

    static constexpr uint16_t MAX_SCAN = 500;
    ScanPoint _rawScan[MAX_SCAN];        // buffer en cours de remplissage
    uint16_t  _rawScanCount = 0;
    ScanPoint _rawScanPub[MAX_SCAN];     // dernière révolution complète
    uint16_t  _rawScanPubCount = 0;

    uint32_t _lastResetMs;

    QueueHandle_t _uartQueue;

    SensorData _data;

    // ── Diagnostics checksum ──────────────────────
    uint32_t _csOkCount  = 0;
    uint32_t _csErrCount = 0;
    uint32_t _lastStatMs = 0;

    // ── Calibration zone morte ────────────────────
    uint16_t _calibHits[LidarProtocol::CALIB_BINS];
    bool     _calibDone;
    uint32_t _calibStartMs;
    float    _blindCenter;
    float    _blindHalf;

    // ─────────────────────────────────────────────
    //  Non-blocking startup (FreeRTOS safe)
    // ─────────────────────────────────────────────
    void _checkCalibDone() {
        if (_calibDone || !_started) return;
        if (millis() - _calibStartMs < LidarProtocol::CALIB_DURATION_MS) return;
        _finalizeCalib();
    }

    // ─────────────────────────────────────────────
    void _finalizeCalib() {
        // Trouver le maximum de hits pour normaliser
        uint16_t maxHits = 0;
        for (uint8_t i = 0; i < LidarProtocol::CALIB_BINS; i++)
            if (_calibHits[i] > maxHits) maxHits = _calibHits[i];

        if (maxHits == 0) {
            _calibDone = true;
            LOGF("[Lidar] Calib: aucune donnée — repli center=%.0f° half=±%.0f°\n",
                 _blindCenter, _blindHalf);
            return;
        }

        // Marquer les bins morts (trop peu de retours)
        bool dead[LidarProtocol::CALIB_BINS];
        uint16_t threshold = (uint16_t)(maxHits * LidarProtocol::CALIB_DEAD_RATIO);
        for (uint8_t i = 0; i < LidarProtocol::CALIB_BINS; i++)
            dead[i] = (_calibHits[i] <= threshold);

        // Trouver le plus long arc contigu de bins morts (tableau circulaire)
        uint8_t bestStart = 0, bestLen = 0, curLen = 0;
        for (uint8_t i = 0; i < 2 * LidarProtocol::CALIB_BINS; i++) {
            if (dead[i % LidarProtocol::CALIB_BINS]) {
                if (++curLen > bestLen) {
                    bestLen   = curLen;
                    bestStart = (uint8_t)((i - curLen + 1) % LidarProtocol::CALIB_BINS);
                }
            } else {
                curLen = 0;
            }
            if (curLen >= LidarProtocol::CALIB_BINS) break;
        }

        if (bestLen < 2) {
            _calibDone = true;
            LOGF("[Lidar] Calib: zone morte non détectée — repli center=%.0f° half=±%.0f°\n",
                 _blindCenter, _blindHalf);
            return;
        }

        // Calculer le centre et la demi-largeur de l'arc mort
        float halfLen = bestLen / 2.0f;
        float center  = fmod((bestStart + halfLen) * LidarProtocol::CALIB_BIN_DEG, 360.0f);
        float half    = halfLen * LidarProtocol::CALIB_BIN_DEG;

        // Sanity clamp
        if (half < LidarProtocol::BLIND_MIN_HALF) half = LidarProtocol::BLIND_MIN_HALF;
        if (half > LidarProtocol::BLIND_MAX_HALF) half = LidarProtocol::BLIND_MAX_HALF;

        _blindCenter = center;
        _blindHalf   = half;
        _calibDone   = true;

        LOGF("[Lidar] Calib OK — zone morte center=%.1f° half=±%.1f° (%d bins)\n",
             _blindCenter, _blindHalf, bestLen);
    }

    // ─────────────────────────────────────────────
    void _handleStartup() {

        if (_started) return;

        if (millis() - _startupTime > LidarProtocol::STARTUP_DELAY_MS) {

            const uint8_t cmd[] = { LidarProtocol::CMD_PREFIX, LidarProtocol::CMD_START };
            uart_write_bytes(UART_NUM_2, cmd, sizeof(cmd));

            _started      = true;
            _calibStartMs = millis();

            LOG("[LidarSensor] Scan Started — calibration zone morte...");
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
    void _resetScan(bool publishNow = false) {
        if (publishNow && _rawScanCount > 0) {
            _rawScanPubCount = _rawScanCount;
            memcpy(_rawScanPub, _rawScan, _rawScanCount * sizeof(ScanPoint));
        }
        _nearestDist  = LidarProtocol::MAX_DIST_M;
        _nearestAngle = 0;
        _validPoints  = 0;
        _rawScanCount = 0;
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

        bool revWrap = (angleStart < _lastAngle - 180.0f);
        bool timeout = (now - _lastResetMs) > LidarProtocol::TOUR_MS;
        if (revWrap) {
            // Nouvelle révolution : publier et tout réinitialiser
            _resetScan(true);
            _lastResetMs = now;
        } else if (timeout) {
            // Timeout : réinitialiser seulement le calcul "plus proche" sans toucher au raw scan
            _nearestDist  = LidarProtocol::MAX_DIST_M;
            _nearestAngle = 0;
            _validPoints  = 0;
            _lastResetMs  = now;
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

            float dist  = (_points[i].distRaw / 4.0f) / 1000.0f;
            float angle = angleStart + i * angleStep;
            angle += LidarProtocol::MOUNT_OFFSET_DEG;
            if (angle >= 360.0f) angle -= 360.0f;

            // Buffer brut : tous les points qualifiés (sans filtre distance ni zone morte)
            if (_rawScanCount < MAX_SCAN) {
                _rawScan[_rawScanCount++] = { angle, dist, _points[i].quality };
            }

            // Calcul nearest : appliquer la zone morte et le filtre distance
            if (dist < LidarProtocol::MIN_DIST_M || dist > LidarProtocol::MAX_DIST_M)
                continue;

            if (_calibDone) {
                float aDiff = angle - _blindCenter;
                if (aDiff >  180.0f) aDiff -= 360.0f;
                if (aDiff < -180.0f) aDiff += 360.0f;
                if (fabsf(aDiff) <= _blindHalf) continue;
            }

            _validPoints++;
            if (dist < _nearestDist) {
                _nearestDist  = dist;
                _nearestAngle = angle;
            }
        }

        _validPoints = min(_validPoints, 250);
    }
}; // end class LidarSensor
