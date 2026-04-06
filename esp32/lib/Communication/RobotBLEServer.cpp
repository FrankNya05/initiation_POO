/*
 * RobotBLEServer.cpp
 * ------------------
 * Implementation of the BLE communication channel.
 *
 * How BLE works on ESP32 (simplified):
 *  1. Initialize the BLE device with a name
 *  2. Create a BLEServer and attach connection callbacks to it
 *  3. Create a BLEService (a logical group of characteristics)
 *  4. Add two BLECharacteristics to the service:
 *       - NOTIFY characteristic: robot → HMI (we call notify() to push data)
 *       - WRITE characteristic:  HMI → robot  (we attach write callbacks)
 *  5. Start the service and begin advertising so the HMI can find the robot
 *
 * When the HMI connects, our ConnectionCallbacks::onConnect() fires.
 * When the HMI writes a command, our WriteCallbacks::onWrite() fires.
 * When we want to send data, we call char_notify_->setValue() + notify().
 */

#include "RobotBLEServer.hpp"
#include <Arduino.h>  // For Serial logging

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

RobotBLEServer::RobotBLEServer()
    : connected_(false),
      ble_server_(nullptr),
      ble_service_(nullptr),
      char_notify_(nullptr),
      char_write_(nullptr) {
    // Callbacks are default-constructed to empty std::function objects.
    // We check them before calling (see fireReceive etc.) to be safe.
}

RobotBLEServer::~RobotBLEServer() {
    // The ESP32 BLE library manages its own memory for BLEServer, BLEService,
    // and BLECharacteristic objects via BLEDevice. We don't delete them manually.
    // If needed in the future, call BLEDevice::deinit(true) here.
}

// ============================================================================
// ICommInterface IMPLEMENTATION
// ============================================================================

void RobotBLEServer::begin() {
    Serial.println("[BLE] Initializing...");

    // Step 1: Initialize the BLE stack with the device name
    BLEDevice::init(BLE_UUID::DEVICE_NAME);

    // Step 2: Create the BLE server and attach our connection callbacks
    ble_server_ = BLEDevice::createServer();
    ble_server_->setCallbacks(new ConnectionCallbacks(this));
    //                        ^^^
    // We pass a raw 'new' here because the ESP32 BLE library takes ownership
    // of this pointer and deletes it internally. This is a library design choice.

    // Step 3: Create the service
    ble_service_ = ble_server_->createService(BLE_UUID::SERVICE);

    // Step 4a: Create the NOTIFY characteristic (robot → HMI)
    // PROPERTY_NOTIFY: the robot can push data to the HMI without being asked
    // PROPERTY_READ:   the HMI can also read the last value at any time
    char_notify_ = ble_service_->createCharacteristic(
        BLE_UUID::CHAR_NOTIFY,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    // BLE2902 is the "Client Characteristic Configuration Descriptor" (CCCD).
    // The HMI must write to this descriptor to subscribe to notifications.
    // Without it, notify() calls won't actually send anything.
    char_notify_->addDescriptor(new BLE2902());

    // Step 4b: Create the WRITE characteristic (HMI → robot)
    // PROPERTY_WRITE:      the HMI sends data with a response acknowledgment
    // PROPERTY_WRITE_NR:   the HMI sends data without requesting a response (faster)
    char_write_ = ble_service_->createCharacteristic(
        BLE_UUID::CHAR_WRITE,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    char_write_->setCallbacks(new WriteCallbacks(this));

    // Step 5: Start the service and begin advertising
    ble_service_->start();
    startAdvertising();

    Serial.println("[BLE] Ready. Waiting for connection...");
}

void RobotBLEServer::send(const std::string& data) {
    if (!connected_ || char_notify_ == nullptr) {
        // Don't attempt to send if no one is connected
        return;
    }

    // Set the value on the characteristic and send a notification to the HMI
    char_notify_->setValue(data);
    char_notify_->notify();
}

bool RobotBLEServer::isConnected() const {
    return connected_;
}

void RobotBLEServer::onReceive(std::function<void(const std::string&)> callback) {
    receive_callback_ = callback;
}

void RobotBLEServer::onConnect(std::function<void()> callback) {
    connect_callback_ = callback;
}

void RobotBLEServer::onDisconnect(std::function<void()> callback) {
    disconnect_callback_ = callback;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void RobotBLEServer::startAdvertising() {
    BLEAdvertising* advertising = BLEDevice::getAdvertising();

    // Tell the advertising packet which service UUID to advertise.
    // This allows the HMI to filter and find this specific robot.
    advertising->addServiceUUID(BLE_UUID::SERVICE);

    // These settings improve iPhone/Android compatibility
    advertising->setScanResponse(false);
    advertising->setMinPreferred(0x0);

    BLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising started.");
}

// ============================================================================
// INNER CLASS: ConnectionCallbacks
// ============================================================================

RobotBLEServer::ConnectionCallbacks::ConnectionCallbacks(RobotBLEServer* parent)
    : parent_(parent) {}

void RobotBLEServer::ConnectionCallbacks::onConnect(BLEServer* pServer) {
    // Update the parent's connection state
    parent_->connected_ = true;
    Serial.println("[BLE] Client connected.");

    // Fire the user's onConnect callback if one was registered
    if (parent_->connect_callback_) {
        parent_->connect_callback_();
    }
}

void RobotBLEServer::ConnectionCallbacks::onDisconnect(BLEServer* pServer) {
    parent_->connected_ = false;
    Serial.println("[BLE] Client disconnected. Restarting advertising...");

    // Fire the user's onDisconnect callback
    if (parent_->disconnect_callback_) {
        parent_->disconnect_callback_();
    }

    // Restart advertising so the HMI can reconnect
    parent_->startAdvertising();
}

// ============================================================================
// INNER CLASS: WriteCallbacks
// ============================================================================

RobotBLEServer::WriteCallbacks::WriteCallbacks(RobotBLEServer* parent)
    : parent_(parent) {}

void RobotBLEServer::WriteCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    // Get the value written by the HMI as a std::string
    std::string received = pCharacteristic->getValue();

    if (received.empty()) {
        return;  // Ignore empty writes
    }

    Serial.printf("[BLE] Received: %s\n", received.c_str());

    // Fire the user's onReceive callback if one was registered
    if (parent_->receive_callback_) {
        parent_->receive_callback_(received);
    }
}