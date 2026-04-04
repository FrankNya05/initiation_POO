namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// CommandAction
// ---------------------------------------------------------------------------
// Enum representing the five motion actions the robot understands.
// Values are named to match the "action" strings in the CMD JSON payload
// exactly, so CommandSerializer can use nameof() or ToString() without
// any manual mapping.
//
// ESP32 CMD payload (architecture doc §6.2):
//   { "type": "CMD", "payload": { "action": "FORWARD", "speed": 200 } }
// ---------------------------------------------------------------------------

public enum CommandAction
{
    Forward,
    Backward,
    Left,
    Right,
    Stop
}

// ---------------------------------------------------------------------------
// MotorCommand
// ---------------------------------------------------------------------------
// Plain data class representing a single motor command to send to the robot.
// Created by ControlPanelViewModel when the user taps a direction button.
// Consumed by CommandSerializer to produce the JSON envelope.
//
// This class has NO logic — it is just data, following the same pattern
// as SensorData on the receiving side.
// ---------------------------------------------------------------------------

public class MotorCommand
{
    /// <summary>
    /// The motion action to perform.
    /// Maps directly to the "action" field in the CMD JSON payload.
    /// </summary>
    public CommandAction Action { get; set; }

    /// <summary>
    /// Motor speed value, 0–255, matching the PWM range on the ESP32.
    /// Maps directly to the "speed" field in the CMD JSON payload.
    /// For STOP, the speed is irrelevant but is kept for completeness.
    /// </summary>
    public int Speed { get; set; }

    /// <summary>Convenience constructor.</summary>
    public MotorCommand(CommandAction action, int speed)
    {
        Action = action;
        Speed  = speed;
    }
}
