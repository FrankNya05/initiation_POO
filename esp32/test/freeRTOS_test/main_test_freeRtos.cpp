#include <Arduino.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include "RTOSConfig.hpp"
#include "RobotContext.hpp"
#include "DriverLedRGB.hpp"
#include "DriverMotor.hpp"
#include "DriverManager.hpp"
#include "StrategyManager.hpp"
#include "AdamantineStrategy.hpp"
#include "BerserkerStrategy.hpp"
#include "TrackStrategy.hpp"
#include "CircleDohyoStrategy.hpp"
#include "SeekStrategy.hpp"
#include "SensorManger.hpp"
#include "IMUsensor.hpp"
#include "IRSensor.hpp"
#include "LineSensor.hpp"
#include "BattSensor.hpp"
#include "LidarSensor.hpp"
#include "SensorSwitch.hpp"
#include "CommunicationManager.hpp"
#include "Tasks.hpp"

static constexpr const char* WIFI_SSID       = "S25Ultra";
static constexpr const char* WIFI_PASS       = "sylvain123";
static constexpr const char* MQTT_BROKER_IP  = "10.102.143.191";
static constexpr uint16_t    MQTT_PORT       = 1883;
static constexpr const char* MQTT_TOPIC_PUB  = "robot/telemetry";
static constexpr const char* MQTT_TOPIC_SUB  = "robot/cmd";

SemaphoreHandle_t g_logMutex  = nullptr;
TaskHandle_t      g_commTask  = nullptr;

DriverLedRGB        led;
StrategyManager     stratManager;
AdamantineStrategy  stratAdamantine;
BerserkerStrategy   stratBerserker;
TrackStrategy       stratTrack;
CircleDohyoStrategy stratCircle;
SeekStrategy        stratSeek;
LedTaskParams       ledParams { &led, &stratManager };

SensorManager sensorManager;
DriverManager driverManager;

// PPR par moteur — gauche 50:1 (1400), droit 50:1 CH-N20-3 (1400)
Encoder encLeft (RobotConfig::ENCODER_MOTOR_LEFT_P,  RobotConfig::ENCODER_MOTOR_LEFT_H,
                 RobotConstants::PULSES_PER_REV);
Encoder encRight(RobotConfig::ENCODER_MOTOR_RIGHT_P, RobotConfig::ENCODER_MOTOR_RIGHT_H,
                 RobotConstants::PULSES_PER_REV_RIGHT);

EKF ekf;
PID pidLeft (2.0f, 0.15f, 0.0f);
PID pidRight(2.0f, 0.15f, 0.0f);
PID pidYaw  (0.5f, 0.0f,  0.0f);  // kp=0.5 → ~0.5 PWM par deg/s d'écart (augmenter si drift, diminuer si oscillations)
EKFParams ekfParams { &ekf, &encLeft, &encRight, &driverManager, &pidLeft, &pidRight, &pidYaw };

static float g_pidKp = 2.0f, g_pidKi = 0.15f, g_pidKd = 0.0f;

LidarSensor          lidar;
CommunicationManager comm;

static void onStrategyCmd(const char* name)            { stratManager.setByName(name); }
static void onLedCmd(uint8_t r, uint8_t g, uint8_t b) { led.setColor(r, g, b); }
static void onPoseResetCmd()                           { RobotContext::instance().setPose({0.0f, 0.0f, 0.0f}); }

static void onPidKp(float v) {
    g_pidKp = v;
    pidLeft.setGains(g_pidKp, g_pidKi, g_pidKd);
    pidRight.setGains(g_pidKp, g_pidKi, g_pidKd);
}
static void onPidKi(float v) {
    g_pidKi = v;
    pidLeft.setGains(g_pidKp, g_pidKi, g_pidKd);
    pidRight.setGains(g_pidKp, g_pidKi, g_pidKd);
}
static void onPidKd(float v) {
    g_pidKd = v;
    pidLeft.setGains(g_pidKp, g_pidKi, g_pidKd);
    pidRight.setGains(g_pidKp, g_pidKi, g_pidKd);
}

static float g_pidYawKp = 0.5f, g_pidYawKi = 0.0f, g_pidYawKd = 0.0f;
static void onPidYawKp(float v) {
    g_pidYawKp = v;
    pidYaw.setGains(g_pidYawKp, g_pidYawKi, g_pidYawKd);
}
static void onPidYawKi(float v) {
    g_pidYawKi = v;
    pidYaw.setGains(g_pidYawKp, g_pidYawKi, g_pidYawKd);
}
static void onPidYawKd(float v) {
    g_pidYawKd = v;
    pidYaw.setGains(g_pidYawKp, g_pidYawKi, g_pidYawKd);
}

CommandTaskParams cmdParams {
    &stratManager,
    onStrategyCmd, onLedCmd,
    onPidKp, onPidKi, onPidKd,
    onPidYawKp, onPidYawKi, onPidYawKd,
    onPoseResetCmd
};

void setup() {
    Serial.begin(115200);
    g_logMutex = xSemaphoreCreateMutex();
    RobotQueues::init();

    Wire.begin(21, 22);
    Wire.setClock(400000);

    sensorManager.add(new IMUSensor(SensorPosition::CENTER));
    sensorManager.add(new TOFSensor(RobotConfig::TOF_IIC_ADDR,  SensorPosition::FRONT_LEFT));
    sensorManager.add(new TOFSensor(RobotConfig::TOF_IIC_ADDR1, SensorPosition::FRONT_RIGHT));
    sensorManager.add(new LineSensor(SensorPosition::FRONT_LEFT,  2000, false));
    sensorManager.add(new LineSensor(SensorPosition::FRONT_RIGHT, 2000, false));
    sensorManager.add(new LineSensor(SensorPosition::BACK,        2000, false));
    sensorManager.add(new BattSensor(SensorPosition::BATTERY));
    sensorManager.add(new SensorSwitch(RobotConfig::START_BUTTON));

    driverManager.add(new DriverMotor());
    bool motorOk = driverManager.initAll();
    Serial.printf("[main] Moteurs init : %s\n", motorOk ? "OK" : "ERREUR");

    encLeft.init(true);
    encRight.init(false);
    pidLeft.setLimits(-100, 100);
    pidRight.setLimits(-100, 100);
    pidYaw.setLimits(-40, 40);   // correction max ±40 PWM pour éviter les oscillations

    bool sensorsOk = sensorManager.initAll();
    Serial.printf("[main] Capteurs init : %s\n", sensorsOk ? "OK" : "ERREUR");

    Serial.printf("[main] Encodeurs : L=%d PPR  R=%d PPR  (gear L=%d:1  R=%d:1)\n",
        RobotConstants::PULSES_PER_REV, RobotConstants::PULSES_PER_REV_RIGHT,
        RobotConstants::GEAR_RATIO_LEFT, RobotConstants::GEAR_RATIO_RIGHT);

    stratManager.registerStrategy(&stratTrack);
    stratManager.registerStrategy(&stratAdamantine);
    stratManager.registerStrategy(&stratBerserker);
    stratManager.registerStrategy(&stratCircle);
    stratManager.registerStrategy(&stratSeek);

    led.init();
    RobotContext::instance().setState(RobotConstants::State::STANDBY);

    comm.configureWifi(WIFI_SSID, WIFI_PASS, MQTT_BROKER_IP,
                       MQTT_PORT, MQTT_TOPIC_PUB, MQTT_TOPIC_SUB);
    comm.selectChannel(CommChannel::WIFI);
    WiFi.setSleep(false);              // désactive le mode veille WiFi → OTA UDP fiable
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    xTaskCreatePinnedToCore(taskLed,      "LED",      RTOSConfig::STACK_LED,      &ledParams,     RTOSConfig::PRIO_LED,      nullptr, RTOSConfig::CORE_LED);
    xTaskCreatePinnedToCore(taskSensors,  "SENSORS",  RTOSConfig::STACK_SENSORS,  &sensorManager, RTOSConfig::PRIO_SENSORS,  nullptr, RTOSConfig::CORE_SENSORS);
    xTaskCreatePinnedToCore(taskLidar,    "LIDAR",    RTOSConfig::STACK_LIDAR,    &lidar,         RTOSConfig::PRIO_LIDAR,    nullptr, RTOSConfig::CORE_LIDAR);
    xTaskCreatePinnedToCore(taskStrategy, "STRATEGY", RTOSConfig::STACK_STRATEGY, &stratManager,  RTOSConfig::PRIO_STRATEGY, nullptr, RTOSConfig::CORE_STRATEGY);
    xTaskCreatePinnedToCore(taskEncoders, "ENCODERS", RTOSConfig::STACK_ENCODER,  &ekfParams,     RTOSConfig::PRIO_ENCODER,  nullptr, RTOSConfig::CORE_ENCODER);
    xTaskCreatePinnedToCore(taskComm,     "COMM",     RTOSConfig::STACK_COMM,     &comm,          RTOSConfig::PRIO_COMM,     &g_commTask, RTOSConfig::CORE_COMM);
    xTaskCreatePinnedToCore(taskCommand,  "CMD",      RTOSConfig::STACK_COMMAND,  &cmdParams,     RTOSConfig::PRIO_COMMAND,  nullptr, RTOSConfig::CORE_COMMAND);
    xTaskCreatePinnedToCore(taskOTA,      "OTA",      RTOSConfig::STACK_OTA,      nullptr,        RTOSConfig::PRIO_OTA,      nullptr, RTOSConfig::CORE_OTA);

    Serial.println("[main] Toutes les taches demarrees");
}

static const char* stateStr(RobotConstants::State s) {
    switch (s) {
        case RobotConstants::State::STANDBY: return "STANDBY";
        case RobotConstants::State::SEARCH:  return "SEARCH";
        case RobotConstants::State::ATTACK:  return "ATTACK";
        case RobotConstants::State::EVADE:   return "EVADE";
        default: return "?";
    }
}

void loop() {
    auto& ctx  = RobotContext::instance();
    auto  cmd  = ctx.getMotorSpeeds();
    auto  encL = ctx.getEncoderData(SensorPosition::LEFT);
    auto  encR = ctx.getEncoderData(SensorPosition::RIGHT);

    SensorData imu    = ctx.getIMUData();
    SensorData tofFL  = ctx.getTOFData(SensorPosition::FRONT_LEFT);
    SensorData tofFR  = ctx.getTOFData(SensorPosition::FRONT_RIGHT);
    SensorData lidarD = ctx.getLidarData();
    SensorData batt   = ctx.getBattData();
    SensorData lineFL = ctx.getLineData(SensorPosition::FRONT_LEFT);
    SensorData lineFR = ctx.getLineData(SensorPosition::FRONT_RIGHT);
    SensorData lineB  = ctx.getLineData(SensorPosition::BACK);

    int rawFL = analogRead(RobotConfig::LINE_SENSOR_FRONT_LEFT);
    int rawFR = analogRead(RobotConfig::LINE_SENSOR_FRONT_RIGHT);
    int rawB  = analogRead(RobotConfig::LINE_SENSOR_BACK);

    EKF::Pose pose  = ctx.getPose();
    float gx        = imu.isValid ? imu.value.imu.gx : 0.0f;  // axe vertical du robot
    float thetaDeg  = pose.theta * 180.0f / M_PI;
    float rpmDiff   = encL.rpm - encR.rpm;  // >0 = R plus rapide → tourne gauche

    // ── Indicateur de rotation (axe X = lacet robot) ─────────
    const char* rotDir = "DROIT ";
    if      (gx >  5.0f) rotDir = "<<<GAUCHE";
    else if (gx < -5.0f) rotDir = "DROITE>>>";
    else if (gx >  1.0f) rotDir = "<< gauche";
    else if (gx < -1.0f) rotDir = "droite >>";

    Serial.println("════════════════════════════════════════════");
    Serial.printf("STATE  %-8s  STRAT %-10s  MOT L=%4d R=%4d\n",
        stateStr(ctx.getState()), stratManager.currentName(),
        cmd.leftSpeed, cmd.rightSpeed);

    // ── Rotation (priorité affichage) ─────────────────────────
    Serial.printf("GYRO   gx=%+7.2f deg/s  →  %s\n", gx, rotDir);
    Serial.printf("CAP    theta=%+7.2f deg  (drift: %+.2f deg/s)\n",
        thetaDeg, gx);
    Serial.printf("ENC    L=%6.1f RPM  R=%6.1f RPM  diff=%+6.1f  %s\n",
        encL.rpm, encR.rpm, rpmDiff,
        rpmDiff >  3.0f ? "(R trop rapide)" :
        rpmDiff < -3.0f ? "(L trop rapide)" : "(equilibre)");
    Serial.printf("PWM    L=%4d  R=%4d   correction PID: L=%+d  R=%+d\n",
        ekfParams.lastPwmLeft, ekfParams.lastPwmRight,
        ekfParams.lastPwmLeft  - cmd.leftSpeed,
        ekfParams.lastPwmRight - cmd.rightSpeed);

    // ── Diagnostics secondaires ───────────────────────────────
    Serial.printf("LINE   FL=%s(%4d)  FR=%s(%4d)  B=%s(%4d)\n",
        (lineFL.isValid && lineFL.value.scalar > 0) ? "BORD" : "ok  ", rawFL,
        (lineFR.isValid && lineFR.value.scalar > 0) ? "BORD" : "ok  ", rawFR,
        (lineB.isValid  && lineB.value.scalar  > 0) ? "BORD" : "ok  ", rawB);
    Serial.printf("TOF    FL=%4.0f mm  FR=%4.0f mm  LIDAR=%.2fm %.1fdeg [%s]\n",
        tofFL.value.scalar, tofFR.value.scalar,
        lidarD.value.vector.x, lidarD.value.vector.y,
        lidarD.isValid ? "OK" : "ERR");
    Serial.printf("BATT   %.2f V  [%s]   MQTT %s\n",
        batt.value.scalar, batt.isValid ? "OK" : "ERR",
        comm.isConnected() ? "connecte" : "deconnecte");
    //Serial.print(" sylvain OTA OK V2 ");
    vTaskDelay(pdMS_TO_TICKS(500));  // 2 Hz pour voir la rotation en temps réel
}
