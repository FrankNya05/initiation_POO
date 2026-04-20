#include <Arduino.h>
#include "DriverLedRGB.hpp"
#include "StrategyManager.hpp"
#include "AdamantineStrategy.hpp"
#include "BerserkerStrategy.hpp"
#include "CircleDohyoStrategy.hpp"
#include "RobotContext.hpp"
#include "RTOSConfig.hpp"
#include "Tasks.hpp"

SemaphoreHandle_t g_logMutex = nullptr;

DriverLedRGB        led;
StrategyManager     strategyManager;
AdamantineStrategy  stratAdamantine;
BerserkerStrategy   stratBerserker;
CircleDohyoStrategy stratCircle;

LedTaskParams ledParams = { &led, &strategyManager };

void setup() {
    Serial.begin(115200);
    g_logMutex = xSemaphoreCreateMutex();

    led.init();
    strategyManager.setStrategy(&stratAdamantine);

    xTaskCreatePinnedToCore(
        taskLed, "Led",
        RTOSConfig::STACK_LED, &ledParams,
        RTOSConfig::PRIO_LED,  nullptr,
        RTOSConfig::CORE_LED);

    LOG("[main] taskLed lancée");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
