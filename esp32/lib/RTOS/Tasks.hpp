#pragma once
#include "RobotContext.hpp"
#include "RobotConstants.hpp"
#include "DriverManager.hpp"
#include "SensorManger.hpp"
#include "StrategyManager.hpp"
#include "Encoder.hpp"

// ═══════════════════════════════════════════════════════════════
//  Paramètre passé à taskEncoders
// ═══════════════════════════════════════════════════════════════
struct EncoderPair {
    Encoder* left;
    Encoder* right;
};

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

        // TOF — avant-gauche et avant-droit
        ctx.setTOFData(SensorPosition::FRONT_LEFT,
            sensors->getDataByPosition(SensorPosition::FRONT_LEFT)
        );
        ctx.setTOFData(SensorPosition::FRONT_RIGHT,
            sensors->getDataByPosition(SensorPosition::FRONT_RIGHT)
        );

        // LineSensors — avant-gauche, avant-droit, arrière
        ctx.setLineData(SensorPosition::FRONT_LEFT,
            sensors->getDataByPosition(SensorPosition::FRONT_LEFT)
        );
        ctx.setLineData(SensorPosition::FRONT_RIGHT,
            sensors->getDataByPosition(SensorPosition::FRONT_RIGHT)
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
//  Lit RobotContext, exécute la stratégie active,
//  publie la commande moteur dans RobotContext
// ───────────────────────────────────────────────────────────────
void taskStrategy(void* pvParameters)
{
    StrategyManager* manager = static_cast<StrategyManager*>(pvParameters);

    for (;;)
    {
        auto& ctx = RobotContext::instance();

        // Exécute la stratégie courante (retourne STOP si aucune définie)
        RobotConstants::ActionCommand cmd = manager->executeStrategy(ctx);

        // Publie la commande — taskMotors la lira au prochain cycle
        ctx.setMotorSpeeds(cmd);

        vTaskDelay(RTOSConfig::PERIOD_STRATEGY);
    }
}

// ───────────────────────────────────────────────────────────────
//  taskEncoders — 200 Hz, Core 1, Prio 5 (la plus haute)
//  Lit les compteurs d'impulsions et calcule le RPM
//  Publie dans RobotContext::setEncoderData()
// ───────────────────────────────────────────────────────────────
void taskEncoders(void* pvParameters)
{
    EncoderPair* enc = static_cast<EncoderPair*>(pvParameters);

    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        const float deltaMs = RTOSConfig::PERIOD_ENCODER * portTICK_PERIOD_MS;

        // Lire et remettre à zéro atomiquement
        int32_t pulsesLeft  = enc->left->getAndReset();
        int32_t pulsesRight = enc->right->getAndReset();

        RobotContext::EncoderData left, right;

        left.rpm        = enc->left->getRPM(pulsesLeft, deltaMs);
        left.totalCount = enc->left->getTotalCount();
        left.isValid    = true;

        right.rpm        = enc->right->getRPM(pulsesRight, deltaMs);
        right.totalCount = enc->right->getTotalCount();
        right.isValid    = true;

        RobotContext::instance().setEncoderData(SensorPosition::LEFT,  left);
        RobotContext::instance().setEncoderData(SensorPosition::RIGHT, right);

        // Délai précis basé sur le tick de réveil (évite la dérive)
        vTaskDelayUntil(&lastWake, RTOSConfig::PERIOD_ENCODER);
    }
}
