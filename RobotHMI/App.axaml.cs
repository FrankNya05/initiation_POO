using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using RobotHMI.Services;
using RobotHMI.ViewModels;
using RobotHMI.Views;

namespace RobotHMI;

// ---------------------------------------------------------------------------
// App.axaml.cs — MODIFIÉ V2.5
// ---------------------------------------------------------------------------
// Changements V2.5 :
//   - TELEMETRY routé vers DashboardPanel + StatusBar (remplace TelemetryPanel + DigitalTwinPanel)
//   - STATE routé vers DashboardPanel (remplace TelemetryPanel)
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
            var commService = new RobotCommunicationService();

            var router = new MessageRouter();
            commService.MessageReceived += (_, msg) => router.Route(msg);

            var mainVm = new MainWindowViewModel(commService);

            // ── TELEMETRY ─────────────────────────────────────────────────
            // → DashboardPanel : canvas dohyo + télémétrie (V2.5)
            // → StatusBar      : batterie barre haute
            router.Register("TELEMETRY", rawJson =>
            {
                mainVm.DashboardPanel.OnTelemetryReceived(rawJson);
                mainVm.StatusBar.OnTelemetryReceived(rawJson);
            });

            // ── STATE ─────────────────────────────────────────────────────
            // → DashboardPanel : état robot dans onglet TABLEAU (V2.5)
            // → StatusBar      : état dans la barre haute
            router.Register("STATE", rawJson =>
            {
                mainVm.DashboardPanel.OnStateReceived(rawJson);
                mainVm.StatusBar.OnStateReceived(rawJson);
            });

            // ── ACK ───────────────────────────────────────────────────────
            router.Register("ACK", mainVm.ControlPanel.OnAckReceived);

            // ── LOG ───────────────────────────────────────────────────────
            router.Register("LOG", rawJson =>
            {
                mainVm.StatusBar.OnLogReceived(rawJson);
                mainVm.LogPanel.OnLogReceived(rawJson);
            });

            desktop.MainWindow = new MainWindow
            {
                DataContext = mainVm
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
