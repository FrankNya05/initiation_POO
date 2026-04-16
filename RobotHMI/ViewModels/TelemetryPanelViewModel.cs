using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// TelemetryPanelViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Expose les données de télémétrie comme propriétés observables pour la Vue.
//
// Changements V2 :
//   - Supprimés : IrFront, IrLeft, IrRight (plus dans le protocole V1)
//   - Supprimés : LineBackRight → LineBack (MODIFIÉ V2)
//   - Ajoutés   : BatteryVoltage, BatteryPercent, BatteryCritical (MODIFIÉ V2)
//   - Ajoutés   : LidarDist, LidarAngle, LidarValid (MODIFIÉ V2)
//   - Mis à jour: RobotStatusText → 4 états V1 (MODIFIÉ V2)
// ---------------------------------------------------------------------------

public class TelemetryPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Batterie — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private float _batteryVoltage;
    /// <summary>Tension en Volts (ex: 7.35).</summary>
    public float BatteryVoltage
    {
        get => _batteryVoltage;
        private set
        {
            if (SetField(ref _batteryVoltage, value))
                OnPropertyChanged(nameof(BatteryVoltageText));
        }
    }

    /// <summary>Affichage formaté ex: "7.35 V".</summary>
    public string BatteryVoltageText =>
        $"{BatteryVoltage:F2} V";  // MODIFIÉ V2

    private int _batteryPercent;
    /// <summary>Pourcentage batterie 0-100.</summary>
    public int BatteryPercent
    {
        get => _batteryPercent;
        private set => SetField(ref _batteryPercent, value);
    }

    private bool _batteryCritical;
    /// <summary>True = batterie critique, badge d'alerte visible.</summary>
    public bool BatteryCritical
    {
        get => _batteryCritical;
        private set => SetField(ref _batteryCritical, value);
    }

    // -----------------------------------------------------------------------
    // Capteurs de ligne — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private bool _lineFrontLeft;
    public bool LineFrontLeft
    {
        get => _lineFrontLeft;
        private set => SetField(ref _lineFrontLeft, value);
    }

    private bool _lineFrontRight;
    public bool LineFrontRight
    {
        get => _lineFrontRight;
        private set => SetField(ref _lineFrontRight, value);
    }

    private bool _lineBackLeft;
    public bool LineBackLeft
    {
        get => _lineBackLeft;
        private set => SetField(ref _lineBackLeft, value);
    }

    private bool _lineBack;
    /// <summary>MODIFIÉ V2 — était LineBackRight, renommé en LineBack.</summary>
    public bool LineBack
    {
        get => _lineBack;
        private set => SetField(ref _lineBack, value);
    }

    // -----------------------------------------------------------------------
    // Lidar — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private float _lidarDist;
    /// <summary>Distance en mètres (ex: 0.42).</summary>
    public float LidarDist
    {
        get => _lidarDist;
        private set
        {
            if (SetField(ref _lidarDist, value))
                OnPropertyChanged(nameof(LidarDistText));
        }
    }

    /// <summary>Affichage formaté ex: "0.42 m".</summary>
    public string LidarDistText => $"{LidarDist:F2} m";  // MODIFIÉ V2

    private float _lidarAngle;
    /// <summary>Angle en degrés (ex: 45.0).</summary>
    public float LidarAngle
    {
        get => _lidarAngle;
        private set
        {
            if (SetField(ref _lidarAngle, value))
                OnPropertyChanged(nameof(LidarAngleText));
        }
    }

    /// <summary>Affichage formaté ex: "45.0°".</summary>
    public string LidarAngleText => $"{LidarAngle:F1}°";  // MODIFIÉ V2

    private bool _lidarValid;
    /// <summary>True = cible valide détectée.</summary>
    public bool LidarValid
    {
        get => _lidarValid;
        private set
        {
            if (SetField(ref _lidarValid, value))
                OnPropertyChanged(nameof(LidarStatusText));
        }
    }

    /// <summary>Texte d'état lidar affiché dans la Vue.</summary>
    public string LidarStatusText =>
        LidarValid ? "● Cible détectée" : "○ Aucune cible";  // MODIFIÉ V2

    // -----------------------------------------------------------------------
    // Coordonnées radar — MODIFIÉ V2
    // -----------------------------------------------------------------------
    // Le Canvas du radar a une taille fixe définie ici.
    // Le robot est toujours au centre-bas du canvas.
    // L'axe Y est inversé (0 = haut dans Avalonia).
    //
    // Convention angle : 0° = devant le robot, sens horaire.
    // Conversion polaire → cartésien :
    //   x = cx + dist_normalisée * sin(angle_rad)
    //   y = cy - dist_normalisée * cos(angle_rad)   ← Y inversé

    // Dimensions du canvas (doivent correspondre au Width/Height dans le AXAML)
    public const double RadarCanvasWidth  = 400;
    public const double RadarCanvasHeight = 300;

    // Distance max affichée sur le radar (en mètres)
    private const double MaxRadarDist = 2.0;

    private double _radarX = RadarCanvasWidth / 2;
    /// <summary>Position X du point ennemi sur le Canvas, en pixels.</summary>
    public double RadarX
    {
        get => _radarX;
        private set
        {
            if (SetField(ref _radarX, value))
                OnPropertyChanged(nameof(RadarDotLeft));
        }
    }

    private double _radarY = RadarCanvasHeight / 2;
    /// <summary>Position Y du point ennemi sur le Canvas, en pixels.</summary>
    public double RadarY
    {
        get => _radarY;
        private set
        {
            if (SetField(ref _radarY, value))
                OnPropertyChanged(nameof(RadarDotTop));
        }
    }

    // Canvas.Left/Top du coin supérieur gauche de l'ellipse (rayon = 8px)
    /// <summary>Canvas.Left de l'ellipse du point ennemi (centré sur RadarX).</summary>
    public double RadarDotLeft => RadarX - 8;
    /// <summary>Canvas.Top de l'ellipse du point ennemi (centré sur RadarY).</summary>
    public double RadarDotTop  => RadarY - 8;

    /// <summary>
    /// Recalcule la position du point ennemi sur le canvas radar.
    /// Appelé après chaque mise à jour des données lidar.
    /// </summary>
    private void UpdateRadarPosition()
    {
        // Centre du canvas = position du robot
        double cx = RadarCanvasWidth / 2.0;
        double cy = RadarCanvasHeight - 20; // robot légèrement au-dessus du bas

        if (!LidarValid || LidarDist <= 0)
        {
            // Aucune cible — cacher le point en le plaçant hors du canvas
            RadarX = -50;
            RadarY = -50;
            return;
        }

        // Normaliser la distance dans la hauteur du canvas
        double distNorm = Math.Min(LidarDist / MaxRadarDist, 1.0);
        double pixelDist = distNorm * (RadarCanvasHeight - 30);

        // Conversion polaire → cartésien (angle en degrés → radians)
        double angleRad = LidarAngle * Math.PI / 180.0;
        double px = cx + pixelDist * Math.Sin(angleRad);
        double py = cy - pixelDist * Math.Cos(angleRad); // Y inversé

        RadarX = px;
        RadarY = py;
    }

    // -----------------------------------------------------------------------
    // État du robot — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private RobotStatus _robotStatus = RobotStatus.Unknown;

    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        private set
        {
            if (SetField(ref _robotStatus, value))
                OnPropertyChanged(nameof(RobotStatusText));
        }
    }

    /// <summary>
    /// Texte d'état affiché dans la Vue.
    /// MODIFIÉ V2 — aligné sur les 4 états V1.
    /// </summary>
    public string RobotStatusText => RobotStatus switch
    {
        RobotStatus.Idle    => "IDLE",
        RobotStatus.Search  => "SEARCH",    // MODIFIÉ V2
        RobotStatus.Attack  => "ATTACK",    // MODIFIÉ V2
        RobotStatus.Defense => "DEFENSE",   // MODIFIÉ V2
        _                   => "—"
    };

    // -----------------------------------------------------------------------
    // Horodatage de la dernière mise à jour
    // -----------------------------------------------------------------------

    private string _lastUpdateText = "Aucune donnée";

    public string LastUpdateText
    {
        get => _lastUpdateText;
        private set => SetField(ref _lastUpdateText, value);
    }

    // -----------------------------------------------------------------------
    // Handlers — appelés par MessageRouter sur thread de fond
    // -----------------------------------------------------------------------

    /// <summary>
    /// Appelé par MessageRouter quand un message TELEMETRY arrive.
    /// Le parsing se fait ici hors UI thread, puis les propriétés sont
    /// mises à jour sur le UI thread via Dispatcher.
    /// </summary>
    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // Batterie — MODIFIÉ V2
            BatteryVoltage  = data.BatteryVoltage;
            BatteryPercent  = data.BatteryPercent;
            BatteryCritical = data.BatteryCritical;

            // Ligne
            LineFrontLeft  = data.LineFrontLeft;
            LineFrontRight = data.LineFrontRight;
            LineBackLeft   = data.LineBackLeft;
            LineBack       = data.LineBack;  // MODIFIÉ V2

            // Lidar — MODIFIÉ V2
            LidarDist  = data.LidarDist;
            LidarAngle = data.LidarAngle;
            LidarValid = data.LidarValid;
            UpdateRadarPosition(); // MODIFIÉ V2 — recalcul position radar

            LastUpdateText = DateTime.Now.ToString("HH:mm:ss.fff");
        });
    }

    /// <summary>
    /// Appelé par MessageRouter quand un message STATE arrive.
    /// </summary>
    public void OnStateReceived(string rawJson)
    {
        var status = TelemetryParser.ParseState(rawJson);

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            RobotStatus = status;
        });
    }
}
