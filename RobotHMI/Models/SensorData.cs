namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// SensorData
// ---------------------------------------------------------------------------
// Plain data class representing a single telemetry snapshot from the robot.
// Created by TelemetryParser when a TELEMETRY message is received.
// Consumed by TelemetryPanelViewModel to update the UI.
//
// This class has NO logic — it is just data.
// Its structure mirrors the JSON payload defined in the architecture doc §6.2:
//
//   "payload": {
//     "line": {
//       "frontLeft": false, "frontRight": false,
//       "backLeft": true,   "backRight": false
//     },
//     "ir": {
//       "front": 42, "left": 18, "right": 31
//     }
//   }
// ---------------------------------------------------------------------------

public class SensorData
{
    // -----------------------------------------------------------------------
    // Line sensors (true = line detected)
    // -----------------------------------------------------------------------

    /// <summary>Front-left line sensor. True when the white border is detected.</summary>
    public bool LineFrontLeft  { get; set; }

    /// <summary>Front-right line sensor.</summary>
    public bool LineFrontRight { get; set; }

    /// <summary>Back-left line sensor.</summary>
    public bool LineBackLeft   { get; set; }

    /// <summary>Back-right line sensor.</summary>
    public bool LineBackRight  { get; set; }

    // -----------------------------------------------------------------------
    // IR / distance sensors (centimetres)
    // -----------------------------------------------------------------------

    /// <summary>Distance measured by the front IR sensor, in centimetres.</summary>
    public int IrFront { get; set; }

    /// <summary>Distance measured by the left IR sensor, in centimetres.</summary>
    public int IrLeft  { get; set; }

    /// <summary>Distance measured by the right IR sensor, in centimetres.</summary>
    public int IrRight { get; set; }
}
