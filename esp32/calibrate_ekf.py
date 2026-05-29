#!/usr/bin/env python3
"""
calibrate_ekf.py  —  Calibration automatique des parametres EKF du minisumo
=============================================================================

3 phases executees automatiquement (robot pose sur surface plane) :

  Phase 1 — NOISE    (10 s, robot immobile)
    Mesure la variance residuelle du gyroscope apres soustraction du biais.
    → calcule rIMU optimal

  Phase 2 — SPIN     (25 s, rotation sur place)
    Compare l'angle EKF (fusion odo+IMU) a l'integral de gx (IMU seule).
    → calcule WHEELBASE_MM optimal
    → calcule qTheta optimal

  Phase 3 — STRAIGHT (6 s, ligne droite)
    Mesure la derive laterale (cross-track) et la derive de cap.
    → calcule qXY optimal
    → valide le deadband gyro

Commandes MQTT utilisees (protocole texte brut) :
  MOTOR:L:-60:R:60   rotation gauche
  MOTOR:L:80:R:80    ligne droite
  MOTOR:STOP         arret

Resultat : valeurs a copier dans RobotConstants.hpp et EKF.hpp
"""

import json, time, math, threading, statistics
import paho.mqtt.client as mqtt

# ─── Configuration ────────────────────────────────────────────────
BROKER           = "10.102.143.191"
CMD_TOPIC        = "robot/cmd"
TELEM_TOPIC      = "robot/telemetry"

WHEELBASE_OLD    = 92.66   # mm  (calibre par spin-test 2026-05-27: ratio EKF/IMU=1.188)
EKF_DT           = 0.005   # s   (200 Hz, periode taskEncoders)
SPIN_PWM         = 60      # PWM pour la rotation de calibration
STRAIGHT_PWM     = 80      # PWM pour la ligne droite
NOISE_DURATION   = 10.0    # s
SPIN_DURATION    = 25.0    # s
STRAIGHT_DURATION= 6.0     # s

# ─── Buffers de donnees (proteges par lock) ───────────────────────
lock    = threading.Lock()
samples = []   # liste de dicts {t, ts, gx, theta}
phase   = "WAIT"

def _append(sample):
    with lock:
        if phase in ("NOISE", "SPIN", "STRAIGHT"):
            samples.append(sample)

# ─── Callback MQTT ────────────────────────────────────────────────
last_ts    = None
theta_prev = None

def on_message(client, userdata, msg):
    global last_ts, theta_prev
    try:
        d = json.loads(msg.payload.decode())
    except Exception:
        return
    if d.get("type") != "TELEMETRY":
        return
    pl   = d.get("payload", {})
    pose = pl.get("pose", {})
    imu  = pl.get("imu",  {})

    ts    = pl.get("ts", 0)          # millis() sur ESP32
    gx    = imu.get("gx",    0.0)   # deg/s, biais deja soustrait
    theta = pose.get("theta", 0.0)  # rad, EKF fusionne
    x     = pose.get("x",    0.0)   # mm, EKF
    y     = pose.get("y",    0.0)   # mm, EKF

    t = time.time()
    _append({"t": t, "ts": ts, "gx": gx, "theta": theta, "x": x, "y": y})

# ─── Connexion MQTT ───────────────────────────────────────────────
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message
client.connect(BROKER, 1883, 60)
client.subscribe(TELEM_TOPIC)
client.loop_start()

def send(cmd: str):
    client.publish(CMD_TOPIC, cmd)

def wait_samples(duration: float):
    """Attend 'duration' secondes en collectant les echantillons."""
    time.sleep(duration)

def stop():
    send("MOTOR:STOP")
    time.sleep(0.3)

def get_samples():
    with lock:
        return list(samples)

def clear_samples():
    with lock:
        samples.clear()

# ─── Utilitaires ─────────────────────────────────────────────────
def unwrap_theta(thetas_rad):
    """Deplie la serie theta (rad) pour eviter les sauts +-pi."""
    if not thetas_rad:
        return []
    out = [thetas_rad[0]]
    for i in range(1, len(thetas_rad)):
        diff = thetas_rad[i] - thetas_rad[i-1]
        while diff >  math.pi: diff -= 2*math.pi
        while diff < -math.pi: diff += 2*math.pi
        out.append(out[-1] + diff)
    return out

def integrate_gx(s):
    """Integre gx (deg/s) * dt (s) -> angle total en degres (signe inclus)."""
    if len(s) < 2:
        return 0.0
    total = 0.0
    for i in range(1, len(s)):
        dt = (s[i]["ts"] - s[i-1]["ts"]) / 1000.0   # ms -> s
        if dt < 0 or dt > 0.5:                        # ignore sauts anormaux
            dt = 0.1
        total += s[i]["gx"] * dt
    return total

# ═══════════════════════════════════════════════════════════════════
print("\n" + "="*60)
print("  CALIBRATION EKF MINISUMO")
print("="*60)
print("Connexion MQTT en cours...")
time.sleep(2.0)
print("Connexion OK\n")

# ═══════════════════════════════════════════════════════════════════
#  PHASE 1 — BRUIT / rIMU
# ═══════════════════════════════════════════════════════════════════
print("─"*60)
print(f"PHASE 1 : BRUIT GYROSCOPE ({NOISE_DURATION:.0f}s, robot immobile)")
print("  NE PAS TOUCHER AU ROBOT...")
print("─"*60)

clear_samples()
with lock:
    phase = "NOISE"

wait_samples(NOISE_DURATION)

with lock:
    phase = "WAIT"

noise_s = get_samples()
clear_samples()

if len(noise_s) < 10:
    print("ERREUR: pas assez d'echantillons en phase bruit.")
else:
    gx_vals = [s["gx"] for s in noise_s]
    gx_mean = statistics.mean(gx_vals)
    gx_std  = statistics.stdev(gx_vals)       # deg/s (RMS bruit residuel)

    # Variance d'une mesure IMU = (sigma_gx_rad * dt)^2
    sigma_gx_rad = gx_std * (math.pi / 180.0)   # rad/s
    r_imu_opt    = (sigma_gx_rad * EKF_DT) ** 2  # rad^2

    print(f"  Echantillons      : {len(gx_vals)}")
    print(f"  gx moyen          : {gx_mean:+.4f} deg/s  (biais residuel)")
    print(f"  gx ecart-type     : {gx_std:.4f} deg/s   (bruit RMS)")
    print(f"  rIMU optimal      : {r_imu_opt:.2e} rad^2")
    print(f"  rIMU actuel       : 0.01 rad^2  (ratio: {0.01/r_imu_opt:.0f}x trop grand)")

    # Deadband optimal = 3*sigma + biais residuel
    deadband_opt = 3 * gx_std + abs(gx_mean)
    print(f"\n  Deadband optimal  : {deadband_opt:.3f} deg/s")
    print(f"  Deadband actuel   : 1.000 deg/s")

    RIMI_RESULT    = r_imu_opt
    DEADBAND_RESULT = deadband_opt
    GX_MEAN_RESULT  = gx_mean

# ═══════════════════════════════════════════════════════════════════
#  PHASE 2 — ROTATION / WHEELBASE + qTheta
# ═══════════════════════════════════════════════════════════════════
print()
print("─"*60)
print(f"PHASE 2 : ROTATION ({SPIN_DURATION:.0f}s, rotation gauche PWM={SPIN_PWM})")
print("  Demarrage dans 3s...")
print("─"*60)
time.sleep(3.0)

with lock:
    phase = "SPIN"

send(f"MOTOR:L:-{SPIN_PWM}:R:{SPIN_PWM}")
wait_samples(SPIN_DURATION)
stop()

with lock:
    phase = "WAIT"

spin_s = get_samples()
clear_samples()

if len(spin_s) < 20:
    print("ERREUR: pas assez d'echantillons en phase spin.")
else:
    thetas_rad   = [s["theta"] for s in spin_s]
    thetas_unwrap = unwrap_theta(thetas_rad)                   # rad, deplie
    theta_ekf_total = math.degrees(thetas_unwrap[-1] - thetas_unwrap[0])  # deg

    theta_imu_total = integrate_gx(spin_s)                    # deg (integral gx*dt)

    print(f"  Echantillons         : {len(spin_s)}")
    print(f"  Angle EKF total      : {theta_ekf_total:.2f} deg")
    print(f"  Angle IMU integre    : {theta_imu_total:.2f} deg")

    if abs(theta_imu_total) < 10.0:
        print("  ATTENTION: rotation trop faible, moteurs non actifs?")
        WHEELBASE_RESULT = WHEELBASE_OLD
    else:
        ratio = theta_ekf_total / theta_imu_total
        wheelbase_new = WHEELBASE_OLD * ratio

        print(f"\n  Ratio EKF/IMU        : {ratio:.4f}")
        print(f"  WHEELBASE actuel     : {WHEELBASE_OLD:.1f} mm")
        print(f"  WHEELBASE optimal    : {wheelbase_new:.2f} mm")

        if abs(ratio - 1.0) < 0.01:
            print("  -> Deja bien calibre (erreur < 1%)")
        elif abs(ratio - 1.0) < 0.05:
            print(f"  -> Correction mineure ({(ratio-1)*100:+.1f}%)")
        else:
            print(f"  -> Correction significative ({(ratio-1)*100:+.1f}%)")

        WHEELBASE_RESULT = wheelbase_new

    # qTheta : variance de la derive theta normalisee par le deplacement angulaire
    # On prend la deviation entre theta_ekf et theta_imu echantillon par echantillon
    thetas_imu_running = [math.degrees(thetas_unwrap[0])]
    for i in range(1, len(spin_s)):
        dt = (spin_s[i]["ts"] - spin_s[i-1]["ts"]) / 1000.0
        if dt < 0 or dt > 0.5: dt = 0.1
        thetas_imu_running.append(thetas_imu_running[-1] + spin_s[i]["gx"] * dt)

    deviations = [math.degrees(thetas_unwrap[i]) - thetas_imu_running[i]
                  for i in range(len(spin_s))]

    if len(deviations) > 5:
        dev_std = statistics.stdev(deviations)     # deg
        dev_std_rad = dev_std * math.pi / 180.0    # rad
        # Par radian tourne : qTheta = dev_std_rad^2 / |theta_total_rad|
        theta_total_rad = abs(math.radians(theta_ekf_total))
        q_theta_raw = (dev_std_rad ** 2) / max(theta_total_rad, 0.1)

        # Correction du biais K : avec rIMU=1e-4 l'EKF fait K≈0.85 (IMU dominant).
        # L'ecart EKF-IMU observe = (1-K) * vrai_bruit_odo → vrai_bruit ≈ ecart/(1-K).
        # Le rIMU calibre en phase 1 permet d'estimer K.
        try:
            # P_ss approximee : racine de (qTheta_step * rIMU), qTheta_step = q_theta_raw * dTheta_moy
            dTheta_moy = abs(math.radians(theta_ekf_total)) / max(len(spin_s), 1)
            P_ss_approx = math.sqrt(max(q_theta_raw * dTheta_moy * RIMI_RESULT, 1e-20))
            K_approx    = P_ss_approx / (P_ss_approx + RIMI_RESULT)
            K_approx    = max(min(K_approx, 0.99), 0.01)
            correction  = 1.0 / max((1.0 - K_approx) ** 2, 0.01)
        except Exception:
            K_approx   = 0.85
            correction = 44.4   # 1/(1-0.85)^2

        q_theta_opt = q_theta_raw * correction

        print(f"\n  Ecart EKF-IMU (std)  : {dev_std:.3f} deg")
        print(f"  K Kalman estime      : {K_approx:.2f}  (IMU{'dominant' if K_approx > 0.5 else 'faible'})")
        print(f"  qTheta brut (biaise) : {q_theta_raw:.6f} rad^2/rad")
        print(f"  qTheta corrige x{correction:.0f}   : {q_theta_opt:.4f} rad^2/rad")
        print(f"  qTheta actuel        : 0.1 rad^2/rad")
        QTHETA_RESULT = q_theta_opt

# ═══════════════════════════════════════════════════════════════════
#  PHASE 3 — LIGNE DROITE / qXY
# ═══════════════════════════════════════════════════════════════════
print()
print("─"*60)
print(f"PHASE 3 : LIGNE DROITE ({STRAIGHT_DURATION:.0f}s, PWM={STRAIGHT_PWM})")
print("  Assure-toi qu'il y a assez d'espace (>60cm devant le robot)")
print("  Demarrage dans 3s...")
print("─"*60)
time.sleep(3.0)

# Remet la pose a zero avant le test
send("POSE:RESET")
time.sleep(0.5)

with lock:
    phase = "STRAIGHT"

send(f"MOTOR:L:{STRAIGHT_PWM}:R:{STRAIGHT_PWM}")
wait_samples(STRAIGHT_DURATION)
stop()

with lock:
    phase = "WAIT"

str_s = get_samples()
clear_samples()

if len(str_s) < 10:
    print("ERREUR: pas assez d'echantillons en phase ligne droite.")
else:
    thetas_str    = [s["theta"] for s in str_s]
    thetas_deg    = [math.degrees(t) for t in thetas_str]
    heading_drift = thetas_deg[-1] - thetas_deg[0]

    print(f"  Echantillons         : {len(str_s)}")
    print(f"  Derive de cap        : {heading_drift:.2f} deg / {STRAIGHT_DURATION:.0f}s")
    print(f"  Derive cap / seconde : {heading_drift/STRAIGHT_DURATION:.3f} deg/s")

    gx_during_straight = [s["gx"] for s in str_s]
    gx_straight_mean = statistics.mean(gx_during_straight) if gx_during_straight else 0
    print(f"  gx moyen (droit)     : {gx_straight_mean:+.3f} deg/s")

    if abs(heading_drift) < 1.0:
        print("  -> Cap stable, deadband actuel adequat")
    else:
        print(f"  -> Derive cap de {heading_drift:.1f}deg : verifier WHEELBASE et deadband gyro")

    # --- Calibration qXY via déviation cross-track ------------------
    # Apres POSE:RESET, le robot part de (0,0,theta0).
    # L'axe avant = cos(theta0), sin(theta0).
    # La déviation latérale (perpendiculaire) mesure l'erreur de position EKF.
    x0      = str_s[0]["x"]
    y0      = str_s[0]["y"]
    theta0  = str_s[0]["theta"]
    cos_h   = math.cos(theta0)
    sin_h   = math.sin(theta0)

    cross_tracks = []
    fwd_dists    = []
    for s in str_s:
        dx = s["x"] - x0
        dy = s["y"] - y0
        fwd_dists.append( dx * cos_h + dy * sin_h)   # mm, le long de l'axe avant
        cross_tracks.append(-dx * sin_h + dy * cos_h) # mm, perpendiculaire

    total_dist = fwd_dists[-1] if fwd_dists[-1] > 10 else 1.0

    print()
    print(f"  Distance parcourue   : {total_dist:.1f} mm")
    print(f"  Cross-track max      : {max(abs(c) for c in cross_tracks):.2f} mm")

    if total_dist < 50:
        print("  ATTENTION: robot peu bouge (<50mm) — verifier moteurs et POSE:RESET")
        QXY_RESULT = 0.5
    else:
        ct_var    = statistics.variance(cross_tracks) if len(cross_tracks) > 1 else 0.0
        q_xy_opt  = ct_var / total_dist
        print(f"  Variance cross-track : {ct_var:.4f} mm^2")
        print(f"  qXY optimal          : {q_xy_opt:.5f} mm^2/mm")
        print(f"  qXY actuel           : 0.5 mm^2/mm")
        if q_xy_opt < 0.001:
            print("  -> qXY tres faible : EKF tres confiant en odometrie (sol lisse)")
        elif q_xy_opt < 0.1:
            print("  -> qXY faible : bonne adhesion, peu de glissement lateral")
        elif q_xy_opt < 0.5:
            print("  -> qXY modere : valeur raisonnable")
        else:
            print("  -> qXY eleve : fort glissement lateral ou surface irreguliere")
        QXY_RESULT = q_xy_opt

client.loop_stop()
client.disconnect()

# ═══════════════════════════════════════════════════════════════════
#  RESUME FINAL — VALEURS A METTRE A JOUR
# ═══════════════════════════════════════════════════════════════════
print()
print("="*60)
print("  RESUME — COPIER CES VALEURS DANS LE CODE")
print("="*60)

try:
    print(f"\n  RobotConstants.hpp:")
    print(f"    WHEELBASE_MM = {WHEELBASE_RESULT:.2f}f   // etait {WHEELBASE_OLD}f")

    print(f"\n  EKF.hpp (valeurs par defaut du constructeur):")
    print(f"    _rIMU   = {RIMI_RESULT:.2e}f  // etait 0.01f")
    print(f"    _qTheta = {QTHETA_RESULT:.4f}f     // etait 0.1f  (corrige biais K)")
    try:
        print(f"    _qXY    = {QXY_RESULT:.5f}f   // etait 0.5f  (calibre cross-track)")
    except NameError:
        print(f"    _qXY    = 0.5f          // non calibre (phase 3 incomplete)")

    print(f"\n  Tasks.hpp:")
    print(f"    GYRO_EKF_DEADBAND_RAD = {DEADBAND_RESULT:.3f}f * DEG_TO_RAD  // etait 1.000f")

except NameError as e:
    print(f"  Certaines phases ont echoue : {e}")

print("="*60)
