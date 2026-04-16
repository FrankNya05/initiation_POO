using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MotorTestPanelViewModel — MODIFIÉ V2 (correction crash SendAsync)
// ---------------------------------------------------------------------------
// Pilote l'onglet MOTEURS (tests uniquement).
//
// Correction V2 :
//   - Tous les appels SendAsync sont maintenant dans un try/catch (MODIFIÉ V2)
//   - Si le robot n'est pas connecté, LastCommandText affiche l'erreur
//     au lieu de crasher l'application.
// ---------------------------------------------------------------------------

public class MotorTestPanelViewModel : ViewModelBase
{
    private readonly RobotCommunicationService _commService;

    public MotorTestPanelViewModel(RobotCommunicationService commService)
    {
        _commService = commService;

        ForwardCommand  = new AsyncRelayCommand(OnForwardAsync);
        BackwardCommand = new AsyncRelayCommand(OnBackwardAsync);
        LeftCommand     = new AsyncRelayCommand(OnLeftAsync);
        RightCommand    = new AsyncRelayCommand(OnRightAsync);
        StopCommand     = new AsyncRelayCommand(OnStopAsync);
    }

    // -----------------------------------------------------------------------
    // Vitesse
    // -----------------------------------------------------------------------

    private int _speed = 200;

    public int Speed
    {
        get => _speed;
        set
        {
            if (SetField(ref _speed, Math.Clamp(value, 0, 255)))
                OnPropertyChanged(nameof(SpeedText));
        }
    }

    public string SpeedText => _speed.ToString();

    // -----------------------------------------------------------------------
    // Commandes
    // -----------------------------------------------------------------------

    public ICommand ForwardCommand  { get; }
    public ICommand BackwardCommand { get; }
    public ICommand LeftCommand     { get; }
    public ICommand RightCommand    { get; }
    public ICommand StopCommand     { get; }

    private async Task OnForwardAsync()
        => await SendSafe(MotorCommand.Direct(Speed, Speed),
                          $"MOTOR:L:{Speed}:R:{Speed}");

    private async Task OnBackwardAsync()
        => await SendSafe(MotorCommand.Direct(-Speed, -Speed),
                          $"MOTOR:L:{-Speed}:R:{-Speed}");

    private async Task OnLeftAsync()
        => await SendSafe(MotorCommand.Direct(-Speed, Speed),
                          $"MOTOR:L:{-Speed}:R:{Speed}");

    private async Task OnRightAsync()
        => await SendSafe(MotorCommand.Direct(Speed, -Speed),
                          $"MOTOR:L:{Speed}:R:{-Speed}");

    private async Task OnStopAsync()
        => await SendSafe(MotorCommand.Stop(), "MOTOR:STOP");

    // -----------------------------------------------------------------------
    // SendSafe — MODIFIÉ V2
    // -----------------------------------------------------------------------
    // Wrapper commun : sérialise, envoie, et catch toute exception.
    // Si le robot n'est pas connecté, affiche l'erreur sans crasher.

    private async Task SendSafe(MotorCommand command, string displayText)
    {
        try
        {
            var raw = CommandSerializer.Serialize(command);
            await _commService.SendAsync(raw);
            LastCommandText = displayText;
        }
        catch (Exception ex)
        {
            // MODIFIÉ V2 — Afficher l'erreur au lieu de crasher
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                LastCommandText = $"⚠ Erreur : {ex.Message}";
            });
        }
    }

    // -----------------------------------------------------------------------
    // Feedback
    // -----------------------------------------------------------------------

    private string _lastCommandText = "Aucune commande";

    public string LastCommandText
    {
        get => _lastCommandText;
        private set => SetField(ref _lastCommandText, value);
    }
}