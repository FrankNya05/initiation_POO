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
        StrategyTrackCommand      = new AsyncRelayCommand(() => OnStrategyAsync("Track"));  // MODIFIÉ V2.3

        SendLedCommand = new AsyncRelayCommand(OnSendLedAsync);  // LED RGB
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
    public ICommand StrategyTrackCommand      { get; }  // MODIFIÉ V2.3

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
                OnPropertyChanged(nameof(IsTrackActive));  // MODIFIÉ V2.3
            }
        }
    }

    public bool IsAdamantineActive => ActiveStrategy == "ADAMANTINE";
    public bool IsBerserkerActive  => ActiveStrategy == "BERSERKER";
    public bool IsCircleActive     => ActiveStrategy == "CIRCLE";
    public bool IsTrackActive      => ActiveStrategy == "Track";   // MODIFIÉ V2.3

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

    // -----------------------------------------------------------------------
    // LED RGB
    // -----------------------------------------------------------------------

    public ICommand SendLedCommand { get; }

    private int _ledR = 0;
    private int _ledG = 0;
    private int _ledB = 0;

    public int LedR
    {
        get => _ledR;
        set { if (SetField(ref _ledR, (int)Math.Clamp(value, 0, 255))) UpdateLedPreview(); }
    }

    public int LedG
    {
        get => _ledG;
        set { if (SetField(ref _ledG, (int)Math.Clamp(value, 0, 255))) UpdateLedPreview(); }
    }

    public int LedB
    {
        get => _ledB;
        set { if (SetField(ref _ledB, (int)Math.Clamp(value, 0, 255))) UpdateLedPreview(); }
    }

    /// <summary>Couleur hex pour la prévisualisation — ex: "#FF0000".</summary>
    private string _ledColorPreview = "#000000";
    public string LedColorPreview
    {
        get => _ledColorPreview;
        private set => SetField(ref _ledColorPreview, value);
    }

    /// <summary>Texte affiché sous les sliders — ex: "R:255 G:0 B:0".</summary>
    public string LedColorText => $"R:{_ledR}  G:{_ledG}  B:{_ledB}";

    private void UpdateLedPreview()
    {
        LedColorPreview = $"#{_ledR:X2}{_ledG:X2}{_ledB:X2}";
        OnPropertyChanged(nameof(LedColorText));
    }

    private async Task OnSendLedAsync()
    {
        // CommandSerializer.SerializeRobotCommand ne convient pas ici —
        // on construit la commande LED directement selon le format du firmware :
        // LED:R:<v>:G:<v>:B:<v>
        var cmd = $"LED:R:{_ledR}:G:{_ledG}:B:{_ledB}";
        await SendSafe(cmd, $"Envoyé : {cmd}");
    }
}
