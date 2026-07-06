namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// SensorData — MODIFIÉ V2.1
// ---------------------------------------------------------------------------
// Ajouts V2.1 :
//   - TofFrontLeft  : distance TOF avant-gauche en mm (MODIFIÉ V2.1)
//   - TofFrontRight : distance TOF avant-droit en mm  (MODIFIÉ V2.1)
// ---------------------------------------------------------------------------

public class SensorData
{
    // Capteurs de ligne
    public bool LineFrontLeft  { get; set; }
    public bool LineFrontRight { get; set; }
    public bool LineBackLeft   { get; set; }
    public bool LineBack       { get; set; }

    // Batterie
    public float BatteryVoltage  { get; set; }
    public int   BatteryPercent  { get; set; }
    public bool  BatteryCritical { get; set; }

    // Lidar
    public float LidarDist  { get; set; }
    public float LidarAngle { get; set; }
    public bool  LidarValid { get; set; }

    // TOF — MODIFIÉ V2.1
    public float TofFrontLeft  { get; set; }  // en mm, -1 si invalide
    public float TofFrontRight { get; set; }  // en mm, -1 si invalide

    // Commande moteurs (PWM actuel) — MODIFIÉ V2.2
    public int MotorLeft  { get; set; }  // -255 à 255
    public int MotorRight { get; set; }  // -255 à 255

    // Pose EKF — MODIFIÉ V2.3
    public float PoseX     { get; set; }  // mm
    public float PoseY     { get; set; }  // mm
    public float PoseTheta { get; set; }  // rad

    // Encodeurs — MODIFIÉ V2.3
    public float EncoderLeftRpm  { get; set; }
    public float EncoderRightRpm { get; set; }

    // Horodatage
    public long Timestamp { get; set; }
}
