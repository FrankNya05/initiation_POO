using System.Windows.Input;

namespace RobotHMI.ViewModels;

// ---------------------------------------------------------------------------
// RelayCommand
// ---------------------------------------------------------------------------
// A minimal ICommand implementation that wraps a plain C# action.
// ViewModels expose RelayCommand instances as public properties;
// Views bind buttons to them using Command="{Binding MyCommand}".
//
// Two variants are provided:
//   RelayCommand        — synchronous action
//   RelayCommand        — constructed with a canExecute predicate
//
// For async operations (ConnectAsync, DisconnectAsync), use AsyncRelayCommand.
// ---------------------------------------------------------------------------

public class RelayCommand : ICommand
{
    private readonly Action _execute;
    private readonly Func<bool>? _canExecute;

    public RelayCommand(Action execute, Func<bool>? canExecute = null)
    {
        _execute    = execute ?? throw new ArgumentNullException(nameof(execute));
        _canExecute = canExecute;
    }

    /// <summary>
    /// Raise this to tell Avalonia to re-evaluate CanExecute.
    /// Call NotifyCanExecuteChanged() from the ViewModel when state changes.
    /// </summary>
    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => _canExecute?.Invoke() ?? true;

    public void Execute(object? parameter) => _execute();

    /// <summary>
    /// Forces Avalonia to re-query CanExecute so buttons enable/disable correctly.
    /// </summary>
    public void NotifyCanExecuteChanged() =>
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}

// ---------------------------------------------------------------------------
// AsyncRelayCommand
// ---------------------------------------------------------------------------
// Like RelayCommand but wraps an async Task-returning action.
// Prevents re-entrant execution while the task is running (IsBusy).
// ---------------------------------------------------------------------------

public class AsyncRelayCommand : ICommand
{
    private readonly Func<Task> _execute;
    private readonly Func<bool>? _canExecute;
    private bool _isBusy;

    public AsyncRelayCommand(Func<Task> execute, Func<bool>? canExecute = null)
    {
        _execute    = execute ?? throw new ArgumentNullException(nameof(execute));
        _canExecute = canExecute;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) =>
        !_isBusy && (_canExecute?.Invoke() ?? true);

    public async void Execute(object? parameter)
    {
        if (!CanExecute(parameter)) return;

        _isBusy = true;
        NotifyCanExecuteChanged();

        try
        {
            await _execute();
        }
        finally
        {
            _isBusy = false;
            NotifyCanExecuteChanged();
        }
    }

    public void NotifyCanExecuteChanged() =>
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
