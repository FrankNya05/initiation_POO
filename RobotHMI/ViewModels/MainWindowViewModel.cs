using System.ComponentModel;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MainWindowViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// ViewModel racine de la fenêtre principale.
// Agrège tous les ViewModels enfants et les expose comme propriétés.
//
// Changements V2 :
//   - Ajout MotorTestPanel (MODIFIÉ V2) — onglet MOTEURS
//   - Ajout LogPanel       (MODIFIÉ V2) — onglet LOGS
//   - Ajout SelectedTabIndex (MODIFIÉ V2) — navigation par onglet
//   - Suppression de StatusBar.ActiveProtocol (plus utilisé en V2) (MODIFIÉ V2)
//
// Fils enfants :
//   ConnectionPanel → onglet CONNEXION (non affiché, géré dans la barre haute)
//   ControlPanel    → onglet CONTRÔLE (onglet 0, par défaut)
//   TelemetryPanel  → onglet TÉLÉMÉTRIE (onglet 1)
//   MotorTestPanel  → onglet MOTEURS    (onglet 2) — MODIFIÉ V2
//   LogPanel        → onglet LOGS       (onglet 3) — MODIFIÉ V2
//   StatusBar       → barres fixes haut et bas
// ---------------------------------------------------------------------------

public class MainWindowViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // ViewModels enfants — inchangés de V1
    // -----------------------------------------------------------------------

    public ConnectionPanelViewModel ConnectionPanel { get; }
    public TelemetryPanelViewModel   TelemetryPanel  { get; }
    public ControlPanelViewModel     ControlPanel    { get; }
    public StatusBarViewModel        StatusBar        { get; }

    // -----------------------------------------------------------------------
    // ViewModels enfants — NOUVEAUX V2
    // -----------------------------------------------------------------------

    /// <summary>MODIFIÉ V2 — ViewModel du pad directionnel de test moteurs.</summary>
    public MotorTestPanelViewModel MotorTestPanel { get; }  // MODIFIÉ V2

    /// <summary>MODIFIÉ V2 — ViewModel de l'onglet logs.</summary>
    public LogPanelViewModel LogPanel { get; }              // MODIFIÉ V2

    /// <summary>NOUVEAU V2.1 — ViewModel de l'onglet réglages PID.</summary>
    public TuningPanelViewModel TuningPanel { get; }        // MODIFIÉ V2.1

    // -----------------------------------------------------------------------
    // Navigation par onglets — MODIFIÉ V2
    // -----------------------------------------------------------------------

    private int _selectedTabIndex = 0;

    public int SelectedTabIndex
    {
        get => _selectedTabIndex;
        set => SetField(ref _selectedTabIndex, value);
    }

    // -----------------------------------------------------------------------
    // Constructeur
    // -----------------------------------------------------------------------

    public MainWindowViewModel(RobotCommunicationService commService)
    {
        // ── VMs existants ────────────────────────────────────────────────
        ConnectionPanel = new ConnectionPanelViewModel(commService);
        TelemetryPanel  = new TelemetryPanelViewModel();
        ControlPanel    = new ControlPanelViewModel(commService);
        StatusBar       = new StatusBarViewModel();

        // ── Nouveaux VMs V2 ──────────────────────────────────────────────
        MotorTestPanel = new MotorTestPanelViewModel(commService);  // MODIFIÉ V2
        LogPanel       = new LogPanelViewModel();                   // MODIFIÉ V2

        // ── Nouveau V2.1 ─────────────────────────────────────────────────
        TuningPanel = new TuningPanelViewModel(commService);       // MODIFIÉ V2.1

        // ── Forwarding de la connexion ───────────────────────────────────
        // Quand ConnectionPanel.IsConnected change, propager aux VMs qui en ont besoin.
        ConnectionPanel.PropertyChanged += OnConnectionPanelPropertyChanged;

        // Initialiser le StatusBar avec le protocole sélectionné au démarrage
        StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
    }

    // -----------------------------------------------------------------------
    // Forwarding état de connexion — inchangé de V1
    // -----------------------------------------------------------------------

    private void OnConnectionPanelPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        switch (e.PropertyName)
        {
            case nameof(ConnectionPanelViewModel.IsConnected):
                // StatusBar met à jour le badge connecté/déconnecté
                // Note : ControlPanel.IsConnected supprimé en V2 — les boutons
                // START/STOP/RESET n'ont pas de guard CanExecute dans cette version.
                StatusBar.IsConnected = ConnectionPanel.IsConnected;
                break;

            case nameof(ConnectionPanelViewModel.SelectedProtocol):
                StatusBar.ActiveProtocol = ConnectionPanel.SelectedProtocol;
                break;
        }
    }
}
