using System.ComponentModel;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MainWindowViewModel — MODIFIÉ V2.5
// ---------------------------------------------------------------------------
// ViewModel racine de la fenêtre principale.
//
// Changements V2.5 :
//   - DashboardPanel remplace TelemetryPanel + MotorTestPanel + DigitalTwinPanel
//   - Passage de 7 onglets à 5 : CONTRÔLE · TABLEAU · LOGS · RÉGLAGES · CONNEXION
// ---------------------------------------------------------------------------

public class MainWindowViewModel : ViewModelBase
{
    // ── ViewModels enfants ────────────────────────────────────────────────

    public ConnectionPanelViewModel ConnectionPanel { get; }
    public ControlPanelViewModel     ControlPanel    { get; }
    public StatusBarViewModel        StatusBar        { get; }
    public LogPanelViewModel         LogPanel         { get; }
    public TuningPanelViewModel      TuningPanel      { get; }

    /// <summary>NOUVEAU V2.5 — Tableau de bord fusionné (dohyo + télémétrie + moteurs).</summary>
    public DashboardPanelViewModel DashboardPanel { get; }

    // ── Navigation ────────────────────────────────────────────────────────

    private int _selectedTabIndex = 0;

    public int SelectedTabIndex
    {
        get => _selectedTabIndex;
        set => SetField(ref _selectedTabIndex, value);
    }

    // ── Constructeur ──────────────────────────────────────────────────────

    public MainWindowViewModel(RobotCommunicationService commService)
    {
        ConnectionPanel = new ConnectionPanelViewModel(commService);
        ControlPanel    = new ControlPanelViewModel(commService);
        StatusBar       = new StatusBarViewModel();
        LogPanel        = new LogPanelViewModel();
        TuningPanel     = new TuningPanelViewModel(commService);
        DashboardPanel  = new DashboardPanelViewModel(commService);

        ConnectionPanel.PropertyChanged += OnConnectionPanelPropertyChanged;
        StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
    }

    // ── Forwarding état de connexion ──────────────────────────────────────

    private void OnConnectionPanelPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        switch (e.PropertyName)
        {
            case nameof(ConnectionPanelViewModel.IsConnected):
                StatusBar.IsConnected = ConnectionPanel.IsConnected;
                break;

            case nameof(ConnectionPanelViewModel.SelectedProtocol):
                StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
                break;
        }
    }
}
