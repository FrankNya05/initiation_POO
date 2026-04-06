using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;
using System.Collections.ObjectModel;
using System.Text.Json;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// StatusBarViewModel
// ---------------------------------------------------------------------------
// Drives the StatusBarView. Owns the four pieces of ambient information shown
// at the bottom of the screen:
//
//   1. IsConnected     — whether the robot is currently connected
//   2. ActiveProtocol  — which protocol is active (BLE / MQTT)
//   3. RobotStatusText — the last STATE received from the robot
//   4. RecentLogs      — the last N log strings from the robot
//
// How data flows in:
//   - IsConnected and ActiveProtocol are forwarded by MainWindowViewModel
//     using the same PropertyChanged-forwarding pattern established in Phase 3.
//   - OnStateReceived and OnLogReceived are registered with MessageRouter
//     in App.axaml.cs, exactly like the Phase 2 and Phase 3 handlers.
//
// Threading:
//   - All three handler methods are called on a background thread by
//     MessageRouter. Every property update is wrapped in
//     Dispatcher.UIThread.InvokeAsync(), consistent with Phase 2 and Phase 3.
//
// Design:
//   - RecentLogs is an ObservableCollection<string> so the View's ItemsControl
//     updates automatically when entries are added, without any extra wiring.
//   - The list is capped at MaxLogEntries to prevent unbounded growth.
//   - TelemetryParser.ParseState() is reused for STATE messages — no duplicate
//     parsing logic.
// ---------------------------------------------------------------------------

public class StatusBarViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    /// <summary>Maximum number of log entries kept in RecentLogs.</summary>
    private const int MaxLogEntries = 5;

    // -----------------------------------------------------------------------
    // Connection state
    // -----------------------------------------------------------------------

    private bool _isConnected = false;

    /// <summary>
    /// True when the robot is connected.
    /// Set by MainWindowViewModel when ConnectionPanel.IsConnected changes.
    /// Drives the connection-state dot and text in the status bar.
    /// </summary>
    public bool IsConnected
    {
        get => _isConnected;
        set
        {
            if (SetField(ref _isConnected, value))
                OnPropertyChanged(nameof(ConnectionStatusText));
        }
    }

    /// <summary>Human-readable connection state shown next to the dot.</summary>
    public string ConnectionStatusText => IsConnected ? "Connected" : "Disconnected";

    // -----------------------------------------------------------------------
    // Active protocol
    // -----------------------------------------------------------------------

    private CommProtocol _activeProtocol = CommProtocol.BLE;

    /// <summary>
    /// The protocol that is currently selected in the connection panel.
    /// Set by MainWindowViewModel when ConnectionPanel.SelectedProtocol changes.
    /// Displayed as a small badge in the status bar.
    /// </summary>
    public CommProtocol ActiveProtocol
    {
        get => _activeProtocol;
        set
        {
            if (SetField(ref _activeProtocol, value))
                OnPropertyChanged(nameof(ActiveProtocolText));
        }
    }

    /// <summary>Display string for the active protocol badge ("BLE" or "MQTT").</summary>
    public string ActiveProtocolText => ActiveProtocol switch
    {
        CommProtocol.BLE  => "BLE",
        CommProtocol.MQTT => "MQTT",
        _                 => "—"
    };

    // -----------------------------------------------------------------------
    // Robot state
    // -----------------------------------------------------------------------

    private RobotStatus _robotStatus = RobotStatus.Unknown;

    /// <summary>
    /// The most recent robot operating state received via a STATE message.
    /// Parsed by TelemetryParser.ParseState and updated by OnStateReceived.
    /// </summary>
    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        private set
        {
            if (SetField(ref _robotStatus, value))
                OnPropertyChanged(nameof(RobotStatusText));
        }
    }

    /// <summary>Human-readable robot state for display in the status bar.</summary>
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
    // Recent log entries
    // -----------------------------------------------------------------------

    /// <summary>
    /// The last N log strings received from the robot via LOG messages.
    /// ObservableCollection so the View's ItemsControl updates automatically.
    /// Newest entries are inserted at the top (index 0).
    /// Capped at MaxLogEntries to prevent unbounded growth.
    /// </summary>
    public ObservableCollection<string> RecentLogs { get; } = new();

    // -----------------------------------------------------------------------
    // Message handlers — registered with MessageRouter in App.axaml.cs,
    //                    called on a background thread
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called by MessageRouter when a STATE message arrives.
    /// Reuses TelemetryParser.ParseState — no duplicate logic.
    /// Marshals the UI update to the UI thread.
    /// </summary>
    public void OnStateReceived(string rawJson)
    {
        // Parsing is stateless, safe to do off the UI thread.
        var status = TelemetryParser.ParseState(rawJson);

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            RobotStatus = status;
        });
    }

    /// <summary>
    /// Called by MessageRouter when a LOG message arrives.
    /// Extracts the payload string and prepends it to RecentLogs.
    ///
    /// Expected format: { "type": "LOG", "payload": "some debug text" }
    ///
    /// Parsing is kept inline here because LOG payloads are trivially simple
    /// (a plain string) and do not warrant a dedicated parser method.
    /// </summary>
    public void OnLogReceived(string rawJson)
    {
        string logText;

        try
        {
            using var doc = JsonDocument.Parse(rawJson);
            logText = doc.RootElement
                         .GetProperty("payload")
                         .GetString() ?? "(empty log)";
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[StatusBarViewModel] Failed to parse LOG: {ex.Message}");
            return;
        }

        // Stamp the entry with a time so the operator can correlate events.
        var entry = $"[{DateTime.Now:HH:mm:ss}] {logText}";

        // Marshal the collection modification to the UI thread.
        // ObservableCollection is not thread-safe.
        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // Insert at top so newest entry is always visible without scrolling.
            RecentLogs.Insert(0, entry);

            // Trim to the cap so the list never grows unbounded.
            while (RecentLogs.Count > MaxLogEntries)
                RecentLogs.RemoveAt(RecentLogs.Count - 1);
        });
    }
}
