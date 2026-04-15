using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using RobotHMI.Models;
using System.Text;

namespace RobotHMI.Communication;

// ---------------------------------------------------------------------------
// MqttCommClient — real MQTT implementation of ICommClient
// ---------------------------------------------------------------------------
// Uses the MQTTnet v4 library to connect to a Mosquitto broker.
//
// Topic alignment with the ESP32:
//   ESP32 publishes to  → "robot/data"      (HMI subscribes)
//   ESP32 subscribes to → "robot/commande"  (HMI publishes)
//
//   These defaults are set in MqttConnectionConfig and pre-filled in the UI.
//   The user can override them in the connection panel if needed.
//
// Threading:
//   - MQTTnet raises ApplicationMessageReceivedAsync on a threadpool thread.
//   - We forward the payload string via MessageReceived without marshalling —
//     callers (TelemetryPanelViewModel, StatusBarViewModel, etc.) already
//     wrap their updates in Dispatcher.UIThread.InvokeAsync(), as required
//     by the architecture contract on ICommClient.MessageReceived.
//
// What this class does NOT do (architecture rule §8.4):
//   - It does not parse JSON. Raw payload strings are forwarded as-is.
//   - It does not know about message types (TELEMETRY, STATE, CMD, …).
//   - It does not interact with any ViewModel.
// ---------------------------------------------------------------------------

public class MqttCommClient : ICommClient
{
    // -----------------------------------------------------------------------
    // MQTTnet objects
    // -----------------------------------------------------------------------

    // MqttFactory is the entry point for all MQTTnet v4 operations.
    private readonly MqttFactory _factory = new();

    // The underlying MQTTnet client. Created fresh for each connection so
    // we never reuse a client that has been through a disconnect cycle.
    private IMqttClient? _client;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    private MqttConnectionConfig? _activeConfig;

    /// <summary>
    /// True when the MQTTnet client is connected to the broker.
    /// Reads directly from the MQTTnet client so it is always accurate,
    /// even if the broker drops the connection unexpectedly.
    /// </summary>
    public bool IsConnected => _client?.IsConnected ?? false;

    // -----------------------------------------------------------------------
    // Event
    // -----------------------------------------------------------------------

    /// <summary>
    /// Raised with the raw UTF-8 payload string whenever a message arrives
    /// on the subscribed topic. Fires on a background thread — callers must
    /// marshal to the UI thread before touching observable properties.
    /// </summary>
    public event EventHandler<string>? MessageReceived;

    // -----------------------------------------------------------------------
    // ConnectAsync
    // -----------------------------------------------------------------------

    public async Task ConnectAsync(ConnectionConfig config)
    {
        if (config is not MqttConnectionConfig mqttConfig)
            throw new ArgumentException(
                "MqttCommClient requires an MqttConnectionConfig.", nameof(config));

        // Dispose any previous client cleanly before creating a new one.
        if (_client != null)
        {
            _client.ApplicationMessageReceivedAsync -= OnMessageReceivedAsync;
            _client.Dispose();
            _client = null;
        }

        _activeConfig = mqttConfig;

        Console.WriteLine($"[MQTT] Connecting to broker {mqttConfig.BrokerIp}:{mqttConfig.Port}");
        Console.WriteLine($"[MQTT] Subscribe topic : {mqttConfig.TelemetryTopic}");
        Console.WriteLine($"[MQTT] Publish topic   : {mqttConfig.CommandTopic}");

        // ----------------------------------------------------------------
        // Build the MQTTnet client and options
        // ----------------------------------------------------------------

        _client = _factory.CreateMqttClient();

        // Register the incoming-message callback BEFORE connecting so we
        // cannot miss any message that arrives immediately after the broker
        // sends CONNACK.
        _client.ApplicationMessageReceivedAsync += OnMessageReceivedAsync;

        var options = new MqttClientOptionsBuilder()
            .WithTcpServer(mqttConfig.BrokerIp, mqttConfig.Port)
            // A unique client ID prevents the broker from kicking out a
            // previous HMI session that did not disconnect cleanly.
            .WithClientId($"RobotHMI_{Guid.NewGuid().ToString("N")[..8]}")
            // CleanSession = true: no queued messages from previous sessions.
            .WithCleanSession(true)
            // Keep-alive: the broker will detect a dead client after 30 s.
            .WithKeepAlivePeriod(TimeSpan.FromSeconds(30))
            .Build();

        // ----------------------------------------------------------------
        // Connect — throws MqttCommunicationException on network failure
        // ----------------------------------------------------------------

        MqttClientConnectResult result;
        try
        {
            result = await _client.ConnectAsync(options, CancellationToken.None);
        }
        catch (Exception ex)
        {
            // Wrap in a plain exception so the ViewModel gets a readable message.
            throw new InvalidOperationException(
                $"MQTT connection failed: {ex.Message}", ex);
        }

        if (result.ResultCode != MqttClientConnectResultCode.Success)
        {
            throw new InvalidOperationException(
                $"Broker refused connection: {result.ResultCode}");
        }

        Console.WriteLine("[MQTT] Connected to broker.");

        // ----------------------------------------------------------------
        // Subscribe to the telemetry/data topic
        // ----------------------------------------------------------------

        var subscribeOptions = _factory.CreateSubscribeOptionsBuilder()
            .WithTopicFilter(f => f
                .WithTopic(mqttConfig.TelemetryTopic)
                // QoS 0: fire-and-forget, lowest overhead — suitable for
                // telemetry where a missed packet is acceptable and we
                // prefer low latency over guaranteed delivery.
                .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtMostOnce))
            .Build();

        await _client.SubscribeAsync(subscribeOptions, CancellationToken.None);

        Console.WriteLine($"[MQTT] Subscribed to '{mqttConfig.TelemetryTopic}'.");
    }

    // -----------------------------------------------------------------------
    // DisconnectAsync
    // -----------------------------------------------------------------------

    public async Task DisconnectAsync()
    {
        // Nothing to do if there is no client at all.
        if (_client == null)
        {
            Console.WriteLine("[MQTT] DisconnectAsync called with no client — nothing to do.");
            _activeConfig = null;
            return;
        }

        Console.WriteLine("[MQTT] Disconnecting from broker...");

        // Unsubscribe the callback first so no messages fire during teardown.
        _client.ApplicationMessageReceivedAsync -= OnMessageReceivedAsync;

        if (_client.IsConnected)
        {
            try
            {
                var disconnectOptions = new MqttClientDisconnectOptionsBuilder()
                    // NormalDisconnection sends an MQTT DISCONNECT packet so the
                    // broker knows this was a clean close, not a network failure.
                    .WithReason(MqttClientDisconnectOptionsReason.NormalDisconnection)
                    .Build();

                await _client.DisconnectAsync(disconnectOptions, CancellationToken.None);
            }
            catch (Exception ex)
            {
                // Log but do not rethrow — clean-up must complete regardless.
                Console.WriteLine($"[MQTT] Error during disconnect: {ex.Message}");
            }
        }

        _client.Dispose();
        _client = null;
        _activeConfig = null;
        Console.WriteLine("[MQTT] Disconnected.");
    }

    // -----------------------------------------------------------------------
    // SendAsync
    // -----------------------------------------------------------------------

    public async Task SendAsync(string message)
    {
        if (_client == null || !_client.IsConnected || _activeConfig == null)
            throw new InvalidOperationException(
                "Cannot send: MQTT client is not connected.");

        Console.WriteLine($"[MQTT] → '{_activeConfig.CommandTopic}': {message}");

        // Encode the JSON string as UTF-8 bytes for the MQTT payload.
        var payload = Encoding.UTF8.GetBytes(message);

        var mqttMessage = new MqttApplicationMessageBuilder()
            .WithTopic(_activeConfig.CommandTopic)
            .WithPayload(payload)
            // QoS 1 for commands: the broker acknowledges receipt, giving us
            // at-least-once delivery without the overhead of QoS 2.
            .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
            // Retain = false: the broker must not cache commands for new
            // subscribers — a late-joining ESP32 should not execute old commands.
            .WithRetainFlag(false)
            .Build();

        await _client.PublishAsync(mqttMessage, CancellationToken.None);
    }

    // -----------------------------------------------------------------------
    // OnMessageReceivedAsync — MQTTnet v4 callback
    // -----------------------------------------------------------------------

    /// <summary>
    /// Called by MQTTnet on a threadpool thread whenever a message arrives
    /// on the subscribed topic. Decodes the payload as UTF-8 and raises
    /// MessageReceived with the raw string — no JSON parsing here.
    /// </summary>
    private Task OnMessageReceivedAsync(MqttApplicationMessageReceivedEventArgs e)
    {
        // Guard: ignore empty payloads (keep-alive pings, retained empty messages).
        var payload = e.ApplicationMessage.PayloadSegment;
        if (payload.Count == 0)
            return Task.CompletedTask;

        string raw;
        try
        {
            raw = Encoding.UTF8.GetString(payload.Array!, payload.Offset, payload.Count);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MQTT] Failed to decode payload as UTF-8: {ex.Message}");
            return Task.CompletedTask;
        }

        Console.WriteLine($"[MQTT] ← '{e.ApplicationMessage.Topic}': {raw}");

        // Fire on the same threadpool thread. The architecture contract
        // (ICommClient.MessageReceived) documents that this event fires on a
        // background thread — all subscribers already handle this correctly.
        MessageReceived?.Invoke(this, raw);

        return Task.CompletedTask;
    }
}