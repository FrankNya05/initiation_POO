using Avalonia;

namespace RobotHMI;

// ---------------------------------------------------------------------------
// Program — Avalonia entry point
// ---------------------------------------------------------------------------
// Standard Avalonia bootstrap. Nothing project-specific goes here.
// All initialization logic belongs in App.axaml.cs.
// ---------------------------------------------------------------------------

internal sealed class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        BuildAvaloniaApp()
            .StartWithClassicDesktopLifetime(args);
    }

    private static AppBuilder BuildAvaloniaApp()
    {
        return AppBuilder.Configure<App>()
            .UsePlatformDetect()   // Works on Linux (Raspberry Pi), Windows, macOS
            .WithInterFont()
            .LogToTrace();
    }
}
