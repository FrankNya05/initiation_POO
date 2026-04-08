using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// TelemetryPanelViewModel — V1 protocol
// ---------------------------------------------------------------------------
// Drives the TelemetryPanelView.
//
// V1 data received via OnTelemetryReceived:
//   - Timestamp
//   - Battery (voltage, percent, critical)
//   - Line sensors (frontLeft, frontRight, backLeft, back)
//   - Lidar (dist, angle, valid)
//
// Threading rule: all SetField calls happen inside Dispatcher.UIThread.InvokeAsync
// because MessageRouter calls these handlers from a background thread.
// ---------------------------------------------------------------------------

public class TelemetryPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Line sensor properties
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

    /// <summary>
    /// V1 uses a single rear sensor ("back"), not separate backLeft/backRight.
    /// The View binds to LineBack for the rear indicator.
    /// </summary>
    private bool _lineBack;
    public bool LineBack
    {
        get => _lineBack;
        private set => SetField(ref _lineBack, value);
    }

    // -----------------------------------------------------------------------
    // Battery properties
    // -----------------------------------------------------------------------

    private float _batteryVoltage;
    /// <summary>Battery voltage in volts (e.g. 7.4).</summary>
    public float BatteryVoltage
    {
        get => _batteryVoltage;
        private set
        {
            if (SetField(ref _batteryVoltage, value))
                OnPropertyChanged(nameof(BatteryVoltageText));
        }
    }

    /// <summary>Formatted voltage string for display, e.g. "7.4 V".</summary>
    public string BatteryVoltageText => $"{BatteryVoltage:F1} V";

    private int _batteryPercent;
    /// <summary>State of charge, 0–100.</summary>
    public int BatteryPercent
    {
        get => _batteryPercent;
        private set => SetField(ref _batteryPercent, value);
    }

    private bool _batteryCritical;
    /// <summary>True when the battery is critically low — can drive a warning colour in the View.</summary>
    public bool BatteryCritical
    {
        get => _batteryCritical;
        private set => SetField(ref _batteryCritical, value);
    }

    // -----------------------------------------------------------------------
    // Lidar properties
    // -----------------------------------------------------------------------

    private float _lidarDist;
    /// <summary>Lidar distance (float, as sent by ESP32).</summary>
    public float LidarDist
    {
        get => _lidarDist;
        private set => SetField(ref _lidarDist, value);
    }

    private float _lidarAngle;
    /// <summary>Lidar target angle in degrees (float, as sent by ESP32).</summary>
    public float LidarAngle
    {
        get => _lidarAngle;
        private set => SetField(ref _lidarAngle, value);
    }

    private bool _lidarValid;
    /// <summary>True when the lidar reading is valid (target in range).</summary>
    public bool LidarValid
    {
        get => _lidarValid;
        private set => SetField(ref _lidarValid, value);
    }

    // -----------------------------------------------------------------------
    // Robot state
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

    public string RobotStatusText => RobotStatus switch
    {
        RobotStatus.Idle       => "IDLE",
        RobotStatus.Searching  => "SEARCH",
        RobotStatus.Attacking  => "ATTACK",
        RobotStatus.Retreating => "DEFENSE",
        RobotStatus.Error      => "ERROR",
        _                      => "—"
    };

    // -----------------------------------------------------------------------
    // Timestamp / last update
    // -----------------------------------------------------------------------

    private string _lastUpdateText = "No data yet";
    public string LastUpdateText
    {
        get => _lastUpdateText;
        private set => SetField(ref _lastUpdateText, value);
    }

    // -----------------------------------------------------------------------
    // Message handlers — called by MessageRouter on a background thread
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called by MessageRouter when a TELEMETRY message arrives.
    /// Delegates parsing to TelemetryParser, then updates UI on the UI thread.
    /// </summary>
    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null)
            return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // Line sensors
            LineFrontLeft  = data.Line.FrontLeft;
            LineFrontRight = data.Line.FrontRight;
            LineBackLeft   = data.Line.BackLeft;
            LineBack       = data.Line.Back;

            // Battery
            BatteryVoltage  = data.Battery.Voltage;
            BatteryPercent  = data.Battery.Percent;
            BatteryCritical = data.Battery.Critical;

            // Lidar
            LidarDist  = data.Lidar.Dist;
            LidarAngle = data.Lidar.Angle;
            LidarValid = data.Lidar.Valid;

            LastUpdateText = $"ts={data.Timestamp}  {DateTime.Now:HH:mm:ss}";
        });
    }

    /// <summary>
    /// Called by MessageRouter when a STATE message arrives.
    /// Delegates parsing to TelemetryParser.ParseState.
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
