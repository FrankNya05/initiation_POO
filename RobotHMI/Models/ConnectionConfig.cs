namespace RobotHMI.Models;

// ---------------------------------------------------------------------------
// ConnectionConfig — base abstraite
// ---------------------------------------------------------------------------
public abstract class ConnectionConfig { }

// ---------------------------------------------------------------------------
// BleConnectionConfig
// ---------------------------------------------------------------------------
public class BleConnectionConfig : ConnectionConfig
{
    public string DeviceName  { get; set; } = string.Empty;
    public string ServiceUuid { get; set; } = string.Empty;
}

// ---------------------------------------------------------------------------
// MqttConnectionConfig — MODIFIÉ V2
// ---------------------------------------------------------------------------
// Topics alignés sur la nomenclature ESP32 réelle :
//   robot/telemetry → ESP32 publie ses données
//   robot/cmd       → ESP32 écoute les commandes
// ---------------------------------------------------------------------------
public class MqttConnectionConfig : ConnectionConfig
{
    public string BrokerIp { get; set; } = "127.0.0.1";
    public int    Port     { get; set; } = 1883;

    // MODIFIÉ V2 — aligné sur ESP32
    public string TelemetryTopic { get; set; } = "robot/telemetry";
    public string CommandTopic   { get; set; } = "robot/cmd";
}