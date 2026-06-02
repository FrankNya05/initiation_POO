using Avalonia.Data.Converters;
using System.Globalization;

namespace RobotHMI.Converters;

// ---------------------------------------------------------------------------
// BoolToStringConverter — NOUVEAU V2
// ---------------------------------------------------------------------------
// Convertit un bool → string selon le paramètre "trueText|falseText".
// Exemple : ConverterParameter="● Cible détectée|○ Aucune cible"
// ---------------------------------------------------------------------------

public class BoolToStringConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        bool active = value is true;
        var parts = (parameter as string ?? "Oui|Non").Split('|');
        return active ? parts[0] : (parts.Length > 1 ? parts[1] : "Non");
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}