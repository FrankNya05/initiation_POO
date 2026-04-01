using RobotHMI.Models;

namespace RobotHMI.Communication;

// ---------------------------------------------------------------------------
// MqttCommClient — MQTT implementation of ICommClient
// ---------------------------------------------------------------------------
// STATUS: STUBBED for Phase 1.
//
// What is real:
//   - Implements the full ICommClient contract
//   - Validates that the config is an MqttConnectionConfig
//   - Simulates a successful broker connection after a short delay
//   - Tracks IsConnected state correctly
//
// What is stubbed (to be implemented in a later phase):
//   - Actual TCP connection to the MQTT broker (e.g., using MQTTnet library)
//   - Subscribing to the telemetry topic
//   - Publishing to the command topic
//   - Running the MQTT keep-alive / loop on a background thread
//
// Recommended MQTT library for .NET / Avalonia: MQTTnet (NuGet)
// When real MQTT is added, ONLY the methods marked STUB need to change.
// ---------------------------------------------------------------------------

public class MqttCommClient : ICommClient
{
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    private bool _isConnected = false;
    private MqttConnectionConfig? _activeConfig;

    public bool IsConnected => _isConnected;

    // -----------------------------------------------------------------------
    // Event
    // -----------------------------------------------------------------------

    /// <summary>
    /// Fires when a JSON string arrives from the robot over MQTT.
    /// In the real implementation this is raised from the MQTTnet message
    /// received callback, which runs on a background thread.
    /// </summary>
    public event EventHandler<string>? MessageReceived;

    // -----------------------------------------------------------------------
    // Connect
    // -----------------------------------------------------------------------

    public async Task ConnectAsync(ConnectionConfig config)
    {
        // Validate that the correct config type was provided.
        if (config is not MqttConnectionConfig mqttConfig)
            throw new ArgumentException(
                "MqttCommClient requires an MqttConnectionConfig.", nameof(config));

        _activeConfig = mqttConfig;

        Console.WriteLine($"[MQTT] Connecting to broker: {mqttConfig.BrokerIp}:{mqttConfig.Port}");
        Console.WriteLine($"[MQTT] Telemetry topic : {mqttConfig.TelemetryTopic}");
        Console.WriteLine($"[MQTT] Command topic   : {mqttConfig.CommandTopic}");

        // STUB: Simulate broker connection delay.
        // Replace with real MQTTnet client connection and topic subscription.
        await Task.Delay(800);

        _isConnected = true;
        Console.WriteLine("[MQTT] Connected (stub).");
    }

    // -----------------------------------------------------------------------
    // Disconnect
    // -----------------------------------------------------------------------

    public async Task DisconnectAsync()
    {
        Console.WriteLine("[MQTT] Disconnecting from broker...");

        // STUB: Replace with real MQTTnet client disconnect.
        await Task.Delay(200);

        _isConnected = false;
        _activeConfig = null;
        Console.WriteLine("[MQTT] Disconnected (stub).");
    }

    // -----------------------------------------------------------------------
    // Send
    // -----------------------------------------------------------------------

    public async Task SendAsync(string message)
    {
        if (!_isConnected || _activeConfig == null)
            throw new InvalidOperationException(
                "Cannot send: MQTT client is not connected.");

        Console.WriteLine($"[MQTT] Publishing to '{_activeConfig.CommandTopic}': {message}");

        // STUB: Replace with real MQTTnet publish call.
        await Task.CompletedTask;
    }

    // -----------------------------------------------------------------------
    // Helper — raise MessageReceived (used by real MQTT message callback)
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called internally when a message arrives on the subscribed topic.
    /// In the real implementation, invoke this from the MQTTnet
    /// ApplicationMessageReceivedAsync handler.
    /// </summary>
    protected void OnMessageReceived(string message)
    {
        MessageReceived?.Invoke(this, message);
    }
}
