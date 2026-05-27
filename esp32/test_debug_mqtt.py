#!/usr/bin/env python3
"""Debug: affiche les 5 premiers messages MQTT bruts puis envoie la commande."""
import json, time
import paho.mqtt.client as mqtt

BROKER = "10.102.143.191"
CMD_TOPIC   = "robot/cmd"
TELEM_TOPIC = "robot/telemetry"

count = 0

def on_message(client, userdata, msg):
    global count
    count += 1
    if count <= 3:
        raw = msg.payload.decode()
        print(f"\n--- Message #{count} (topic={msg.topic}) ---")
        try:
            d = json.loads(raw)
            # Pretty print top-level keys and pose sub-key
            print(f"  type     = {d.get('type')}")
            pl = d.get("payload", {})
            print(f"  payload keys = {list(pl.keys())}")
            pose = pl.get("pose", "ABSENT")
            print(f"  pose     = {pose}")
            state_d = pl.get("state", "ABSENT")
            print(f"  state    = {state_d}")
        except Exception as e:
            print(f"  PARSE ERROR: {e}")
            print(f"  RAW (first 200 chars): {raw[:200]}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message
client.connect(BROKER, 1883, 60)
client.subscribe(TELEM_TOPIC)
client.loop_start()

print("Connexion MQTT, attente 3s de messages...")
time.sleep(3)
print(f"\nEnvoi commande strategy:Square ...")
client.publish(CMD_TOPIC, json.dumps({"cmd": "strategy", "value": "Square"}))
time.sleep(5)

client.loop_stop()
client.disconnect()
print(f"\nTotal messages recus: {count}")
