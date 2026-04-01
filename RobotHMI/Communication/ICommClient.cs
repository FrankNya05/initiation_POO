using RobotHMI.Models;

namespace RobotHMI.Communication;

// ---------------------------------------------------------------------------
// ICommClient — communication abstraction interface
// ---------------------------------------------------------------------------
// This interface is the ONLY way the rest of the application interacts with
// any communication protocol. BLE and MQTT details never leak above this line.
//
// Design rules (from architecture document §5.1):
//   - Messages are plain strings (JSON). No byte arrays.
//   - MessageReceived fires on a BACKGROUND thread.
//     Any ViewModel that handles this event must marshal to the UI thread
//     using Dispatcher.UIThread.InvokeAsync() before touching bound properties.
//   - ConnectAsync and SendAsync are async to avoid blocking the UI thread.
// ---------------------------------------------------------------------------

public interface ICommClient
{
    /// <summary>
    /// True when a connection to the robot is active.
    /// </summary>
    bool IsConnected { get; }

    /// <summary>
    /// Opens a connection to the robot using the provided configuration.
    /// The config type determines which protocol-specific fields are used.
    /// Throws on failure so the caller can display an error message.
    /// </summary>
    Task ConnectAsync(ConnectionConfig config);

    /// <summary>
    /// Closes the active connection gracefully.
    /// Safe to call even when already disconnected.
    /// </summary>
    Task DisconnectAsync();

    /// <summary>
    /// Sends a JSON string to the robot.
    /// The string must follow the unified message envelope:
    ///   { "type": "...", "payload": ... }
    /// </summary>
    Task SendAsync(string message);

    /// <summary>
    /// Raised every time a JSON string arrives from the robot.
    /// IMPORTANT: This event fires on a background thread.
    /// </summary>
    event EventHandler<string> MessageReceived;
}
