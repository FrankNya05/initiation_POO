#pragma once
#include "RobotContext.hpp"
#include "RobotConstants.hpp"
#include "DriverManager.hpp"
#include "DriverLedRGB.hpp"
#include "SensorManger.hpp"
#include "StrategyManager.hpp"
#include "CommunicationManager.hpp"
#include "CommandParser.hpp"
#include "Queues.hpp"
#include "Encoder.hpp"
#include "EKF.hpp"
#include "PID.hpp"

// ═══════════════════════════════════════════════════════════════
//  Paramètres passés à taskEncoders (intègre EKF + PID vitesse)
// ═══════════════════════════════════════════════════════════════
struct EKFParams {
    EKF*            ekf;
    Encoder*        left;
    Encoder*        right;
    DriverManager*  driver;
    PID*            pidLeft;
    PID*            pidRight;
    volatile int    lastPwmLeft  = 0;  // debug — dernière sortie PID gauche
    volatile int    lastPwmRight = 0;  // debug — dernière sortie PID droit
};

// ───────────────────────────────────────────────────────────────
//  Paramètres de taskLed
// ───────────────────────────────────────────────────────────────
struct LedTaskParams {
    DriverLedRGB*    led;
    StrategyManager* manager;
};

// ───────────────────────────────────────────────────────────────
//  ledStartup — séquence visuelle de démarrage (5 s)
//  Rouge → Vert → Bleu, ~1667 ms chacun.
//  À appeler au début de chaque tâche, avant son for(;;).
// ───────────────────────────────────────────────────────────────
static inline void ledStartup(DriverLedRGB* led) {
    constexpr TickType_t STEP = pdMS_TO_TICKS(5000 / 3);
    led->setColor(LedColor::RED);   vTaskDelay(STEP);
    led->setColor(LedColor::GREEN); vTaskDelay(STEP);
    led->setColor(LedColor::BLUE);  vTaskDelay(STEP);
    led->off();
}

// ───────────────────────────────────────────────────────────────
//  taskLed — 10 Hz, Core 0, Prio 0
//  STANDBY → YELLOW
//  Actif   → couleur de la stratégie courante
// ───────────────────────────────────────────────────────────────
void taskLed(void* pvParameters)
{
    LedTaskParams* p = static_cast<LedTaskParams*>(pvParameters);
    ledStartup(p->led);

    for (;;)
    {
        if (RobotContext::instance().getState() == RobotConstants::State::STANDBY) {
            p->led->setColor(LedColor::YELLOW);
        } else {
            p->led->setColor(p->manager->currentColor());
        }
        vTaskDelay(RTOSConfig::PERIOD_LED);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskMotors — 100 Hz, Core 1, Prio 4
//  Lit la commande dans RobotContext et l'applique aux moteurs
// ───────────────────────────────────────────────────────────────
void taskMotors(void* pvParameters)
{
    DriverManager* devices = static_cast<DriverManager*>(pvParameters);
    for (;;)
    {
        RobotConstants::ActionCommand cmd = RobotContext::instance().getMotorSpeeds();
        devices->setSpeedAll(cmd.leftSpeed, cmd.rightSpeed);
        vTaskDelay(RTOSConfig::PERIOD_MOTORS);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskSensors — 50 Hz, Core 1, Prio 3
//  Met à jour tous les capteurs et publie dans RobotContext
// ───────────────────────────────────────────────────────────────
void taskSensors(void* pvParameters)
{
    SensorManager* sensors = static_cast<SensorManager*>(pvParameters);

    for (;;)
    {
        sensors->updateAll();

        auto& ctx = RobotContext::instance();

        // Lidar (FRONT) — distance + angle de l'adversaire
        ctx.setLidarData(
            sensors->getDataByPosition(SensorPosition::FRONT)
        );

        // TOF — avant-gauche et avant-droit (filtre par type pour éviter
        // la collision avec les LineSensors à la même position)
        ctx.setTOFData(SensorPosition::FRONT_LEFT,
            sensors->getDataByPositionAndType(SensorPosition::FRONT_LEFT,  "TOF")
        );
        ctx.setTOFData(SensorPosition::FRONT_RIGHT,
            sensors->getDataByPositionAndType(SensorPosition::FRONT_RIGHT, "TOF")
        );

        // LineSensors — avant-gauche, avant-droit, arrière
        ctx.setLineData(SensorPosition::FRONT_LEFT,
            sensors->getDataByPositionAndType(SensorPosition::FRONT_LEFT,  "LINE")
        );
        ctx.setLineData(SensorPosition::FRONT_RIGHT,
            sensors->getDataByPositionAndType(SensorPosition::FRONT_RIGHT, "LINE")
        );
        ctx.setLineData(SensorPosition::BACK,
            sensors->getDataByPosition(SensorPosition::BACK)
        );

        // IMU (CENTER) — ax/ay/az + gx/gy/gz
        ctx.setIMUData(
            sensors->getDataByPosition(SensorPosition::CENTER)
        );

        // Bouton start
        ctx.setSwitchData(
            sensors->getDataByPosition(SensorPosition::UNKNOWN)
        );

        // Batterie
        ctx.setBattData(
            sensors->getDataByPosition(SensorPosition::BATTERY)
        );

        vTaskDelay(RTOSConfig::PERIOD_SENSORS);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskStrategy — 20 Hz, Core 1, Prio 2
//  Lit RobotContext, gère le bouton start, exécute la stratégie
// ───────────────────────────────────────────────────────────────
void taskStrategy(void* pvParameters)
{
    StrategyManager* manager = static_cast<StrategyManager*>(pvParameters);

    for (;;)
    {
        auto& ctx   = RobotContext::instance();
        auto  state = ctx.getState();

        // ── Gestion bouton start ──────────────────────────────
        SensorData sw = ctx.getSwitchData();
        if (sw.isValid) {
            PressType press = static_cast<PressType>(
                                static_cast<uint8_t>(sw.value.scalar));

            // Appui court en STANDBY → délai 5s réglementaire puis démarrage
            if (press == PressType::SHORT &&
                state  == RobotConstants::State::STANDBY) {
                LOG("[Strategy] Départ dans 5 secondes...");
                ctx.setMotorSpeeds({});           // moteurs arrêtés pendant l'attente
                vTaskDelay(pdMS_TO_TICKS(5000));  // délai réglementaire
                ctx.setState(RobotConstants::State::SEARCH);
                LOG("[Strategy] GO !");
            }

            // Appui long → arrêt d'urgence
            if (press == PressType::LONG) {
                LOG("[Strategy] Arrêt d'urgence");
                ctx.setState(RobotConstants::State::STANDBY);
                ctx.setMotorSpeeds({});
            }

            // Double appui rapide → cycle de stratégie
            if (press == PressType::RAPID &&
                state != RobotConstants::State::STANDBY) {
                manager->nextStrategy();
                LOGF("[Strategy] Stratégie : %s\n", manager->currentName());
            }
        }

        // ── Protection batterie ───────────────────────────────
        SensorData batt = ctx.getBattData();
        if (batt.isValid && batt.value.scalar <= 6.20f &&
            state != RobotConstants::State::STANDBY) {
            LOG("[Strategy] BATTERIE CRITIQUE → STANDBY force");
            ctx.setState(RobotConstants::State::STANDBY);
            ctx.setMotorSpeeds({});
            vTaskDelay(RTOSConfig::PERIOD_STRATEGY);
            continue;
        }

        // ── Exécution stratégie (seulement hors STANDBY) ─────
        if (state != RobotConstants::State::STANDBY) {
            RobotConstants::ActionCommand cmd = manager->executeStrategy(ctx);
            ctx.setMotorSpeeds(cmd);
        }

        vTaskDelay(RTOSConfig::PERIOD_STRATEGY);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskComm — 10 Hz, Core 0, Prio 1
//  Gère BLE/UART : envoie la telémétrie, reçoit les commandes
// ───────────────────────────────────────────────────────────────
void taskComm(void* pvParameters)
{
    CommunicationManager* comm = static_cast<CommunicationManager*>(pvParameters);

    comm->onReceive([](const std::string& msg) {
        RobotQueues::sendCommand(msg.c_str());
    });

    comm->begin();  // connexion WiFi + MQTT (bloquant jusqu'à connexion)

    for (;;)
    {
        // 1. Traiter les messages entrants + autoSelect BLE/UART
        comm->update();

        // 2. Envoyer la télémétrie si connecté
        if (comm->isConnected()) {
            auto& ctx        = RobotContext::instance();
            EKF::Pose pose   = ctx.getPose();
            SensorData batt  = ctx.getBattData();
            SensorData lidar = ctx.getLidarData();

            // Lidar — adversaire
            float enemyDist  = lidar.isValid ? lidar.value.vector.x : -1.0f;
            float enemyAngle = lidar.isValid ? lidar.value.vector.y : -1.0f;

            // Capteurs de ligne : 1=bord, 0=ok, -1=invalide
            SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
            SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
            SensorData lineB  = ctx.getLineData(SensorPosition::BACK);
            auto lineVal = [](const SensorData& d) -> int {
                if (!d.isValid) return -1;
                return d.value.scalar > 0.0f ? 1 : 0;
            };

            // TOF
            SensorData tofFL = ctx.getTOFData(SensorPosition::FRONT_LEFT);
            SensorData tofFR = ctx.getTOFData(SensorPosition::FRONT_RIGHT);

            // IMU
            SensorData imu = ctx.getIMUData();
            float ax = imu.isValid ? imu.value.imu.ax : 0.0f;
            float ay = imu.isValid ? imu.value.imu.ay : 0.0f;
            float az = imu.isValid ? imu.value.imu.az : 0.0f;
            float gx = imu.isValid ? imu.value.imu.gx : 0.0f;
            float gy = imu.isValid ? imu.value.imu.gy : 0.0f;
            float gz = imu.isValid ? imu.value.imu.gz : 0.0f;

            // Encodeurs
            RobotContext::EncoderData encL = ctx.getEncoderData(SensorPosition::LEFT);
            RobotContext::EncoderData encR = ctx.getEncoderData(SensorPosition::RIGHT);

            // Commande moteur
            RobotConstants::ActionCommand cmd = ctx.getMotorSpeeds();

            // Incertitude EKF
            // (stockée dans EKF directement — accès via pose uniquement ici)

            // ── Calculs complémentaires pour le protocole V1 ──────────
            // Batterie : percent et critical calculés localement
            // (BattSensor::getPercent/isCritical ne sont pas dans RobotContext)
            float  batVoltage  = batt.isValid ? batt.value.scalar : 0.0f;
            int    batPercent  = batt.isValid
                                 ? (int)((batVoltage - 6.0f) / (8.4f - 6.0f) * 100.0f)
                                 : 0;
            // Clamping 0-100
            if (batPercent < 0)   batPercent = 0;
            if (batPercent > 100) batPercent = 100;
            bool batCritical = batt.isValid && (batVoltage <= 6.60f);
 
            // Lidar : validité explicite (le HMI attend un bool "valid")
            bool lidarValid = lidar.isValid && (lidar.value.vector.x > 0.0f);
            float lidarDist  = lidarValid ? lidar.value.vector.x : 0.0f;
            float lidarAngle = lidarValid ? lidar.value.vector.y : 0.0f;
 
            // Capteurs de ligne : conversion int (0/1/-1) → bool
            // lineVal() existe déjà dans la tâche, on l'utilise directement
            bool lineFLbool = (lineVal(lineFL) == 1);
            bool lineFRbool = (lineVal(lineFR) == 1);
            bool lineBbool  = (lineVal(lineB)  == 1);

            // TOF
            float tofFLmm = tofFL.isValid ? tofFL.value.scalar : -1.0f;
            float tofFRmm = tofFR.isValid ? tofFR.value.scalar : -1.0f;

            char buf[640];
            snprintf(buf, sizeof(buf),
                "{"
                    "\"type\":\"TELEMETRY\","
                    "\"payload\":{"
                        "\"ts\":%lu,"
                        "\"battery\":{"
                            "\"voltage\":%.2f,"
                            "\"percent\":%d,"
                            "\"critical\":%s"
                        "},"
                        "\"line\":{"
                            "\"frontLeft\":%s,"
                            "\"frontRight\":%s,"
                            "\"backLeft\":false,"
                            "\"back\":%s"
                        "},"
                        "\"tof\":{"
                            "\"frontLeft\":%.0f,"
                            "\"frontRight\":%.0f"
                        "},"
                        "\"lidar\":{"
                            "\"dist\":%.3f,"
                            "\"angle\":%.1f,"
                            "\"valid\":%s"
                        "},"
                        "\"imu\":{"
                            "\"ax\":%.3f,"
                            "\"ay\":%.3f,"
                            "\"az\":%.3f,"
                            "\"gx\":%.3f,"
                            "\"gy\":%.3f,"
                            "\"gz\":%.3f,"
                            "\"valid\":%s"
                        "},"
                        "\"motors\":{"
                            "\"left\":%d,"
                            "\"right\":%d"
                        "}"
                    "}"
                "}",
                (unsigned long)millis(),
                batVoltage,
                batPercent,
                batCritical  ? "true" : "false",
                lineFLbool   ? "true" : "false",
                lineFRbool   ? "true" : "false",
                lineBbool    ? "true" : "false",
                tofFLmm,
                tofFRmm,
                lidarDist,
                lidarAngle,
                lidarValid   ? "true" : "false",
                ax, ay, az,
                gx, gy, gz,
                imu.isValid  ? "true" : "false",
                cmd.leftSpeed, cmd.rightSpeed
            );
            comm->send(std::string(buf));
        }

        vTaskDelay(RTOSConfig::PERIOD_COMM);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskEncoders — 200 Hz, Core 1, Prio 5
//  Lit les impulsions, calcule RPM, met à jour l'EKF et publie
//  dans RobotContext. Intègre taskEKF pour éviter un double
//  getAndReset() sur les mêmes encodeurs.
// ───────────────────────────────────────────────────────────────
void taskEncoders(void* pvParameters)
{
    static constexpr float SPEED_MAX_RPM = 160.0f; // vitesse maxi mesurée ~130 RPM à 200/255 PWM

    EKFParams* p = static_cast<EKFParams*>(pvParameters);

    TickType_t lastWake = xTaskGetTickCount();
    uint32_t   lastMs   = millis();

    for (;;)
    {
        const float deltaMs = RTOSConfig::PERIOD_ENCODER * portTICK_PERIOD_MS;

        // 1. Lire et remettre à zéro atomiquement (une seule fois)
        int32_t pulsesLeft  =  p->left->getAndReset();
        int32_t pulsesRight = -p->right->getAndReset();  // encodeur droit câblé inversé

        // 2. Calculer RPM et publier dans RobotContext
        RobotContext::EncoderData left, right;

        left.rpm        = p->left->getRPM(pulsesLeft, deltaMs);
        left.totalCount = p->left->getTotalCount();
        left.isValid    = true;

        right.rpm        = p->right->getRPM(pulsesRight, deltaMs);
        right.totalCount = p->right->getTotalCount();
        right.isValid    = true;

        auto& ctx = RobotContext::instance();
        ctx.setEncoderData(SensorPosition::LEFT,  left);
        ctx.setEncoderData(SensorPosition::RIGHT, right);

        // 3. Prédiction EKF odométrie
        p->ekf->predict(pulsesLeft, pulsesRight);

        // 4. Correction EKF gyroscope
        SensorData imuData = ctx.getIMUData();
        if (imuData.isValid) {
            uint32_t now = millis();
            float dtSec  = (now - lastMs) / 1000.0f;
            lastMs       = now;
            p->ekf->updateIMU(imuData.value.imu.gx * DEG_TO_RAD, dtSec);
        }

        // 5. Publier la pose estimée
        ctx.setPose(p->ekf->getPose());

        // 6. Asservissement de vitesse — feedforward + correction PID ──
        RobotConstants::ActionCommand cmd = ctx.getMotorSpeeds();

        // Feedforward + correction PID pour égaliser les vitesses
        static RobotConstants::ActionCommand prevCmd = {0, 0};
        if (cmd.leftSpeed != prevCmd.leftSpeed || cmd.rightSpeed != prevCmd.rightSpeed) {
            p->pidLeft->reset();
            p->pidRight->reset();
            prevCmd = cmd;
        }

        if (cmd.isStop()) {
            p->driver->setSpeedAll(0, 0);
            p->pidLeft->reset();
            p->pidRight->reset();
        } else {
            float targetL = (cmd.leftSpeed  / 255.0f) * SPEED_MAX_RPM;
            float targetR = (cmd.rightSpeed / 255.0f) * SPEED_MAX_RPM;

            p->pidLeft->setSetpoint(targetL);
            p->pidRight->setSetpoint(targetR);

            int pwmL = cmd.leftSpeed  + (int)p->pidLeft->compute(left.rpm);
            int pwmR = cmd.rightSpeed + (int)p->pidRight->compute(right.rpm);
            pwmL = constrain(pwmL, -255, 255);
            pwmR = constrain(pwmR, -255, 255);

            p->lastPwmLeft  = pwmL;
            p->lastPwmRight = pwmR;
            p->driver->setSpeedAll(pwmL, pwmR);
        }

        vTaskDelayUntil(&lastWake, RTOSConfig::PERIOD_ENCODER);
    }
}

// ───────────────────────────────────────────────────────────────
//  Paramètres de taskCommand
// ───────────────────────────────────────────────────────────────
struct CommandTaskParams {
    StrategyManager* manager;

    // Callbacks vers main.cpp (résolution des dépendances locales)
    void (*onStrategy)(const char* name);       // STRATEGY:xxx
    void (*onLed)(uint8_t r, uint8_t g, uint8_t b); // LED:R:G:B
    void (*onPidKp)(float v);                   // PID:KP
    void (*onPidKi)(float v);                   // PID:KI
    void (*onPidKd)(float v);                   // PID:KD
    void (*onPoseReset)();                       // POSE:RESET / RESET
};

// ───────────────────────────────────────────────────────────────
//  taskCommand — event-driven, Core 1, Prio 2
//  Lit la queue de commandes HMI et les applique au robot
//  Gère aussi l'exécution des séquences de mouvement
// ───────────────────────────────────────────────────────────────
void taskCommand(void* pvParameters)
{
    CommandTaskParams* p = static_cast<CommandTaskParams*>(pvParameters);
    StrategyManager* manager = p->manager;

    char rawCmd[RobotQueues::CMD_MAX_LEN];

    for (;;)
    {
        // Bloque jusqu'à réception d'une commande (pas de busy-wait)
        if (!RobotQueues::receiveCommand(rawCmd)) continue;

        RobotCommand cmd = parseCommand(rawCmd);
        auto& ctx = RobotContext::instance();

        switch (cmd.type) {

            case CommandType::START:
                if (ctx.getState() == RobotConstants::State::STANDBY) {
                    LOG("[CMD] Départ dans 5 secondes...");
                    ctx.setMotorSpeeds({});
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    ctx.setState(RobotConstants::State::SEARCH);
                    LOG("[CMD] GO !");
                }
                break;

            case CommandType::STOP:
                ctx.setState(RobotConstants::State::STANDBY);
                ctx.setMotorSpeeds({});  // STOP moteurs
                break;

            case CommandType::RESET:
            case CommandType::POSE_RESET:
                ctx.setPose({ 0.0f, 0.0f, 0.0f });
                if (p->onPoseReset) p->onPoseReset();
                LOG("[CMD] Pose réinitialisée");
                break;

            case CommandType::STRATEGY:
                LOGF("[CMD] Stratégie : %s\n", cmd.strategyName);
                if (p->onStrategy) p->onStrategy(cmd.strategyName);
                break;

            case CommandType::MOTOR_DIRECT:
                ctx.setMotorSpeeds({ cmd.motorLeft, cmd.motorRight });
                break;

            case CommandType::MOTOR_STOP:
                ctx.setMotorSpeeds({});
                break;

            case CommandType::PID_KP:
                LOGF("[CMD] PID KP = %.3f\n", cmd.pidValue);
                if (p->onPidKp) p->onPidKp(cmd.pidValue);
                break;

            case CommandType::PID_KI:
                LOGF("[CMD] PID KI = %.3f\n", cmd.pidValue);
                if (p->onPidKi) p->onPidKi(cmd.pidValue);
                break;

            case CommandType::PID_KD:
                LOGF("[CMD] PID KD = %.3f\n", cmd.pidValue);
                if (p->onPidKd) p->onPidKd(cmd.pidValue);
                break;

            case CommandType::LED:
                LOGF("[CMD] LED R=%d G=%d B=%d\n", cmd.ledR, cmd.ledG, cmd.ledB);
                if (p->onLed) p->onLed(cmd.ledR, cmd.ledG, cmd.ledB);
                break;

            case CommandType::SEQUENCE:
                // Exécuter chaque step de la séquence
                LOGF("[CMD] SEQ %d steps\n", cmd.seqLength);
                ctx.setState(RobotConstants::State::STANDBY);  // pause stratégie
                for (uint8_t i = 0; i < cmd.seqLength; i++) {
                    ctx.setMotorSpeeds({
                        cmd.seqSteps[i].leftSpeed,
                        cmd.seqSteps[i].rightSpeed
                    });
                    vTaskDelay(pdMS_TO_TICKS(cmd.seqSteps[i].durationMs));
                }
                ctx.setMotorSpeeds({});  // stop après séquence
                break;

            default:
                LOGF("[CMD] Commande inconnue : %s\n", rawCmd);
                break;
        }
    }
}
