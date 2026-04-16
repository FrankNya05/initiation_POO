#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════
//  Queues.hpp — Queues FreeRTOS globales du robot
//
//  CommandQueue : commandes HMI → taskCommand
//  Format       : string de max CMD_MAX_LEN caractères
// ═══════════════════════════════════════════════════════════════

namespace RobotQueues {

    constexpr uint8_t CMD_MAX_LEN    = 64;  // longueur max d'une commande
    constexpr uint8_t CMD_QUEUE_SIZE =  8;  // nb commandes en attente max

    // Queue globale — initialisée dans main.cpp via init()
    inline QueueHandle_t commandQueue = nullptr;

    // Créer la queue — appeler dans setup() avant les tâches
    inline void init() {
        commandQueue = xQueueCreate(CMD_QUEUE_SIZE, CMD_MAX_LEN);
    }

    // Envoyer une commande (non bloquant — depuis callback onReceive)
    inline bool sendCommand(const char* cmd) {
        if (!commandQueue) return false;
        char buf[CMD_MAX_LEN] = {};
        strncpy(buf, cmd, CMD_MAX_LEN - 1);
        return xQueueSend(commandQueue, buf, 0) == pdTRUE;
    }

    // Recevoir une commande (bloquant avec timeout)
    inline bool receiveCommand(char* out, TickType_t timeout = portMAX_DELAY) {
        if (!commandQueue) return false;
        return xQueueReceive(commandQueue, out, timeout) == pdTRUE;
    }
}
