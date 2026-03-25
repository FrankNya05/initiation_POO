#pragma once

#include "ICommInterface.hpp"
#include <Arduino.h>      // For HardwareSerial
#include <functional>
#include <string>

/*
 * UARTComm.hpp
 * ------------
 * UART (serial) implementation of ICommInterface.
 *
 * OOP concepts:
 *  - Inheritance: implements ICommInterface
 *  - Encapsulation: Serial port and callbacks are private
 *  - Polymorphism: can be used anywhere ICommInterface* is expected
 *
 * On the ESP32, you have three hardware UART ports:
 *  - Serial  (UART0) — usually used for USB debug output
 *  - Serial1 (UART1) — recommended for communication with Raspberry Pi
 *  - Serial2 (UART2) — alternative
 *
 * UARTComm wraps whichever HardwareSerial you pass in.
 * This makes it reusable and testable without hardcoding Serial1.
 *
 * Note: UART has no concept of "connect" or "disconnect" like BLE does.
 * isConnected() always returns true once begin() is called, because
 * UART is a physical wire — either it's wired or it isn't.
 * The onConnect/onDisconnect callbacks are accepted but never fired.
 */
class UARTComm : public ICommInterface {
public:
    // Constructor: takes a reference to the HardwareSerial port to use
    // and the baud rate. Default baud rate: 115200.
    explicit UARTComm(HardwareSerial& serial, unsigned long baud_rate = 115200);

    ~UARTComm() override = default;

    // --- ICommInterface overrides ---

    // Calls serial.begin() to start the UART port
    void begin() override;

    // Sends a string followed by a newline delimiter
    void send(const std::string& data) override;

    // UART has no handshake — returns true once begin() has been called
    bool isConnected() const override;

    // Stores the callback; must call update() in the main loop to trigger it
    void onReceive(std::function<void(const std::string&)> callback) override;

    // Accepted for interface compatibility, but never fires on UART
    void onConnect(std::function<void()> callback) override;

    // Accepted for interface compatibility, but never fires on UART
    void onDisconnect(std::function<void()> callback) override;

    // --- UART-specific method ---

    // Must be called periodically (e.g. from a FreeRTOS task or loop).
    // Reads any available bytes and fires receive_callback_ when a full line arrives.
    void update();

private:
    HardwareSerial& serial_;
    unsigned long   baud_rate_;
    bool            initialized_;

    std::string                             receive_buffer_;  // Accumulates incoming bytes
    std::function<void(const std::string&)> receive_callback_;
    std::function<void()>                   connect_callback_;
    std::function<void()>                   disconnect_callback_;
};