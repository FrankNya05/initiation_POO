using System.Collections.ObjectModel;
using System.Windows.Input;
using Avalonia.Threading;
using RobotHMI.Services;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// LogPanelViewModel — MODIFIÉ V2.4
// ---------------------------------------------------------------------------
// Ajouts V2.4 :
//   - FilterText : filtre les entrées affichées par mot-clé (insensible à la casse)
//   - FilteredEntries : collection affichée (sous-ensemble de LogEntries)
//   - ExportCommand : sauvegarde tous les logs sur le Bureau en .txt horodaté
//   - StatusText : retour d'action (export réussi, erreur, etc.)
// ---------------------------------------------------------------------------

public class LogPanelViewModel : ViewModelBase
{
    private const int MaxLogEntries = 100;

    // ── Collections ───────────────────────────────────────────────────────

    /// <summary>Source de vérité — tous les messages reçus.</summary>
    public ObservableCollection<string> LogEntries     { get; } = new();

    /// <summary>Vue filtrée — ce que la ListBox affiche.</summary>
    public ObservableCollection<string> FilteredEntries { get; } = new();

    // ── Filtre texte — MODIFIÉ V2.4 ──────────────────────────────────────

    private string _filterText = string.Empty;

    public string FilterText
    {
        get => _filterText;
        set
        {
            if (SetField(ref _filterText, value))
                RefreshFilter();
        }
    }

    // ── Commandes ─────────────────────────────────────────────────────────

    public ICommand ClearCommand  { get; }
    public ICommand ExportCommand { get; }   // MODIFIÉ V2.4

    // ── Feedback ──────────────────────────────────────────────────────────

    private string _statusText = "Aucun log reçu";
    public string StatusText
    {
        get => _statusText;
        private set => SetField(ref _statusText, value);
    }

    // ── Constructeur ──────────────────────────────────────────────────────

    public LogPanelViewModel()
    {
        ClearCommand  = new RelayCommand(OnClear);
        ExportCommand = new RelayCommand(OnExport);  // MODIFIÉ V2.4
    }

    // ── Handler MQTT ──────────────────────────────────────────────────────

    public void OnLogReceived(string rawJson)
    {
        var message = TelemetryParser.ParseLog(rawJson);
        if (string.IsNullOrEmpty(message)) return;

        var entry = $"[{DateTime.Now:HH:mm:ss}] {message}";

        Dispatcher.UIThread.InvokeAsync(() =>
        {
            // Ajouter en tête (plus récent en premier)
            LogEntries.Insert(0, entry);
            while (LogEntries.Count > MaxLogEntries)
                LogEntries.RemoveAt(LogEntries.Count - 1);

            // Ajouter à FilteredEntries seulement si ça passe le filtre
            if (MatchesFilter(entry))
            {
                FilteredEntries.Insert(0, entry);
                // Synchroniser la limite sur FilteredEntries aussi
                while (FilteredEntries.Count > MaxLogEntries)
                    FilteredEntries.RemoveAt(FilteredEntries.Count - 1);
            }

            OnPropertyChanged(nameof(EntryCountText));
        });
    }

    // ── Filtre ────────────────────────────────────────────────────────────

    private bool MatchesFilter(string entry)
    {
        var f = _filterText.Trim();
        return string.IsNullOrEmpty(f)
            || entry.Contains(f, StringComparison.OrdinalIgnoreCase);
    }

    private void RefreshFilter()
    {
        FilteredEntries.Clear();
        foreach (var entry in LogEntries)
            if (MatchesFilter(entry))
                FilteredEntries.Add(entry);
        OnPropertyChanged(nameof(EntryCountText));
    }

    // ── Effacer ───────────────────────────────────────────────────────────

    private void OnClear()
    {
        LogEntries.Clear();
        FilteredEntries.Clear();
        OnPropertyChanged(nameof(EntryCountText));
        StatusText = "Liste effacée.";
    }

    // ── Export .txt — MODIFIÉ V2.4 ────────────────────────────────────────

    private void OnExport()
    {
        if (LogEntries.Count == 0)
        {
            StatusText = "Aucun log à exporter.";
            return;
        }

        try
        {
            var desktop  = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            var filename = $"robot_logs_{DateTime.Now:yyyyMMdd_HHmmss}.txt";
            var path     = Path.Combine(desktop, filename);

            // Écrire du plus ancien au plus récent (LogEntries est du plus récent au plus ancien)
            var lines = LogEntries.Reverse().ToArray();
            File.WriteAllLines(path, lines);

            StatusText = $"Exporté → {filename}";
        }
        catch (Exception ex)
        {
            StatusText = $"Erreur export : {ex.Message}";
        }
    }

    // ── Compteur ──────────────────────────────────────────────────────────

    public string EntryCountText
    {
        get
        {
            int total    = LogEntries.Count;
            int filtered = FilteredEntries.Count;
            return string.IsNullOrWhiteSpace(_filterText)
                ? $"{total} / {MaxLogEntries} messages"
                : $"{filtered} résultat(s) sur {total}";
        }
    }
}
