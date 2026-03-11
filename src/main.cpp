/*
 * test_batt_sensor.cpp
 * -----------------------------------------------
 * Test hardware du BattSensor sur ESP32 réel
 *
 * Ce que ce test vérifie :
 *  1. init()      — ADC configuré sans crash
 *  2. update()    — lecture filtrée stable
 *  3. getVoltage()— tension dans la plage LiPo 2S
 *  4. getPercent()— pourcentage cohérent (0–100)
 *  5. isCritical()— alerte bas niveau
 *  6. getData()   — SensorData bien rempli
 *
 * Branchement requis :
 *  - Pont diviseur R1=100kΩ / R2=47kΩ entre VBAT et GND
 *  - Point milieu → GPIO 36 (VP)
 *  - Batterie LiPo 2S connectée
 *
 * Lecture dans le Serial Monitor à 115200 baud
 */

#ifndef UNIT_TEST

#include <Arduino.h>
#include "BattSensor.hpp"

// ── Instance globale ──────────────────────────
BattSensor batt(SensorPosition::CENTER);

// ── Résultat d'un test ────────────────────────
void printResult(const char* label, bool passed) {
    Serial.printf("  [%s] %s\n", passed ? "PASS" : "FAIL", label);
}

// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   TEST HARDWARE — BattSensor");
    Serial.println("========================================\n");

    // ── TEST 1 : init() ───────────────────────
    Serial.println("TEST 1 : Initialisation");
    bool initOk = batt.init();
    printResult("init() retourne true", initOk);
    Serial.println();

    // ── TEST 2 : update() + tension ───────────
    Serial.println("TEST 2 : Lecture de tension");
    bool updateOk = batt.update();
    float voltage = batt.getVoltage();

    printResult("update() retourne true",  updateOk);
    printResult("tension >= 6.0V (min LiPo 2S)", voltage >= 6.0f);
    printResult("tension <= 8.4V (max LiPo 2S)", voltage <= 8.4f);
    Serial.printf("  → Tension mesurée : %.3f V\n", voltage);
    Serial.println();

    // ── TEST 3 : getPercent() ─────────────────
    Serial.println("TEST 3 : Pourcentage de charge");
    int percent = batt.getPercent();
    printResult("pourcentage entre 0 et 100", percent >= 0 && percent <= 100);
    Serial.printf("  → Charge estimée  : %d%%\n", percent);
    Serial.println();

    // ── TEST 4 : getData() ────────────────────
    Serial.println("TEST 4 : SensorData");
    SensorData data = batt.getData();
    printResult("isValid == true",            data.isValid);
    printResult("timestamp > 0",              data.timestamp > 0);
    printResult("dims == SCALAR",             data.dims == SensorDims::SCALAR);
    printResult("position == CENTER",         data.position == SensorPosition::CENTER);
    printResult("value.scalar == getVoltage()",
                data.value.scalar == batt.getVoltage());
    Serial.println();

    // ── TEST 5 : alerte critique ──────────────
    Serial.println("TEST 5 : Alerte bas niveau");
    bool critical = batt.isCritical();
    Serial.printf("  → isCritical() = %s  (seuil : 6.60V)\n",
                  critical ? "OUI ⚠" : "non");
    Serial.println("  [INFO] Ce test est informatif — pas de PASS/FAIL.");
    Serial.println();

    // ── TEST 6 : stabilité sur 10 lectures ────
    Serial.println("TEST 6 : Stabilité du filtre ADC (10 lectures)");
    float vmin = 99.0f, vmax = 0.0f, vsum = 0.0f;
    for (int i = 0; i < 10; i++) {
        batt.update();
        float v = batt.getVoltage();
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
        vsum += v;
        delay(50);
    }
    float vmoy   = vsum / 10.0f;
    float jitter = vmax - vmin;
    printResult("jitter < 0.1V (filtre efficace)", jitter < 0.1f);
    Serial.printf("  → Min: %.3fV  Max: %.3fV  Moy: %.3fV  Jitter: %.3fV\n",
                  vmin, vmax, vmoy, jitter);
    Serial.println();

    Serial.println("========================================");
    Serial.println("   FIN DES TESTS — Surveillance continue");
    Serial.println("========================================\n");
}

// ─────────────────────────────────────────────
//  Surveillance continue toutes les 2 secondes
// ─────────────────────────────────────────────
void loop() {
    batt.update();

    Serial.printf("BATT | %.3f V | %3d%% | %s\n",
        batt.getVoltage(),
        batt.getPercent(),
        batt.isCritical() ? "⚠ CRITIQUE" : "OK");

    delay(2000);
}

#endif