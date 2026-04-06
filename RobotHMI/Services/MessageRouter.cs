using System.Text.Json;

namespace RobotHMI.Services;

// ---------------------------------------------------------------------------
// MessageRouter
// ---------------------------------------------------------------------------
// Receives every raw JSON string forwarded by RobotCommunicationService
// and dispatches it to the handler registered for its "type" field.
//
// Responsibilities (architecture doc §5.5):
//   - Parse ONLY the "type" field of the envelope
//   - Call the registered Action<string> for that type
//   - Log unrecognised types to the console without crashing
//   - Never parse the payload — that is the job of the individual parsers
//
// Usage (App.axaml.cs):
//   var router = new MessageRouter();
//   commService.MessageReceived += (_, msg) => router.Route(msg);
//   router.Register("TELEMETRY", telemetryVM.OnTelemetryReceived);
//   router.Register("STATE",     telemetryVM.OnStateReceived);
//
// Design note:
//   The handler Action<string> receives the FULL original JSON string,
//   not just the payload. This lets each handler use its own parser
//   without MessageRouter knowing anything about message contents.
// ---------------------------------------------------------------------------

public class MessageRouter
{
    // -----------------------------------------------------------------------
    // Handler registry
    // -----------------------------------------------------------------------

    // Maps message type strings (e.g. "TELEMETRY") to their handler.
    // One handler per type — the last Register() call wins if called twice.
    private readonly Dictionary<string, Action<string>> _handlers = new();

    // -----------------------------------------------------------------------
    // Register
    // -----------------------------------------------------------------------

    /// <summary>
    /// Registers a handler for a given message type.
    /// The handler receives the full raw JSON string when a matching message arrives.
    /// Call this once per message type at application startup (App.axaml.cs).
    /// </summary>
    /// <param name="messageType">
    ///   The "type" value to match, e.g. "TELEMETRY", "STATE", "ACK", "LOG".
    ///   Matching is case-sensitive — use uppercase to match ESP32 convention.
    /// </param>
    /// <param name="handler">
    ///   Action called with the full raw JSON string when a matching message arrives.
    /// </param>
    public void Register(string messageType, Action<string> handler)
    {
        _handlers[messageType] = handler;
    }

    // -----------------------------------------------------------------------
    // Route
    // -----------------------------------------------------------------------

    /// <summary>
    /// Parses the "type" field of the JSON envelope and dispatches to the
    /// registered handler. Called by App.axaml.cs on every message received
    /// from RobotCommunicationService.
    ///
    /// Runs on the background thread that delivered the message.
    /// Handlers are responsible for marshalling to the UI thread if needed.
    /// </summary>
    public void Route(string rawJson)
    {
        if (string.IsNullOrWhiteSpace(rawJson))
            return;

        try
        {
            // Parse only the top-level object to extract the "type" field.
            // We do NOT parse the full message here — that stays in the parsers.
            using var doc = JsonDocument.Parse(rawJson);

            if (!doc.RootElement.TryGetProperty("type", out var typeProp))
            {
                Console.WriteLine($"[MessageRouter] Message has no 'type' field: {rawJson}");
                return;
            }

            var messageType = typeProp.GetString();

            if (string.IsNullOrEmpty(messageType))
            {
                Console.WriteLine($"[MessageRouter] Empty 'type' field in: {rawJson}");
                return;
            }

            // Dispatch to the registered handler, or log if none is registered.
            if (_handlers.TryGetValue(messageType, out var handler))
            {
                handler(rawJson);
            }
            else
            {
                Console.WriteLine($"[MessageRouter] No handler registered for type: '{messageType}'");
            }
        }
        catch (JsonException ex)
        {
            // Malformed JSON from the robot — log and continue. Never crash.
            Console.WriteLine($"[MessageRouter] JSON parse error: {ex.Message} — raw: {rawJson}");
        }
    }
}
