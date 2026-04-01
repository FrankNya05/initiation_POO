using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// TelemetryPanelViewModel
// ---------------------------------------------------------------------------
// Drives the TelemetryPanelView. Owns all observable properties that the
// telemetry panel displays: line sensor states, IR distances, robot status.
//
// How it fits into the flow (architecture doc §7.1):
//   ESP32 → ICommClient → RobotCommunicationService → MessageRouter
//     → OnTelemetryReceived(rawJson) / OnStateReceived(rawJson)
//       → TelemetryParser.ParseTelemetry / ParseState
//         → Dispatcher.UIThread.InvokeAsync → SetField → binding updates
//
// Rules followed:
//   - Does NOT parse JSON itself — delegates to TelemetryParser
//   - Does NOT reference any communication class
//   - ALL property updates that touch bound properties happen inside
//     Dispatcher.UIThread.InvokeAsync() because the router calls these
//     handlers from a background thread
// ---------------------------------------------------------------------------

public class TelemetryPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Line sensor properties
    // -----------------------------------------------------------------------

    private bool _lineFrontLeft;
    /// <summary>True when the front-left line sensor detects the white border.</summary>
    public bool LineFrontLeft
    {
        get => _lineFrontLeft;
        private set => SetField(ref _lineFrontLeft, value);
    }

    private bool _lineFrontRight;
    /// <summary>True when the front-right line sensor detects the white border.</summary>
    public bool LineFrontRight
    {
        get => _lineFrontRight;
        private set => SetField(ref _lineFrontRight, value);
    }

    private bool _lineBackLeft;
    /// <summary>True when the back-left line sensor detects the white border.</summary>
    public bool LineBackLeft
    {
        get => _lineBackLeft;
        private set => SetField(ref _lineBackLeft, value);
    }

    private bool _lineBackRight;
    /// <summary>True when the back-right line sensor detects the white border.</summary>
    public bool LineBackRight
    {
        get => _lineBackRight;
        private set => SetField(ref _lineBackRight, value);
    }

    // -----------------------------------------------------------------------
    // IR / distance sensor properties (centimetres)
    // -----------------------------------------------------------------------

    private int _irFront;
    /// <summary>Distance in centimetres from the front IR sensor.</summary>
    public int IrFront
    {
        get => _irFront;
        private set => SetField(ref _irFront, value);
    }

    private int _irLeft;
    /// <summary>Distance in centimetres from the left IR sensor.</summary>
    public int IrLeft
    {
        get => _irLeft;
        private set => SetField(ref _irLeft, value);
    }

    private int _irRight;
    /// <summary>Distance in centimetres from the right IR sensor.</summary>
    public int IrRight
    {
        get => _irRight;
        private set => SetField(ref _irRight, value);
    }

    // -----------------------------------------------------------------------
    // Robot state
    // -----------------------------------------------------------------------

    private RobotStatus _robotStatus = RobotStatus.Unknown;
    /// <summary>Current operating state reported by the robot.</summary>
    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        private set
        {
            if (SetField(ref _robotStatus, value))
                // Keep the display string in sync automatically.
                OnPropertyChanged(nameof(RobotStatusText));
        }
    }

    /// <summary>
    /// Human-readable version of RobotStatus shown directly in the View.
    /// Derived property — updated whenever RobotStatus changes.
    /// </summary>
    public string RobotStatusText => RobotStatus switch
    {
        RobotStatus.Idle       => "IDLE",
        RobotStatus.Running    => "RUNNING",
        RobotStatus.Searching  => "SEARCHING",
        RobotStatus.Attacking  => "ATTACKING",
        RobotStatus.Retreating => "RETREATING",
        RobotStatus.Error      => "ERROR",
        _                      => "—"
    };

    // -----------------------------------------------------------------------
    // Timestamp of the last received telemetry update
    // -----------------------------------------------------------------------

    private string _lastUpdateText = "No data yet";
    /// <summary>Short string showing when the last telemetry packet arrived.</summary>
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
    /// Parses the JSON and updates all observable properties on the UI thread.
    /// </summary>
    public void OnTelemetryReceived(string rawJson)
    {
        // Parsing happens here, off the UI thread — this is fine because
        // TelemetryParser is stateless and creates no UI objects.
        var data = TelemetryParser.ParseTelemetry(rawJson);

        if (data == null)
            return; // Parser already logged the error

        // Marshal property updates to the UI thread.
        // This is REQUIRED because MessageRouter calls us from a background thread.
        Dispatcher.UIThread.InvokeAsync(() =>
        {
            LineFrontLeft  = data.LineFrontLeft;
            LineFrontRight = data.LineFrontRight;
            LineBackLeft   = data.LineBackLeft;
            LineBackRight  = data.LineBackRight;

            IrFront = data.IrFront;
            IrLeft  = data.IrLeft;
            IrRight = data.IrRight;

            LastUpdateText = $"Last update: {DateTime.Now:HH:mm:ss}";
        });
    }

    /// <summary>
    /// Called by MessageRouter when a STATE message arrives.
    /// Parses the state string and updates RobotStatus on the UI thread.
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
