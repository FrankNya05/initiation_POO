using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ConnectionPanelViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Pilote le panneau de connexion (onglet CONNEXION).
//
// Correction V2 :
//   - Valeurs par défaut des topics corrigées (MODIFIÉ V2) :
//       MqttTelemetryTopic : "robot/data"     (était "robot/telemetry")
//       MqttCommandTopic   : "robot/commande" (était "robot/cmd")
//   - SelectedProtocol par défaut : MQTT (MODIFIÉ V2) — le projet utilise MQTT
// ---------------------------------------------------------------------------

public class ConnectionPanelViewModel : ViewModelBase
{
    private readonly RobotCommunicationService _commService;

    public ConnectionPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        ConnectCommand    = new AsyncRelayCommand(ConnectAsync,    CanConnect);
        DisconnectCommand = new AsyncRelayCommand(DisconnectAsync, CanDisconnect);
    }

    // -----------------------------------------------------------------------
    // Sélection du protocole
    // -----------------------------------------------------------------------

    public IReadOnlyList<CommProtocol> AvailableProtocols { get; } =
        new[] { CommProtocol.BLE, CommProtocol.MQTT };

    // MODIFIÉ V2 — défaut MQTT (protocole utilisé par le projet)
    private CommProtocol _selectedProtocol = CommProtocol.MQTT;

    public CommProtocol SelectedProtocol
    {
        get => _selectedProtocol;
        set
        {
            if (SetField(ref _selectedProtocol, value))
            {
                OnPropertyChanged(nameof(IsBleSelected));
                OnPropertyChanged(nameof(IsMqttSelected));
                ConnectCommand.NotifyCanExecuteChanged();
            }
        }
    }

    public bool IsBleSelected  => SelectedProtocol == CommProtocol.BLE;
    public bool IsMqttSelected => SelectedProtocol == CommProtocol.MQTT;

    // -----------------------------------------------------------------------
    // Paramètres BLE
    // -----------------------------------------------------------------------

    private string _bleDeviceName = string.Empty;
    public string BleDeviceName
    {
        get => _bleDeviceName;
        set { SetField(ref _bleDeviceName, value); ConnectCommand.NotifyCanExecuteChanged(); }
    }

    private string _bleServiceUuid = string.Empty;
    public string BleServiceUuid
    {
        get => _bleServiceUuid;
        set => SetField(ref _bleServiceUuid, value);
    }

    // -----------------------------------------------------------------------
    // Paramètres MQTT
    // -----------------------------------------------------------------------

    private string _mqttBrokerIp = "127.0.0.1";
    public string MqttBrokerIp
    {
        get => _mqttBrokerIp;
        set { SetField(ref _mqttBrokerIp, value); ConnectCommand.NotifyCanExecuteChanged(); }
    }

    private string _mqttPort = "1883";
    public string MqttPort
    {
        get => _mqttPort;
        set => SetField(ref _mqttPort, value);
    }

    // Aligné sur la nomenclature ESP32 réelle (MODIFIÉ V2)
    private string _mqttTelemetryTopic = "robot/telemetry";
    public string MqttTelemetryTopic
    {
        get => _mqttTelemetryTopic;
        set => SetField(ref _mqttTelemetryTopic, value);
    }

    // Aligné sur la nomenclature ESP32 réelle (MODIFIÉ V2)
    private string _mqttCommandTopic = "robot/cmd";
    public string MqttCommandTopic
    {
        get => _mqttCommandTopic;
        set => SetField(ref _mqttCommandTopic, value);
    }

    // -----------------------------------------------------------------------
    // État de connexion
    // -----------------------------------------------------------------------

    private bool _isConnected = false;
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

    private string _statusText = "Déconnecté";
    public string StatusText
    {
        get => _statusText;
        private set => SetField(ref _statusText, value);
    }

    // -----------------------------------------------------------------------
    // Commandes
    // -----------------------------------------------------------------------

    public AsyncRelayCommand ConnectCommand    { get; }
    public AsyncRelayCommand DisconnectCommand { get; }

    private bool CanConnect()
    {
        if (IsConnected) return false;
        return SelectedProtocol switch
        {
            CommProtocol.BLE  => !string.IsNullOrWhiteSpace(BleDeviceName),
            CommProtocol.MQTT => !string.IsNullOrWhiteSpace(MqttBrokerIp),
            _ => false
        };
    }

    private bool CanDisconnect() => IsConnected;

    private async Task ConnectAsync()
    {
        StatusText = "Connexion en cours...";
        try
        {
            await _commService.ConnectAsync(SelectedProtocol, BuildConfig());
            IsConnected = true;
            StatusText  = $"Connecté via {SelectedProtocol}";
        }
        catch (Exception ex)
        {
            IsConnected = false;
            StatusText  = $"Échec : {ex.Message}";
        }
    }

    private async Task DisconnectAsync()
    {
        StatusText = "Déconnexion...";
        try
        {
            await _commService.DisconnectAsync();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ConnectionPanel] Disconnect error: {ex.Message}");
        }
        finally
        {
            IsConnected = false;
            StatusText  = "Déconnecté";
        }
    }

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
                BrokerIp       = MqttBrokerIp.Trim(),
                Port           = int.TryParse(MqttPort, out int p) ? p : 1883,
                TelemetryTopic = MqttTelemetryTopic.Trim(),
                CommandTopic   = MqttCommandTopic.Trim()
            },
            _ => throw new InvalidOperationException($"Protocole inconnu : {SelectedProtocol}")
        };
    }
}