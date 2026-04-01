/*
 * main_example.cpp
 * ----------------
 * Shows how to wire up CommunicationManager in main.cpp.
 *
 * Notice: main.cpp knows nothing about BLE or UART internals.
 * It only interacts with CommunicationManager — that's the point of the abstraction.
 */

#include <Arduino.h>
#include "../../lib/Communication/CommunicationManager.hpp"

// Use Serial1 for UART communication with the Raspberry Pi (pins TX=17, RX=16 on ESP32)
CommunicationManager comm(Serial1, 115200);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // --- Register callbacks BEFORE begin() ---
    // These will be stored and applied to whichever channel is active.

    comm.onReceive([](const std::string& message) {
        Serial.printf("[main] Received: %s\n", message.c_str());

        if (message == "START") {
            Serial.println("[main] Starting robot...");
            // robot.start();
        } else if (message == "STOP") {
            Serial.println("[main] Stopping robot...");
            // robot.stop();
        }
    });

    comm.onConnect([]() {
        Serial.println("[main] HMI connected!");
        // e.g. set an LED to green
    });

    comm.onDisconnect([]() {
        Serial.println("[main] HMI disconnected.");
        // e.g. set an LED to red
    });

    // --- Option A: manually pick a channel ---
    comm.selectChannel(CommChannel::BLE);    // or CommChannel::UART

    // --- Option B: let CommunicationManager switch automatically ---
    // comm.setAutoSelectEnabled(true);      // autoSelect() will run inside update()

    // --- Initialize hardware ---
    comm.begin();
}

void loop() {
    // IMPORTANT: call update() regularly so:
    //  - UART bytes are read and the receive callback fires
    //  - autoSelect() runs if enabled
    comm.update();

    // Periodically send a status message to the HMI when connected
    static unsigned long last_send = 0;
    if (comm.isConnected() && millis() - last_send > 1000) {
        comm.send("STATUS:OK");
        last_send = millis();
    }
}