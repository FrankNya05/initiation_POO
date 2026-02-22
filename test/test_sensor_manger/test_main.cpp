/*
simple test d'implementation du sensor manger 
*/

#include "SensorManger.hpp"
#include "..\lib\Sensors\LineSensor.hpp"
#include <unity.h>
#include "SensorSwitch.hpp"

using namespace std;

void setUp() {}
void tearDown() {}

void test_sensor_addition() {
    SensorManager manager;

    manager.addSensor(make_unique<LineSensor>(SensorType::LINE_LEFT));
    manager.addSensor(make_unique<LineSensor>(SensorType::LINE_RIGHT));
    manager.addSensor(make_unique<SensorSwitch>(SensorType::ULTRASONIC));

    TEST_ASSERT_TRUE(manager.initAll());
}
void test_sensor_get() {
    SensorManager manager;
    auto sensor = make_unique<SensorSwitch>(SensorType::ULTRASONIC);
    manager.addSensor(move(sensor));

    float value =  manager.getValue(SensorType::ULTRASONIC);

    TEST_ASSERT_EQUAL(-10,value);
}


int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_addition);
    RUN_TEST(test_sensor_get);
    return UNITY_END();
}