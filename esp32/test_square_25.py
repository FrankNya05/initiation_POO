#!/usr/bin/env python3
"""Test TURN_SLOW=25 -- observe square quality via MQTT telemetry.
   Protocol: plain-text commands (STRATEGY:Square), JSON telemetry.
"""
import json, time, math, threading
import paho.mqtt.client as mqtt

BROKER = "10.102.143.191"
CMD_TOPIC   = "robot/cmd"
TELEM_TOPIC = "robot/telemetry"

poses = []          # (t, x, y, theta)
state_log = []      # (t, state)
current_state = "?"
lock = threading.Lock()
start_time = None
square_started = False
square_done   = False

def on_message(client, userdata, msg):
    global current_state, square_done
    try:
        d = json.loads(msg.payload.decode())
    except Exception:
        return

    msg_type = d.get("type", "")
    payload  = d.get("payload", {})

    if msg_type == "STATE":
        st = payload.get("state", "?")
        with lock:
            if st != current_state:
                current_state = st
                if square_started:
                    state_log.append((time.time() - start_time, st))
            # Square done when robot goes back to IDLE after running
            if square_started and st == "IDLE" and len(poses) > 10:
                square_done = True

    elif msg_type == "TELEMETRY":
        pose_d = payload.get("pose", {})
        x      = pose_d.get("x",     0.0)
        y      = pose_d.get("y",     0.0)
        theta  = pose_d.get("theta", 0.0)
        with lock:
            if square_started and not square_done:
                poses.append((time.time() - start_time, x, y, theta))

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message
client.connect(BROKER, 1883, 60)
client.subscribe(TELEM_TOPIC)
client.loop_start()

print("Connexion MQTT, attente 2s...")
time.sleep(2)

# Reset EKF pose to (0,0,0) — critical if previous test left accumulated position
print("Envoi POSE:RESET ...")
client.publish(CMD_TOPIC, "POSE:RESET")
time.sleep(0.5)   # laisser l'EKF se stabiliser

# Send Square command — plain text protocol: STRATEGY:Square
print("Envoi STRATEGY:Square ...")
client.publish(CMD_TOPIC, "STRATEGY:Square")
time.sleep(0.1)
start_time = time.time()
square_started = True

print("Telemetrie en cours (max 45s)...")
for _ in range(450):
    if square_done:
        break
    time.sleep(0.1)

client.loop_stop()
client.disconnect()

if not poses:
    print("ERREUR: aucune telemetrie pose recue.")
    exit(1)

# Check if pose ever moved
max_dist = max(math.sqrt(p[1]**2 + p[2]**2) for p in poses)
if max_dist < 5.0:
    print(f"ATTENTION: pose n'a jamais depasse 5mm (max={max_dist:.1f}mm).")
    print("  -> robot probablement en STANDBY / commande non recue")
    exit(1)

print(f"\n=== Resultats TURN_SLOW=25 ===")
print(f"Echantillons telemetrie : {len(poses)}")

x0, y0 = poses[0][1], poses[0][2]
xf, yf = poses[-1][1], poses[-1][2]
tf_deg = math.degrees(poses[-1][3])
duration = poses[-1][0]

closure = math.sqrt((xf - x0)**2 + (yf - y0)**2)
print(f"Duree totale            : {duration:.1f} s")
print(f"Pose initiale           : x={x0:.1f}mm, y={y0:.1f}mm")
print(f"Pose finale             : x={xf:.1f}mm, y={yf:.1f}mm, theta={tf_deg:.1f}deg")
print(f"Erreur fermeture        : {closure:.1f}mm")
print(f"Erreur angulaire finale : {tf_deg:.1f}deg (ideal~0deg)")

if state_log:
    print("\nTransitions d'etat :")
    for t, st in state_log:
        print(f"  t={t:5.1f}s  -> {st}")

print(f"\nTrajectoire (chaque seconde environ) :")
step = max(1, len(poses) // 20)
for t, x, y, th in poses[::step]:
    print(f"  t={t:5.1f}s  x={x:6.1f}mm  y={y:6.1f}mm  theta={math.degrees(th):6.1f}deg")

print(f"\nConclusion:")
if closure < 15 and abs(tf_deg) < 8:
    print("  EXCELLENT: carre tres ferme (<15mm, <8deg)")
elif closure < 25:
    print("  BON: fermeture acceptable (<25mm)")
elif closure < 40:
    print("  MOYEN: fermeture a ameliorer")
else:
    print("  MAUVAIS: forme trop loin d'un carre")
