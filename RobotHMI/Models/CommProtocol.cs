namespace RobotHMI.Models;

/// <summary>
/// Identifies which communication protocol is active.
/// Used by ConnectionPanelViewModel to drive the protocol selector,
/// and by RobotCommunicationService to instantiate the right ICommClient.
/// </summary>
public enum CommProtocol
{
    BLE,
    MQTT
}
