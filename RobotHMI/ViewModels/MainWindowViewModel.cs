using System.ComponentModel;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MainWindowViewModel
// ---------------------------------------------------------------------------
// The root ViewModel for the application window.
// Holds all child ViewModels and exposes them as properties so Views can bind.
//
// Phase 1: ConnectionPanelViewModel
// Phase 2: TelemetryPanelViewModel  (added)
// Phase 3: ControlPanelViewModel    (added)
// Phase 4: StatusBarViewModel       (added)
//
// Connection-state forwarding:
//   MainWindowViewModel subscribes to ConnectionPanel.PropertyChanged and
//   forwards relevant values to child VMs that need them:
//     - ControlPanel.IsConnected     (Phase 3)
//     - StatusBar.IsConnected        (Phase 4)
//     - StatusBar.ActiveProtocol     (Phase 4)
//   This keeps all cross-VM wiring in one place using only the
//   INotifyPropertyChanged mechanism already present in the codebase.
// ---------------------------------------------------------------------------

public class MainWindowViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Child ViewModels
    // -----------------------------------------------------------------------

    /// <summary>
    /// ViewModel for the connection panel (protocol selector, params, connect button).
    /// Bound to ConnectionPanelView in MainWindow.axaml.
    /// </summary>
    public ConnectionPanelViewModel ConnectionPanel { get; }

    /// <summary>
    /// ViewModel for the telemetry panel (sensor values, robot state).
    /// Bound to TelemetryPanelView in MainWindow.axaml.
    /// Added in Phase 2.
    /// </summary>
    public TelemetryPanelViewModel TelemetryPanel { get; }

    /// <summary>
    /// ViewModel for the control panel (direction buttons, speed slider).
    /// Bound to ControlPanelView in MainWindow.axaml.
    /// Added in Phase 3.
    /// </summary>
    public ControlPanelViewModel ControlPanel { get; }

    /// <summary>
    /// ViewModel for the status bar (connection badge, protocol, robot state, logs).
    /// Bound to StatusBarView in MainWindow.axaml.
    /// Added in Phase 4.
    /// </summary>
    public StatusBarViewModel StatusBar { get; }

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    public MainWindowViewModel(RobotCommunicationService commService)
    {
        // All VMs that need to send or receive share the same service instance.
        ConnectionPanel = new ConnectionPanelViewModel(commService);

        // TelemetryPanelViewModel receives data via MessageRouter callbacks —
        // no direct dependency on commService.
        TelemetryPanel = new TelemetryPanelViewModel();

        // ControlPanelViewModel needs commService to call SendAsync.
        ControlPanel = new ControlPanelViewModel(commService);

        // StatusBarViewModel receives data via MessageRouter callbacks and
        // connection-state forwarding — no direct dependency on commService.
        StatusBar = new StatusBarViewModel();

        // Forward connection state changes from ConnectionPanel to the VMs
        // that need them. A single subscription handles all forwarding so
        // the logic stays in one place.
        ConnectionPanel.PropertyChanged += OnConnectionPanelPropertyChanged;

        // Initialise the status bar with the current protocol selection so
        // it displays correctly before any connection is made.
        StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
    }

    // -----------------------------------------------------------------------
    // Connection-state forwarding
    // -----------------------------------------------------------------------

    private void OnConnectionPanelPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        switch (e.PropertyName)
        {
            // IsConnected changed — update both ControlPanel and StatusBar.
            case nameof(ConnectionPanelViewModel.IsConnected):
                ControlPanel.IsConnected = ConnectionPanel.IsConnected;
                StatusBar.IsConnected    = ConnectionPanel.IsConnected;
                break;

            // SelectedProtocol changed — update the StatusBar badge.
            case nameof(ConnectionPanelViewModel.SelectedProtocol):
                StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
                break;
        }
    }
}
