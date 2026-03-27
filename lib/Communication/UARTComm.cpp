/*
 * UARTComm.cpp
 * ------------
 * Implements UART communication conforming to ICommInterface.
 *
 * Protocol convention used here:
 *   - Messages are plain strings terminated by '\n'
 *   - sender calls send("HELLO") → wire carries "HELLO\n"
 *   - receiver accumulates bytes until '\n', then fires the callback
 *
 * This is the simplest possible framing strategy and is easy to match
 * on the Raspberry Pi side in C# (ReadLine() / split on '\n').
 */

#include "UARTComm.hpp"

UARTComm::UARTComm(HardwareSerial& serial, unsigned long baud_rate)
    : serial_(serial),
      baud_rate_(baud_rate),
      initialized_(false) {}

// ============================================================================
// ICommInterface implementation
// ============================================================================

void UARTComm::begin() {
    serial_.begin(baud_rate_);
    initialized_ = true;
    Serial.printf("[UART] Started at %lu baud.\n", baud_rate_);
}

void UARTComm::send(const std::string& data) {
    if (!initialized_) return;

    // Print the message followed by a newline so the receiver knows where it ends
    serial_.println(data.c_str());
}

bool UARTComm::isConnected() const {
    // UART is a physical wire — we assume it's connected once initialized.
    return initialized_;
}

void UARTComm::onReceive(std::function<void(const std::string&)> callback) {
    receive_callback_ = callback;
}

void UARTComm::onConnect(std::function<void()> callback) {
    // Stored for interface compliance, but UART has no connect event.
    connect_callback_ = callback;
}

void UARTComm::onDisconnect(std::function<void()> callback) {
    // Stored for interface compliance, but UART has no disconnect event.
    disconnect_callback_ = callback;
}

// ============================================================================
// UART-specific: must be polled to read incoming data
// ============================================================================

void UARTComm::update() {
    // Read all available bytes from the serial buffer
    while (serial_.available()) {
        char c = static_cast<char>(serial_.read());

        if (c == '\n') {
            // We have a complete message — fire the callback if registered
            if (!receive_buffer_.empty() && receive_callback_) {
                receive_callback_(receive_buffer_);
            }
            receive_buffer_.clear();  // Reset for the next message
        } else if (c != '\r') {
            // Accumulate the character (skip carriage return '\r' for Windows compatibility)
            receive_buffer_ += c;
        }
    }
}