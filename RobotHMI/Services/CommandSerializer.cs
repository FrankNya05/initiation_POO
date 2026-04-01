using System.Text.Json;
using RobotHMI.Models;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// CommandSerializer
// ---------------------------------------------------------------------------
// The ONLY place in the application that knows the shape of an outgoing
// CMD message. Converts a MotorCommand model into the unified JSON envelope
// defined in the architecture document §6.2.
//
// Output format:
//   {
//     "type": "CMD",
//     "payload": {
//       "action": "FORWARD",
//       "speed":  200
//     }
//   }
//
// Design decisions:
//   - Static class: no state, no dependencies, easy to read and test.
//   - Action strings are uppercase to match ESP32 convention and the
//     architecture document. CommandAction enum values are Title-case
//     in C# (Forward, Backward…) and converted to uppercase here.
//   - Uses System.Text.Json, already declared as a project dependency.
// ---------------------------------------------------------------------------

public static class CommandSerializer
{
    // Shared options: no indentation (compact wire format), case-insensitive
    // reading not needed here since we only write.
    private static readonly JsonWriterOptions _writerOptions = new()
    {
        Indented = false
    };

    /// <summary>
    /// Serializes a MotorCommand into the unified CMD JSON envelope string.
    /// The returned string is ready to pass directly to
    /// RobotCommunicationService.SendAsync().
    /// </summary>
    public static string Serialize(MotorCommand command)
    {
        // Convert the C# enum value (e.g. Forward) to the uppercase wire
        // string the ESP32 expects (e.g. "FORWARD").
        var actionString = command.Action.ToString().ToUpperInvariant();

        // Clamp speed to the valid range so no invalid value ever reaches
        // the robot, regardless of what the slider or caller provides.
        var speed = Math.Clamp(command.Speed, 0, 255);

        using var stream = new System.IO.MemoryStream();
        using var writer = new Utf8JsonWriter(stream, _writerOptions);

        writer.WriteStartObject();                      // {
        writer.WriteString("type", "CMD");              //   "type": "CMD",
        writer.WriteStartObject("payload");             //   "payload": {
        writer.WriteString("action", actionString);     //     "action": "FORWARD",
        writer.WriteNumber("speed",  speed);            //     "speed": 200
        writer.WriteEndObject();                        //   }
        writer.WriteEndObject();                        // }
        writer.Flush();

        return System.Text.Encoding.UTF8.GetString(stream.ToArray());
    }
}
