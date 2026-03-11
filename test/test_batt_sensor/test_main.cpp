/*
 * test/test_batt_sensor/test_main.cpp
 * -----------------------------------------------
 * Test hardware du BattSensor avec Unity + PlatformIO
 * Lancé via : pio test -e esp32dev -f test_batt_sensor
 */

#include <Arduino.h>
#include <unity.h>
#include "BattSensor.hpp"

// ── Instance partagée entre les tests ────────
BattSensor batt(SensorPosition::CENTER);

// ─────────────────────────────────────────────
//  Appelé avant chaque test
// ─────────────────────────────────────────────
void setUp() {
    batt.init();
    batt.update();
}

//  Appelé après chaque test
void tearDown() {}

// ─────────────────────────────────────────────
//  TEST 1 : init() ne plante pas
// ─────────────────────────────────────────────
void test_init_returns_true() {
    BattSensor fresh(SensorPosition::CENTER);
    TEST_ASSERT_TRUE(fresh.init());
}

// ─────────────────────────────────────────────
//  TEST 2 : tension dans la plage LiPo 2S
// ─────────────────────────────────────────────
void test_voltage_in_range() {
    float v = batt.getVoltage();
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(6.0f, v);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(8.4f, v);
}

// ─────────────────────────────────────────────
//  TEST 3 : pourcentage entre 0 et 100
// ─────────────────────────────────────────────
void test_percent_in_range() {
    int p = batt.getPercent();
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0,   p);
    TEST_ASSERT_LESS_OR_EQUAL_INT   (100, p);
}

// ─────────────────────────────────────────────
//  TEST 4 : SensorData correctement rempli
// ─────────────────────────────────────────────
void test_sensor_data_valid() {
    SensorData data = batt.getData();
    TEST_ASSERT_TRUE (data.isValid);
    TEST_ASSERT_TRUE (data.timestamp > 0);
    TEST_ASSERT_EQUAL(SensorDims::SCALAR,          data.dims);
    TEST_ASSERT_EQUAL(SensorPosition::CENTER,      data.position);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, batt.getVoltage(), data.value.scalar);
}

// ─────────────────────────────────────────────
//  TEST 5 : stabilité du filtre ADC
//  Le jitter entre 10 lectures doit rester < 0.1V
// ─────────────────────────────────────────────
void test_filter_stability() {
    float vmin = 99.0f, vmax = 0.0f;

    for (int i = 0; i < 10; i++) {
        batt.update();
        float v = batt.getVoltage();
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
        delay(50);
    }

    float jitter = vmax - vmin;
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.1f, jitter);
}

// ─────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────
int main(int argc, char **argv) {
    delay(2000);  // Laisse le temps à l'ESP32 de démarrer
    UNITY_BEGIN();

    RUN_TEST(test_init_returns_true);
    RUN_TEST(test_voltage_in_range);
    RUN_TEST(test_percent_in_range);
    RUN_TEST(test_sensor_data_valid);
    RUN_TEST(test_filter_stability);

    return UNITY_END();
}