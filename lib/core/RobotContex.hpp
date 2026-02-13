/*
 * RobotContext.h
 * -------------------------------
 * Résumé : 
 *  Cette classe centralise toutes les données partagées du robot (capteurs, vitesse, angle, états).
 *  Elle permet à toutes les tâches FreeRTOS d'accéder à l'état du robot de façon sécurisée.
 * 
 * Principe OOP appliqué :
 *  - Singleton : une seule instance globale du robot.
 *  - Encapsulation : les données sont privées, accessibles uniquement via getters/setters.
 *  - Abstraction : les tâches n'ont pas besoin de connaître la structure interne.
 *  - Thread-safe : accès protégé par un mutex FreeRTOS.
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class RobotContext {
public:
    static RobotContext& instance();  // Accès global à l'instance

    // Setter / Getter de l'angle
    void setAngle(float a);
    float getAngle();

private:
    RobotContext();
    RobotContext(const RobotContext&) = delete;
    RobotContext& operator=(const RobotContext&) = delete;

    float angle;                  // Angle actuel du robot
    SemaphoreHandle_t mutex;      // Mutex pour protéger les données
};
