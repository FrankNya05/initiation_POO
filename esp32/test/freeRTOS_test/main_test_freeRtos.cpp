#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "RTOSConfig.hpp"
#include "SensorManger.hpp"
#include "BattSensor.hpp"

// ═══════════════════════════════════════════════════════════════
//  LEÇON FREERTOS — Code minimal
//
//  Concept appris ici :
//  1. Créer une tâche avec xTaskCreatePinnedToCore
//  2. Boucle périodique avec vTaskDelayUntil
//  3. Partager des données entre tâches avec un mutex
//  4. LOG thread-safe
//
//  2 tâches seulement :
//  ┌─────────────────┐     ┌─────────────────┐
//  │  taskSensors    │     │  taskBlink      │
//  │  Core 1 / P2    │     │  Core 1 / P1    │
//  │  toutes 500 ms  │     │  toutes 1000 ms │
//  │  lit batterie   │     │  clignote LED   │
//  │  LOG résultat   │     │  prouve que les │
//  └─────────────────┘     │  tâches tournent│
//                          │  en parallèle   │
//                          └─────────────────┘
// ═══════════════════════════════════════════════════════════════

// ── Mutex Serial — DOIT être créé avant tout LOG() ────────────
SemaphoreHandle_t g_logMutex = nullptr;

// ── SensorManager — partagé entre tâches ──────────────────────
SensorManager sensorManager;

// ── LED embarquée ESP32 ───────────────────────────────────────
static constexpr uint8_t LED_PIN = 2;

// ═══════════════════════════════════════════════════════════════
//  TÂCHE 1 — taskSensors
//
//  Ce qu'elle fait :
//  → Lit la batterie toutes les 500 ms
//  → Affiche la tension dans le moniteur série
//
//  Ce qu'on apprend :
//  → vTaskDelayUntil  : pause précise sans dérive
//  → SensorManager    : accès thread-safe via mutex interne
// ═══════════════════════════════════════════════════════════════
void taskSensors(void* /*pv*/) {

    // xLastWakeTime mémorise l'heure du dernier réveil
    // → vTaskDelayUntil calcule exactement quand se réveiller
    // → la tâche tourne EXACTEMENT toutes les 500 ms, même si
    //   le travail à l'intérieur prend un peu de temps
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {

        // ── Lecture batterie (thread-safe via mutex interne) ──
        SensorData batt = sensorManager.getDataByPosition(SensorPosition::CENTER);

        // ── Affichage ─────────────────────────────────────────
        if (batt.isValid) {
            LOGF("[Sensors] Batterie : %.2f V\n", batt.value.scalar);
        } else {
            LOGF("[Sensors] Batterie : donnée invalide::%d et V = %.2f", batt.isValid, batt.value.scalar);
        }

        // ── Pause précise de 500 ms ───────────────────────────
        // vTaskDelayUntil est MIEUX que vTaskDelay(500) car :
        // vTaskDelay(500)      → 500 ms APRES la fin du travail
        // vTaskDelayUntil(500) → 500 ms DEPUIS le début (période fixe)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

// ═══════════════════════════════════════════════════════════════
//  TÂCHE 2 — taskBlink
//
//  Ce qu'elle fait :
//  → Fait clignoter la LED embarquée toutes les secondes
//
//  Ce qu'on apprend :
//  → 2 tâches tournent vraiment en parallèle
//  → si taskSensors était bloquée (ex: delay()), taskBlink
//    continuerait quand même de clignoter → preuve du parallélisme
// ═══════════════════════════════════════════════════════════════
void taskBlink(void* /*pv*/) {

    pinMode(LED_PIN, OUTPUT);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));   // LED allumée 100 ms

        digitalWrite(LED_PIN, LOW);

        LOG("[Blink] LED clignote — les 2 tâches tournent en parallèle ✅");

        // Attend le prochain cycle d'1 seconde
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

// ═══════════════════════════════════════════════════════════════
//  setup() — point d'entrée
// ═══════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    // ── ÉTAPE 1 : créer le mutex Serial AVANT tout LOG ────────
    g_logMutex = xSemaphoreCreateMutex();
    LOG("=== Mini Sumo — boot FreeRTOS minimal ===");

    // ── ÉTAPE 2 : enregistrer et initialiser les capteurs ─────
    sensorManager.add(new BattSensor(SensorPosition::CENTER), true);

    if (!sensorManager.initAll()) {
        LOG("[setup] ERREUR init capteurs !");
    } else {
        LOG("[setup] BattSensor prêt.");
    }

    // ── ÉTAPE 3 : créer les tâches ────────────────────────────
    //
    //  xTaskCreatePinnedToCore(
    //      fonction,    ← la tâche à exécuter
    //      "nom",       ← pour le débogage
    //      stack,       ← RAM réservée (bytes)
    //      paramètre,   ← passé à void* pv (nullptr ici)
    //      priorité,    ← 0=basse, 24=haute
    //      handle,      ← pour contrôler la tâche après (nullptr = on garde pas)
    //      core         ← 0 ou 1
    //  );

    xTaskCreatePinnedToCore(
        taskSensors, "Sensors",
        2048, nullptr, 2, nullptr, 1
    );

    xTaskCreatePinnedToCore(
        taskBlink, "Blink",
        1024, nullptr, 1, nullptr, 1
    );

    LOG("[setup] 2 tâches démarrées — observe le moniteur série !");
}

// ═══════════════════════════════════════════════════════════════
//  loop() — supprimée
//
//  Avec FreeRTOS, loop() est elle-même une tâche Arduino.
//  On la supprime pour libérer ses ressources.
//  vTaskDelete(nullptr) = "supprime la tâche courante"
// ═══════════════════════════════════════════════════════════════
void loop() {
    vTaskDelete(nullptr);
}
