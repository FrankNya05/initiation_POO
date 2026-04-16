#pragma once
#include "ICommInterface.hpp"
#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include <string>

class WiFiComm : public ICommInterface {

private:
    const char*  _ssid;
    const char*  _password;
    const char*  _brokerIp;
    uint16_t     _brokerPort;
    const char*  _topicPub;
    const char*  _topicSub;

    WiFiClient   _wifiClient;
    mutable PubSubClient _client;

    // ── Callbacks enregistrés par l'utilisateur ──────────────────
    std::function<void(const std::string&)> _onReceiveCb    = nullptr;  // ✅ corrigé
    std::function<void()>                   _onConnectCb    = nullptr;
    std::function<void()>                   _onDisconnectCb = nullptr;

    //static WiFiComm* _instance;
    inline static WiFiComm* _instance = nullptr;

    // ── Callback statique PubSubClient ───────────────────────────
    static void _mqttCallback(char* topic, byte* payload, unsigned int length) {
        if (_instance == nullptr) return;
        if (_instance->_onReceiveCb == nullptr) return;

        std::string msg(reinterpret_cast<char*>(payload), length);
        _instance->_onReceiveCb(msg);
    }

    bool _connectWifi() {
        WiFi.begin(_ssid, _password);
        Serial.print("[WiFiComm] Connexion WiFi");
        int tentatives = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            if (++tentatives > 20) {
                Serial.println(" ❌ Échec WiFi !");
                return false;
            }
        }
        Serial.printf(" ✅ IP : %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    bool _connectMQTT() {
        int tentatives = 0;
        while (!_client.connected()) {
            if (_client.connect("ESP32-Robot-99")) {
                _client.subscribe(_topicSub);
                Serial.println("[WiFiComm] ✅ MQTT connecté !");
                if (_onConnectCb != nullptr) _onConnectCb();
                return true;
            }
            delay(1000);
            if (++tentatives > 5) {
                Serial.println("[WiFiComm] ❌ Échec MQTT !");
                return false;
            }
        }
        return true;
    }

public:

    WiFiComm(const char* ssid,
             const char* password,
             const char* brokerIp,
             uint16_t    brokerPort = 1883,
             const char* topicPub   = "robot/telemetry",
             const char* topicSub   = "robot/cmd")
        : _ssid(ssid)
        , _password(password)
        , _brokerIp(brokerIp)
        , _brokerPort(brokerPort)
        , _topicPub(topicPub)
        , _topicSub(topicSub)
        , _client(_wifiClient)
    {}

    // ── Interface UML ─────────────────────────────────────────────

    void begin() override {
        _instance = this;
        _client.setServer(_brokerIp, _brokerPort);
        _client.setCallback(_mqttCallback);
        _connectWifi();
        _connectMQTT();
    }

    void send(const std::string& data) override {
        if (!_client.connected()) _connectMQTT();
        _client.publish(_topicPub, data.c_str());
    }

    bool isConnected() const override {
        return _client.connected();
    }

    void onReceive(std::function<void(const std::string&)> cb) override {
        _onReceiveCb = cb;
    }

    void onConnect(std::function<void()> cb) override {
        _onConnectCb = cb;
    }

    void onDisconnect(std::function<void()> cb) override {
        _onDisconnectCb = cb;
    }

    // ── Extra : maintenir la connexion ────────────────────────────
    void loop() {
        if (!_client.connected()) {
            if (_onDisconnectCb != nullptr) _onDisconnectCb();
            _connectMQTT();
        }
        _client.loop();
    }
};

<<<<<<< HEAD
//WiFiComm* WiFiComm::_instance = nullptr;
=======
inline WiFiComm* WiFiComm::_instance = nullptr;
>>>>>>> origin/main
