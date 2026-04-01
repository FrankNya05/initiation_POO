#pragma once

#include "ICommInterface.hpp"
#include "RobotBLEServer.hpp"
#include "UARTComm.hpp"

#include <memory>      // std::unique_ptr
#include <functional>
#include <string>

/*
 * CommunicationManager.hpp
 * -------------------------
 * Central hub for all robot communication.
 *
 * RESPONSIBILITIES:
 *  - Owns both communication channels (BLE and UART)
 *  - Selects which channel is currently active
 *  - Delegates all ICommInterface operations to the active channel
 *  - Provides autoSelect() to pick the channel automatically at runtime
 *  - Exposes a single, stable API to the rest of the robot
 *
 * WHY THIS CLASS EXISTS:
 *  The robot's logic should never care whether it's talking over BLE or UART.
 *  CommunicationManager hides that decision. If BLE drops and you switch to
 *  UART, nothing else in the code needs to change.
 *
 * -------------------------------------------------------------------------
 * KEY C++ DESIGN POINT — How active_comm_ is stored
 * -------------------------------------------------------------------------
 *
 * The UML shows _activeComm as a reference to ICommInterface.
 * In C++, you have three realistic options:
 *
 *   Option A — raw pointer:   ICommInterface* active_comm_
 *   Option B — reference:     ICommInterface& active_comm_   ← NOT usable here
 *   Option C — unique_ptr:    std::unique_ptr<ICommInterface> active_comm_
 *
 * Option B (reference) looks natural but is IMPOSSIBLE in this context:
 *   - A C++ reference must be bound at construction and CANNOT be rebound later.
 *   - Since autoSelect() needs to switch channels at runtime, a reference breaks.
 *   - References also cannot be null — but before any channel is ready, null is valid.
 *
 * Option C (unique_ptr) is ideal for OWNING objects, but here CommunicationManager
 *   already owns BLE and UART separately via their own unique_ptrs. active_comm_
 *   is just a VIEW into one of those — it should not own anything.
 *
 * Option A (raw pointer) is the correct choice here:
 *   - It can be reassigned (switch channels at runtime)    OK
 *   - It can be nullptr before initialization              OK
 *   - Lifetime is safe: ble_ and uart_ outlive it         OK
 *   - It does NOT own the object — ownership stays with unique_ptrs  OK
 *
 * This is a standard C++ idiom: unique_ptr for ownership, raw pointer for observation.
 *
 * -------------------------------------------------------------------------
 * OOP CONCEPTS DEMONSTRATED:
 *  - Composition: owns BLE and UART via unique_ptr (RAII)
 *  - Polymorphism: active_comm_ is ICommInterface*, works with any concrete type
 *  - Encapsulation: channel switching logic is hidden from callers
 *  - Abstraction: callers only see send(), isConnected(), onReceive()
 */

// Identifies which channel is currently active.
// enum class prevents name collisions (no bare "BLE" in the global scope).
enum class CommChannel {
    BLE,
    UART
};

class CommunicationManager {
public:
    // Constructor: creates both channels, defaults to BLE as the active one.
    //   uart_serial : which hardware serial port to use (e.g. Serial1 for Raspberry Pi)
    //   uart_baud   : baud rate for UART (must match the Raspberry Pi setting)
    explicit CommunicationManager(HardwareSerial& uart_serial = Serial1,
                                  unsigned long uart_baud = 115200);

    // Destructor: unique_ptrs clean up automatically (RAII — nothing manual needed)
    ~CommunicationManager() = default;

    // -----------------------------------------------------------------------
    // Channel selection
    // -----------------------------------------------------------------------

    // Manually select which channel should be active.
    // Typically called before begin(), or between matches.
    void selectChannel(CommChannel channel);

    // Automatically pick the best available channel.
    // Logic: if BLE is connected → use BLE. Otherwise → fall back to UART.
    // Call this periodically for dynamic failover.
    void autoSelect();

    // Enable or disable automatic channel switching inside update()
    void setAutoSelectEnabled(bool enabled);

    // Returns which channel is currently active
    CommChannel activeChannel() const;

    // -----------------------------------------------------------------------
    // ICommInterface delegation (forwarded to the active channel)
    // -----------------------------------------------------------------------

    // Initialize all owned channels and start the active one.
    void begin();

    // Send a message through the active channel.
    void send(const std::string& data);

    // Returns true if the active channel has a live connection.
    bool isConnected() const;

    // Register what to do when a message arrives.
    void onReceive(std::function<void(const std::string&)> callback);

    // Register what to do when a device connects.
    void onConnect(std::function<void()> callback);

    // Register what to do when a device disconnects.
    void onDisconnect(std::function<void()> callback);

    // -----------------------------------------------------------------------
    // Polling — call from a FreeRTOS task or the Arduino loop()
    // -----------------------------------------------------------------------

    // Handles periodic work:
    //   - Reads incoming UART bytes and fires the receive callback
    //   - Runs autoSelect() if auto-select is enabled
    void update();

private:
    // --- Owned channels ---
    // CommunicationManager is responsible for the lifetime of both channels.
    // unique_ptr guarantees they are destroyed when CommunicationManager is destroyed.
    std::unique_ptr<RobotBLEServer> ble_;
    std::unique_ptr<UARTComm>       uart_;

    // --- Active channel pointer (raw pointer = observer, NOT owner) ---
    //
    // Points to either ble_.get() or uart_.get().
    // NEVER call delete on this — ownership stays with the unique_ptrs above.
    ICommInterface* active_comm_;

    // --- Internal state ---
    CommChannel current_channel_;
    bool        auto_select_enabled_;

    // Stored callbacks — kept here so they survive a channel switch.
    // When selectChannel() is called, these are re-applied to the new channel.
    std::function<void(const std::string&)> receive_callback_;
    std::function<void()>                   connect_callback_;
    std::function<void()>                   disconnect_callback_;

    // --- Internal helpers ---

    // Applies the stored callbacks to a given channel interface pointer.
    void applyCallbacksTo(ICommInterface* channel);
};