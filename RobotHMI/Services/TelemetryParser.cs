using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// TelemetryParser — V1 protocol
// ---------------------------------------------------------------------------
// Static helper that converts raw JSON strings into typed model objects.
// This is the ONLY place in the application that knows the shape of the
// incoming message payloads.
//
// V1 incoming message formats:
//
//   TELEMETRY:
//   {
//     "type": "TELEMETRY",
//     "payload": {
//       "ts": 1234567,
//       "battery": { "voltage": 7.4, "percent": 85, "critical": false },
//       "line":    { "frontLeft": bool, "frontRight": bool,
//                    "backLeft": bool,  "back": bool },
//       "lidar":   { "dist": 42, "angle": 15, "valid": true }
//     }
//   }
//
//   STATE:
//   { "type": "STATE", "payload": { "state": "ATTACK" } }
//
//   ACK (parsed in ControlPanelViewModel):
//   { "type": "ACK", "payload": { "command": "FORWARD" } }
//
//   LOG (parsed in StatusBarViewModel):
//   { "type": "LOG", "payload": { "message": "Battery low" } }
// ---------------------------------------------------------------------------

public static class TelemetryParser
{
    // -----------------------------------------------------------------------
    // ParseTelemetry
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parses a full TELEMETRY JSON envelope into a SensorData object.
    /// Returns null if the JSON is malformed or the payload is missing.
    /// Missing sections (battery, line, lidar) are filled with safe defaults
    /// rather than causing a failure — the firmware may omit optional fields.
    /// </summary>
    public static SensorData? ParseTelemetry(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY missing 'payload'.");
                return null;
            }

            var data = new SensorData();

            // ----------------------------------------------------------------
            // Timestamp
            // ----------------------------------------------------------------
            data.Timestamp = ReadLong(payload, "ts");

            // ----------------------------------------------------------------
            // Battery
            // ----------------------------------------------------------------
            if (payload.TryGetProperty("battery", out var battery))
            {
                data.Battery.Voltage  = ReadFloat(battery, "voltage");
                data.Battery.Percent  = ReadInt(battery,   "percent");
                data.Battery.Critical = ReadBool(battery,  "critical");
            }

            // ----------------------------------------------------------------
            // Line sensors
            // Note: V1 uses "back" (single rear sensor), not "backRight".
            // ----------------------------------------------------------------
            if (payload.TryGetProperty("line", out var line))
            {
                data.Line.FrontLeft  = ReadBool(line, "frontLeft");
                data.Line.FrontRight = ReadBool(line, "frontRight");
                data.Line.BackLeft   = ReadBool(line, "backLeft");
                data.Line.Back       = ReadBool(line, "back");
            }

            // ----------------------------------------------------------------
            // Lidar (replaces the previous IR section)
            // ----------------------------------------------------------------
            if (payload.TryGetProperty("lidar", out var lidar))
            {
                data.Lidar.Dist  = ReadFloat(lidar, "dist");
                data.Lidar.Angle = ReadFloat(lidar, "angle");
                data.Lidar.Valid = ReadBool(lidar,  "valid");
            }

            return data;
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Failed to parse TELEMETRY: {ex.Message}");
            return null;
        }
    }

    // -----------------------------------------------------------------------
    // ParseState
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parses a STATE JSON envelope into a RobotStatus enum value.
    ///
    /// V1 final format: { "type": "STATE", "payload": { "state": "ATTACK" } }
    /// V1 state strings: IDLE | SEARCH | ATTACK | DEFENSE
    /// The payload is an object with a "state" string field.
    /// </summary>
    public static RobotStatus ParseState(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] STATE missing 'payload'.");
                return RobotStatus.Unknown;
            }

            // V1: payload is an object { "state": "ATTACK" }
            var stateString = ReadString(payload, "state");

            var status = stateString.ToUpperInvariant() switch
            {
                // V1 final protocol states
                "IDLE"    => RobotStatus.Idle,
                "SEARCH"  => RobotStatus.Searching,
                "ATTACK"  => RobotStatus.Attacking,
                "DEFENSE" => RobotStatus.Retreating,
                // Fallback tolerance for older firmware during transition
                "RUNNING"    => RobotStatus.Idle,
                "SEARCHING"  => RobotStatus.Searching,
                "ATTACKING"  => RobotStatus.Attacking,
                "RETREATING" => RobotStatus.Retreating,
                _            => RobotStatus.Unknown
            };

            if (status == RobotStatus.Unknown && !string.IsNullOrEmpty(stateString))
                Console.WriteLine($"[TelemetryParser] Unknown robot state: '{stateString}'");

            return status;
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Failed to parse STATE: {ex.Message}");
            return RobotStatus.Unknown;
        }
    }

    // -----------------------------------------------------------------------
    // ParseAckCommand
    // -----------------------------------------------------------------------

    /// <summary>
    /// Extracts the acknowledged command name from an ACK message.
    ///
    /// V1 format: { "type": "ACK", "payload": { "command": "FORWARD" } }
    /// Returns an empty string on any failure.
    /// </summary>
    public static string ParseAckCommand(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
                return string.Empty;

            return ReadString(payload, "command");
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Failed to parse ACK: {ex.Message}");
            return string.Empty;
        }
    }

    // -----------------------------------------------------------------------
    // ParseLogMessage
    // -----------------------------------------------------------------------

    /// <summary>
    /// Extracts the log text from a LOG message.
    ///
    /// V1 format: { "type": "LOG", "payload": { "message": "Battery low" } }
    /// Returns an empty string on any failure.
    /// </summary>
    public static string ParseLogMessage(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
                return string.Empty;

            return ReadString(payload, "message");
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Failed to parse LOG: {ex.Message}");
            return string.Empty;
        }
    }

    // -----------------------------------------------------------------------
    // Private helpers — safe field readers
    // -----------------------------------------------------------------------
    // All return a safe default when the field is absent or the wrong type.
    // This means the UI stays functional even if the firmware omits fields.

    private static bool ReadBool(JsonElement parent, string name)
    {
        if (parent.TryGetProperty(name, out var p) &&
            p.ValueKind is JsonValueKind.True or JsonValueKind.False)
            return p.GetBoolean();
        return false;
    }

    private static int ReadInt(JsonElement parent, string name)
    {
        if (parent.TryGetProperty(name, out var p) && p.TryGetInt32(out int v))
            return v;
        return 0;
    }

    private static long ReadLong(JsonElement parent, string name)
    {
        if (parent.TryGetProperty(name, out var p) && p.TryGetInt64(out long v))
            return v;
        return 0;
    }

    private static float ReadFloat(JsonElement parent, string name)
    {
        if (parent.TryGetProperty(name, out var p) && p.TryGetSingle(out float v))
            return v;
        return 0f;
    }

    private static string ReadString(JsonElement parent, string name)
    {
        if (parent.TryGetProperty(name, out var p))
            return p.GetString() ?? string.Empty;
        return string.Empty;
    }
}
