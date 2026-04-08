# Mini-Sumo Robot — MQTT Communication Protocol V1

**Topics:** `robot/cmd` (Pi → Robot) · `robot/telemetry` (Robot → Pi)  
**Version:** 1.0 — Fixed for final integration sprint  
**Parsers:** ArduinoJson (ESP32) · System.Text.Json (Raspberry Pi / C#)

---

## 1. What Was Inferred from the Repository

Before defining anything, the existing code was inspected carefully. Here is what the repository reveals about sensor data and robot state.

### 1.1 Sensors confirmed in the codebase

| Sensor | Class | Output type | Notes |
|--------|-------|-------------|-------|
| **Line sensors** | `LineSensor` | `SensorData` (scalar, bool-like) | 4 physical sensors: `FRONT_LEFT`, `FRONT_RIGHT`, `BACK_LEFT`, `BACK` (not `BACK_RIGHT`). Positions come from `SensorPosition` enum. Value `> 0` means line detected. |
| **TOF distance** | `TOFSensor` (in `IRSensor.hpp`) | `SensorData` (scalar, metres) | Up to 2 sensors at I²C addresses `0x09` and `0x0A`. The project uses a TOF Mini Laser sensor, not a Sharp IR. The protocol uses the key `tof` everywhere. |
| **Lidar** | `LidarSensor` | `SensorData` (VEC3: x=distance m, y=angle °, z=valid point count) | YDLIDAR Tmini Plus. Publishes the **nearest detected object** (filtered with EMA). Range 0.05–12 m. |
| **Battery** | `BattSensor` | `SensorData` (scalar, Volts) | LiPo 2S, 6.0–8.4 V. Has `getVoltage()`, `getPercent()`, `isCritical()`. |
| **Encoders** | `Encoder` | `getCount()`, `getRPM()`, `getAngleDeg()` | Two encoders (left/right). Fully implemented but excluded from V1 telemetry — not yet useful on the HMI side. |
| **MPU6050 / IMU** | Not found in repo | — | Referenced in `PinConfig.hpp` via the shared I²C bus (SDA=21, SCL=22) but no IMU class exists yet. Excluded from V1 telemetry. |

### 1.2 Robot states — V1 mapping

`RobotConstants::State` in the firmware defines: `STANDBY`, `SEARCH`, `ATTACK`, `EVADE`.

**Decision for V1:** The protocol uses four fixed English uppercase strings. The firmware and HMI both adapt to these strings.

| Protocol string | ESP32 `State` enum | Meaning |
|---|---|---|
| `"IDLE"` | `STANDBY` | Waiting for start signal (5-second countdown or post-match) |
| `"SEARCH"` | `SEARCH` | Rotating and scanning — no opponent detected yet |
| `"ATTACK"` | `ATTACK` | Opponent detected, charging at full speed |
| `"DEFENSE"` | `EVADE` | Border line detected, recovery maneuver in progress |

> **Note for the HMI:** `TelemetryParser.cs` currently maps `"SEARCHING"` and `"RETREATING"`. Both strings must be updated: `"SEARCHING"` → `"SEARCH"` and `"RETREATING"` → `"DEFENSE"`. This is a one-line change per string in the existing `switch` block.

### 1.3 V1 telemetry scope decision

To keep the final integration sprint practical and focused, V1 telemetry is intentionally minimal. Only the sensor data that is currently useful and displayable on the HMI is included.

| Sensor | Included in V1 telemetry | Reason |
|---|---|---|
| Battery | ✅ | Critical safety indicator; HMI already has a display slot for it |
| Line sensors | ✅ | Border detection is the most visible real-time indicator |
| Lidar | ✅ | Opponent detection is the core combat data |
| TOF | ❌ | Sensor works, but no HMI panel targets it yet in V1 |
| Encoders | ❌ | Implemented but no HMI panel targets them in V1 |
| IMU / MPU6050 | ❌ | No driver class exists yet |

---

## 2. Envelope Rule (All Messages)

Every single message, in both directions, must follow this structure:

```json
{
  "type": "<MESSAGE_TYPE>",
  "payload": { ... }
}
```

- `type` — `string`, **required**, uppercase. Identifies what the payload contains.
- `payload` — **required**. Either a JSON object or a JSON string depending on the message type.

There are no other top-level fields. This keeps parsing trivial on both sides.

---

## 3. Pi → Robot Messages (`robot/cmd`)

### 3.1 `CMD_MOTOR` — Motor movement command

**Direction:** Pi → Robot  
**Purpose:** Directly control robot wheel speeds from the HMI (manual override mode).

#### JSON structure

```json
{
  "type": "CMD_MOTOR",
  "payload": {
    "action": "<ACTION>",
    "speed": 0
  }
}
```

#### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"CMD_MOTOR"` |
| `payload.action` | string | ✅ | One of: `"FORWARD"`, `"BACKWARD"`, `"LEFT"`, `"RIGHT"`, `"STOP"` |
| `payload.speed` | int | ✅ | PWM speed value. Range: `0–255`. For `"STOP"`, send `0`. Ignored by the firmware for `"STOP"` but must always be present to keep the structure consistent. |

#### Action values

| Value | Behaviour on ESP32 |
|---|---|
| `"FORWARD"` | Both motors forward at `speed` |
| `"BACKWARD"` | Both motors backward at `speed` |
| `"LEFT"` | Left motor backward, right motor forward at `speed` (pivot turn) |
| `"RIGHT"` | Right motor backward, left motor forward at `speed` (pivot turn) |
| `"STOP"` | Both motors to 0 (coast) |

#### Examples

```json
{ "type": "CMD_MOTOR", "payload": { "action": "FORWARD", "speed": 200 } }
```

```json
{ "type": "CMD_MOTOR", "payload": { "action": "STOP", "speed": 0 } }
```

```json
{ "type": "CMD_MOTOR", "payload": { "action": "LEFT", "speed": 150 } }
```

---

### 3.2 `CMD_ROBOT` — High-level robot control command

**Direction:** Pi → Robot  
**Purpose:** Start/stop the autonomous match, change strategy, or trigger a system reset.

#### JSON structure

```json
{
  "type": "CMD_ROBOT",
  "payload": {
    "command": "<COMMAND>",
    "param": ""
  }
}
```

#### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"CMD_ROBOT"` |
| `payload.command` | string | ✅ | See command table below |
| `payload.param` | string | ⬜ optional | Extra parameter for commands that need it (e.g. strategy name). Send `""` or omit when unused. |

#### Command values

| Value | `param` | Behaviour on ESP32 |
|---|---|---|
| `"START"` | — | Begin the 5-second countdown, then start autonomous operation |
| `"STOP"` | — | Immediately stop all motors and return to `IDLE` |
| `"SET_STRATEGY"` | `"AGGRESSIVE"` or `"DEFENSIVE"` | Switch the active strategy at runtime |
| `"RESET"` | — | Soft reset: clear state, re-initialize sensors |

#### Examples

```json
{ "type": "CMD_ROBOT", "payload": { "command": "START", "param": "" } }
```

```json
{ "type": "CMD_ROBOT", "payload": { "command": "SET_STRATEGY", "param": "AGGRESSIVE" } }
```

```json
{ "type": "CMD_ROBOT", "payload": { "command": "STOP", "param": "" } }
```

---

## 4. Robot → Pi Messages (`robot/telemetry`)

### 4.1 `TELEMETRY` — Live sensor data snapshot

**Direction:** Robot → Pi  
**Purpose:** Periodic broadcast of the sensor readings that are currently displayed on the HMI. Intended to be sent every 100–200 ms during a match.

**V1 scope:** battery + line sensors + lidar only. TOF, encoders, and IMU are reserved for V2.

#### JSON structure

```json
{
  "type": "TELEMETRY",
  "payload": {
    "ts": 0,
    "battery": {
      "voltage": 0.0,
      "percent": 0,
      "critical": false
    },
    "line": {
      "frontLeft":  false,
      "frontRight": false,
      "backLeft":   false,
      "back":       false
    },
    "lidar": {
      "dist":  0.0,
      "angle": 0.0,
      "valid": false
    }
  }
}
```

#### Fields — top level

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"TELEMETRY"` |
| `payload.ts` | int | ✅ | Timestamp in milliseconds since boot (`millis()`). Useful for detecting stale packets on the HMI side. |

#### Fields — `battery` object

| Field | Type | Required | Description |
|---|---|---|---|
| `voltage` | float | ✅ | Measured battery voltage in Volts (e.g. `7.35`). From `BattSensor::getVoltage()`. |
| `percent` | int | ✅ | Estimated charge level 0–100. From `BattSensor::getPercent()`. |
| `critical` | bool | ✅ | `true` when voltage ≤ 6.60 V. From `BattSensor::isCritical()`. Should trigger a visible warning in the HMI. |

#### Fields — `line` object

Represents the 4 line sensors physically present on the robot (see `PinConfig.hpp`: `LINE_SENSOR_FRONT_LEFT`, `LINE_SENSOR_FRONT_RIGHT`, `LINE_SENSOR_BACK_LEFT`, `LINE_SENSOR_BACK`).

| Field | Type | Required | Description |
|---|---|---|---|
| `frontLeft` | bool | ✅ | `true` if the front-left sensor detects the white border |
| `frontRight` | bool | ✅ | `true` if the front-right sensor detects the white border |
| `backLeft` | bool | ✅ | `true` if the back-left sensor detects the white border |
| `back` | bool | ✅ | `true` if the rear center sensor detects the white border |

> **Why not `backRight`?** The hardware defines only 4 line sensors, and the back pin (`LINE_SENSOR_BACK`, GPIO 35) is a single rear-center sensor. There is no back-right. The key `"back"` matches the physical layout.

#### Fields — `lidar` object

Data from the YDLIDAR Tmini Plus. Reports the nearest detected object in the current scan.

| Field | Type | Required | Description |
|---|---|---|---|
| `dist` | float | ✅ | Distance to the nearest detected object in **metres**. EMA-filtered. Range 0.05–12.0. Treat as unreliable when `valid` is `false`. |
| `angle` | float | ✅ | Angle to the nearest object in **degrees**, 0–360°. 0° = directly in front of the robot. |
| `valid` | bool | ✅ | `true` if the lidar returned at least one valid point this cycle. Always check this flag before using `dist` and `angle`. |

#### Full realistic examples

Normal match — opponent detected, back-left border grazed, battery healthy:

```json
{
  "type": "TELEMETRY",
  "payload": {
    "ts": 14823,
    "battery": {
      "voltage": 7.82,
      "percent": 72,
      "critical": false
    },
    "line": {
      "frontLeft":  false,
      "frontRight": false,
      "backLeft":   true,
      "back":       false
    },
    "lidar": {
      "dist":  0.42,
      "angle": 12.5,
      "valid": true
    }
  }
}
```

Match idle — no opponent visible, no border, battery critical:

```json
{
  "type": "TELEMETRY",
  "payload": {
    "ts": 52100,
    "battery": {
      "voltage": 6.58,
      "percent": 8,
      "critical": true
    },
    "line": {
      "frontLeft":  false,
      "frontRight": false,
      "backLeft":   false,
      "back":       false
    },
    "lidar": {
      "dist":  0.0,
      "angle": 0.0,
      "valid": false
    }
  }
}
```

---

### 4.2 `STATE` — Robot operating state change

**Direction:** Robot → Pi  
**Purpose:** Notify the HMI whenever the robot's internal state machine transitions. Sent on every state change, not periodically.

#### JSON structure

```json
{
  "type": "STATE",
  "payload": "<STATE_STRING>"
}
```

#### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"STATE"` |
| `payload` | string | ✅ | One of the four state values below. Any other string is a firmware bug. |

#### State values

| String | Firmware `State` enum | Meaning |
|---|---|---|
| `"IDLE"` | `STANDBY` | Robot is waiting (before countdown or after match) |
| `"SEARCH"` | `SEARCH` | Rotating and scanning — no opponent detected yet |
| `"ATTACK"` | `ATTACK` | Opponent detected, charging at full speed |
| `"DEFENSE"` | `EVADE` | Border line detected, recovery maneuver in progress |

#### Examples

```json
{ "type": "STATE", "payload": "IDLE" }
```

```json
{ "type": "STATE", "payload": "ATTACK" }
```

```json
{ "type": "STATE", "payload": "DEFENSE" }
```

---

### 4.3 `ACK` — Command acknowledgement

**Direction:** Robot → Pi  
**Purpose:** Confirm that a `CMD_MOTOR` or `CMD_ROBOT` command was received and will be executed. Sent immediately after parsing a valid command.

#### JSON structure

```json
{
  "type": "ACK",
  "payload": "<ECHOED_COMMAND>"
}
```

#### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"ACK"` |
| `payload` | string | ✅ | The value of the `action` or `command` field from the received command. This echo lets the HMI confirm which specific command was acknowledged. |

#### Examples

```json
{ "type": "ACK", "payload": "FORWARD" }
```

```json
{ "type": "ACK", "payload": "START" }
```

```json
{ "type": "ACK", "payload": "SET_STRATEGY" }
```

---

### 4.4 `LOG` — Debug / informational message

**Direction:** Robot → Pi  
**Purpose:** Freeform text message for debugging. Displayed in the HMI status bar. Should not be sent at high frequency during a match — only on significant events (state changes, errors, sensor init results).

#### JSON structure

```json
{
  "type": "LOG",
  "payload": "<MESSAGE_TEXT>"
}
```

#### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | ✅ | Always `"LOG"` |
| `payload` | string | ✅ | Human-readable message, max ~100 characters recommended. No structured data — for developer eyes only. |

#### Examples

```json
{ "type": "LOG", "payload": "Strategy switched to: Aggressive" }
```

```json
{ "type": "LOG", "payload": "Sensor init OK — Lidar, Line x4, Battery" }
```

```json
{ "type": "LOG", "payload": "WARN: Battery critical at 6.58V" }
```

---

## 5. Naming Conventions

| Convention | Rule | Example |
|---|---|---|
| Message types | SCREAMING_SNAKE_CASE | `CMD_MOTOR`, `CMD_ROBOT`, `TELEMETRY` |
| JSON object keys | camelCase | `frontLeft`, `critical`, `dist` |
| State strings | SCREAMING_SNAKE_CASE | `"IDLE"`, `"ATTACK"`, `"DEFENSE"` |
| Action/command strings | SCREAMING_SNAKE_CASE | `"FORWARD"`, `"SET_STRATEGY"` |
| Distance unit | metres (float) | `0.42` |
| Voltage unit | Volts (float) | `7.82` |
| Angle unit | degrees (float, 0–360°) | `12.5` |
| Float precision | 2 decimal places recommended | `7.82`, `0.42` |

---

## 6. Summary Table

| Type | Direction | Payload shape | Sent when | V1 status |
|---|---|---|---|---|
| `CMD_MOTOR` | Pi → Robot | object (`action`, `speed`) | User presses a direction button | **Required** |
| `CMD_ROBOT` | Pi → Robot | object (`command`, `param`) | User presses Start/Stop or changes strategy | **Required** |
| `TELEMETRY` | Robot → Pi | object (battery, line, lidar) | Periodic, every ~100–200 ms during match | **Required** |
| `STATE` | Robot → Pi | string | On every state machine transition | **Required** |
| `ACK` | Robot → Pi | string | After every valid command is received | **Required** |
| `LOG` | Robot → Pi | string | On significant events, errors, debug info | **Required** |

---

## 7. What Is Fixed in V1 vs What May Evolve

### Fixed for V1 (do not change without incrementing the version)

- The envelope structure: `{ "type": "...", "payload": ... }`
- All field names listed in sections 3 and 4
- All type, state, action, and command string values
- The four state strings: `"IDLE"`, `"SEARCH"`, `"ATTACK"`, `"DEFENSE"`
- The unit conventions (metres, Volts, degrees)
- The 4 line sensor keys: `frontLeft`, `frontRight`, `backLeft`, `back`
- The V1 telemetry scope: battery + line + lidar only

### May be added in V2 without breaking V1 parsers

V2 extensions add new optional objects inside the existing `TELEMETRY` payload. A V1 parser ignores unknown keys, so no V1 code breaks when V2 fields are added.

| Extension | How to add |
|---|---|
| TOF sensor data | Add `"tof": { "front": 0.0 }` inside `TELEMETRY` |
| Encoder data | Add `"encoders": { "leftRPM": 0.0, "rightRPM": 0.0 }` inside `TELEMETRY` |
| IMU data | Add `"imu": { "yaw": 0.0, "pitch": 0.0, "roll": 0.0 }` inside `TELEMETRY` once the MPU6050 driver class is implemented |
| Active strategy in state | Add a `"strategy": "AGGRESSIVE"` field alongside the state string payload |
| ACK latency | Add `"ts": 0` to the `ACK` payload to enable round-trip measurement |

### Items explicitly deferred to V2

- Any IMU / MPU6050 data — no driver class exists yet
- TOF sensor readings in telemetry — sensor works but no HMI panel targets it
- Encoder RPM and position in telemetry — no HMI panel targets them
- Match timer / countdown state
- A second TOF sensor (hardware not confirmed at a second position)

---

## 8. HMI Migration Notes

The existing `TelemetryParser.cs` and `SensorData.cs` were written against an earlier protocol draft. The following changes are required to align with V1.

1. **Remove `"ir"` parsing entirely.** The `ir` block no longer exists in the V1 `TELEMETRY` payload. Remove the `TryGetProperty("ir", ...)` block and the `IrFront`, `IrLeft`, `IrRight` properties from `SensorData.cs` and `TelemetryPanelViewModel.cs`. These will return as `"tof"` in V2.

2. **Add `"battery"` parsing.** Parse `payload.battery.voltage` (float), `payload.battery.percent` (int), and `payload.battery.critical` (bool). Add the corresponding properties to `SensorData.cs` and bind them in `TelemetryPanelViewModel.cs`.

3. **Update `"line"` parsing.** Replace the `"backRight"` key with `"back"`. Drop `LineBackRight`, add `LineBack`.

4. **Add `"lidar"` parsing.** Parse `payload.lidar.dist` (float), `payload.lidar.angle` (float), and `payload.lidar.valid` (bool). Add the corresponding properties to `SensorData.cs`.

5. **Update state string mapping** in `TelemetryParser.cs`:
   - `"SEARCHING"` → `"SEARCH"`
   - `"RETREATING"` → `"DEFENSE"`
   - Remove or keep `"RUNNING"` and `"ERROR"` mapped to `Unknown` as safe fallbacks for any future states.

6. **Update `CommandSerializer.cs`** to emit `"CMD_MOTOR"` and `"CMD_ROBOT"` as the `type` field instead of the old generic `"CMD"`.
