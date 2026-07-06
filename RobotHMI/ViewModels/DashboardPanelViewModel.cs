using System.Windows.Input;
using Avalonia;
using Avalonia.Collections;
using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// DashboardPanelViewModel — NOUVEAU V2.5
// ---------------------------------------------------------------------------
// Fusionne DigitalTwinViewModel + TelemetryPanelViewModel + MotorTestPanelViewModel.
// Remplace les 3 onglets JUMEAU / TÉLÉMÉTRIE / MOTEURS par un seul onglet TABLEAU.
// ---------------------------------------------------------------------------

public class DashboardPanelViewModel : ViewModelBase
{
    private readonly RobotCommunicationService _commService;

    // ── Constantes canvas ────────────────────────────────────────────────
    public const  double CanvasSize = 500.0;
    private const double Center     = CanvasSize / 2;
    private const double DohyoRadMm = 385.0;
    private const double DrawRadius = 230.0;
    private const double Scale      = DrawRadius / DohyoRadMm;
    public  const double RobotPx    = 100.0 * Scale;
    private const double HalfRobot  = RobotPx / 2;
    private const double ArrowLen   = 46.0;
    private const double EnemyRad   = 8.0;
    private const double TofRad     = 7.0;
    private const double TofOffPx   = HalfRobot + 12.0;
    private const int    MaxTrail   = 150;
    private const double FrontDeg   = 10.0;

    // ── Position robot ───────────────────────────────────────────────────
    private double _robotLeft  = Center - HalfRobot;
    private double _robotTop   = Center - HalfRobot;
    private double _robotAngle = 0.0;

    public double RobotLeft  { get => _robotLeft;  private set => SetField(ref _robotLeft,  value); }
    public double RobotTop   { get => _robotTop;   private set => SetField(ref _robotTop,   value); }
    public double RobotAngle { get => _robotAngle; private set => SetField(ref _robotAngle, value); }

    // ── Flèche de cap ────────────────────────────────────────────────────
    private Point _arrowStart = new(Center, Center);
    private Point _arrowEnd   = new(Center, Center - ArrowLen);

    public Point ArrowStart { get => _arrowStart; private set => SetField(ref _arrowStart, value); }
    public Point ArrowEnd   { get => _arrowEnd;   private set => SetField(ref _arrowEnd,   value); }

    // ── Adversaire (lidar) ───────────────────────────────────────────────
    private double _enemyLeft    = -30;
    private double _enemyTop     = -30;
    private bool   _enemyVisible = false;

    public double EnemyLeft    { get => _enemyLeft;    private set => SetField(ref _enemyLeft,    value); }
    public double EnemyTop     { get => _enemyTop;     private set => SetField(ref _enemyTop,     value); }
    public bool   EnemyVisible { get => _enemyVisible; private set => SetField(ref _enemyVisible, value); }

    // ── Indicateurs TOF canvas ───────────────────────────────────────────
    private double _tofFlLeft = -20; private double _tofFlTop = -20;
    private double _tofFrLeft = -20; private double _tofFrTop = -20;
    private bool   _tofFlActive = false;
    private bool   _tofFrActive = false;

    public double TofFlLeft   { get => _tofFlLeft;   private set => SetField(ref _tofFlLeft,   value); }
    public double TofFlTop    { get => _tofFlTop;    private set => SetField(ref _tofFlTop,    value); }
    public double TofFrLeft   { get => _tofFrLeft;   private set => SetField(ref _tofFrLeft,   value); }
    public double TofFrTop    { get => _tofFrTop;    private set => SetField(ref _tofFrTop,    value); }
    public bool   TofFlActive { get => _tofFlActive; private set => SetField(ref _tofFlActive, value); }
    public bool   TofFrActive { get => _tofFrActive; private set => SetField(ref _tofFrActive, value); }

    // ── Traîne ───────────────────────────────────────────────────────────
    public AvaloniaList<Point> Trail { get; } = new();

    // ── Pose EKF / Encodeurs ─────────────────────────────────────────────
    private string _poseText     = "En attente de données…";
    private string _encLeftText  = "—";
    private string _encRightText = "—";

    public string PoseText     { get => _poseText;     private set => SetField(ref _poseText,     value); }
    public string EncLeftText  { get => _encLeftText;  private set => SetField(ref _encLeftText,  value); }
    public string EncRightText { get => _encRightText; private set => SetField(ref _encRightText, value); }

    // ── Batterie ─────────────────────────────────────────────────────────
    private float _batteryVoltage;
    private int   _batteryPercent;
    private bool  _batteryCritical;

    public float BatteryVoltage  { get => _batteryVoltage;  private set { if (SetField(ref _batteryVoltage,  value)) OnPropertyChanged(nameof(BatteryVoltageText)); } }
    public int   BatteryPercent  { get => _batteryPercent;  private set =>    SetField(ref _batteryPercent,  value); }
    public bool  BatteryCritical { get => _batteryCritical; private set =>    SetField(ref _batteryCritical, value); }
    public string BatteryVoltageText => $"{_batteryVoltage:F2} V";

    // ── Capteurs de ligne ─────────────────────────────────────────────────
    private bool _lineFrontLeft, _lineFrontRight, _lineBackLeft, _lineBack;

    public bool LineFrontLeft  { get => _lineFrontLeft;  private set => SetField(ref _lineFrontLeft,  value); }
    public bool LineFrontRight { get => _lineFrontRight; private set => SetField(ref _lineFrontRight, value); }
    public bool LineBackLeft   { get => _lineBackLeft;   private set => SetField(ref _lineBackLeft,   value); }
    public bool LineBack       { get => _lineBack;       private set => SetField(ref _lineBack,       value); }

    // ── Lidar ────────────────────────────────────────────────────────────
    private float _lidarDist;
    private float _lidarAngle;
    private bool  _lidarValid;

    public float LidarDist  { get => _lidarDist;  private set { if (SetField(ref _lidarDist,  value)) OnPropertyChanged(nameof(LidarDistText)); } }
    public float LidarAngle { get => _lidarAngle; private set { if (SetField(ref _lidarAngle, value)) OnPropertyChanged(nameof(LidarAngleText)); } }
    public bool  LidarValid { get => _lidarValid; private set { if (SetField(ref _lidarValid, value)) OnPropertyChanged(nameof(LidarStatusText)); } }

    public string LidarDistText   => $"{_lidarDist:F2} m";
    public string LidarAngleText  => $"{_lidarAngle:F1}°";
    public string LidarStatusText => _lidarValid ? "Cible détectée" : "Aucune cible";

    // ── TOF texte ─────────────────────────────────────────────────────────
    private string _tofFlText = "—";
    private string _tofFrText = "—";

    public string TofFlText { get => _tofFlText; private set => SetField(ref _tofFlText, value); }
    public string TofFrText { get => _tofFrText; private set => SetField(ref _tofFrText, value); }

    // ── Moteurs PWM ───────────────────────────────────────────────────────
    private string _motorLeftText  = "0";
    private string _motorRightText = "0";

    public string MotorLeftText  { get => _motorLeftText;  private set => SetField(ref _motorLeftText,  value); }
    public string MotorRightText { get => _motorRightText; private set => SetField(ref _motorRightText, value); }

    // ── État robot ────────────────────────────────────────────────────────
    private RobotStatus _robotStatus = RobotStatus.Unknown;

    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        private set { if (SetField(ref _robotStatus, value)) OnPropertyChanged(nameof(RobotStatusText)); }
    }

    public string RobotStatusText => _robotStatus switch
    {
        RobotStatus.Idle    => "IDLE",
        RobotStatus.Search  => "SEARCH",
        RobotStatus.Attack  => "ATTACK",
        RobotStatus.Defense => "DEFENSE",
        _                   => "—"
    };

    private string _lastUpdateText = "—";
    public string LastUpdateText { get => _lastUpdateText; private set => SetField(ref _lastUpdateText, value); }

    // ── Commandes moteur (test) ───────────────────────────────────────────
    private int _speed = 200;

    public int Speed
    {
        get => _speed;
        set { if (SetField(ref _speed, Math.Clamp(value, 0, 255))) OnPropertyChanged(nameof(SpeedText)); }
    }
    public string SpeedText => _speed.ToString();

    public ICommand ForwardCommand  { get; }
    public ICommand BackwardCommand { get; }
    public ICommand LeftCommand     { get; }
    public ICommand RightCommand    { get; }
    public ICommand StopCommand     { get; }

    private string _lastCommandText = "Aucune";
    public string LastCommandText { get => _lastCommandText; private set => SetField(ref _lastCommandText, value); }

    // ── Constructeur ──────────────────────────────────────────────────────
    public DashboardPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        ForwardCommand  = new AsyncRelayCommand(OnForwardAsync);
        BackwardCommand = new AsyncRelayCommand(OnBackwardAsync);
        LeftCommand     = new AsyncRelayCommand(OnLeftAsync);
        RightCommand    = new AsyncRelayCommand(OnRightAsync);
        StopCommand     = new AsyncRelayCommand(OnStopAsync);
    }

    // ── Handler MQTT TELEMETRY ─────────────────────────────────────────────
    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // ── Canvas dohyo ──────────────────────────────────────────────
            double theta = data.PoseTheta;
            double cx = Center + data.PoseX * Scale;
            double cy = Center - data.PoseY * Scale;

            RobotLeft  = cx - HalfRobot;
            RobotTop   = cy - HalfRobot;
            RobotAngle = -theta * 180.0 / Math.PI;

            ArrowStart = new Point(cx, cy);
            ArrowEnd   = new Point(cx + ArrowLen * Math.Cos(theta),
                                   cy - ArrowLen * Math.Sin(theta));

            if (data.LidarValid && data.LidarDist > 0)
            {
                double worldAngle = theta - (data.LidarAngle - FrontDeg) * Math.PI / 180.0;
                double distPx     = data.LidarDist * 1000.0 * Scale;
                EnemyLeft    = cx + distPx * Math.Cos(worldAngle) - EnemyRad;
                EnemyTop     = cy - distPx * Math.Sin(worldAngle) - EnemyRad;
                EnemyVisible = true;
            }
            else
            {
                EnemyVisible = false;
            }

            double tofAngL = theta + 20.0 * Math.PI / 180.0;
            double tofAngR = theta - 20.0 * Math.PI / 180.0;
            TofFlLeft   = cx + TofOffPx * Math.Cos(tofAngL) - TofRad;
            TofFlTop    = cy - TofOffPx * Math.Sin(tofAngL) - TofRad;
            TofFrLeft   = cx + TofOffPx * Math.Cos(tofAngR) - TofRad;
            TofFrTop    = cy - TofOffPx * Math.Sin(tofAngR) - TofRad;
            TofFlActive = data.TofFrontLeft  is > 80 and < 280;
            TofFrActive = data.TofFrontRight is > 80 and < 280;

            Trail.Add(new Point(cx, cy));
            while (Trail.Count > MaxTrail) Trail.RemoveAt(0);

            double thetaDeg = theta * 180.0 / Math.PI;
            PoseText     = $"x: {data.PoseX:F0} mm   y: {data.PoseY:F0} mm   θ: {thetaDeg:F1}°";
            EncLeftText  = $"{data.EncoderLeftRpm:F0}";
            EncRightText = $"{data.EncoderRightRpm:F0}";

            // ── Télémétrie ────────────────────────────────────────────────
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

            TofFlText   = data.TofFrontLeft  >= 0 ? $"{data.TofFrontLeft:F0} mm"  : "—";
            TofFrText   = data.TofFrontRight >= 0 ? $"{data.TofFrontRight:F0} mm" : "—";

            MotorLeftText  = data.MotorLeft.ToString();
            MotorRightText = data.MotorRight.ToString();

            LastUpdateText = DateTime.Now.ToString("HH:mm:ss.fff");
        });
    }

    // ── Handler MQTT STATE ────────────────────────────────────────────────
    public void OnStateReceived(string rawJson)
    {
        var status = TelemetryParser.ParseState(rawJson);
        Dispatcher.UIThread.InvokeAsync(() => RobotStatus = status);
    }

    // ── Commandes moteur ──────────────────────────────────────────────────
    private async Task OnForwardAsync()
        => await SendSafe(MotorCommand.Direct(Speed, Speed), $"↑ {Speed}");

    private async Task OnBackwardAsync()
        => await SendSafe(MotorCommand.Direct(-Speed, -Speed), $"↓ {Speed}");

    private async Task OnLeftAsync()
        => await SendSafe(MotorCommand.Direct(-Speed, Speed), $"← {Speed}");

    private async Task OnRightAsync()
        => await SendSafe(MotorCommand.Direct(Speed, -Speed), $"→ {Speed}");

    private async Task OnStopAsync()
        => await SendSafe(MotorCommand.Stop(), "STOP");

    private async Task SendSafe(MotorCommand command, string displayText)
    {
        try
        {
            await _commService.SendAsync(CommandSerializer.Serialize(command));
            LastCommandText = displayText;
        }
        catch (Exception ex)
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
                LastCommandText = $"⚠ {ex.Message}");
        }
    }
}
