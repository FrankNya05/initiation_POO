/*
 * test_driver_motor.cpp
 * -----------------------------------------------
 * Test hardware du DriverMotor sur ESP32 réel
 *
 * Ce que ce test vérifie :
 *  1. init()          — canaux PWM configurés sans crash
 *  2. setSpeed(+,+)   — avance (moteurs tournent vers l'avant)
 *  3. setSpeed(-,-)   — recule (moteurs tournent vers l'arrière)
 *  4. setSpeed(+,-)   — rotation gauche
 *  5. setSpeed(-,+)   — rotation droite
 *  6. stop()          — arrêt immédiat
 *
 * Branchement requis :
 *  - Pont en H connecté aux GPIO définis dans PinConfig.hpp
 *    (LEFT_PWM=25, LEFT_DIR=26, RIGHT_PWM=27, RIGHT_DIR=14)
 *  - Alimentation moteur active
 *
 * Lecture dans le Serial Monitor à 115200 baud
 */

#ifndef UNIT_TEST

#include <Arduino.h>
#include "DriverMotor.hpp"

DriverMotor motor;

void printResult(const char* label, bool passed) {
    Serial.printf("  [%s] %s\n", passed ? "PASS" : "FAIL", label);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   TEST HARDWARE — DriverMotor");
    Serial.println("========================================\n");

    // ── TEST 1 : init() ───────────────────────────────────────────
    Serial.println("TEST 1 : Initialisation");
    bool initOk = motor.init();
    printResult("init() retourne true", initOk);
    Serial.println();
    delay(500);

    // ── TEST 2 : avance pleine vitesse ────────────────────────────
    Serial.println("TEST 2 : Avance (setSpeed +255, +255) — 2 secondes");
    motor.setSpeed(255, 255);
    delay(2000);
    motor.stop();
    printResult("avance sans crash", true);
    Serial.println();
    delay(500);

    // ── TEST 3 : recul pleine vitesse ─────────────────────────────
    Serial.println("TEST 3 : Recul (setSpeed -255, -255) — 2 secondes");
    motor.setSpeed(-255, -255);
    delay(2000);
    motor.stop();
    printResult("recul sans crash", true);
    Serial.println();
    delay(500);

    // ── TEST 4 : rotation gauche ──────────────────────────────────
    Serial.println("TEST 4 : Rotation gauche (setSpeed -150, +150) — 1 seconde");
    motor.setSpeed(-150, 150);
    delay(1000);
    motor.stop();
    printResult("rotation gauche sans crash", true);
    Serial.println();
    delay(500);

    // ── TEST 5 : rotation droite ──────────────────────────────────
    Serial.println("TEST 5 : Rotation droite (setSpeed +150, -150) — 1 seconde");
    motor.setSpeed(150, -150);
    delay(1000);
    motor.stop();
    printResult("rotation droite sans crash", true);
    Serial.println();
    delay(500);

    // ── TEST 6 : vitesse progressive ─────────────────────────────
    Serial.println("TEST 6 : Rampe de vitesse (0 → 255 → 0)");
    for (int spd = 0; spd <= 255; spd += 5) {
        motor.setSpeed(spd, spd);
        delay(20);
    }
    for (int spd = 255; spd >= 0; spd -= 5) {
        motor.setSpeed(spd, spd);
        delay(20);
    }
    motor.stop();
    printResult("rampe sans crash", true);
    Serial.println();

    // ── TEST 7 : stop() ───────────────────────────────────────────
    Serial.println("TEST 7 : Arrêt d'urgence");
    motor.setSpeed(200, 200);
    delay(300);
    motor.stop();
    printResult("stop() sans crash", true);
    Serial.println();

    Serial.println("========================================");
    Serial.println("   FIN DES TESTS");
    Serial.println("========================================\n");
}

void loop() {
    // rien — les tests se font une seule fois dans setup()
}

#endif
