using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// ViewModelBase
// ---------------------------------------------------------------------------
// Base class for every ViewModel in the application.
// Provides a standard implementation of INotifyPropertyChanged so that
// Avalonia data bindings update automatically when properties change.
//
// Usage:
//   Override properties in subclasses using SetField<T>() in the setter.
//   Example:
//
//     private string _status = string.Empty;
//     public string Status
//     {
//         get => _status;
//         set => SetField(ref _status, value);
//     }
//
// SetField<T>() only raises PropertyChanged when the value actually changes,
// which avoids redundant UI updates.
// ---------------------------------------------------------------------------

public abstract class ViewModelBase : INotifyPropertyChanged
{
    // Avalonia's binding engine listens to this event.
    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>
    /// Raises PropertyChanged for the given property name.
    /// The [CallerMemberName] attribute fills the name automatically —
    /// you rarely need to pass it explicitly.
    /// </summary>
    protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }

    /// <summary>
    /// Sets a backing field and raises PropertyChanged only if the value changed.
    /// Returns true when the value was actually updated.
    ///
    /// Typical usage in a property setter:
    ///   set => SetField(ref _myField, value);
    /// </summary>
    protected bool SetField<T>(ref T field, T value,
        [CallerMemberName] string? propertyName = null)
    {
        // Skip update if the new value is the same as the current one.
        if (EqualityComparer<T>.Default.Equals(field, value))
            return false;

        field = value;
        OnPropertyChanged(propertyName);
        return true;
    }
}
