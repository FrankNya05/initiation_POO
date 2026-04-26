using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// TelemetryPanelViewModel — MODIFIÉ V2.1
// ---------------------------------------------------------------------------
// Ajouts V2.1 :
//   - TofFrontLeft / TofFrontRight et leurs textes formatés
//   - TofValid pour masquer la section quand données absentes
// ---------------------------------------------------------------------------

public class TelemetryPanelViewModel : ViewModelBase
{
    // ── Batterie ─────────────────────────────────────────────────────────

    private float _batteryVoltage;
    public float BatteryVoltage
    {
        get => _batteryVoltage;
        private set { if (SetField(ref _batteryVoltage, value)) OnPropertyChanged(nameof(BatteryVoltageText)); }
    }
    public string BatteryVoltageText => $"{BatteryVoltage:F2} V";

    private int _batteryPercent;
    public int BatteryPercent
    {
        get => _batteryPercent;
        private set => SetField(ref _batteryPercent, value);
    }

    private bool _batteryCritical;
    public bool BatteryCritical
    {
        get => _batteryCritical;
        private set => SetField(ref _batteryCritical, value);
    }

    // ── Capteurs de ligne ─────────────────────────────────────────────────

    private bool _lineFrontLeft;
    public bool LineFrontLeft  { get => _lineFrontLeft;  private set => SetField(ref _lineFrontLeft,  value); }

    private bool _lineFrontRight;
    public bool LineFrontRight { get => _lineFrontRight; private set => SetField(ref _lineFrontRight, value); }

    private bool _lineBackLeft;
    public bool LineBackLeft   { get => _lineBackLeft;   private set => SetField(ref _lineBackLeft,   value); }

    private bool _lineBack;
    public bool LineBack       { get => _lineBack;       private set => SetField(ref _lineBack,       value); }

    // ── Lidar ─────────────────────────────────────────────────────────────

    private float _lidarDist;
    public float LidarDist
    {
        get => _lidarDist;
        private set { if (SetField(ref _lidarDist, value)) OnPropertyChanged(nameof(LidarDistText)); }
    }
    public string LidarDistText => $"{LidarDist:F2} m";

    private float _lidarAngle;
    public float LidarAngle
    {
        get => _lidarAngle;
        private set { if (SetField(ref _lidarAngle, value)) OnPropertyChanged(nameof(LidarAngleText)); }
    }
    public string LidarAngleText => $"{LidarAngle:F1}°";

    private bool _lidarValid;
    public bool LidarValid
    {
        get => _lidarValid;
        private set { if (SetField(ref _lidarValid, value)) OnPropertyChanged(nameof(LidarStatusText)); }
    }
    public string LidarStatusText => LidarValid ? "Cible détectée" : "Aucune cible";

    // ── TOF — MODIFIÉ V2.1 ───────────────────────────────────────────────

    private float _tofFrontLeft = -1f;
    public float TofFrontLeft
    {
        get => _tofFrontLeft;
        private set { if (SetField(ref _tofFrontLeft, value)) OnPropertyChanged(nameof(TofFrontLeftText)); }
    }
    /// <summary>Affichage : "342 mm" ou "---" si invalide.</summary>
    public string TofFrontLeftText  => _tofFrontLeft  >= 0 ? $"{_tofFrontLeft:F0} mm" : "---";

    private float _tofFrontRight = -1f;
    public float TofFrontRight
    {
        get => _tofFrontRight;
        private set { if (SetField(ref _tofFrontRight, value)) OnPropertyChanged(nameof(TofFrontRightText)); }
    }
    /// <summary>Affichage : "310 mm" ou "---" si invalide.</summary>
    public string TofFrontRightText => _tofFrontRight >= 0 ? $"{_tofFrontRight:F0} mm" : "---";

    /// <summary>True si au moins un TOF a reçu des données valides.</summary>
    public bool TofValid => _tofFrontLeft >= 0 || _tofFrontRight >= 0;

    // ── Moteurs (PWM actuel) — MODIFIÉ V2.2 ──────────────────────────────

    private int _motorLeft;
    public int MotorLeft
    {
        get => _motorLeft;
        private set { if (SetField(ref _motorLeft, value)) OnPropertyChanged(nameof(MotorLeftText)); }
    }
    /// <summary>Ex: "200" ou "-150".</summary>
    public string MotorLeftText => _motorLeft.ToString();

    private int _motorRight;
    public int MotorRight
    {
        get => _motorRight;
        private set { if (SetField(ref _motorRight, value)) OnPropertyChanged(nameof(MotorRightText)); }
    }
    public string MotorRightText => _motorRight.ToString();

    // ── Horodatage ────────────────────────────────────────────────────────

    private string _lastUpdateText = "Aucune donnée";
    public string LastUpdateText
    {
        get => _lastUpdateText;
        private set => SetField(ref _lastUpdateText, value);
    }

    // ── Radar ─────────────────────────────────────────────────────────────

    public const double RadarCanvasWidth  = 400;
    public const double RadarCanvasHeight = 400;  // Carré pour un cercle complet
    private const double MaxRadarDist = 0.5;      // Arène 77cm — affichage limité à 0.5m

    private double _radarDotLeft = -50;
    public double RadarDotLeft
    {
        get => _radarDotLeft;
        private set => SetField(ref _radarDotLeft, value);
    }

    private double _radarDotTop = -50;
    public double RadarDotTop
    {
        get => _radarDotTop;
        private set => SetField(ref _radarDotTop, value);
    }

    private void UpdateRadarPosition()
    {
        // Robot au centre exact du canvas — cercle complet 360°
        double cx = RadarCanvasWidth  / 2.0;  // 200
        double cy = RadarCanvasHeight / 2.0;  // 200

        if (!LidarValid || LidarDist <= 0)
        {
            RadarDotLeft = -50;
            RadarDotTop  = -50;
            return;
        }

        // Normaliser — au-delà de MaxRadarDist on clamp au bord
        double distNorm  = Math.Min(LidarDist / MaxRadarDist, 1.0);
        // Rayon utile = 190px (laisse une marge de 10px par rapport au bord)
        double pixelDist = distNorm * 190.0;

        // Convention lidar : 0° = avant du robot (haut du canvas)
        // Avalonia : Y augmente vers le bas → on soustrait pour aller vers le haut
        double angleRad = LidarAngle * Math.PI / 180.0;
        RadarDotLeft = cx + pixelDist * Math.Sin(angleRad) - 8;
        RadarDotTop  = cy - pixelDist * Math.Cos(angleRad) - 8;
    }

    // ── État robot ────────────────────────────────────────────────────────

    private RobotStatus _robotStatus = RobotStatus.Unknown;
    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        private set { if (SetField(ref _robotStatus, value)) OnPropertyChanged(nameof(RobotStatusText)); }
    }
    public string RobotStatusText => RobotStatus switch
    {
        RobotStatus.Idle    => "IDLE",
        RobotStatus.Search  => "SEARCH",
        RobotStatus.Attack  => "ATTACK",
        RobotStatus.Defense => "DEFENSE",
        _                   => "—"
    };

    // ── Handlers MessageRouter ────────────────────────────────────────────

    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            BatteryVoltage  = data.BatteryVoltage;
            BatteryPercent  = data.BatteryPercent;
            BatteryCritical = data.BatteryCritical;

            LineFrontLeft  = data.LineFrontLeft;
            LineFrontRight = data.LineFrontRight;
            LineBackLeft   = data.LineBackLeft;
            LineBack       = data.LineBack;

            LidarDist  = data.LidarDist;
            LidarAngle = data.LidarAngle;
            LidarValid = data.LidarValid;
            UpdateRadarPosition();

            // TOF — MODIFIÉ V2.1
            TofFrontLeft  = data.TofFrontLeft;
            TofFrontRight = data.TofFrontRight;
            OnPropertyChanged(nameof(TofValid));

            // Moteurs — MODIFIÉ V2.2
            MotorLeft  = data.MotorLeft;
            MotorRight = data.MotorRight;

            LastUpdateText = DateTime.Now.ToString("HH:mm:ss.fff");
        });
    }

    public void OnStateReceived(string rawJson)
    {
        var status = TelemetryParser.ParseState(rawJson);
        Dispatcher.UIThread.InvokeAsync(() => RobotStatus = status);
    }
}
