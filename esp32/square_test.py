import paho.mqtt.client as mqtt
import json, time, threading, math

BROKER     = "10.102.143.191"
PORT       = 1883
PUB        = "robot/cmd"
SUB        = "robot/telemetry"
SIDE_MM    = 450.0   # cible reduite pour compenser overshoot (~50mm a PWM150)
FWD_SPEED  = 150
TURN_FAST  = 60
TURN_SLOW  = 30
TURN_SLOW_DEG = 30   # commence a ralentir plus tot
TURN_TOL   = 3.0

pose = {"x": 0.0, "y": 0.0, "theta": 0.0}
lock = threading.Lock()
updated = threading.Event()

def on_message(client, userdata, msg):
    try:
        d = json.loads(msg.payload)
        p = d["payload"]["pose"]
        with lock:
            pose["x"]     = p["x"]
            pose["y"]     = p["y"]
            pose["theta"] = p["theta"]
        updated.set()
    except Exception:
        pass

def get_pose():
    with lock:
        return pose["x"], pose["y"], pose["theta"]

def norm(a):
    while a >  math.pi: a -= 2*math.pi
    while a < -math.pi: a += 2*math.pi
    return a

def wait_pose(timeout=0.25):
    updated.wait(timeout=timeout)
    updated.clear()
    return get_pose()

def stop():
    c.publish(PUB, "MOTOR:STOP")

def drive(l, r):
    c.publish(PUB, "MOTOR:L:%d:R:%d" % (l, r))

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
c.on_message = on_message
c.connect(BROKER, PORT, 60)
c.subscribe(SUB)
c.loop_start()
time.sleep(0.5)

print("==> POSE:RESET")
c.publish(PUB, "POSE:RESET")
time.sleep(1.0)

x0, y0, t0 = get_pose()
print("Depart: x=%.0f y=%.0f theta=%.1fdeg" % (x0, y0, math.degrees(t0)))
print("Carre 50cm x 50cm\n")

for side in range(4):
    sx, sy, stheta = get_pose()
    print("--- Cote %d ---" % (side + 1))

    # --- Avancer ---
    drive(FWD_SPEED, FWD_SPEED)
    last_dist = 0.0
    last_progress = time.time()
    while True:
        x, y, theta = wait_pose()
        dist = math.sqrt((x - sx)**2 + (y - sy)**2)
        if dist > last_dist + 2.0:
            last_dist = dist
            last_progress = time.time()
        if time.time() - last_progress > 1.5:
            print("  !! BLOQUE — arret urgence")
            break
        if dist >= SIDE_MM:
            break
        print("  fwd %.0fmm" % dist)
    stop()
    time.sleep(0.5)
    x, y, theta = get_pose()
    dist = math.sqrt((x - sx)**2 + (y - sy)**2)
    print("  -> Arret : %.0fmm  theta=%.1fdeg" % (dist, math.degrees(theta)))

    if side < 3:
        # --- Tourner 90deg a gauche (bidirectionnel) ---
        # Cible absolue basee sur le cap initial t0 (pas stheta) pour eviter l'accumulation d'erreur
        target = norm(t0 + (side + 1) * math.pi / 2)
        print("  Virage -> cible=%.1fdeg" % math.degrees(target))

        while True:
            x, y, theta = wait_pose()
            err_rad = norm(target - theta)
            err_deg = math.degrees(err_rad)

            if abs(err_deg) <= TURN_TOL:
                stop()
                break

            spd = TURN_FAST if abs(err_deg) > TURN_SLOW_DEG else TURN_SLOW
            if err_deg > 0:
                drive(-spd, spd)    # tourner gauche
            else:
                drive(spd, -spd)    # tourner droite (correction depassement)

            print("  turn %.1fdeg  err=%.1fdeg" % (math.degrees(theta), err_deg))

        time.sleep(0.4)
        x, y, theta = get_pose()
        print("  -> OK: theta=%.1fdeg (cible=%.1fdeg)\n" % (math.degrees(theta), math.degrees(target)))

x, y, theta = get_pose()
print("=== Fin ===")
print("Pose finale   : x=%.0f y=%.0f theta=%.1fdeg" % (x, y, math.degrees(theta)))
print("Erreur retour : dx=%.0fmm  dy=%.0fmm" % (x - x0, y - y0))
c.loop_stop()
c.disconnect()
