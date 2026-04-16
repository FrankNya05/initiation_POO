using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// CommandSerializer — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Seul endroit de l'application qui connaît le format des commandes envoyées.
// Convertit les objets modèles en strings brutes MQTT.
//
// IMPORTANT : Le firmware ESP32 attend des strings brutes — PAS du JSON.
// Le topic "robot/commande" reçoit des strings comme "START", "MOTOR:L:200:R:200".
//
// Changements V2 :
//   - Suppression totale de la sérialisation JSON (MODIFIÉ V2)
//   - Serialize(MotorCommand) → string brute (MODIFIÉ V2)
//   - Ajout de méthodes statiques pour toutes les commandes robot (MODIFIÉ V2)
//
// Exemples de strings produites :
//   SerializeRobotCommand("START")     → "START"
//   SerializeRobotCommand("STOP")      → "STOP"
//   SerializeStrategy("BERSERKER")     → "STRATEGY:BERSERKER"
//   Serialize(MotorCommand.Direct(...))→ "MOTOR:L:200:R:200"
//   Serialize(MotorCommand.Stop())     → "MOTOR:STOP"
//   SerializePid("KP", 1.5f)          → "PID:KP:1.5"
// ---------------------------------------------------------------------------

public static class CommandSerializer
{
    // -----------------------------------------------------------------------
    // Commandes robot générales
    // -----------------------------------------------------------------------

    /// <summary>
    /// Sérialise une commande robot simple (START, STOP, RESET).
    /// Retourne la string telle quelle — c'est ce que le firmware attend.
    /// </summary>
    public static string SerializeRobotCommand(string command)  // MODIFIÉ V2
    {
        // Validation défensive : on s'assure que la commande est une string valide.
        if (string.IsNullOrWhiteSpace(command))
            throw new ArgumentException("La commande ne peut pas être vide.", nameof(command));

        return command.Trim().ToUpperInvariant();
    }

    // -----------------------------------------------------------------------
    // Commandes stratégie
    // -----------------------------------------------------------------------

    /// <summary>
    /// Sérialise une sélection de stratégie.
    /// Exemple : "BERSERKER" → "STRATEGY:BERSERKER"
    /// Stratégies valides : "ADAMANTINE", "BERSERKER", "CIRCLE"
    /// </summary>
    public static string SerializeStrategy(string strategyName)  // MODIFIÉ V2
    {
        if (string.IsNullOrWhiteSpace(strategyName))
            throw new ArgumentException("Le nom de stratégie ne peut pas être vide.", nameof(strategyName));

        return $"STRATEGY:{strategyName.Trim().ToUpperInvariant()}";
    }

    // -----------------------------------------------------------------------
    // Commandes moteur — MODIFIÉ V2
    // -----------------------------------------------------------------------

    /// <summary>
    /// Sérialise une commande moteur en string brute.
    ///
    /// Exemples de sortie :
    ///   Direct(200, 200)   → "MOTOR:L:200:R:200"   (avance)
    ///   Direct(-200, -200) → "MOTOR:L:-200:R:-200"  (recule)
    ///   Direct(-150, 150)  → "MOTOR:L:-150:R:150"   (tourne gauche)
    ///   Stop()             → "MOTOR:STOP"
    /// </summary>
    public static string Serialize(MotorCommand command)  // MODIFIÉ V2
    {
        return command.Type switch
        {
            MotorCommandType.Stop => "MOTOR:STOP",

            MotorCommandType.Direct => BuildMotorDirect(command.LeftSpeed, command.RightSpeed),

            _ => throw new ArgumentOutOfRangeException(nameof(command.Type),
                     $"Type de commande moteur inconnu : {command.Type}")
        };
    }

    /// <summary>
    /// Construit la string moteur directe avec clamping des vitesses.
    /// Les vitesses sont limitées à [-255, 255] pour protéger le hardware.
    /// </summary>
    private static string BuildMotorDirect(int left, int right)  // MODIFIÉ V2
    {
        var l = Math.Clamp(left,  -255, 255);
        var r = Math.Clamp(right, -255, 255);
        return $"MOTOR:L:{l}:R:{r}";
    }

    // -----------------------------------------------------------------------
    // Commandes PID
    // -----------------------------------------------------------------------

    /// <summary>
    /// Sérialise une valeur de paramètre PID.
    /// Exemple : SerializePid("KP", 1.5f) → "PID:KP:1.5"
    /// Paramètres valides : "KP", "KI", "KD"
    /// </summary>
    public static string SerializePid(string parameter, float value)  // MODIFIÉ V2
    {
        if (string.IsNullOrWhiteSpace(parameter))
            throw new ArgumentException("Le paramètre PID ne peut pas être vide.", nameof(parameter));

        // Formatage sans virgule de groupement, point comme séparateur décimal.
        // Exemple : 1.5f → "1.5" (pas "1,5" qui dépend de la locale)
        return $"PID:{parameter.Trim().ToUpperInvariant()}:{value.ToString("G", System.Globalization.CultureInfo.InvariantCulture)}";
    }
}
