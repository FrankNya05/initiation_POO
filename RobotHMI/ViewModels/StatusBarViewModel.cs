using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;
using System.Collections.ObjectModel;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// StatusBarViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Pilote les barres fixes haut et bas.
//
// Changements V2 par rapport à la version V1 :
//   - Conservé  : IsConnected, ConnectionStatusText, ActiveProtocol, RecentLogs
//   - Ajouté    : BatteryPercent, BatteryVoltage, BatteryCritical, BatterySummaryText (MODIFIÉ V2)
//   - Ajouté    : RobotStatus, RobotStatusText (MODIFIÉ V2)
//   - Ajouté    : ActiveStrategy (MODIFIÉ V2)
//   - Ajouté    : LastLogText pour la barre inférieure (MODIFIÉ V2)
//   - Ajouté    : OnTelemetryReceived() handler (MODIFIÉ V2)
//   - Mis à jour: RobotStatusText → 4 états V1 (MODIFIÉ V2)
//   - Conservé  : OnStateReceived(), OnLogReceived() — même signature
// ---------------------------------------------------------------------------

public class StatusBarViewModel : ViewModelBase
{
    private const int MaxLogEntries = 5;  // inchangé

    // -----------------------------------------------------------------------
    // Connexion — inchangé V1
    // -----------------------------------------------------------------------

    private bool _isConnected = false;

    public bool IsConnected
    {
        get => _isConnected;
        set
        {
            if (SetField(ref _isConnected, value))
                OnPropertyChanged(nameof(ConnectionStatusText));
        }
    }

    public string ConnectionStatusText => IsConnected ? "● Connecté" : "○ Déconnecté";

    // Conservé de V1 — utilisé par MainWindowViewModel.OnConnectionPanelPropertyChanged
    private CommProtocol _activeProtocol = CommProtocol.MQTT;

    public CommProtocol ActiveProtocol
    {
        get => _activeProtocol;
        set
        {
            if (SetField(ref _activeProtocol, value))
                OnPropertyChanged(nameof(ActiveProtocolText));  // MODIFIÉ V2 — notifier la vue
        }
    }

    /// <summary>Texte du badge protocole ("BLE" ou "MQTT") — requis par StatusBarView.axaml.</summary>
    public string ActiveProtocolText => ActiveProtocol switch  // MODIFIÉ V2 — conservé de V1
    {
        CommProtocol.BLE  => "BLE",
        CommProtocol.MQTT => "MQTT",
        _                 => "—"
    };

    // -----------------------------------------------------------------------
    // Batterie — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private int _batteryPercent;
    public int BatteryPercent
    {
        get => _batteryPercent;
        set
        {
            if (SetField(ref _batteryPercent, value))
                OnPropertyChanged(nameof(BatterySummaryText));
        }
    }

    private float _batteryVoltage;
    public float BatteryVoltage
    {
        get => _batteryVoltage;
        set
        {
            if (SetField(ref _batteryVoltage, value))
                OnPropertyChanged(nameof(BatterySummaryText));
        }
    }

    private bool _batteryCritical;
    public bool BatteryCritical
    {
        get => _batteryCritical;
        set => SetField(ref _batteryCritical, value);
    }

    /// <summary>MODIFIÉ V2 — Ex: "⚡ 78% — 7.35V" affiché dans la barre haute.</summary>
    public string BatterySummaryText =>
        $"⚡ {BatteryPercent}% — {BatteryVoltage:F2}V";

    // -----------------------------------------------------------------------
    // État robot — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private RobotStatus _robotStatus = RobotStatus.Unknown;

    public RobotStatus RobotStatus
    {
        get => _robotStatus;
        set
        {
            if (SetField(ref _robotStatus, value))
                OnPropertyChanged(nameof(RobotStatusText));
        }
    }

    /// <summary>MODIFIÉ V2 — 4 états V1 alignés sur le protocole.</summary>
    public string RobotStatusText => RobotStatus switch
    {
        RobotStatus.Idle    => "IDLE",
        RobotStatus.Search  => "SEARCH",
        RobotStatus.Attack  => "ATTACK",
        RobotStatus.Defense => "DEFENSE",
        _                   => "—"
    };

    // -----------------------------------------------------------------------
    // Stratégie active — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private string _activeStrategy = "—";

    /// <summary>MODIFIÉ V2 — Stratégie active, mise à jour par ControlPanelViewModel.</summary>
    public string ActiveStrategy
    {
        get => _activeStrategy;
        set => SetField(ref _activeStrategy, value);
    }

    // -----------------------------------------------------------------------
    // Dernier LOG pour la barre inférieure — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private string _lastLogText = "Aucun message LOG";

    /// <summary>MODIFIÉ V2 — Affiché dans la barre inférieure.</summary>
    public string LastLogText
    {
        get => _lastLogText;
        private set => SetField(ref _lastLogText, value);
    }

    // -----------------------------------------------------------------------
    // RecentLogs — conservé de V1
    // -----------------------------------------------------------------------

    public ObservableCollection<string> RecentLogs { get; } = new();

    // -----------------------------------------------------------------------
    // Handlers — appelés par MessageRouter sur thread de fond
    // -----------------------------------------------------------------------

    /// <summary>MODIFIÉ V2 — Nouveau handler. Met à jour la batterie.</summary>
    public void OnTelemetryReceived(string rawJson)
    {
        var data = TelemetryParser.ParseTelemetry(rawJson);
        if (data == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            BatteryPercent  = data.BatteryPercent;
            BatteryVoltage  = data.BatteryVoltage;
            BatteryCritical = data.BatteryCritical;
        });
    }

    /// <summary>Handler STATE — conservé de V1, mapping mis à jour V2.</summary>
    public void OnStateReceived(string rawJson)
    {
        var status = TelemetryParser.ParseState(rawJson);

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            RobotStatus = status;
        });
    }

    /// <summary>Handler LOG — MODIFIÉ V2 : met à jour LastLogText + RecentLogs.</summary>
    public void OnLogReceived(string rawJson)
    {
        var message = TelemetryParser.ParseLog(rawJson);  // MODIFIÉ V2 — délègue au parser
        if (string.IsNullOrEmpty(message)) return;

        var entry = $"[{DateTime.Now:HH:mm:ss}] {message}";

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            LastLogText = entry;  // MODIFIÉ V2 — barre inférieure

            RecentLogs.Insert(0, entry);
            while (RecentLogs.Count > MaxLogEntries)
                RecentLogs.RemoveAt(RecentLogs.Count - 1);
        });
    }
}