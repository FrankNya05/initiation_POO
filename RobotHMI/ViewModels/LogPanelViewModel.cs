using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// LogPanelViewModel — NOUVEAU V2
// ---------------------------------------------------------------------------
// Pilote l'onglet LOGS.
//
// Responsabilités :
//   - Maintient une liste des 100 derniers messages LOG
//   - Chaque entrée est horodatée [HH:mm:ss]
//   - Expose un ICommand "Effacer" pour vider la liste
//   - Reçoit les LOG via le MessageRouter (handler OnLogReceived)
// ---------------------------------------------------------------------------

public class LogPanelViewModel : ViewModelBase
{
    // Limite du nombre de messages conservés en mémoire
    private const int MaxLogEntries = 100;

    // -----------------------------------------------------------------------
    // Liste observable — mise à jour automatiquement dans la Vue
    // -----------------------------------------------------------------------

    /// <summary>
    /// Collection des messages LOG horodatés.
    /// ObservableCollection notifie automatiquement la Vue quand on ajoute/supprime.
    /// </summary>
    public ObservableCollection<string> LogEntries { get; } = new();

    // -----------------------------------------------------------------------
    // Commande Effacer
    // -----------------------------------------------------------------------

    public ICommand ClearCommand { get; }

    // -----------------------------------------------------------------------
    // Constructeur
    // -----------------------------------------------------------------------

    public LogPanelViewModel()
    {
        ClearCommand = new RelayCommand(OnClear);
    }

    // -----------------------------------------------------------------------
    // Handler — appelé par MessageRouter sur thread de fond
    // -----------------------------------------------------------------------

    /// <summary>
    /// Appelé par MessageRouter quand un message LOG arrive.
    /// Ajoute l'entrée horodatée en haut de la liste (plus récent en premier).
    /// </summary>
    public void OnLogReceived(string rawJson)
    {
        var message = TelemetryParser.ParseLog(rawJson);
        if (string.IsNullOrEmpty(message)) return;

        var timestamp = DateTime.Now.ToString("HH:mm:ss");
        var entry     = $"[{timestamp}] {message}";

        // Toujours marshaller vers le thread UI avant de modifier la collection
        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // Insérer en début de liste pour avoir le plus récent en haut
            LogEntries.Insert(0, entry);

            // Garder seulement les 100 derniers messages
            while (LogEntries.Count > MaxLogEntries)
                LogEntries.RemoveAt(LogEntries.Count - 1);

            // Notifier le compteur affiché
            OnPropertyChanged(nameof(EntryCountText));
        });
    }

    // -----------------------------------------------------------------------
    // Effacer la liste
    // -----------------------------------------------------------------------

    private void OnClear()
    {
        LogEntries.Clear();
        OnPropertyChanged(nameof(EntryCountText));
    }

    // -----------------------------------------------------------------------
    // Compteur affiché
    // -----------------------------------------------------------------------

    /// <summary>Texte ex: "42 / 100 messages" affiché en bas de l'onglet.</summary>
    public string EntryCountText => $"{LogEntries.Count} / {MaxLogEntries} messages";
}
