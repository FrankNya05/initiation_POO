/*
 * CommunicationManager.cpp
 * -------------------------
 * See CommunicationManager.hpp for the full design explanation.
 *
 * Logging: all output uses LOG / LOGF from RTOSConfig.hpp.
 * No raw Serial calls are present in this file.
 */

#include "CommunicationManager.hpp"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

CommunicationManager::CommunicationManager(const char*     ssid,
                                           const char*     password,
                                           const char*     brokerIp,
                                           uint16_t        brokerPort,
                                           ICommInterface* secondary)
    : wifi_(std::make_unique<WiFiComm>(
          ssid,
          password,
          brokerIp,
          brokerPort,
          "robot/telemetry",   // V1 publish topic  (Robot → Pi)
          "robot/cmd"          // V1 subscribe topic (Pi → Robot)
      ))
    , secondary_(secondary)
    , motor_command_callback_(nullptr)
    , robot_command_callback_(nullptr)
{
    // Register _handleIncoming as the receive callback on the primary channel.
    // Done here so it is in place before begin() is called.
    wifi_->onReceive([this](const std::string& raw) {
        _handleIncoming(raw);
    });

<<<<<<< HEAD
    // If a secondary channel was provided, register the same receive handler.
    // Outgoing messages always go through wifi_.
    if (secondary_ != nullptr) {
        secondary_->onReceive([this](const std::string& raw) {
            _handleIncoming(raw);
        });
=======
// ============================================================================
// CHANNEL SELECTION
// ============================================================================

void CommunicationManager::configureWifi(const char* ssid,
                                         const char* password,
                                         const char* brokerIp,
                                         uint16_t    brokerPort,
                                         const char* topicPub,
                                         const char* topicSub)
{
    wifi_ = std::make_unique<WiFiComm>(ssid, password, brokerIp,
                                       brokerPort, topicPub, topicSub);
    Serial.println("[CommManager] WiFi/MQTT channel configured.");
}

void CommunicationManager::selectChannel(CommChannel channel) {
    // Si WiFi demandé mais non configuré, repli sur UART
    if (channel == CommChannel::WIFI && wifi_ == nullptr) {
        Serial.println("[CommManager] WiFi not configured, falling back to UART.");
        channel = CommChannel::UART;
    }

    current_channel_ = channel;

    switch (channel) {
        case CommChannel::BLE:
            active_comm_ = ble_.get();
            Serial.println("[CommManager] Active channel: BLE");
            break;

        case CommChannel::WIFI:
            active_comm_ = wifi_.get();
            Serial.println("[CommManager] Active channel: WiFi/MQTT");
            break;

        case CommChannel::UART:
            active_comm_ = uart_.get();
            Serial.println("[CommManager] Active channel: UART");
            break;
    }

    applyCallbacksTo(active_comm_);
}

void CommunicationManager::autoSelect() {
    // Priorité : BLE > WiFi/MQTT > UART
    if (ble_->isConnected()) {
        if (current_channel_ != CommChannel::BLE)
            selectChannel(CommChannel::BLE);
    } else if (wifi_ != nullptr && wifi_->isConnected()) {
        if (current_channel_ != CommChannel::WIFI)
            selectChannel(CommChannel::WIFI);
    } else {
        if (current_channel_ != CommChannel::UART)
            selectChannel(CommChannel::UART);
>>>>>>> origin/main
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void CommunicationManager::begin() {
<<<<<<< HEAD
    // Apply transport callbacks before connecting so they fire correctly.
    if (connect_callback_)    wifi_->onConnect(connect_callback_);
    if (disconnect_callback_) wifi_->onDisconnect(disconnect_callback_);

    // Connect to WiFi and MQTT (blocking until success or timeout).
    wifi_->begin();
=======
    Serial.println("[CommManager] Initializing BLE channel...");
    ble_->begin();

    if (wifi_ != nullptr) {
        Serial.println("[CommManager] Initializing WiFi/MQTT channel...");
        wifi_->begin();
    }

    Serial.println("[CommManager] Initializing UART channel...");
    uart_->begin();
>>>>>>> origin/main

    // Initialize the secondary channel if present.
    // Its failure must not block the primary path.
    if (secondary_ != nullptr) {
        if (connect_callback_)    secondary_->onConnect(connect_callback_);
        if (disconnect_callback_) secondary_->onDisconnect(disconnect_callback_);
        secondary_->begin();
    }
}

void CommunicationManager::update() {
    // WiFiComm::loop() keeps the MQTT connection alive and dispatches
    // received messages through the registered onReceive callback.
    wifi_->loop();

    // Note: the secondary channel (BLE or UART) may need its own polling.
    // UARTComm::update() and WiFiComm::loop() are not part of the
    // ICommInterface contract, so they cannot be called generically here.
    // If you use a secondary UART channel, call uart_->update() explicitly
    // in your task loop.
}

bool CommunicationManager::isConnected() const {
    return wifi_->isConnected();
}

// ============================================================================
// TRANSPORT EVENT CALLBACKS
// ============================================================================

void CommunicationManager::onConnect(std::function<void()> callback) {
    connect_callback_ = callback;
    wifi_->onConnect(callback);
    if (secondary_ != nullptr) secondary_->onConnect(callback);
}

void CommunicationManager::onDisconnect(std::function<void()> callback) {
    disconnect_callback_ = callback;
    wifi_->onDisconnect(callback);
    if (secondary_ != nullptr) secondary_->onDisconnect(callback);
}

// ============================================================================
// OUTGOING MESSAGES
// ============================================================================

<<<<<<< HEAD
void CommunicationManager::sendTelemetry(const TelemetryData& data) {
    std::string json = TelemetrySerializer::buildTelemetry(data);
    sendRaw(json);
}

void CommunicationManager::sendState(RobotConstants::State state) {
    std::string json = TelemetrySerializer::buildState(state);
    sendRaw(json);
}

void CommunicationManager::sendAck(const std::string& command) {
    std::string json = TelemetrySerializer::buildAck(command);
    sendRaw(json);
}

void CommunicationManager::sendLog(const std::string& message) {
    std::string json = TelemetrySerializer::buildLog(message);
    sendRaw(json);
}

void CommunicationManager::sendRaw(const std::string& data) {
    // Connection guard: do not attempt to publish when MQTT is not active.
    // PubSubClient silently fails in this case, which is hard to debug.
    // We make the drop explicit with a log warning instead.
    if (!wifi_->isConnected()) {
        LOG("[CommManager] sendRaw: not connected, message dropped.");
        return;
=======
void CommunicationManager::update() {
    // UART : polling permanent (pas d'interruption hardware sur réception)
    uart_->update();

    // WiFi/MQTT : maintenir la connexion et traiter les messages entrants.
    // loop() doit être appelé fréquemment — on le fait ici à chaque cycle taskComm.
    if (wifi_ != nullptr) {
        wifi_->loop();
    }

    if (auto_select_enabled_) {
        autoSelect();
>>>>>>> origin/main
    }
    wifi_->send(data);
}

// ============================================================================
// INCOMING COMMAND CALLBACKS
// ============================================================================

void CommunicationManager::onMotorCommand(std::function<void(const MotorCommand&)> callback) {
    motor_command_callback_ = callback;
}

void CommunicationManager::onRobotCommand(std::function<void(const RobotCommand&)> callback) {
    robot_command_callback_ = callback;
}

// ============================================================================
// PRIVATE — INCOMING MESSAGE HANDLER
// ============================================================================

void CommunicationManager::_handleIncoming(const std::string& raw) {
    // Step 1: extract the message type from the envelope.
    std::string type = CommandParser::parseType(raw);

    LOGF("[CommManager] RAW RX: %s\n", raw.c_str());
    
    if (type.empty()) {
        // CommandParser already logged the JSON error.
        return;
    }

    // Step 2: route to the correct parser, send ACK, fire typed callback.

    if (type == "CMD_MOTOR") {
        MotorCommand cmd;
        if (CommandParser::parseCmdMotor(raw, cmd)) {
            // ACK is sent before acting on the command so the HMI knows
            // reception was confirmed regardless of what the robot does next.
            sendAck(cmd.action);
            if (motor_command_callback_) motor_command_callback_(cmd);
        }
        return;
    }

    if (type == "CMD_ROBOT") {
        RobotCommand cmd;
        if (CommandParser::parseCmdRobot(raw, cmd)) {
            sendAck(cmd.command);
            if (robot_command_callback_) robot_command_callback_(cmd);
        }
        return;
    }

    // Unknown type — surface the mismatch for the developer.
    LOGF("[CommManager] Unknown message type: '%s'\n", type.c_str());
}
