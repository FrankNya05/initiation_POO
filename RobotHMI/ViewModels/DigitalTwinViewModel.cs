using Avalonia;
using Avalonia.Collections;
using Avalonia.Threading;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// DigitalTwinViewModel — NOUVEAU V2.3
// ---------------------------------------------------------------------------
// Calcule toutes les coordonnées canvas à partir de la télémétrie EKF.
//
// Repère monde (EKF) :
//   x → droite, y → haut, theta → sens trigonométrique (rad)
//
// Repère canvas (Avalonia) :
//   X → droite, Y → bas
//   cx = Center + worldX * Scale
//   cy = Center - worldY * Scale   (Y inversé)
//
// Dohyo : 770 mm diamètre = 385 mm rayon.
// Canvas : 500×500px, cercle affiché sur 460px (DrawRadius=230px).
// Scale  : 230 / 385 ≈ 0.597 px/mm
// ---------------------------------------------------------------------------

public class DigitalTwinViewModel : ViewModelBase
{
    // ── Constantes canvas ────────────────────────────────────────────────
    public const  double CanvasSize  = 500.0;
    private const double Center      = CanvasSize / 2;          // 250 px
    private const double DohyoRadMm  = 385.0;
    private const double DrawRadius  = 230.0;                   // px
    private const double Scale       = DrawRadius / DohyoRadMm; // ≈ 0.597 px/mm
    public  const double RobotPx     = 100.0 * Scale;          // ≈ 59.7 px
    private const double HalfRobot   = RobotPx / 2;
    private const double ArrowLen    = 46.0;                    // px
    private const double EnemyRad    = 8.0;                     // px rayon du point ennemi
    private const double TofRad      = 7.0;                     // px rayon indicateur TOF
    private const double TofOffPx    = HalfRobot + 12.0;       // offset TOF depuis centre
    private const int    MaxTrail    = 150;
    private const double FrontDeg    = 10.0;                    // offset "avant" lidar

    // ── Robot ────────────────────────────────────────────────────────────

    private double _robotLeft  = Center - HalfRobot;
    private double _robotTop   = Center - HalfRobot;
    private double _robotAngle = 0.0;

    public double RobotLeft  { get => _robotLeft;  private set => SetField(ref _robotLeft,  value); }
    public double RobotTop   { get => _robotTop;   private set => SetField(ref _robotTop,   value); }
    /// <summary>Angle de rotation en degrés pour le RenderTransform Avalonia.</summary>
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

    // ── Indicateurs TOF ──────────────────────────────────────────────────

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
    // AvaloniaList<Point> implémente INotifyCollectionChanged :
    // Polyline se met à jour automatiquement quand des points sont ajoutés.

    public AvaloniaList<Point> Trail { get; } = new();

    // ── Données texte ────────────────────────────────────────────────────

    private string _poseText     = "En attente de données…";
    private string _encLeftText  = "—";
    private string _encRightText = "—";
    private string _tofFlText    = "—";
    private string _tofFrText    = "—";

    public string PoseText     { get => _poseText;     private set => SetField(ref _poseText,     value); }
    public string EncLeftText  { get => _encLeftText;  private set => SetField(ref _encLeftText,  value); }
    public string EncRightText { get => _encRightText; private set => SetField(ref _encRightText, value); }
    public string TofFlText    { get => _tofFlText;    private set => SetField(ref _tofFlText,    value); }
    public string TofFrText    { get => _tofFrText;    private set => SetField(ref _tofFrText,    value); }

    // ── Handler MQTT ─────────────────────────────────────────────────────

    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            double theta = data.PoseTheta;

            // Position du robot sur le canvas
            double cx = Center + data.PoseX * Scale;
            double cy = Center - data.PoseY * Scale;   // Y inversé

            // ── Robot ────────────────────────────────────────────────────
            RobotLeft  = cx - HalfRobot;
            RobotTop   = cy - HalfRobot;
            // Avalonia RotateTransform : degrés, positif = CW sur écran.
            // theta positif (CCW dans le monde) → rotation CW sur canvas → négatif.
            RobotAngle = -theta * 180.0 / Math.PI;

            // ── Flèche de cap ─────────────────────────────────────────────
            ArrowStart = new Point(cx, cy);
            ArrowEnd   = new Point(cx + ArrowLen * Math.Cos(theta),
                                   cy - ArrowLen * Math.Sin(theta));

            // ── Adversaire lidar ─────────────────────────────────────────
            // Convention lidar YDLIDAR : angle croissant = CW (vers la droite du robot)
            // FRONT_DEG = 10° = axe avant robot dans le repère lidar
            // worldAngle = theta - (lidarAngle - FRONT_DEG) * PI/180
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

            // ── TOF ───────────────────────────────────────────────────────
            // TOF front-left  ≈ +20° à gauche du cap
            // TOF front-right ≈ -20° à droite du cap
            double tofAngL = theta + 20.0 * Math.PI / 180.0;
            double tofAngR = theta - 20.0 * Math.PI / 180.0;
            TofFlLeft   = cx + TofOffPx * Math.Cos(tofAngL) - TofRad;
            TofFlTop    = cy - TofOffPx * Math.Sin(tofAngL) - TofRad;
            TofFrLeft   = cx + TofOffPx * Math.Cos(tofAngR) - TofRad;
            TofFrTop    = cy - TofOffPx * Math.Sin(tofAngR) - TofRad;
            TofFlActive = data.TofFrontLeft  is > 80 and < 280;
            TofFrActive = data.TofFrontRight is > 80 and < 280;
            TofFlText   = data.TofFrontLeft  >= 0 ? $"{data.TofFrontLeft:F0} mm" : "—";
            TofFrText   = data.TofFrontRight >= 0 ? $"{data.TofFrontRight:F0} mm" : "—";

            // ── Traîne ────────────────────────────────────────────────────
            Trail.Add(new Point(cx, cy));
            while (Trail.Count > MaxTrail)
                Trail.RemoveAt(0);

            // ── Texte ─────────────────────────────────────────────────────
            double thetaDeg = theta * 180.0 / Math.PI;
            PoseText     = $"x: {data.PoseX:F0} mm   y: {data.PoseY:F0} mm   θ: {thetaDeg:F1}°";
            EncLeftText  = $"{data.EncoderLeftRpm:F0}";
            EncRightText = $"{data.EncoderRightRpm:F0}";
        });
    }
}
