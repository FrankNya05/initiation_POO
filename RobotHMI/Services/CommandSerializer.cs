using System.Text;
using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// CommandSerializer — V1 protocol
// ---------------------------------------------------------------------------
// The ONLY place in the application that knows the shape of outgoing messages.
//
// V1 outgoing message formats:
//
//   CMD_MOTOR (motor movement):
//   { "type": "CMD_MOTOR", "payload": { "action": "FORWARD", "speed": 150 } }
//
//   CMD_ROBOT (robot-level commands):
//   { "type": "CMD_ROBOT", "payload": { "command": "START", "param": "" } }
//
// Design:
//   - Static class with no state — easy to understand and test.
//   - Uses Utf8JsonWriter for compact, allocation-friendly serialisation.
//   - Speed is clamped 0–255 so no invalid value ever reaches the robot.
// ---------------------------------------------------------------------------

public static class CommandSerializer
{
    private static readonly JsonWriterOptions _options = new() { Indented = false };

    // -----------------------------------------------------------------------
    // BuildCmdMotor
    // -----------------------------------------------------------------------

    /// <summary>
    /// Builds a CMD_MOTOR JSON string for a motion command.
    ///
    /// Output: { "type": "CMD_MOTOR", "payload": { "action": "FORWARD", "speed": 150 } }
    ///
    /// Called by ControlPanelViewModel for all five direction buttons.
    /// The action string should already be uppercase (FORWARD, BACKWARD,
    /// LEFT, RIGHT, STOP) — the caller is responsible for the conversion.
    /// </summary>
    public static string BuildCmdMotor(string action, int speed)
    {
        speed = Math.Clamp(speed, 0, 255);

        using var stream = new System.IO.MemoryStream();
        using var writer = new Utf8JsonWriter(stream, _options);

        writer.WriteStartObject();
        writer.WriteString("type", "CMD_MOTOR");
        writer.WriteStartObject("payload");
        writer.WriteString("action", action);
        writer.WriteNumber("speed",  speed);
        writer.WriteEndObject();
        writer.WriteEndObject();
        writer.Flush();

        return Encoding.UTF8.GetString(stream.ToArray());
    }

    // -----------------------------------------------------------------------
    // BuildCmdRobot
    // -----------------------------------------------------------------------

    /// <summary>
    /// Builds a CMD_ROBOT JSON string for a robot-level command.
    ///
    /// Output: { "type": "CMD_ROBOT", "payload": { "command": "START", "param": "" } }
    ///
    /// V1 robot-level commands:
    ///   BuildCmdRobot("START",        "")           → start autonomous match
    ///   BuildCmdRobot("STOP",         "")           → emergency stop
    ///   BuildCmdRobot("SET_STRATEGY", "AGGRESSIVE") → change strategy
    ///   BuildCmdRobot("RESET",        "")           → reset robot state
    /// </summary>
    public static string BuildCmdRobot(string command, string param = "")
    {
        using var stream = new System.IO.MemoryStream();
        using var writer = new Utf8JsonWriter(stream, _options);

        writer.WriteStartObject();
        writer.WriteString("type", "CMD_ROBOT");
        writer.WriteStartObject("payload");
        writer.WriteString("command", command);
        writer.WriteString("param",   param);
        writer.WriteEndObject();
        writer.WriteEndObject();
        writer.Flush();

        return Encoding.UTF8.GetString(stream.ToArray());
    }

    // -----------------------------------------------------------------------
    // Serialize (kept for backward compatibility during transition)
    // -----------------------------------------------------------------------

    /// <summary>
    /// Legacy entry point — wraps BuildCmdMotor using a MotorCommand model.
    /// ControlPanelViewModel now calls BuildCmdMotor() directly, but this
    /// method is kept so no other call site breaks.
    /// </summary>
    public static string Serialize(MotorCommand command)
    {
        var actionString = command.Action.ToString().ToUpperInvariant();
        return BuildCmdMotor(actionString, command.Speed);
    }
}
