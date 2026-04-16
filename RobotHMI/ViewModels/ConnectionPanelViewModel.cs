using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ConnectionPanelViewModel
// ---------------------------------------------------------------------------
// Drives the ConnectionPanel UI. Owns:
//   - Protocol selector (BLE / MQTT)
//   - Protocol-dependent input fields (device name, broker IP, etc.)
//   - Connect and Disconnect commands
//   - A status string displayed to the user
//
// Rules followed (architecture §2.3):
//   - Does NOT reference BleCommClient or MqttCommClient directly
//   - All communication goes through RobotCommunicationService
//   - No Avalonia UI types here — only plain C# and the service
// ---------------------------------------------------------------------------

public class ConnectionPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Dependencies
    // -----------------------------------------------------------------------

    private readonly RobotCommunicationService _commService;

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    public ConnectionPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        // Wire commands to their implementations.
        ConnectCommand    = new AsyncRelayCommand(ConnectAsync,    CanConnect);
        DisconnectCommand = new AsyncRelayCommand(DisconnectAsync, CanDisconnect);
    }

    // -----------------------------------------------------------------------
    // Protocol selection
    // -----------------------------------------------------------------------

    // Available protocols for the dropdown / selector in the View.
    public IReadOnlyList<CommProtocol> AvailableProtocols { get; } =
        new[] { CommProtocol.BLE, CommProtocol.MQTT };

    private CommProtocol _selectedProtocol = CommProtocol.BLE;

    /// <summary>
    /// The protocol currently selected by the user.
    /// Changing this switches which parameter fields are visible in the View.
    /// </summary>
    public CommProtocol SelectedProtocol
    {
        get => _selectedProtocol;
        set
        {
            if (SetField(ref _selectedProtocol, value))
            {
                // Notify the View that the visibility of BLE / MQTT fields changed.
                OnPropertyChanged(nameof(IsBleSelected));
                OnPropertyChanged(nameof(IsMqttSelected));

                // Re-evaluate command availability.
                ConnectCommand.NotifyCanExecuteChanged();
            }
        }
    }

    /// <summary>True when BLE is selected — used for conditional field visibility.</summary>
    public bool IsBleSelected  => SelectedProtocol == CommProtocol.BLE;

    /// <summary>True when MQTT is selected — used for conditional field visibility.</summary>
    public bool IsMqttSelected => SelectedProtocol == CommProtocol.MQTT;

    // -----------------------------------------------------------------------
    // BLE parameters
    // -----------------------------------------------------------------------

    private string _bleDeviceName = string.Empty;

    /// <summary>BLE device name advertised by the ESP32 (e.g., "MiniSumoRobot").</summary>
    public string BleDeviceName
    {
        get => _bleDeviceName;
        set
        {
            SetField(ref _bleDeviceName, value);
            ConnectCommand.NotifyCanExecuteChanged();
        }
    }

    private string _bleServiceUuid = string.Empty;

    /// <summary>BLE GATT service UUID (must match ESP32 configuration).</summary>
    public string BleServiceUuid
    {
        get => _bleServiceUuid;
        set => SetField(ref _bleServiceUuid, value);
    }

    // -----------------------------------------------------------------------
    // MQTT parameters
    // -----------------------------------------------------------------------

    private string _mqttBrokerIp = "127.0.0.1";

    /// <summary>IP address of the MQTT broker (e.g., "192.168.1.100").</summary>
    public string MqttBrokerIp
    {
        get => _mqttBrokerIp;
        set
        {
            SetField(ref _mqttBrokerIp, value);
            ConnectCommand.NotifyCanExecuteChanged();
        }
    }

    private string _mqttPort = "1883";

    /// <summary>MQTT broker port as a string (validated on connect).</summary>
    public string MqttPort
    {
        get => _mqttPort;
        set => SetField(ref _mqttPort, value);
    }

    private string _mqttTelemetryTopic = "robot/telemetry";

    /// <summary>Topic the HMI subscribes to for incoming robot data.</summary>
    public string MqttTelemetryTopic
    {
        get => _mqttTelemetryTopic;
        set => SetField(ref _mqttTelemetryTopic, value);
    }

    private string _mqttCommandTopic = "robot/cmd";

    /// <summary>Topic the HMI publishes motor commands to.</summary>
    public string MqttCommandTopic
    {
        get => _mqttCommandTopic;
        set => SetField(ref _mqttCommandTopic, value);
    }

    // -----------------------------------------------------------------------
    // Connection state & status text
    // -----------------------------------------------------------------------

    private bool _isConnected = false;

    /// <summary>
    /// Reflects the live connection state.
    /// Drives button enable/disable and status color in the View.
    /// </summary>
    public bool IsConnected
    {
        get => _isConnected;
        private set
        {
            if (SetField(ref _isConnected, value))
            {
                ConnectCommand.NotifyCanExecuteChanged();
                DisconnectCommand.NotifyCanExecuteChanged();
            }
        }
    }

    private string _statusText = "Disconnected";

    /// <summary>Human-readable connection status shown to the user.</summary>
    public string StatusText
    {
        get => _statusText;
        private set => SetField(ref _statusText, value);
    }

    // -----------------------------------------------------------------------
    // Commands
    // -----------------------------------------------------------------------

    public AsyncRelayCommand ConnectCommand    { get; }
    public AsyncRelayCommand DisconnectCommand { get; }

    // -----------------------------------------------------------------------
    // CanExecute guards
    // -----------------------------------------------------------------------

    private bool CanConnect()
    {
        if (IsConnected) return false;

        return SelectedProtocol switch
        {
            // Require at least a device name for BLE.
            CommProtocol.BLE  => !string.IsNullOrWhiteSpace(BleDeviceName),
            // Require at least a broker IP for MQTT.
            CommProtocol.MQTT => !string.IsNullOrWhiteSpace(MqttBrokerIp),
            _ => false
        };
    }

    private bool CanDisconnect() => IsConnected;

    // -----------------------------------------------------------------------
    // ConnectAsync
    // -----------------------------------------------------------------------

    private async Task ConnectAsync()
    {
        StatusText = "Connecting...";

        try
        {
            ConnectionConfig config = BuildConfig();
            await _commService.ConnectAsync(SelectedProtocol, config);

            IsConnected = true;
            StatusText  = $"Connected via {SelectedProtocol}";
        }
        catch (Exception ex)
        {
            IsConnected = false;
            StatusText  = $"Connection failed: {ex.Message}";
        }
    }

    // -----------------------------------------------------------------------
    // DisconnectAsync
    // -----------------------------------------------------------------------

    private async Task DisconnectAsync()
    {
        StatusText = "Disconnecting...";

        try
        {
            await _commService.DisconnectAsync();
        }
        catch (Exception ex)
        {
            // Log but do not block — we still consider ourselves disconnected.
            Console.WriteLine($"[ConnectionPanelViewModel] Disconnect error: {ex.Message}");
        }
        finally
        {
            IsConnected = false;
            StatusText  = "Disconnected";
        }
    }

    // -----------------------------------------------------------------------
    // BuildConfig — assemble the right ConnectionConfig from the current fields
    // -----------------------------------------------------------------------

    private ConnectionConfig BuildConfig()
    {
        return SelectedProtocol switch
        {
            CommProtocol.BLE => new BleConnectionConfig
            {
                DeviceName  = BleDeviceName.Trim(),
                ServiceUuid = BleServiceUuid.Trim()
            },

            CommProtocol.MQTT => new MqttConnectionConfig
            {
                BrokerIp        = MqttBrokerIp.Trim(),
                Port            = int.TryParse(MqttPort, out int p) ? p : 1883,
                TelemetryTopic  = MqttTelemetryTopic.Trim(),
                CommandTopic    = MqttCommandTopic.Trim()
            },

            _ => throw new InvalidOperationException(
                     $"Unknown protocol: {SelectedProtocol}")
        };
    }
}