namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// RobotStatus
// ---------------------------------------------------------------------------
// Mirrors the operating-state values the ESP32 sends in a STATE message:
//   { "type": "STATE", "payload": "RUNNING" }
//
// TelemetryParser converts the raw string payload into this enum.
// TelemetryPanelViewModel exposes the parsed value as an observable property.
//
// Unknown strings received from the robot are mapped to Unknown so the
// UI can display something meaningful without crashing.
// ---------------------------------------------------------------------------

public enum RobotStatus
{
    Unknown,
    Idle,
    Running,
    Searching,
    Attacking,
    Retreating,
    Error
}
