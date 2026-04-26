using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using RobotHMI.Services;
using RobotHMI.ViewModels;
using RobotHMI.Views;

namespace RobotHMI;

// ---------------------------------------------------------------------------
// App.axaml.cs — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Point de composition de l'application (racine DI manuelle).
// Seul fichier autorisé à appeler "new" sur les services.
//
// Changements V2 :
//   - Enregistrement "TELEMETRY" vers StatusBar.OnTelemetryReceived (MODIFIÉ V2)
//   - Enregistrement "LOG" vers LogPanel.OnLogReceived en plus de StatusBar (MODIFIÉ V2)
//   - STATE dispatché aussi vers LogPanel via lambda étendue (MODIFIÉ V2)
//   - Aucune autre ligne modifiée — ajouts uniquement
// ---------------------------------------------------------------------------

public partial class App : Application
{
    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            // ────────────────────────────────────────────────────────────
            // Service partagé unique — reçu par tous les VMs qui en ont besoin
            // ────────────────────────────────────────────────────────────
            var commService = new RobotCommunicationService();

            // ────────────────────────────────────────────────────────────
            // MessageRouter — dispatch des messages par type
            // Tous les messages reçus de l'ESP32 passent ici.
            // ────────────────────────────────────────────────────────────
            var router = new MessageRouter();
            commService.MessageReceived += (_, msg) => router.Route(msg);

            // ────────────────────────────────────────────────────────────
            // ViewModel racine — agrège tous les enfants
            // ────────────────────────────────────────────────────────────
            var mainVm = new MainWindowViewModel(commService);

            // ────────────────────────────────────────────────────────────
            // ENREGISTREMENTS MESSAGEROUTER
            //
            // Règle : un seul enregistrement par type "type" JSON.
            // Pour dispatcher vers plusieurs VMs, utiliser une lambda.
            // ────────────────────────────────────────────────────────────

            // ── TELEMETRY ────────────────────────────────────────────────
            // → TelemetryPanelViewModel : met à jour les capteurs affiché
            // → StatusBarViewModel      : met à jour batterie dans la barre haute (MODIFIÉ V2)
            router.Register("TELEMETRY", rawJson =>
            {
                mainVm.TelemetryPanel.OnTelemetryReceived(rawJson);
                mainVm.StatusBar.OnTelemetryReceived(rawJson);  // MODIFIÉ V2
            });

            // ── STATE ────────────────────────────────────────────────────
            // → TelemetryPanelViewModel : icône d'état dans l'onglet télémétrie
            // → StatusBarViewModel      : état dans la barre haute
            router.Register("STATE", rawJson =>
            {
                mainVm.TelemetryPanel.OnStateReceived(rawJson);
                mainVm.StatusBar.OnStateReceived(rawJson);
            });

            // ── ACK ──────────────────────────────────────────────────────
            // → ControlPanelViewModel : affiche confirmation de commande reçue
            router.Register("ACK", mainVm.ControlPanel.OnAckReceived);

            // ── LOG ──────────────────────────────────────────────────────
            // → StatusBarViewModel : dernier log dans la barre inférieure
            // → LogPanelViewModel  : liste des 100 derniers logs (MODIFIÉ V2)
            router.Register("LOG", rawJson =>
            {
                mainVm.StatusBar.OnLogReceived(rawJson);
                mainVm.LogPanel.OnLogReceived(rawJson);  // MODIFIÉ V2
            });

            // ────────────────────────────────────────────────────────────
            // Affichage de la fenêtre principale
            // ────────────────────────────────────────────────────────────
            desktop.MainWindow = new MainWindow
            {
                DataContext = mainVm
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
