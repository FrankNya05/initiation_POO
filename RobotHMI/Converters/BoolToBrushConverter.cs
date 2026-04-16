using Avalonia.Data.Converters;
using Avalonia.Media;
using System.Globalization;

namespace RobotHMI.Converters;

// ---------------------------------------------------------------------------
// BoolToBrushConverter — NOUVEAU V2
// ---------------------------------------------------------------------------
// Convertit un bool → SolidColorBrush selon le paramètre "trueColor|falseColor".
// Couleurs supportées : danger, success, warning, accent, secondary, bgpanel
// Exemple : ConverterParameter="danger|secondary"
//   true  → #FF1744 (rouge/danger)
//   false → #78909C (gris/secondary)
// ---------------------------------------------------------------------------

public class BoolToBrushConverter : IValueConverter
{
    private static readonly Dictionary<string, string> ColorMap = new()
    {
        { "danger",    "#FF1744" },
        { "success",   "#00E676" },
        { "warning",   "#FF9100" },
        { "accent",    "#00D4FF" },
        { "secondary", "#2A3F55" },
        { "bgpanel",   "#1C2B3A" }
    };

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        bool active = value is true;
        var parts = (parameter as string ?? "success|secondary").Split('|');
        string colorKey = active ? parts[0] : (parts.Length > 1 ? parts[1] : "secondary");

        if (ColorMap.TryGetValue(colorKey, out var hex))
            return new SolidColorBrush(Color.Parse(hex));

        return new SolidColorBrush(Colors.Gray);
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}