using RobotHMI.Communication;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// RobotCommunicationService
// ---------------------------------------------------------------------------
// The single entry point for all communication in the application.
// ViewModels call this service to connect, disconnect, and send messages.
// They never interact with BleCommClient or MqttCommClient directly.
//
// Responsibilities (architecture document §5.4):
//   - Instantiate the correct ICommClient based on the chosen protocol
//   - Forward calls to the active client
//   - Re-fire MessageReceived so the rest of the app can subscribe to one place
//   - Track connection state
//
// Phase 1 scope:
//   - ConnectAsync / DisconnectAsync / IsConnected are fully implemented
//   - SendAsync is present but not yet called by any Phase 1 component
//   - MessageReceived is wired through but has no subscriber yet (Phase 2+)
// ---------------------------------------------------------------------------

public class RobotCommunicationService
{
    // -----------------------------------------------------------------------
    // Private state
    // -----------------------------------------------------------------------

    // The active communication client. Null when not connected.
    private ICommClient? _activeClient;

    // -----------------------------------------------------------------------
    // Public properties
    // -----------------------------------------------------------------------

    /// <summary>
    /// True when a client is active and reports a live connection.
    /// </summary>
    public bool IsConnected => _activeClient?.IsConnected ?? false;

    // -----------------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------------

    /// <summary>
    /// Raised whenever the active ICommClient receives a message from the robot.
    /// Fires on a background thread — subscribers must marshal to UI thread.
    /// Phase 2 will subscribe MessageRouter to this event.
    /// </summary>
    public event EventHandler<string>? MessageReceived;

    // -----------------------------------------------------------------------
    // ConnectAsync
    // -----------------------------------------------------------------------

    /// <summary>
    /// Creates the appropriate ICommClient for the chosen protocol,
    /// then connects using the provided configuration.
    /// Throws on connection failure — the ViewModel catches and displays the error.
    /// </summary>
    public async Task ConnectAsync(CommProtocol protocol, ConnectionConfig config)
    {
        // If already connected, disconnect the current client cleanly first.
        if (_activeClient != null)
            await DisconnectAsync();

        // Instantiate the correct client for the chosen protocol.
        _activeClient = protocol switch
        {
            CommProtocol.BLE  => new BleCommClient(),
            CommProtocol.MQTT => new MqttCommClient(),
            _ => throw new ArgumentOutOfRangeException(nameof(protocol),
                     $"Unknown protocol: {protocol}")
        };

        // Wire the client's MessageReceived event to our own so every
        // subscriber above this layer gets the message automatically.
        _activeClient.MessageReceived += OnClientMessageReceived;

        // Attempt the connection. Will throw on failure.
        await _activeClient.ConnectAsync(config);
    }

    // -----------------------------------------------------------------------
    // DisconnectAsync
    // -----------------------------------------------------------------------

    /// <summary>
    /// Disconnects the active client and clears the reference.
    /// Safe to call when already disconnected.
    /// </summary>
    public async Task DisconnectAsync()
    {
        if (_activeClient == null)
            return;

        // Unsubscribe before disconnecting to avoid stale events.
        _activeClient.MessageReceived -= OnClientMessageReceived;

        await _activeClient.DisconnectAsync();
        _activeClient = null;
    }

    // -----------------------------------------------------------------------
    // SendAsync
    // -----------------------------------------------------------------------

    /// <summary>
    /// Sends a JSON string to the robot through the active client.
    /// Used starting Phase 3 (motor commands). Defined now so the service
    /// contract is complete and Phase 3 requires no structural changes.
    /// </summary>
    public async Task SendAsync(string message)
    {
        if (_activeClient == null || !_activeClient.IsConnected)
            throw new InvalidOperationException(
                "Cannot send: no active connection.");

        await _activeClient.SendAsync(message);
    }

    // -----------------------------------------------------------------------
    // Private — forward client events
    // -----------------------------------------------------------------------

    private void OnClientMessageReceived(object? sender, string message)
    {
        // Re-fire on the same background thread.
        // Subscribers (MessageRouter in Phase 2) are responsible for
        // marshalling to the UI thread when needed.
        MessageReceived?.Invoke(this, message);
    }
}
