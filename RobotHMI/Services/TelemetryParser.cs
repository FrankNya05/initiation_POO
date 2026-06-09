using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// TelemetryParser — MODIFIÉ V2.1
// ---------------------------------------------------------------------------
// Ajout V2.1 : parsing de la section "tof" (TofFrontLeft, TofFrontRight)
// ---------------------------------------------------------------------------

public static class TelemetryParser
{
    public static SensorData? ParseTelemetry(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY : 'payload' manquant.");
                return null;
            }

            var data = new SensorData();

            // Horodatage
            if (payload.TryGetProperty("ts", out var ts))
                data.Timestamp = ts.GetInt64();

            // Batterie
            if (payload.TryGetProperty("battery", out var battery))
            {
                data.BatteryVoltage  = ReadFloat(battery, "voltage");
                data.BatteryPercent  = ReadInt(battery,   "percent");
                data.BatteryCritical = ReadBool(battery,  "critical");
            }

            // Capteurs de ligne
            if (payload.TryGetProperty("line", out var line))
            {
                data.LineFrontLeft  = ReadBool(line, "frontLeft");
                data.LineFrontRight = ReadBool(line, "frontRight");
                data.LineBackLeft   = ReadBool(line, "backLeft");
                data.LineBack       = ReadBool(line, "back");
            }

            // Lidar
            if (payload.TryGetProperty("lidar", out var lidar))
            {
                data.LidarDist  = ReadFloat(lidar, "dist");
                data.LidarAngle = ReadFloat(lidar, "angle");
                data.LidarValid = ReadBool(lidar,  "valid");
            }

            // TOF — MODIFIÉ V2.1 : nouveau bloc
            if (payload.TryGetProperty("tof", out var tof))
            {
                data.TofFrontLeft  = ReadFloat(tof, "frontLeft");
                data.TofFrontRight = ReadFloat(tof, "frontRight");
            }
            else
            {
                // Valeur sentinelle : -1 = données non disponibles
                data.TofFrontLeft  = -1f;
                data.TofFrontRight = -1f;
            }

            // Moteurs (PWM actuel) — MODIFIÉ V2.2
            if (payload.TryGetProperty("motors", out var motors))
            {
                data.MotorLeft  = ReadInt(motors, "left");
                data.MotorRight = ReadInt(motors, "right");
            }

            // Pose EKF — MODIFIÉ V2.3
            if (payload.TryGetProperty("pose", out var pose))
            {
                data.PoseX     = ReadFloat(pose, "x");
                data.PoseY     = ReadFloat(pose, "y");
                data.PoseTheta = ReadFloat(pose, "theta");
            }

            // Encodeurs — MODIFIÉ V2.3
            if (payload.TryGetProperty("encoders", out var encoders))
            {
                data.EncoderLeftRpm  = ReadFloat(encoders, "leftRpm");
                data.EncoderRightRpm = ReadFloat(encoders, "rightRpm");
            }

            return data;
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Échec parsing TELEMETRY : {ex.Message}");
            return null;
        }
    }

    public static RobotStatus ParseState(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
                return RobotStatus.Unknown;

            var stateString = payload.GetString() ?? string.Empty;

            return stateString.ToUpperInvariant() switch
            {
                "IDLE"    => RobotStatus.Idle,
                "SEARCH"  => RobotStatus.Search,
                "ATTACK"  => RobotStatus.Attack,
                "DEFENSE" => RobotStatus.Defense,
                _         => RobotStatus.Unknown
            };
        }
        catch (JsonException)
        {
            return RobotStatus.Unknown;
        }
    }

    public static string? ParseAck(string rawJson)
    {
        // Format robot : {"type":"ACK","payload":{"command":"..."}}
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            if (!doc.RootElement.TryGetProperty("payload", out var p)) return null;
            if (p.TryGetProperty("command", out var cmd)) return cmd.GetString();
            return p.ValueKind == JsonValueKind.String ? p.GetString() : null;
        }
        catch { return null; }
    }

    public static string? ParseLog(string rawJson)
    {
        // Format robot : {"type":"LOG","payload":{"message":"..."}}
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            if (!doc.RootElement.TryGetProperty("payload", out var p)) return null;
            if (p.TryGetProperty("message", out var msg)) return msg.GetString();
            return p.ValueKind == JsonValueKind.String ? p.GetString() : null;
        }
        catch { return null; }
    }

    // ── Helpers ──────────────────────────────────────────────────────────

    private static bool ReadBool(JsonElement parent, string key)
    {
        if (parent.TryGetProperty(key, out var prop) &&
            (prop.ValueKind == JsonValueKind.True || prop.ValueKind == JsonValueKind.False))
            return prop.GetBoolean();
        return false;
    }

    private static int ReadInt(JsonElement parent, string key)
    {
        if (parent.TryGetProperty(key, out var prop) && prop.TryGetInt32(out int v))
            return v;
        return 0;
    }

    private static float ReadFloat(JsonElement parent, string key)
    {
        if (parent.TryGetProperty(key, out var prop) && prop.TryGetSingle(out float v))
            return v;
        return 0f;
    }
}
