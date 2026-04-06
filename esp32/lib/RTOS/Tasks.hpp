#pragma once
#include "RobotContext.hpp"
#include "RobotConstants.hpp"
#include "DriverManager.hpp"
#include "SensorManger.hpp"

void taskMotors(void* pvParameters)
{  DriverManager * divices  = static_cast<DriverManager*> (pvParameters);
    for(;;)
    {
        RobotConstants::ActionCommand cmd = RobotContext::instance().getMotorSpeeds();
        divices->setSpeedAll(cmd.leftSpeed, cmd.rightSpeed);
        vTaskDelay(pdMS_TO_TICKS(RTOSConfig::PERIOD_MOTORS));
    }
}

void taskSensors(void * pvParameters)
{
SensorManager *sensors = static_cast<SensorManager*> (pvParameters);
sensors->updateAll();


}
