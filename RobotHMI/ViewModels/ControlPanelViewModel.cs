using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ControlPanelViewModel — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Pilote l'onglet CONTRÔLE : START / STOP / RESET + sélection stratégie.
//
// Correction V2 :
//   - Tous les appels SendAsync dans un try/catch (MODIFIÉ V2)
//   - Erreur affichée dans LastCommandText sans crasher l'application
// ---------------------------------------------------------------------------

public class ControlPanelViewModel : ViewModelBase
{
    private readonly RobotCommunicationService _commService;

    public ControlPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        StartCommand  = new AsyncRelayCommand(OnStartAsync);
        StopCommand   = new AsyncRelayCommand(OnStopAsync);
        ResetCommand  = new AsyncRelayCommand(OnResetAsync);

        StrategyAdamantineCommand = new AsyncRelayCommand(() => OnStrategyAsync("ADAMANTINE"));
        StrategyBerserkerCommand  = new AsyncRelayCommand(() => OnStrategyAsync("BERSERKER"));
        StrategyCircleCommand     = new AsyncRelayCommand(() => OnStrategyAsync("CIRCLE"));
    }

    // -----------------------------------------------------------------------
    // Commandes match
    // -----------------------------------------------------------------------

    public ICommand StartCommand  { get; }
    public ICommand StopCommand   { get; }
    public ICommand ResetCommand  { get; }

    private async Task OnStartAsync()
        => await SendSafe(CommandSerializer.SerializeRobotCommand("START"), "Envoyé : START");

    private async Task OnStopAsync()
        => await SendSafe(CommandSerializer.SerializeRobotCommand("STOP"), "Envoyé : STOP");

    private async Task OnResetAsync()
        => await SendSafe(CommandSerializer.SerializeRobotCommand("RESET"), "Envoyé : RESET");

    // -----------------------------------------------------------------------
    // Commandes stratégie
    // -----------------------------------------------------------------------

    public ICommand StrategyAdamantineCommand { get; }
    public ICommand StrategyBerserkerCommand  { get; }
    public ICommand StrategyCircleCommand     { get; }

    private string _activeStrategy = string.Empty;

    public string ActiveStrategy
    {
        get => _activeStrategy;
        private set
        {
            if (SetField(ref _activeStrategy, value))
            {
                OnPropertyChanged(nameof(IsAdamantineActive));
                OnPropertyChanged(nameof(IsBerserkerActive));
                OnPropertyChanged(nameof(IsCircleActive));
            }
        }
    }

    public bool IsAdamantineActive => ActiveStrategy == "ADAMANTINE";
    public bool IsBerserkerActive  => ActiveStrategy == "BERSERKER";
    public bool IsCircleActive     => ActiveStrategy == "CIRCLE";

    private async Task OnStrategyAsync(string strategyName)
        => await SendSafe(
               CommandSerializer.SerializeStrategy(strategyName),
               $"Envoyé : STRATEGY:{strategyName}",
               () => ActiveStrategy = strategyName);

    // -----------------------------------------------------------------------
    // SendSafe — MODIFIÉ V2
    // -----------------------------------------------------------------------
    // Wrapper commun avec try/catch.
    // onSuccess : action optionnelle exécutée sur le UI thread si l'envoi réussit.

    private async Task SendSafe(string rawCommand, string successText,
                                Action? onSuccess = null)
    {
        try
        {
            await _commService.SendAsync(rawCommand);

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastCommandText = successText;
                onSuccess?.Invoke();
            });
        }
        catch (Exception ex)
        {
            // MODIFIÉ V2 — afficher l'erreur sans crasher
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastCommandText = $"⚠ Non connecté — {ex.Message}";
            });
        }
    }

    // -----------------------------------------------------------------------
    // Handler ACK
    // -----------------------------------------------------------------------

    public void OnAckReceived(string rawJson)
    {
        var ackPayload = TelemetryParser.ParseAck(rawJson);
        if (ackPayload == null) return;

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            LastCommandText = $"ACK reçu : {ackPayload}";
        });
    }

    // -----------------------------------------------------------------------
    // Feedback
    // -----------------------------------------------------------------------

    private string _lastCommandText = "Aucune commande envoyée";

    public string LastCommandText
    {
        get => _lastCommandText;
        private set => SetField(ref _lastCommandText, value);
    }
}