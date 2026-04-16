namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// RobotStatus — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Enum des états opérationnels du robot, aligné sur le protocole V1.
//
// Mapping protocole → enum :
//   "IDLE"    → Idle     (en attente, avant le départ)
//   "SEARCH"  → Search   (rotation/scan, adversaire non détecté)
//   "ATTACK"  → Attack   (adversaire détecté, charge à pleine vitesse)
//   "DEFENSE" → Defense  (ligne détectée, manœuvre de récupération)
//
// Changements V2 :
//   - Supprimés : Running, Searching, Attacking, Retreating, Error (MODIFIÉ V2)
//   - Ajoutés   : Search, Attack, Defense (MODIFIÉ V2)
//   - Conservé  : Idle, Unknown
//
// Note : côté ESP32, l'état STANDBY → "IDLE", EVADE → "DEFENSE".
// Le TelemetryParser fait le mapping string → enum.
// ---------------------------------------------------------------------------

public enum RobotStatus
{
    Unknown,   // Fallback — état non reconnu ou pas encore reçu
    Idle,      // MODIFIÉ V2 — "IDLE"    : en attente du signal de départ
    Search,    // MODIFIÉ V2 — "SEARCH"  : recherche de l'adversaire
    Attack,    // MODIFIÉ V2 — "ATTACK"  : attaque en cours
    Defense    // MODIFIÉ V2 — "DEFENSE" : évitement de la ligne blanche
}
