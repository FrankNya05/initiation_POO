#pragma once

#include "ICommInterface.hpp"
#include <functional>
#include <string>

// ESP32 Arduino BLE library headers
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*
 * BLE UUID constants
 * ------------------
 * UUIDs uniquely identify a BLE service and its characteristics.
 * The Raspberry Pi HMI must use these same UUIDs to find and connect
 * to the robot's BLE service.
 *
 * Placed in a namespace to avoid polluting the global scope.
 * This demonstrates the NAMESPACE concept from the reference code.
 *
 * You can generate your own UUIDs at: https://www.uuidgenerator.net/
 */
namespace BLE_UUID {
    // The main service that groups all robot communication
    constexpr const char* SERVICE       = "12345678-1234-1234-1234-123456789abc";

    // Characteristic for sending data FROM the robot TO the HMI
    // The HMI subscribes to this to receive notifications
    constexpr const char* CHAR_NOTIFY   = "12345678-1234-1234-1234-123456789abd";

    // Characteristic for receiving data FROM the HMI TO the robot
    // The HMI writes to this to send commands
    constexpr const char* CHAR_WRITE    = "12345678-1234-1234-1234-123456789abe";

    // BLE device name visible during scanning
    constexpr const char* DEVICE_NAME   = "MiniSumoRobot";
}

/*
 * RobotBLEServer.hpp
 * ------------------
 * BLE implementation of the ICommInterface.
 * 
 * OOP concepts used:
 *  - Inheritance: inherits from ICommInterface
 *  - Polymorphism: overrides all pure virtual methods
 *  - Encapsulation: internal BLE objects are private
 *  - Inner classes: BLE callbacks are defined as nested classes
 *    (they need access to RobotBLEServer's private members via a pointer)
 *  - RAII: BLE resources are initialized in begin() and managed by the object
 *
 * Note on naming: We name this class RobotBLEServer (not BLEServer) to avoid
 * a naming conflict with the ESP32 library's own BLEServer class.
 */
class RobotBLEServer : public ICommInterface {
public:
    // Constructor: just stores the device name, no hardware init yet
    RobotBLEServer();

    // Destructor: stops advertising if needed
    ~RobotBLEServer() override;

    // --- ICommInterface overrides ---

    // Initializes the BLE stack, creates service and characteristics, starts advertising
    void begin() override;

    // Sends a string over BLE using notify on the TX characteristic
    void send(const std::string& data) override;

    // Returns true if a BLE client (HMI) is currently connected
    bool isConnected() const override;

    // Stores the callback to invoke when a message is received from the HMI
    void onReceive(std::function<void(const std::string&)> callback) override;

    // Stores the callback to invoke when a client connects
    void onConnect(std::function<void()> callback) override;

    // Stores the callback to invoke when a client disconnects
    void onDisconnect(std::function<void()> callback) override;

private:
    // --- Internal state ---
    bool connected_;

    // Raw BLE object pointers (managed by the ESP32 BLE library)
    BLEServer*         ble_server_;
    BLEService*        ble_service_;
    BLECharacteristic* char_notify_;   // For sending data to HMI (TX)
    BLECharacteristic* char_write_;    // For receiving data from HMI (RX)

    // --- Stored user callbacks ---
    // std::function can hold lambdas, free functions, or bound member functions
    std::function<void(const std::string&)> receive_callback_;
    std::function<void()>                   connect_callback_;
    std::function<void()>                   disconnect_callback_;

    // --- Internal methods ---
    void startAdvertising();

    // -------------------------------------------------------------------------
    // INNER CALLBACK CLASSES
    // -------------------------------------------------------------------------
    // The ESP32 BLE library requires you to subclass its callback types.
    // We define them here as inner classes so they can hold a pointer back
    // to the RobotBLEServer and call its stored callbacks when events occur.
    // This is the standard "bridge" pattern for the ESP32 BLE library.
    // -------------------------------------------------------------------------

    /*
     * ConnectionCallbacks
     * Handles BLE connect/disconnect events at the server level.
     */
    class ConnectionCallbacks : public BLEServerCallbacks {
    public:
        explicit ConnectionCallbacks(RobotBLEServer* parent);

        void onConnect(BLEServer* pServer) override;
        void onDisconnect(BLEServer* pServer) override;

    private:
        RobotBLEServer* parent_;  // Pointer back to RobotBLEServer
    };

    /*
     * WriteCallbacks
     * Handles data written by the HMI to the write characteristic.
     */
    class WriteCallbacks : public BLECharacteristicCallbacks {
    public:
        explicit WriteCallbacks(RobotBLEServer* parent);

        void onWrite(BLECharacteristic* pCharacteristic) override;

    private:
        RobotBLEServer* parent_;
    };
};