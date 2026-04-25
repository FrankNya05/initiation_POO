namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// Sub-models for V1 telemetry payload sections
// ---------------------------------------------------------------------------
// Each class is a plain POCO (no logic, no dependencies).
// They mirror the JSON sections inside the TELEMETRY payload exactly.
// ---------------------------------------------------------------------------

/// <summary>
/// Battery information from the TELEMETRY payload.
/// JSON: { "voltage": 7.4, "percent": 85, "critical": false }
/// </summary>
public class BatteryData
{
    /// <summary>Battery voltage in volts (e.g. 7.4).</summary>
    public float Voltage  { get; set; }

    /// <summary>State of charge, 0–100.</summary>
    public int   Percent  { get; set; }

    /// <summary>True when the battery is critically low.</summary>
    public bool  Critical { get; set; }
}

/// <summary>
/// Line sensor readings from the TELEMETRY payload.
/// JSON: { "frontLeft": bool, "frontRight": bool, "backLeft": bool, "back": bool }
/// Note: the ESP32 V1 protocol uses "back" (single) for the rear sensor.
/// </summary>
public class LineSensorData
{
    public bool FrontLeft  { get; set; }
    public bool FrontRight { get; set; }
    public bool BackLeft   { get; set; }

    /// <summary>"back" in the JSON — single rear sensor in V1 protocol.</summary>
    public bool Back       { get; set; }
}

/// <summary>
/// Lidar reading from the TELEMETRY payload.
/// Replaces the previous IR section.
/// JSON: { "dist": 0.42, "angle": 15.5, "valid": true }
/// dist and angle are floats on the ESP32 side.
/// </summary>
public class LidarData
{
    /// <summary>Measured distance (float, as sent by ESP32).</summary>
    public float Dist  { get; set; }

    /// <summary>Angle of the detected target in degrees (float, as sent by ESP32).</summary>
    public float Angle { get; set; }

    /// <summary>True when the reading is valid (target in range).</summary>
    public bool  Valid { get; set; }
}

// ---------------------------------------------------------------------------
// SensorData — top-level telemetry snapshot
// ---------------------------------------------------------------------------
// Produced by TelemetryParser.ParseTelemetry().
// Consumed by TelemetryPanelViewModel to update the UI.
//
// V1 JSON payload structure:
//   {
//     "ts":      1234567,
//     "battery": { "voltage": 7.4, "percent": 85, "critical": false },
//     "line":    { "frontLeft": bool, "frontRight": bool, "backLeft": bool, "back": bool },
//     "lidar":   { "dist": 0.42, "angle": 15.5, "valid": true }
//   }
// ---------------------------------------------------------------------------

public class SensorData
{
    /// <summary>Timestamp from the ESP32 (milliseconds since boot).</summary>
    public long Timestamp { get; set; }

    /// <summary>Battery state. Never null — defaults to safe values if absent.</summary>
    public BatteryData Battery { get; set; } = new();

    /// <summary>Line sensor states. Never null — defaults to all false if absent.</summary>
    public LineSensorData Line { get; set; } = new();

    /// <summary>Lidar reading. Never null — defaults to zero/invalid if absent.</summary>
    public LidarData Lidar { get; set; } = new();
}
