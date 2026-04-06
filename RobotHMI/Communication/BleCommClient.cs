using RobotHMI.Models;

namespace RobotHMI.Communication;

// ---------------------------------------------------------------------------
// BleCommClient — BLE implementation of ICommClient
// ---------------------------------------------------------------------------
// STATUS: STUBBED for Phase 1.
//
// What is real:
//   - Implements the full ICommClient contract
//   - Validates that the config is a BleConnectionConfig
//   - Simulates a successful connection after a short delay
//   - Tracks IsConnected state correctly
//
// What is stubbed (to be implemented in a later phase):
//   - Actual BLE scanning (BluetoothLEAdvertisementWatcher or equivalent)
//   - GATT service / characteristic discovery
//   - Subscribing to BLE notifications
//   - Writing to a BLE characteristic for outgoing messages
//
// When real BLE is added, ONLY the methods marked STUB need to change.
// The rest of the application is unaffected.
// ---------------------------------------------------------------------------

public class BleCommClient : ICommClient
{
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    private bool _isConnected = false;

    public bool IsConnected => _isConnected;

    // -----------------------------------------------------------------------
    // Event
    // -----------------------------------------------------------------------

    /// <summary>
    /// Fires when a JSON string arrives from the robot over BLE.
    /// In the real implementation this will be raised from a BLE notification
    /// callback, which runs on a background thread.
    /// </summary>
    public event EventHandler<string>? MessageReceived;

    // -----------------------------------------------------------------------
    // Connect
    // -----------------------------------------------------------------------

    public async Task ConnectAsync(ConnectionConfig config)
    {
        // Validate that the correct config type was provided.
        if (config is not BleConnectionConfig bleConfig)
            throw new ArgumentException(
                "BleCommClient requires a BleConnectionConfig.", nameof(config));

        Console.WriteLine($"[BLE] Connecting to device: '{bleConfig.DeviceName}' " +
                          $"(service: {bleConfig.ServiceUuid})");

        // STUB: Simulate connection delay.
        // Replace this block with real BLE scanning and GATT connection.
        await Task.Delay(800);

        _isConnected = true;
        Console.WriteLine("[BLE] Connected (stub).");
    }

    // -----------------------------------------------------------------------
    // Disconnect
    // -----------------------------------------------------------------------

    public async Task DisconnectAsync()
    {
        Console.WriteLine("[BLE] Disconnecting...");

        // STUB: Replace with real GATT disconnection.
        await Task.Delay(200);

        _isConnected = false;
        Console.WriteLine("[BLE] Disconnected (stub).");
    }

    // -----------------------------------------------------------------------
    // Send
    // -----------------------------------------------------------------------

    public async Task SendAsync(string message)
    {
        if (!_isConnected)
            throw new InvalidOperationException(
                "Cannot send: BLE client is not connected.");

        Console.WriteLine($"[BLE] Sending: {message}");

        // STUB: Replace with writing to the correct GATT characteristic.
        await Task.CompletedTask;
    }

    // -----------------------------------------------------------------------
    // Helper — raise MessageReceived (used by real BLE notification callback)
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called internally when a BLE notification arrives.
    /// In the real implementation, invoke this from the GATT notification handler.
    /// </summary>
    protected void OnMessageReceived(string message)
    {
        MessageReceived?.Invoke(this, message);
    }
}
