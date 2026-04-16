using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// TelemetryParser — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Seul endroit de l'application qui connaît le format des messages reçus.
// Convertit les strings JSON brutes en objets modèles typés.
//
// Changements V2 :
//   - ParseTelemetry() : ajout parsing "battery" et "lidar" (MODIFIÉ V2)
//   - ParseTelemetry() : "back" remplace "backRight" pour la ligne (MODIFIÉ V2)
//   - ParseTelemetry() : suppression du parsing "ir" (MODIFIÉ V2)
//   - ParseState()     : "SEARCH" / "ATTACK" / "DEFENSE" remplacent (MODIFIÉ V2)
//                        "SEARCHING" / "ATTACKING" / "RETREATING"
//   - Ajout helper ReadFloat() (MODIFIÉ V2)
//
// Format TELEMETRY attendu (protocole V1) :
// {
//   "type": "TELEMETRY",
//   "payload": {
//     "ts": 12345,
//     "battery": { "voltage": 7.35, "percent": 78, "critical": false },
//     "line":    { "frontLeft": false, "frontRight": false,
//                  "backLeft":  false, "back": false },
//     "lidar":   { "dist": 0.42, "angle": 45.0, "valid": true }
//   }
// }
//
// Format STATE attendu :
// { "type": "STATE", "payload": "IDLE" }
// ---------------------------------------------------------------------------

public static class TelemetryParser
{
    // -----------------------------------------------------------------------
    // ParseTelemetry
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parse une enveloppe JSON TELEMETRY complète en objet SensorData.
    /// Retourne null si le parsing échoue — le caller conserve la dernière
    /// valeur connue sans planter.
    /// </summary>
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

            // ── Horodatage ──────────────────────────────────────────────────
            if (payload.TryGetProperty("ts", out var ts))
                data.Timestamp = ts.GetInt64();  // MODIFIÉ V2

            // ── Batterie ────────────────────────────────────────────────────
            // MODIFIÉ V2 — section entièrement nouvelle
            if (payload.TryGetProperty("battery", out var battery))
            {
                data.BatteryVoltage  = ReadFloat(battery, "voltage");
                data.BatteryPercent  = ReadInt(battery,   "percent");
                data.BatteryCritical = ReadBool(battery,  "critical");
            }
            else
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY : section 'battery' absente.");
            }

            // ── Capteurs de ligne ───────────────────────────────────────────
            // MODIFIÉ V2 — "back" remplace "backRight"
            if (payload.TryGetProperty("line", out var line))
            {
                data.LineFrontLeft  = ReadBool(line, "frontLeft");
                data.LineFrontRight = ReadBool(line, "frontRight");
                data.LineBackLeft   = ReadBool(line, "backLeft");
                data.LineBack       = ReadBool(line, "back");   // MODIFIÉ V2 — était "backRight"
            }
            else
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY : section 'line' absente.");
            }

            // ── Lidar ───────────────────────────────────────────────────────
            // MODIFIÉ V2 — section entièrement nouvelle
            if (payload.TryGetProperty("lidar", out var lidar))
            {
                data.LidarDist  = ReadFloat(lidar, "dist");
                data.LidarAngle = ReadFloat(lidar, "angle");
                data.LidarValid = ReadBool(lidar,  "valid");
            }
            else
            {
                Console.WriteLine("[TelemetryParser] TELEMETRY : section 'lidar' absente.");
            }

            // Note : la section "ir" a été supprimée en V1 — ignorée si présente.

            return data;
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Échec parsing TELEMETRY : {ex.Message}");
            return null;
        }
    }

    // -----------------------------------------------------------------------
    // ParseState
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parse une enveloppe JSON STATE en valeur d'enum RobotStatus.
    /// Retourne RobotStatus.Unknown si la string n'est pas reconnue.
    /// </summary>
    public static RobotStatus ParseState(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
            {
                Console.WriteLine("[TelemetryParser] STATE : 'payload' manquant.");
                return RobotStatus.Unknown;
            }

            var stateString = payload.GetString() ?? string.Empty;

            // MODIFIÉ V2 — mapping mis à jour sur les 4 états V1
            var status = stateString.ToUpperInvariant() switch
            {
                "IDLE"    => RobotStatus.Idle,
                "SEARCH"  => RobotStatus.Search,    // MODIFIÉ V2 — était "SEARCHING"
                "ATTACK"  => RobotStatus.Attack,    // MODIFIÉ V2 — était "ATTACKING"
                "DEFENSE" => RobotStatus.Defense,   // MODIFIÉ V2 — était "RETREATING"
                _         => RobotStatus.Unknown
            };

            if (status == RobotStatus.Unknown && !string.IsNullOrEmpty(stateString))
                Console.WriteLine($"[TelemetryParser] État inconnu reçu : '{stateString}'");

            return status;
        }
        catch (JsonException ex)
        {
            Console.WriteLine($"[TelemetryParser] Échec parsing STATE : {ex.Message}");
            return RobotStatus.Unknown;
        }
    }

    // -----------------------------------------------------------------------
    // ParseAck
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parse une enveloppe JSON ACK et retourne la string de commande confirmée.
    /// Exemple : { "type": "ACK", "payload": "START" } → "START"
    /// </summary>
    public static string? ParseAck(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
                return null;

            return payload.GetString();
        }
        catch (JsonException)
        {
            return null;
        }
    }

    // -----------------------------------------------------------------------
    // ParseLog
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parse une enveloppe JSON LOG et retourne le message texte.
    /// Exemple : { "type": "LOG", "payload": "Sensor init OK" } → "Sensor init OK"
    /// </summary>
    public static string? ParseLog(string rawJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("payload", out var payload))
                return null;

            return payload.GetString();
        }
        catch (JsonException)
        {
            return null;
        }
    }

    // -----------------------------------------------------------------------
    // Helpers privés — lecture sécurisée des champs JSON
    // -----------------------------------------------------------------------
    // Ces helpers retournent une valeur par défaut au lieu de planter si un
    // champ est absent ou du mauvais type. Le firmware peut évoluer, on ne
    // plante jamais sur un champ optionnel manquant.

    private static bool ReadBool(JsonElement parent, string propertyName)
    {
        if (parent.TryGetProperty(propertyName, out var prop) &&
            (prop.ValueKind == JsonValueKind.True || prop.ValueKind == JsonValueKind.False))
        {
            return prop.GetBoolean();
        }
        return false;
    }

    private static int ReadInt(JsonElement parent, string propertyName)
    {
        if (parent.TryGetProperty(propertyName, out var prop) &&
            prop.TryGetInt32(out int value))
        {
            return value;
        }
        return 0;
    }

    // MODIFIÉ V2 — nouveau helper pour les float (battery voltage, lidar dist/angle)
    private static float ReadFloat(JsonElement parent, string propertyName)
    {
        if (parent.TryGetProperty(propertyName, out var prop) &&
            prop.TryGetSingle(out float value))
        {
            return value;
        }
        return 0f;
    }
}
