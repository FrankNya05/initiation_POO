/*
simple test d'implementation du sensor manger 
*/

#include "..\lib\Sensors\SensorManger.hpp"
#include "..\lib\Sensors\LineSensor.hpp"


SensorManager sensorManager;
LineSensor left_sensor, rigth_sensor;

void main()
{
 sensorManager.addSensor(&left_sensor);
 if (sensorManager.initAll())
 {
    while (true)
    {
        sensorManager.updateAll();

        float left = sensorManager.getValue(SensorType::LINE_LEFT);
    }
    
 }
 
}