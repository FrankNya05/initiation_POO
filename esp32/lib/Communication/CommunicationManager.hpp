#pragma once

#include "ICommInterface.hpp"
#include "RobotBLEServer.hpp"
#include "UARTComm.hpp"
#include "WiFiComm.hpp"

#include <memory>
#include <functional>
#include <string>

/*
 * CommunicationManager.hpp
 * -------------------------
 * Central hub for all robot ↔ HMI communication.
 * Implements the V1 MQTT protocol on top of WiFiComm.
 *
 * ── Design intent ──────────────────────────────────────────────────────────
 *
 * CommunicationManager is the single interface the rest of the robot uses
 * for all HMI communication. No other class needs to know about JSON,
 * MQTT topics, or ArduinoJson.
 *
 * It does three things:
 *   1. Wraps a WiFiComm transport (primary and only required transport).
 *   2. Exposes high-level outgoing methods (sendTelemetry, sendState, …).
 *   3. Parses incoming JSON and fires typed callbacks (onMotorCommand, onRobotCommand).
 *
 * JSON work is fully delegated to TelemetrySerializer and CommandParser.
 *
 * ── WiFiComm is the primary transport ─────────────────────────────────────
 *
 * CommunicationManager owns a WiFiComm instance. The V1 MQTT topics are
 * fixed at construction:
 *   publish  → "robot/telemetry"
 *   subscribe → "robot/cmd"
 *
 * An optional secondary ICommInterface* can be supplied (e.g. a BLE or UART
 * channel already instantiated elsewhere). If provided, incoming messages on
 * that channel are also parsed and routed. Outgoing messages always go through
 * WiFiComm. Ownership of the secondary channel stays with the caller.
 *
 * ── sendRaw() connection guard ─────────────────────────────────────────────
 *
 * sendRaw() checks isConnected() before forwarding to WiFiComm. If the MQTT
 * broker connection is not active the message is silently dropped and a LOG
 * warning is emitted. This prevents PubSubClient errors on disconnect.
 *
 * ── Decoupling from RobotContext ──────────────────────────────────────────
 *
 * sendTelemetry() takes a const TelemetryData& — the caller (task layer)
 * fills the struct from RobotContext and passes it in. CommunicationManager
 * never imports RobotContext.
 *
 * ── Logging ───────────────────────────────────────────────────────────────
 *
 * All log output uses LOG / LOGF from RTOSConfig.hpp.
 * No raw Serial calls are present in this file or its implementation.
 *
 * ── OOP concepts demonstrated ─────────────────────────────────────────────
 *   - Composition: owns WiFiComm via unique_ptr (RAII / clear ownership)
 *   - Polymorphism: secondary channel is ICommInterface* (any transport)
 *   - Encapsulation: JSON and protocol details invisible to callers
 *   - Delegation: formatting → TelemetrySerializer, parsing → CommandParser
 *   - Callbacks: std::function for flexible, decoupled event handling
 */
<<<<<<< HEAD
=======

// Identifies which channel is currently active.
// enum class prevents name collisions (no bare "BLE" in the global scope).
enum class CommChannel {
    BLE,
    WIFI,   // MQTT over WiFi — priorité intermédiaire
    UART
};

>>>>>>> origin/main
class CommunicationManager {
public:

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------
    // Creates a WiFiComm transport with the given network parameters.
    // Topics are fixed to V1 values at construction time.
    //
    // secondary (optional): an ICommInterface* whose incoming messages are
    // also parsed. Ownership stays with the caller. Pass nullptr if not needed.
    //
    // Example — WiFiComm only:
    //   CommunicationManager comm("MySSID", "mypassword", "192.168.1.100");
    //
    // Example — with a secondary receive channel:
    //   CommunicationManager comm("MySSID", "mypassword", "192.168.1.100",
    //                             1883, &mySecondaryChannel);
    // -----------------------------------------------------------------------
    CommunicationManager(const char*     ssid,
                         const char*     password,
                         const char*     brokerIp,
                         uint16_t        brokerPort = 1883,
                         ICommInterface* secondary  = nullptr);

    ~CommunicationManager() = default;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

<<<<<<< HEAD
    // Connects to WiFi and MQTT. Call once in setup().
    // Also registers callbacks on the secondary channel if one was provided.
=======
    // Manually select which channel should be active.
    // Typically called before begin(), or between matches.
    void selectChannel(CommChannel channel);

    // Configure le canal WiFi/MQTT (optionnel — appeler avant begin()).
    // Si non appelé, le canal WiFi reste désactivé.
    void configureWifi(const char* ssid,
                       const char* password,
                       const char* brokerIp,
                       uint16_t    brokerPort = 1883,
                       const char* topicPub   = "robot/data",
                       const char* topicSub   = "robot/commande");

    // Automatically pick the best available channel.
    // Priorité : BLE > WiFi > UART
    void autoSelect();

    // Enable or disable automatic channel switching inside update()
    void setAutoSelectEnabled(bool enabled);

    // Returns which channel is currently active
    CommChannel activeChannel() const;

    // -----------------------------------------------------------------------
    // ICommInterface delegation (forwarded to the active channel)
    // -----------------------------------------------------------------------

    // Initialize all owned channels and start the active one.
>>>>>>> origin/main
    void begin();

    // Must be called every loop iteration (or from a FreeRTOS task).
    // Keeps the MQTT connection alive and dispatches incoming messages.
    void update();

    // True when the MQTT broker connection is active.
    bool isConnected() const;

    // -----------------------------------------------------------------------
    // Transport event callbacks
    // -----------------------------------------------------------------------

    void onConnect(std::function<void()> callback);
    void onDisconnect(std::function<void()> callback);

    // -----------------------------------------------------------------------
    // Outgoing messages — high-level API
    // -----------------------------------------------------------------------

    // sendTelemetry
    // --------------
    // Serializes a pre-filled TelemetryData struct and sends it as a
    // TELEMETRY message over MQTT.
    //
    // The caller (task layer) fills the struct from RobotContext.
    // CommunicationManager only serializes and forwards.
    //
    // Silently dropped (with a LOG warning) if not connected.
    void sendTelemetry(const TelemetryData& data);

    // sendState
    // ----------
    // Sends a STATE message for the given firmware state enum value.
    // Call whenever the robot's state machine transitions.
    //
    // Silently dropped (with a LOG warning) if not connected.
    void sendState(RobotConstants::State state);

    // sendAck
    // --------
    // Sends an ACK message echoing the processed command string.
    // Called automatically by _handleIncoming() — rarely needed manually.
    //
    // Silently dropped (with a LOG warning) if not connected.
    void sendAck(const std::string& command);

    // sendLog
    // --------
    // Sends a LOG message with a freeform debug string (≤100 chars recommended).
    //
    // Silently dropped (with a LOG warning) if not connected.
    void sendLog(const std::string& message);

    // -----------------------------------------------------------------------
    // Incoming commands — typed callbacks
    // -----------------------------------------------------------------------

    // onMotorCommand
    // ---------------
    // Registers a callback that fires when a valid CMD_MOTOR message arrives.
    // The callback receives a MotorCommand struct — no JSON handling needed.
    //
    // Example:
    //   comm.onMotorCommand([](const MotorCommand& cmd) {
    //       if (cmd.action == "FORWARD") driver.setSpeed(cmd.speed, cmd.speed);
    //   });
    void onMotorCommand(std::function<void(const MotorCommand&)> callback);

    // onRobotCommand
    // ---------------
    // Registers a callback that fires when a valid CMD_ROBOT message arrives.
    // The callback receives a RobotCommand struct — no JSON handling needed.
    //
    // Example:
    //   comm.onRobotCommand([](const RobotCommand& cmd) {
    //       if (cmd.command == "START") robot.start();
    //   });
    void onRobotCommand(std::function<void(const RobotCommand&)> callback);

    // -----------------------------------------------------------------------
    // Raw send — low-level escape hatch
    // -----------------------------------------------------------------------
    // Includes a connection guard: does nothing if not connected.
    // Prefer the high-level methods above for all normal usage.
    void sendRaw(const std::string& data);

private:
<<<<<<< HEAD
=======
    // --- Owned channels ---
    // CommunicationManager is responsible for the lifetime of all channels.
    // unique_ptr guarantees they are destroyed when CommunicationManager is destroyed.
    // wifi_ reste nullptr si configureWifi() n'a pas été appelé.
    std::unique_ptr<RobotBLEServer> ble_;
    std::unique_ptr<WiFiComm>       wifi_;   // nullptr si WiFi non configuré
    std::unique_ptr<UARTComm>       uart_;
>>>>>>> origin/main

    // ── Primary transport: WiFiComm (owned) ───────────────────────────────
    std::unique_ptr<WiFiComm> wifi_;

    // ── Secondary transport: any ICommInterface (not owned, receive-only) ─
    // nullptr when no secondary channel is configured.
    ICommInterface* secondary_;

    // ── Stored transport callbacks ────────────────────────────────────────
    std::function<void()> connect_callback_;
    std::function<void()> disconnect_callback_;

    // ── Protocol-level typed callbacks ────────────────────────────────────
    std::function<void(const MotorCommand&)> motor_command_callback_;
    std::function<void(const RobotCommand&)> robot_command_callback_;

    // ── Internal helpers ──────────────────────────────────────────────────

    // Parses a raw incoming JSON string, routes to the typed callback,
    // and sends an automatic ACK. Registered as onReceive on all channels.
    void _handleIncoming(const std::string& raw);
};
