namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// ConnectionConfig — abstract base class
// ---------------------------------------------------------------------------
// Holds the parameters needed to open a connection to the robot.
// Each protocol has its own subtype with its own fields.
// RobotCommunicationService receives a ConnectionConfig and passes it
// straight to the active ICommClient.ConnectAsync() — no casting needed
// until inside the concrete client implementation.
// ---------------------------------------------------------------------------

public abstract class ConnectionConfig
{
    // Intentionally empty — acts as a common type for the service layer.
    // Protocol-specific fields live in the subclasses below.
}

// ---------------------------------------------------------------------------
// BleConnectionConfig
// ---------------------------------------------------------------------------
// Parameters required to connect to the robot over BLE.
// DeviceName is the advertised name the ESP32 broadcasts.
// ServiceUuid identifies the GATT service that carries robot data.
// ---------------------------------------------------------------------------

public class BleConnectionConfig : ConnectionConfig
{
    /// <summary>
    /// The BLE device name advertised by the ESP32 (e.g., "MiniSumoRobot").
    /// </summary>
    public string DeviceName { get; set; } = string.Empty;

    /// <summary>
    /// The GATT service UUID used by the robot's BLE server.
    /// Must match the UUID defined on the ESP32 side.
    /// </summary>
    public string ServiceUuid { get; set; } = string.Empty;
}

// ---------------------------------------------------------------------------
// MqttConnectionConfig
// ---------------------------------------------------------------------------
// Parameters required to connect to the MQTT broker over WiFi.
// ---------------------------------------------------------------------------

public class MqttConnectionConfig : ConnectionConfig
{
    /// <summary>
    /// IP address of the MQTT broker (e.g., "192.168.1.100").
    /// </summary>
    public string BrokerIp { get; set; } = "127.0.0.1";

    /// <summary>
    /// MQTT broker port. Standard is 1883 (no TLS).
    /// </summary>
    public int Port { get; set; } = 1883;

    /// <summary>
    /// Topic the HMI subscribes to in order to receive data from the robot.
    /// Matches the ESP32 _topicPub: "robot/data"
    /// </summary>
    public string TelemetryTopic { get; set; } = "robot/data";

    /// <summary>
    /// Topic the HMI publishes to in order to send commands to the robot.
    /// Matches the ESP32 _topicSub: "robot/commande"
    /// </summary>
    public string CommandTopic { get; set; } = "robot/commande";
}