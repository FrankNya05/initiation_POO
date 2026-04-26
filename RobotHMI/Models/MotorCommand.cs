namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// MotorCommand — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Représente une commande envoyée aux moteurs du robot.
// Le CommandSerializer convertit ce modèle en string brute MQTT.
//
// Changements V2 :
//   - MotorCommandType remplace l'ancien enum CommandAction (MODIFIÉ V2)
//   - Ajout de LeftSpeed et RightSpeed séparés (MODIFIÉ V2)
//   - Speed générique supprimé — les deux moteurs ont des vitesses indépendantes
//
// Exemples de strings produites par CommandSerializer :
//   MotorCommand(Direct, 200, 200)   → "MOTOR:L:200:R:200"
//   MotorCommand(Direct, -150, 150)  → "MOTOR:L:-150:R:150"
//   MotorCommand(Stop, 0, 0)         → "MOTOR:STOP"
// ---------------------------------------------------------------------------

/// <summary>
/// Type de commande moteur envoyée au robot.
/// </summary>
public enum MotorCommandType  // MODIFIÉ V2 — était CommandAction
{
    Direct,  // Vitesses gauche et droite spécifiées séparément → "MOTOR:L:v:R:v"
    Stop     // Arrêt immédiat des moteurs → "MOTOR:STOP"
}

/// <summary>
/// Commande moteur avec vitesses individuelles gauche/droite.
/// Plage de valeurs : -255 à 255 (négatif = marche arrière).
/// </summary>
public class MotorCommand  // MODIFIÉ V2
{
    /// <summary>Type de commande (Direct ou Stop).</summary>
    public MotorCommandType Type { get; set; }  // MODIFIÉ V2

    /// <summary>Vitesse moteur gauche, de -255 à 255.</summary>
    public int LeftSpeed { get; set; }   // MODIFIÉ V2

    /// <summary>Vitesse moteur droit, de -255 à 255.</summary>
    public int RightSpeed { get; set; }  // MODIFIÉ V2

    // -----------------------------------------------------------------------
    // Constructeurs pratiques
    // -----------------------------------------------------------------------

    /// <summary>Crée une commande arrêt moteur.</summary>
    public static MotorCommand Stop() =>
        new() { Type = MotorCommandType.Stop };  // MODIFIÉ V2

    /// <summary>Crée une commande vitesse directe gauche/droite.</summary>
    public static MotorCommand Direct(int left, int right) =>   // MODIFIÉ V2
        new() { Type = MotorCommandType.Direct, LeftSpeed = left, RightSpeed = right };
}
