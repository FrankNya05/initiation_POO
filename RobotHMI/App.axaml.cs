using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using RobotHMI.Services;
using RobotHMI.ViewModels;
using RobotHMI.Views;
using Avalonia.Markup.Xaml;

namespace RobotHMI;

// ---------------------------------------------------------------------------
// App — application startup and manual dependency injection
// ---------------------------------------------------------------------------
// This is the composition root of the application.
// All services and ViewModels are created here and connected to each other.
//
// Architecture rule: only this file is allowed to call "new" on services.
// ViewModels receive their dependencies through constructor parameters.
//
// How to extend in future phases (architecture §10.2):
//   Phase 2: create TelemetryPanelViewModel, register TELEMETRY handler
//   Phase 3: create ControlPanelViewModel,   register ACK handler
//   Phase 4: create StatusBarViewModel,       register STATE + LOG handlers
//
// Never rewrite this file — only add new registrations at the bottom of
// OnFrameworkInitializationCompleted().
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
            // ----------------------------------------------------------------
            // Phase 1 — Create shared services
            // ----------------------------------------------------------------

            // One shared communication service for the entire application.
            // All ViewModels that need to communicate receive this same instance.
            var commService = new RobotCommunicationService();

            // ----------------------------------------------------------------
            // Phase 2 — Wire the MessageRouter
            // ----------------------------------------------------------------
            // MessageRouter sits between RobotCommunicationService and the
            // ViewModels. It receives every raw JSON string and dispatches it
            // to the handler registered for its "type" field.
            // It never parses payloads — that is the job of TelemetryParser.
            var router = new MessageRouter();
            commService.MessageReceived += (_, msg) => router.Route(msg);

            // ----------------------------------------------------------------
            // Phase 1 — Create root ViewModel
            // ----------------------------------------------------------------

            var mainWindowViewModel = new MainWindowViewModel(commService);

            // ----------------------------------------------------------------
            // Phase 2 — Register telemetry handlers
            // ----------------------------------------------------------------
            // These two lines connect the router to TelemetryPanelViewModel.
            // When the router sees type == "TELEMETRY" it calls OnTelemetryReceived.
            // When it sees type == "STATE"     it calls OnStateReceived.
            router.Register("TELEMETRY", mainWindowViewModel.TelemetryPanel.OnTelemetryReceived);
            router.Register("STATE",     mainWindowViewModel.TelemetryPanel.OnStateReceived);

            // ----------------------------------------------------------------
            // Phase 3 — Register ACK handler (uncomment in Phase 3)
            // ----------------------------------------------------------------
            // router.Register("ACK", mainWindowViewModel.ControlPanel.OnAckReceived);

            // ----------------------------------------------------------------
            // Phase 4 — Register log and state handlers (uncomment in Phase 4)
            // ----------------------------------------------------------------
            // router.Register("LOG",   mainWindowViewModel.StatusBar.OnLogReceived);
            // router.Register("STATE", mainWindowViewModel.StatusBar.OnStateReceived);

            // ----------------------------------------------------------------
            // Show the main window
            // ----------------------------------------------------------------

            desktop.MainWindow = new MainWindow
            {
                DataContext = mainWindowViewModel
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
