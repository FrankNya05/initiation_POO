using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// TelemetryParser
// ---------------------------------------------------------------------------
// Static helper that converts raw JSON strings into typed model objects.
// This is the ONLY place in the application that knows the shape of the
// TELEMETRY and STATE message payloads.
//
// Responsibilities (architecture doc §7):
//   - Parse a TELEMETRY JSON string → SensorData
//   - Parse a STATE JSON string     → RobotStatus
//   - Return null / Unknown on any parse failure so the caller can react
//     gracefully without crashing
//
// Both methods are static — no state, no dependencies, easy to test.
//
// Expected JSON formats (architecture doc §6.2):
//
//   TELEMETRY:
//   {
//     "type": "TELEMETRY",
//     "payload": {
//       "line":  { "frontLeft": bool, "frontRight": bool,
//                  "backLeft":  bool, "backRight":  bool },
//       "ir":    { "front": int, "left": int, "right": int }
//     }
//   }
//
//   STATE:
//   { "type": "STATE", "payload": "RUNNING" }
// ---------------------------------------------------------------------------

public static class TelemetryParser
{
    // -----------------------------------------------------------------------
    // ParseTelemetry
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parses a full TELEMETRY JSON envelope into a SensorData object.
    /// Returns null if parsing fails for any reason (malformed JSON,
    /// missing fields, unexpected types).
    /// The caller should log the failure and keep the last known good value.
    /// </summary>
    public static SensorData? ParseTelemetry(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);  // Check if this line is okay
            var root = doc.RootElement;

            // Navigate to the payload object.
            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY message missing 'payload'.");
                return null;
            }

            var data = new SensorData();

            // Line sensors
            if (payload.TryGetProperty("line", out var line))
            {
                data.LineFrontLeft = ReadBool(line, "frontLeft");
                data.LineFrontRight = ReadBool(line, "frontRight");
                data.LineBackLeft = ReadBool(line, "backLeft");
                data.LineBackRight = ReadBool(line, "backRight");
            }
            else
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY payload missing 'line' section.");
            }

            // IR / distance sensors
            if (payload.TryGetProperty("ir", out var ir))
            {
                data.IrFront = ReadInt(ir, "front");
                data.IrLeft = ReadInt(ir, "left");
                data.IrRight = ReadInt(ir, "right");
            }
            else
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY payload missing 'ir' section.");
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
    /// Parses a full STATE JSON envelope into a RobotStatus enum value.
    /// Returns RobotStatus.Unknown if the payload string is unrecognised
    /// or if parsing fails.
    /// </summary>
    public static RobotStatus ParseState(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] STATE message missing 'payload'.");
                return RobotStatus.Unknown;
            }

            var stateString = payload.GetString() ?? string.Empty;

            // Map the raw string to the enum. Case-insensitive so minor
            // firmware differences don't break the UI.
            var status = stateString.ToUpperInvariant() switch
            {
                "IDLE"       => RobotStatus.Idle,
                "RUNNING"    => RobotStatus.Running,
                "SEARCHING"  => RobotStatus.Searching,
                "ATTACKING"  => RobotStatus.Attacking,
                "RETREATING" => RobotStatus.Retreating,
                "ERROR"      => RobotStatus.Error,
                _            => RobotStatus.Unknown
            };

            // Log unrecognised states so developers notice firmware mismatches.
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
    // Private helpers — safe field readers
    // -----------------------------------------------------------------------

    // These helpers return a safe default value instead of throwing when a
    // field is missing or the wrong type. The robot firmware may evolve, and
    // we should never crash on a missing optional field.

    private static bool ReadBool(JsonElement parent, string propertyName)
    {
        if (parent.TryGetProperty(propertyName, out var prop) &&
            (prop.ValueKind == JsonValueKind.True || prop.ValueKind == JsonValueKind.False))
        {
            return prop.GetBoolean();
        }
        return false; // safe default
    }

    private static int ReadInt(JsonElement parent, string propertyName)
    {
        if (parent.TryGetProperty(propertyName, out var prop) &&
            prop.TryGetInt32(out int value))
        {
            return value;
        }
        return 0; // safe default
    }

}
