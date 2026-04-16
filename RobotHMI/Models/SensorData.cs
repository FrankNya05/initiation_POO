namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// SensorData — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Représente un snapshot de télémétrie reçu de l'ESP32.
// Aligné sur le protocole MQTT V1 (MQTT_PROTOCOL_V1.md).
//
// Changements V2 :
//   - Supprimé : IrFront, IrLeft, IrRight (section "ir" n'existe plus en V1)
//   - Supprimé : LineBackRight (le capteur physique s'appelle "back", pas "backRight")
//   - Ajouté   : LineBack (MODIFIÉ V2)
//   - Ajouté   : BatteryVoltage, BatteryPercent, BatteryCritical (MODIFIÉ V2)
//   - Ajouté   : LidarDist, LidarAngle, LidarValid (MODIFIÉ V2)
//   - Ajouté   : Timestamp (MODIFIÉ V2)
// ---------------------------------------------------------------------------

public class SensorData
{
    // -----------------------------------------------------------------------
    // Capteurs de ligne — 4 capteurs physiques
    // -----------------------------------------------------------------------
    // Vrai = ligne blanche détectée (bord du dohyo).
    // Positions : FRONT_LEFT, FRONT_RIGHT, BACK_LEFT, BACK (pas BACK_RIGHT).

    public bool LineFrontLeft  { get; set; }   // MODIFIÉ V2 — inchangé
    public bool LineFrontRight { get; set; }   // MODIFIÉ V2 — inchangé
    public bool LineBackLeft   { get; set; }   // MODIFIÉ V2 — inchangé
    public bool LineBack       { get; set; }   // MODIFIÉ V2 — était LineBackRight

    // -----------------------------------------------------------------------
    // Batterie
    // -----------------------------------------------------------------------

    public float BatteryVoltage  { get; set; }  // MODIFIÉ V2 — en Volts (ex: 7.35)
    public int   BatteryPercent  { get; set; }  // MODIFIÉ V2 — 0-100
    public bool  BatteryCritical { get; set; }  // MODIFIÉ V2 — true si niveau critique

    // -----------------------------------------------------------------------
    // Lidar — YDLIDAR Tmini Plus
    // -----------------------------------------------------------------------
    // Publie l'objet le plus proche détecté (filtré EMA côté ESP32).

    public float LidarDist  { get; set; }  // MODIFIÉ V2 — en mètres (ex: 0.42)
    public float LidarAngle { get; set; }  // MODIFIÉ V2 — en degrés (ex: 45.0)
    public bool  LidarValid { get; set; }  // MODIFIÉ V2 — true = cible valide détectée

    // -----------------------------------------------------------------------
    // Horodatage
    // -----------------------------------------------------------------------

    public long Timestamp { get; set; }  // MODIFIÉ V2 — ms depuis démarrage ESP32
}
