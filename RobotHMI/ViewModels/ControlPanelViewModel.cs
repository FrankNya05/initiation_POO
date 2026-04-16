using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ControlPanelViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Pilote l'onglet CONTRÔLE :
//   - Boutons START / STOP / RESET
//   - Sélection de stratégie (ADAMANTINE / BERSERKER / CIRCLE)
//   - Affichage de l'ACK reçu du robot
//
// Règles architecturales :
//   - N'écrit jamais de string de commande directement — passe par CommandSerializer
//   - Reçoit les ACK via le MessageRouter (handler OnAckReceived)
// ---------------------------------------------------------------------------

public class ControlPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Dépendances
    // -----------------------------------------------------------------------

    private readonly RobotCommunicationService _commService;

    // -----------------------------------------------------------------------
    // Constructeur
    // -----------------------------------------------------------------------

    public ControlPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        // Création des commandes ICommand → chaque bouton de la Vue se lie à une commande
        StartCommand  = new AsyncRelayCommand(OnStartAsync);
        StopCommand   = new AsyncRelayCommand(OnStopAsync);
        ResetCommand  = new AsyncRelayCommand(OnResetAsync);

        StrategyAdamantineCommand = new AsyncRelayCommand(() => OnStrategyAsync("ADAMANTINE"));
        StrategyBerserkerCommand  = new AsyncRelayCommand(() => OnStrategyAsync("BERSERKER"));
        StrategyCircleCommand     = new AsyncRelayCommand(() => OnStrategyAsync("CIRCLE"));
    }

    // -----------------------------------------------------------------------
    // Commandes — boutons du match
    // -----------------------------------------------------------------------

    public ICommand StartCommand  { get; }
    public ICommand StopCommand   { get; }
    public ICommand ResetCommand  { get; }

    private async Task OnStartAsync()
    {
        // CommandSerializer produit la string brute "START"
        await _commService.SendAsync(CommandSerializer.SerializeRobotCommand("START"));
        LastCommandText = "Envoyé : START";
    }

    private async Task OnStopAsync()
    {
        await _commService.SendAsync(CommandSerializer.SerializeRobotCommand("STOP"));
        LastCommandText = "Envoyé : STOP";
    }

    private async Task OnResetAsync()
    {
        await _commService.SendAsync(CommandSerializer.SerializeRobotCommand("RESET"));
        LastCommandText = "Envoyé : RESET";
    }

    // -----------------------------------------------------------------------
    // Commandes — sélection stratégie
    // -----------------------------------------------------------------------

    public ICommand StrategyAdamantineCommand { get; }
    public ICommand StrategyBerserkerCommand  { get; }
    public ICommand StrategyCircleCommand     { get; }

    private string _activeStrategy = string.Empty;

    /// <summary>
    /// Nom de la stratégie active ("ADAMANTINE", "BERSERKER", "CIRCLE" ou vide).
    /// Le View utilise cette valeur pour mettre en surbrillance le bon bouton.
    /// </summary>
    public string ActiveStrategy
    {
        get => _activeStrategy;
        private set
        {
            if (SetField(ref _activeStrategy, value))
            {
                // Notifier la Vue que l'apparence des 3 boutons doit se mettre à jour
                OnPropertyChanged(nameof(IsAdamantineActive));
                OnPropertyChanged(nameof(IsBerserkerActive));
                OnPropertyChanged(nameof(IsCircleActive));
            }
        }
    }

    // Propriétés booléennes pour appliquer le style "actif" dans la Vue
    public bool IsAdamantineActive => ActiveStrategy == "ADAMANTINE";
    public bool IsBerserkerActive  => ActiveStrategy == "BERSERKER";
    public bool IsCircleActive     => ActiveStrategy == "CIRCLE";

    private async Task OnStrategyAsync(string strategyName)
    {
        // CommandSerializer produit "STRATEGY:BERSERKER" etc.
        await _commService.SendAsync(CommandSerializer.SerializeStrategy(strategyName));
        ActiveStrategy  = strategyName;
        LastCommandText = $"Envoyé : STRATEGY:{strategyName}";
    }

    // -----------------------------------------------------------------------
    // Affichage du dernier retour
    // -----------------------------------------------------------------------

    private string _lastCommandText = "Aucune commande envoyée";

    /// <summary>Texte de la dernière commande envoyée ou ACK reçu.</summary>
    public string LastCommandText
    {
        get => _lastCommandText;
        private set => SetField(ref _lastCommandText, value);
    }

    // -----------------------------------------------------------------------
    // Handler — ACK reçu du robot (appelé par MessageRouter)
    // -----------------------------------------------------------------------

    /// <summary>
    /// Appelé par MessageRouter sur un thread de fond quand un ACK arrive.
    /// Met à jour l'interface sur le thread UI.
    /// </summary>
    public void OnAckReceived(string rawJson)
    {
        var ackPayload = TelemetryParser.ParseAck(rawJson);
        if (ackPayload == null) return;

        // Toujours marshaller vers le thread UI avant de modifier des propriétés bindées
        Dispatcher.UIThread.InvokeAsync(() =>
        {
            LastCommandText = $"ACK reçu : {ackPayload}";
        });
    }
}
