using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ControlPanelViewModel
// ---------------------------------------------------------------------------
// Drives the ControlPanelView. Owns:
//   - Five direction commands (Forward, Backward, Left, Right, Stop)
//   - Speed property bound to the slider
//   - IsConnected mirror — disables all commands when not connected
//   - LastCommandText — feedback showing what was sent last
//   - LastAckText     — feedback showing the last ACK received from the robot
//
// Architecture rules followed:
//   - Does NOT reference BleCommClient or MqttCommClient
//   - Does NOT serialize JSON itself — delegates to CommandSerializer
//   - All sending goes through RobotCommunicationService.SendAsync()
//   - ACK property update is marshalled to the UI thread (background thread rule)
// ---------------------------------------------------------------------------

public class ControlPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Dependencies
    // -----------------------------------------------------------------------

    private readonly RobotCommunicationService _commService;

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    public ControlPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        // Each direction button gets its own AsyncRelayCommand.
        // All share the same CanExecute guard (IsConnected check).
        ForwardCommand  = new AsyncRelayCommand(() => SendCommandAsync(CommandAction.Forward),  CanSend);
        BackwardCommand = new AsyncRelayCommand(() => SendCommandAsync(CommandAction.Backward), CanSend);
        LeftCommand     = new AsyncRelayCommand(() => SendCommandAsync(CommandAction.Left),     CanSend);
        RightCommand    = new AsyncRelayCommand(() => SendCommandAsync(CommandAction.Right),    CanSend);
        StopCommand     = new AsyncRelayCommand(() => SendCommandAsync(CommandAction.Stop),     CanSend);
    }

    // -----------------------------------------------------------------------
    // Speed
    // -----------------------------------------------------------------------

    private int _speed = 150; // Reasonable mid-range default

    /// <summary>
    /// Motor speed, 0–255.
    /// Bound to the speed slider in ControlPanelView.
    /// Included in every CMD message except STOP (where it is still sent
    /// but the ESP32 may ignore it).
    /// </summary>
    public int Speed
    {
        get => _speed;
        set => SetField(ref _speed, Math.Clamp(value, 0, 255));
    }

    // -----------------------------------------------------------------------
    // Connection awareness
    // -----------------------------------------------------------------------

    private bool _isConnected = false;

    /// <summary>
    /// True when the robot is connected. Controls whether command buttons
    /// are enabled. Set externally by whoever tracks connection state.
    ///
    /// Phase 3 approach: ConnectionPanelViewModel owns the real connection
    /// state. ControlPanelViewModel needs a copy so it can disable buttons.
    /// The simplest student-friendly wiring is to let App.axaml.cs subscribe
    /// to RobotCommunicationService events, or for ConnectionPanelViewModel
    /// to update this via an event. For Phase 3, this property is set directly
    /// from the outside by App.axaml.cs after reading commService.IsConnected.
    ///
    /// Alternatively, ControlPanelViewModel checks commService.IsConnected
    /// inside CanSend() without caching it — see CanSend() below.
    /// That approach is used here to keep it simple.
    /// </summary>
    public bool IsConnected
    {
        get => _isConnected;
        set
        {
            if (SetField(ref _isConnected, value))
                NotifyAllCommandsCanExecuteChanged();
        }
    }

    // -----------------------------------------------------------------------
    // Feedback properties
    // -----------------------------------------------------------------------

    private string _lastCommandText = "No command sent yet";

    /// <summary>
    /// Short description of the last command dispatched to the robot.
    /// Displayed in the control panel for operator feedback.
    /// </summary>
    public string LastCommandText
    {
        get => _lastCommandText;
        private set => SetField(ref _lastCommandText, value);
    }

    private string _lastAckText = "No ACK received yet";

    /// <summary>
    /// The payload string of the last ACK message received from the robot.
    /// Updated by OnAckReceived(), which is called by MessageRouter.
    /// REAL: the property and handler are fully implemented.
    /// The ACK registration in App.axaml.cs is uncommented in this phase.
    /// </summary>
    public string LastAckText
    {
        get => _lastAckText;
        private set => SetField(ref _lastAckText, value);
    }

    // -----------------------------------------------------------------------
    // Commands (one per direction button)
    // -----------------------------------------------------------------------

    public AsyncRelayCommand ForwardCommand  { get; }
    public AsyncRelayCommand BackwardCommand { get; }
    public AsyncRelayCommand LeftCommand     { get; }
    public AsyncRelayCommand RightCommand    { get; }
    public AsyncRelayCommand StopCommand     { get; }

    // -----------------------------------------------------------------------
    // CanExecute guard
    // -----------------------------------------------------------------------

    /// <summary>
    /// Commands are only available when connected.
    /// Reads _isConnected which is kept in sync via the IsConnected property.
    /// </summary>
    private bool CanSend() => _isConnected;

    // -----------------------------------------------------------------------
    // Core send logic
    // -----------------------------------------------------------------------

    /// <summary>
    /// Builds a MotorCommand, serializes it via CommandSerializer, and sends
    /// it through RobotCommunicationService. Updates LastCommandText on success
    /// or shows an error string on failure.
    ///
    /// This method is the target of all five direction commands. The action
    /// enum value is the only thing that differs between them.
    /// </summary>
    private async Task SendCommandAsync(CommandAction action)
    {
        // STOP always uses speed 0 — the value on the slider is irrelevant.
        var speed   = action == CommandAction.Stop ? 0 : Speed;
        var command = new MotorCommand(action, speed);

        // CommandSerializer is the ONLY place that knows the wire format.
        var json = CommandSerializer.Serialize(command);

        try
        {
            await _commService.SendAsync(json);

            // Update feedback on the UI thread (SendAsync may already have
            // returned on the UI thread via async continuation, but we use
            // InvokeAsync for consistency with the rest of the codebase).
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastCommandText = $"Sent: {action.ToString().ToUpperInvariant()} @ {speed}";
            });
        }
        catch (Exception ex)
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastCommandText = $"Error: {ex.Message}";
            });
        }
    }

    // -----------------------------------------------------------------------
    // ACK message handler — called by MessageRouter on a background thread
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called by MessageRouter when an ACK message arrives from the robot.
    ///
    /// Expected format: { "type": "ACK", "payload": "FORWARD" }
    ///
    /// Implementation: lightweight — we parse only what we need (the payload
    /// string) directly here, since ACK payloads are trivially simple and
    /// do not warrant a dedicated parser method.
    ///
    /// The property update is marshalled to the UI thread as required.
    /// </summary>
    public void OnAckReceived(string rawJson)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(rawJson);
            var payload = doc.RootElement
                             .GetProperty("payload")
                             .GetString() ?? "?";

            // Marshal to UI thread — this handler is called from a background thread.
            Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastAckText = $"ACK: {payload}  ({DateTime.Now:HH:mm:ss})";
            });
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ControlPanelViewModel] Failed to parse ACK: {ex.Message}");
        }
    }

    // -----------------------------------------------------------------------
    // Helper — refresh all command buttons at once
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called when IsConnected changes so all five buttons update
    /// their enabled state simultaneously.
    /// </summary>
    private void NotifyAllCommandsCanExecuteChanged()
    {
        ForwardCommand.NotifyCanExecuteChanged();
        BackwardCommand.NotifyCanExecuteChanged();
        LeftCommand.NotifyCanExecuteChanged();
        RightCommand.NotifyCanExecuteChanged();
        StopCommand.NotifyCanExecuteChanged();
    }
}
