using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MainWindowViewModel
// ---------------------------------------------------------------------------
// The root ViewModel for the application window.
// Its only job in Phase 1 is to hold the child ViewModels and expose them
// as properties so the Views can bind to them.
//
// Phase 1: holds ConnectionPanelViewModel only.
// Phase 2: TelemetryPanelViewModel will be added here.
// Phase 3: ControlPanelViewModel will be added here.
// Phase 4: StatusBarViewModel will be added here.
//
// The pattern is always the same: add a new property, instantiate the VM
// in the constructor, done. No existing code changes.
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

    // Phase 3: public ControlPanelViewModel   ControlPanel   { get; }
    // Phase 4: public StatusBarViewModel      StatusBar      { get; }

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    public MainWindowViewModel(RobotCommunicationService commService)
    {
        // Pass the shared service instance to each child ViewModel that needs it.
        // All VMs share the same service — one connection for the whole app.
        ConnectionPanel = new ConnectionPanelViewModel(commService);

        // TelemetryPanelViewModel has no dependency on commService directly —
        // it receives parsed data via MessageRouter handler callbacks (App.axaml.cs).
        TelemetryPanel = new TelemetryPanelViewModel();
    }
}
