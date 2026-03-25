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
      current_channel_(CommChannel::BLE),
      auto_select_enabled_(false)
{
    // Point active_comm_ at the default channel (BLE).
    // We do this via selectChannel() rather than directly, so the logic
    // lives in one place only.
    selectChannel(CommChannel::BLE);
}

// ============================================================================
// CHANNEL SELECTION
// ============================================================================

void CommunicationManager::selectChannel(CommChannel channel) {
    current_channel_ = channel;

    // Update active_comm_ to point at the right channel object.
    // This is the ONLY place in the codebase that assigns active_comm_.
    switch (channel) {
        case CommChannel::BLE:
            active_comm_ = ble_.get();
            Serial.println("[CommManager] Active channel: BLE");
            break;

        case CommChannel::UART:
            active_comm_ = uart_.get();
            Serial.println("[CommManager] Active channel: UART");
            break;
    }

    // Re-apply any callbacks that were registered before the switch.
    // This ensures the new channel fires the same callbacks as the old one.
    applyCallbacksTo(active_comm_);
}

void CommunicationManager::autoSelect() {
    // Strategy: prefer BLE when a client is connected, otherwise use UART.
    // This is a simple fallback policy — you can make it more sophisticated later.
    if (ble_->isConnected()) {
        if (current_channel_ != CommChannel::BLE) {
            Serial.println("[CommManager] autoSelect: switching to BLE.");
            selectChannel(CommChannel::BLE);
        }
    } else {
        if (current_channel_ != CommChannel::UART) {
            Serial.println("[CommManager] autoSelect: BLE not connected, switching to UART.");
            selectChannel(CommChannel::UART);
        }
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
    // Initialize both channels so they are ready to use.
    // Even if only one is active, the other should be initialized —
    // for example, BLE must be advertising even if we're currently on UART.
    Serial.println("[CommManager] Initializing BLE channel...");
    ble_->begin();

    Serial.println("[CommManager] Initializing UART channel...");
    uart_->begin();

    Serial.println("[CommManager] All channels initialized.");
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
    // UART does not generate hardware interrupts for received bytes — it must
    // be polled. We always poll it regardless of which channel is active,
    // so incoming bytes are never lost if the channel switches mid-message.
    uart_->update();

    // If auto-select is on, evaluate whether to switch channels this cycle.
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