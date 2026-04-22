using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// CommandSerializer — MODIFIÉ V2.3
// ---------------------------------------------------------------------------
// Changements V2.3 :
//   - SerializeStrategy() : suppression de ToUpperInvariant() sur le nom
//     de stratégie. Le firmware utilise strcasecmp donc la casse est
//     tolérée, mais "Track" doit être envoyé tel quel car onStrategy()
//     dans main.cpp compare en majuscules pour les 3 anciennes stratégies
//     et setByName() utilise strcasecmp pour les nouvelles. (MODIFIÉ V2.3)
//   - Ajout SerializePidYaw() pour PID:YAW:KP/KI/KD (MODIFIÉ V2.3)
// ---------------------------------------------------------------------------

public static class CommandSerializer
{
    // ── Commandes robot générales ────────────────────────────────────────

    public static string SerializeRobotCommand(string command)
    {
        if (string.IsNullOrWhiteSpace(command))
            throw new ArgumentException("La commande ne peut pas être vide.", nameof(command));
        return command.Trim().ToUpperInvariant();
    }

    // ── Stratégie ────────────────────────────────────────────────────────

    /// <summary>
    /// Sérialise une stratégie.
    /// MODIFIÉ V2.3 : le nom est envoyé avec sa casse d'origine (pas ToUpper)
    /// car le firmware compare via strcasecmp(). Exemples :
    ///   "ADAMANTINE" → "STRATEGY:ADAMANTINE"
    ///   "Track"      → "STRATEGY:Track"
    /// </summary>
    public static string SerializeStrategy(string strategyName)  // MODIFIÉ V2.3
    {
        if (string.IsNullOrWhiteSpace(strategyName))
            throw new ArgumentException("Le nom de stratégie ne peut pas être vide.", nameof(strategyName));
        // Pas de ToUpperInvariant — le firmware utilise strcasecmp
        return $"STRATEGY:{strategyName.Trim()}";
    }

    // ── Moteurs ──────────────────────────────────────────────────────────

    public static string Serialize(MotorCommand command)
    {
        return command.Type switch
        {
            MotorCommandType.Stop   => "MOTOR:STOP",
            MotorCommandType.Direct => BuildMotorDirect(command.LeftSpeed, command.RightSpeed),
            _ => throw new ArgumentOutOfRangeException(nameof(command.Type))
        };
    }

    private static string BuildMotorDirect(int left, int right)
    {
        var l = Math.Clamp(left,  -255, 255);
        var r = Math.Clamp(right, -255, 255);
        return $"MOTOR:L:{l}:R:{r}";
    }

    // ── PID moteurs (pidLeft + pidRight) ─────────────────────────────────

    /// <summary>
    /// Sérialise un gain PID moteurs.
    /// SerializePid("KP", 1.5f) → "PID:KP:1.5"
    /// </summary>
    public static string SerializePid(string parameter, float value)
    {
        if (string.IsNullOrWhiteSpace(parameter))
            throw new ArgumentException("Le paramètre PID ne peut pas être vide.", nameof(parameter));
        return $"PID:{parameter.Trim().ToUpperInvariant()}:{value.ToString("G", System.Globalization.CultureInfo.InvariantCulture)}";
    }

    // ── PID cap gyroscope (pidYaw) — MODIFIÉ V2.3 ────────────────────────

    /// <summary>
    /// Sérialise un gain PID yaw (correction angulaire gyroscope).
    /// SerializePidYaw("KP", 0.5f) → "PID:YAW:KP:0.5"
    /// Paramètres valides : "KP", "KI", "KD"
    /// </summary>
    public static string SerializePidYaw(string parameter, float value)  // MODIFIÉ V2.3
    {
        if (string.IsNullOrWhiteSpace(parameter))
            throw new ArgumentException("Le paramètre PID YAW ne peut pas être vide.", nameof(parameter));
        return $"PID:YAW:{parameter.Trim().ToUpperInvariant()}:{value.ToString("G", System.Globalization.CultureInfo.InvariantCulture)}";
    }
}
