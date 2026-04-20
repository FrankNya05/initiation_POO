/*
 * CommunicationManager.cpp
 * -------------------------
 * Implements channel ownership, selection, delegation, and auto-selection.
 */

#include "CommunicationManager.hpp"
#include <Arduino.h>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

CommunicationManager::CommunicationManager(HardwareSerial& uart_serial,
                                           unsigned long uart_baud)
    : ble_(std::make_unique<RobotBLEServer>()),
      uart_(std::make_unique<UARTComm>(uart_serial, uart_baud)),
      active_comm_(nullptr),          // Not yet pointing anywhere — set in selectChannel()
      current_channel_(CommChannel::WIFI),
      auto_select_enabled_(false)
{
    // wifi_ n'est pas encore configuré ici → selectChannel() replie sur UART.
    // Dès que configureWifi() + selectChannel(WIFI) sont appelés, le canal actif
    // bascule automatiquement sur MQTT.
    selectChannel(CommChannel::WIFI);
}

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
    }
}

void CommunicationManager::setAutoSelectEnabled(bool enabled) {
    auto_select_enabled_ = enabled;
    Serial.printf("[CommManager] Auto-select %s.\n", enabled ? "enabled" : "disabled");
}

CommChannel CommunicationManager::activeChannel() const {
    return current_channel_;
}

// ============================================================================
// ICommInterface DELEGATION
// ============================================================================

void CommunicationManager::begin() {
    switch (current_channel_) {
        case CommChannel::BLE:
            Serial.println("[CommManager] Initializing BLE channel...");
            ble_->begin();
            break;
        case CommChannel::WIFI:
            if (wifi_ != nullptr) {
                Serial.println("[CommManager] Initializing WiFi/MQTT channel...");
                wifi_->begin();
            }
            break;
        case CommChannel::UART:
            Serial.println("[CommManager] Initializing UART channel...");
            uart_->begin();
            break;
    }
    Serial.println("[CommManager] Channel initialized.");
}

void CommunicationManager::send(const std::string& data) {
    if (active_comm_ == nullptr) {
        Serial.println("[CommManager] ERROR: send() called but no active channel.");
        return;
    }
    active_comm_->send(data);
}

bool CommunicationManager::isConnected() const {
    if (active_comm_ == nullptr) return false;
    return active_comm_->isConnected();
}

void CommunicationManager::onReceive(std::function<void(const std::string&)> callback) {
    receive_callback_ = callback;           // Store for re-application after channel switch
    applyCallbacksTo(active_comm_);         // Apply immediately to the current channel
}

void CommunicationManager::onConnect(std::function<void()> callback) {
    connect_callback_ = callback;
    applyCallbacksTo(active_comm_);
}

void CommunicationManager::onDisconnect(std::function<void()> callback) {
    disconnect_callback_ = callback;
    applyCallbacksTo(active_comm_);
}

// ============================================================================
// POLLING
// ============================================================================

void CommunicationManager::update() {
    switch (current_channel_) {
        case CommChannel::UART:
            uart_->update();
            break;
        case CommChannel::WIFI:
            if (wifi_ != nullptr) wifi_->loop();
            break;
        default:
            break;
    }

    if (auto_select_enabled_) {
        autoSelect();
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void CommunicationManager::applyCallbacksTo(ICommInterface* channel) {
    // Safety: do nothing if the pointer is null or no callbacks are set yet.
    if (channel == nullptr) return;

    // Only apply callbacks that have actually been registered.
    // An empty std::function evaluates to false in a boolean context.
    if (receive_callback_)    channel->onReceive(receive_callback_);
    if (connect_callback_)    channel->onConnect(connect_callback_);
    if (disconnect_callback_) channel->onDisconnect(disconnect_callback_);
}