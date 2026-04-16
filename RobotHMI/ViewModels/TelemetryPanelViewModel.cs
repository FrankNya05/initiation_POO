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
