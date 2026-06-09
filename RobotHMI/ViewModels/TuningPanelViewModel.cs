using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// TuningPanelViewModel — MODIFIÉ V2.3
// ---------------------------------------------------------------------------
// Changements V2.3 :
//   - Ajout des commandes PID YAW (KP, KI, KD) via SerializePidYaw()
//   - Valeurs initiales YAW : KP=0.5, KI=0.0, KD=0.0 (firmware)
// ---------------------------------------------------------------------------

public class TuningPanelViewModel : ViewModelBase
{
    private readonly RobotCommunicationService _commService;

    // ── Valeurs initiales PID moteurs — synchronisées avec main.cpp ─────────
    // PID pidLeft(2.0f, 0.15f, 0.0f) / pidRight(2.0f, 0.15f, 0.0f)
    private string _kpText = "2.0";
    private string _kiText = "0.15";
    private string _kdText = "0.0";

    // ── Valeurs initiales PID YAW — synchronisées avec main.cpp ──────────
    // PID pidYaw(5.0f, 0.5f, 0.0f)
    private string _yawKpText = "5.0";
    private string _yawKiText = "0.5";
    private string _yawKdText = "0.0";

    public TuningPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        // PID moteurs
        SendKpCommand = new AsyncRelayCommand(OnSendKpAsync);
        SendKiCommand = new AsyncRelayCommand(OnSendKiAsync);
        SendKdCommand = new AsyncRelayCommand(OnSendKdAsync);

        // PID YAW — MODIFIÉ V2.3
        SendYawKpCommand = new AsyncRelayCommand(OnSendYawKpAsync);
        SendYawKiCommand = new AsyncRelayCommand(OnSendYawKiAsync);
        SendYawKdCommand = new AsyncRelayCommand(OnSendYawKdAsync);
    }

    // ── Propriétés PID moteurs ────────────────────────────────────────────

    public string KpText { get => _kpText; set => SetField(ref _kpText, value); }
    public string KiText { get => _kiText; set => SetField(ref _kiText, value); }
    public string KdText { get => _kdText; set => SetField(ref _kdText, value); }

    // ── Propriétés PID YAW — MODIFIÉ V2.3 ────────────────────────────────

    public string YawKpText { get => _yawKpText; set => SetField(ref _yawKpText, value); }
    public string YawKiText { get => _yawKiText; set => SetField(ref _yawKiText, value); }
    public string YawKdText { get => _yawKdText; set => SetField(ref _yawKdText, value); }

    // ── Commandes PID moteurs ─────────────────────────────────────────────

    public ICommand SendKpCommand { get; }
    public ICommand SendKiCommand { get; }
    public ICommand SendKdCommand { get; }

    private async Task OnSendKpAsync() => await SendPid("KP", KpText);
    private async Task OnSendKiAsync() => await SendPid("KI", KiText);
    private async Task OnSendKdAsync() => await SendPid("KD", KdText);

    // ── Commandes PID YAW — MODIFIÉ V2.3 ─────────────────────────────────

    public ICommand SendYawKpCommand { get; }
    public ICommand SendYawKiCommand { get; }
    public ICommand SendYawKdCommand { get; }

    private async Task OnSendYawKpAsync() => await SendPidYaw("KP", YawKpText);
    private async Task OnSendYawKiAsync() => await SendPidYaw("KI", YawKiText);
    private async Task OnSendYawKdAsync() => await SendPidYaw("KD", YawKdText);

    // ── Helpers d'envoi ───────────────────────────────────────────────────

    private async Task SendPid(string parameter, string valueText)
    {
        if (!TryParseFloat(valueText, out float value, parameter)) return;
        await SendSafe(CommandSerializer.SerializePid(parameter, value));
    }

    private async Task SendPidYaw(string parameter, string valueText)  // MODIFIÉ V2.3
    {
        if (!TryParseFloat(valueText, out float value, $"YAW:{parameter}")) return;
        await SendSafe(CommandSerializer.SerializePidYaw(parameter, value));
    }

    private async Task SendSafe(string rawCommand)
    {
        try
        {
            await _commService.SendAsync(rawCommand);
            await Dispatcher.UIThread.InvokeAsync(() => StatusText = $"Envoyé : {rawCommand}");
        }
        catch (Exception ex)
        {
            await Dispatcher.UIThread.InvokeAsync(() => StatusText = $"Erreur : {ex.Message}");
        }
    }

    private bool TryParseFloat(string text, out float value, string label)
    {
        if (float.TryParse(text,
                System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture,
                out value))
            return true;

        Dispatcher.UIThread.InvokeAsync(() =>
            StatusText = $"Valeur invalide pour {label} : '{text}'");
        value = 0f;
        return false;
    }

    // ── Feedback ─────────────────────────────────────────────────────────

    private string _statusText = "Aucune commande envoyée";
    public string StatusText
    {
        get => _statusText;
        private set => SetField(ref _statusText, value);
    }
}
