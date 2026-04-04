#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "BattSensor.hpp"
#include "RTOSConfig.hpp"
BattSensor batt(SensorPosition::CENTER);
//float g_battVoltage = 0.0f; 

SemaphoreHandle_t g_logMutex  = nullptr;
QueueHandle_t g_battQueue = nullptr;

void taskSensors(void* pv)
{
    batt.init();
    TickType_t xLastWake = xTaskGetTickCount();
    for(;;){
        batt.update();
       // xSemaphoreTake(g_muxtex,portMAX_DELAY);

        float v = batt.getVoltage();
        xQueueSend(g_battQueue,&v,0);
    LOGF("BATT | %.3f V }| %d%% \n", batt.getVoltage(), batt.getPercent());
   // Serial.printf("BATT | %.3f V | %d%% \n", batt.getVoltage(), batt.getPercent());
   // vTaskDelay(pdMS_TO_TICKS(500));
   vTaskDelayUntil(&xLastWake,pdMS_TO_TICKS(500));

    }
}

void taskComm(void* pv){
    //TickType_t xLastWake = xTaskGetTickCount();
    for(;;)
    {
       // xSemaphoreTake(g_muxtex,portMAX_DELAY);
        float v;
        xQueueReceive(g_battQueue, &v, portMAX_DELAY);
        LOGF("[COMM] tension recue : %.3f V\n", v);
     

        //vTaskDelayUntil(&xLastWake,pdMS_TO_TICKS(2000));
    }
}

void setup ()
{
     Serial.begin(115200);
     delay(1000);

     //g_muxtex = xSemaphoreCreateMutex();
    g_logMutex  = xSemaphoreCreateMutex();
    g_battQueue =  xQueueCreate(5,sizeof(float));


     xTaskCreatePinnedToCore(
        taskSensors,
        "sensorBatt",
        4096,
        nullptr,
        1,
        nullptr,
        1
     );

     xTaskCreatePinnedToCore(taskComm, "Comm", 4096, nullptr,1,nullptr,0);

}

void loop()
{

}