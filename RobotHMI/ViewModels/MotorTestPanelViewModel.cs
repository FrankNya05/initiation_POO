using System.Windows.Input;
using RobotHMI.Models;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// MotorTestPanelViewModel — NOUVEAU V2
// ---------------------------------------------------------------------------
// Pilote l'onglet MOTEURS (tests uniquement).
//
// Commandes disponibles :
//   ↑  Forward  → "MOTOR:L:v:R:v"   (les deux positifs)
//   ↓  Backward → "MOTOR:L:-v:R:-v" (les deux négatifs)
//   ←  Left     → "MOTOR:L:-v:R:v"  (pivot gauche)
//   →  Right    → "MOTOR:L:v:R:-v"  (pivot droit)
//   ⏹  Stop     → "MOTOR:STOP"
//
// Le slider contrôle la vitesse (0-255) appliquée à toutes les commandes.
// ---------------------------------------------------------------------------

public class MotorTestPanelViewModel : ViewModelBase
{
    // -----------------------------------------------------------------------
    // Dépendances
    // -----------------------------------------------------------------------

    private readonly RobotCommunicationService _commService;

    // -----------------------------------------------------------------------
    // Constructeur
    // -----------------------------------------------------------------------

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
    // Vitesse — contrôlée par le slider
    // -----------------------------------------------------------------------

    private int _speed = 200;

    /// <summary>
    /// Vitesse utilisée pour les commandes directionnelles.
    /// Plage 0-255. Modifié par le slider dans la Vue.
    /// </summary>
    public int Speed
    {
        get => _speed;
        set
        {
            // Clamping défensif même si le slider impose déjà la plage
            if (SetField(ref _speed, Math.Clamp(value, 0, 255)))
                OnPropertyChanged(nameof(SpeedText));
        }
    }

    /// <summary>Affichage formaté du slider ex: "200".</summary>
    public string SpeedText => _speed.ToString();

    // -----------------------------------------------------------------------
    // Commandes du pad directionnel
    // -----------------------------------------------------------------------

    public ICommand ForwardCommand  { get; }
    public ICommand BackwardCommand { get; }
    public ICommand LeftCommand     { get; }
    public ICommand RightCommand    { get; }
    public ICommand StopCommand     { get; }

    // Avance : les deux moteurs à vitesse positive
    private async Task OnForwardAsync()
    {
        var cmd = MotorCommand.Direct(Speed, Speed);
        await _commService.SendAsync(CommandSerializer.Serialize(cmd));
        LastCommandText = $"MOTOR:L:{Speed}:R:{Speed}";
    }

    // Recule : les deux moteurs à vitesse négative
    private async Task OnBackwardAsync()
    {
        var cmd = MotorCommand.Direct(-Speed, -Speed);
        await _commService.SendAsync(CommandSerializer.Serialize(cmd));
        LastCommandText = $"MOTOR:L:{-Speed}:R:{-Speed}";
    }

    // Tourne gauche : moteur gauche arrière, moteur droit avant
    private async Task OnLeftAsync()
    {
        var cmd = MotorCommand.Direct(-Speed, Speed);
        await _commService.SendAsync(CommandSerializer.Serialize(cmd));
        LastCommandText = $"MOTOR:L:{-Speed}:R:{Speed}";
    }

    // Tourne droite : moteur gauche avant, moteur droit arrière
    private async Task OnRightAsync()
    {
        var cmd = MotorCommand.Direct(Speed, -Speed);
        await _commService.SendAsync(CommandSerializer.Serialize(cmd));
        LastCommandText = $"MOTOR:L:{Speed}:R:{-Speed}";
    }

    // Arrêt moteurs
    private async Task OnStopAsync()
    {
        var cmd = MotorCommand.Stop();
        await _commService.SendAsync(CommandSerializer.Serialize(cmd));
        LastCommandText = "MOTOR:STOP";
    }

    // -----------------------------------------------------------------------
    // Dernière commande envoyée — affichage informatif
    // -----------------------------------------------------------------------

    private string _lastCommandText = "Aucune commande";

    public string LastCommandText
    {
        get => _lastCommandText;
        private set => SetField(ref _lastCommandText, value);
    }
}
