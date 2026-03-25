#pragma once

#include <functional>  // For std::function
#include <string>

/*
 * ICommInterface.hpp
 * ------------------
 * Abstract interface (contract) for all communication channels.
 * 
 * OOP concepts used:
 *  - Abstract class (pure virtual functions)
 *  - Interface pattern: defines WHAT to do, not HOW
 *  - Virtual destructor: required for safe polymorphic deletion
 * 
 * Any communication class (BLE, UART, etc.) must implement all these methods.
 * The rest of the robot only depends on this interface, not on concrete classes.
 * This means you could swap BLE for UART without changing any other code.
 */
class ICommInterface {
public:
    // Virtual destructor: ensures proper cleanup when deleting via base pointer
    virtual ~ICommInterface() = default;

    // Initialize the communication channel (set up hardware, start services, etc.)
    virtual void begin() = 0;

    // Send a string message to the connected device
    virtual void send(const std::string& data) = 0;

    // Returns true if a remote device is currently connected
    virtual bool isConnected() const = 0;

    // Register a callback to be called when a message is received
    // The callback receives the message as a std::string
    virtual void onReceive(std::function<void(const std::string&)> callback) = 0;

    // Register a callback to be called when a device connects
    virtual void onConnect(std::function<void()> callback) = 0;

    // Register a callback to be called when a device disconnects
    virtual void onDisconnect(std::function<void()> callback) = 0;
};