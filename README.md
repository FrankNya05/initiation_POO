# initiation_POO
                       Structure de l'architecture

MiniSumoRobot/
│
├── src/
│   └── main.cpp
│
├── lib/
│   ├── Core/
│   │   ├── RobotContext.cpp
│   │   └── RobotContext.h
│   │
│   ├── Sensors/
│   │   └── (capteurs individuels)
│   │
│   ├── Actuators/
│   │   └── (drivers moteurs, PWM…)
│   │
│   ├── Communication/
│   │   ├── BLEComm.cpp
│   │   ├── BLEComm.h
│   │   ├── WiFiComm.cpp
│   │   ├── WiFiComm.h
│   │   ├── CommandParser.cpp
│   │   ├── CommandParser.h
│   │   └── ICommInterface.h
│   │
│   ├── RTOS/
│   │   ├── Tasks.cpp
│   │   ├── Tasks.h
│   │   ├── Queues.cpp
│   │   └── Queues.h
│   │
│   └── config/
│       ├── PinConfig.hpp
│       ├── RobotConstants.hpp
│       ├── RTOSConfig.hpp
│       └── CommConfig.hpp
│
└── test/
    └── test_motor/

🧠 Philosophie de l’architecture
L’objectif est de séparer clairement :
- Le matériel (drivers capteurs, moteurs)
- La logique (RobotContext, comportements)
- La communication (BLE, WiFi, parser)
- Le temps réel (tâches FreeRTOS, queues)
- La configuration (pins, seuils, constantes)
Cette séparation garantit :
- une meilleure lisibilité
- une maintenance simplifiée
- une évolution rapide du robot
- des tests unitaires plus faciles
- une réutilisation des modules dans d’autres projets

🧩 Description des modules
Core/
Contient la logique centrale du robot.
- RobotContext : point d’accès global aux modules
- Gestion des états (Idle, Search, Attack…)
- Coordination capteurs ↔ moteurs ↔ communication

Sensors/
Chaque capteur est un module indépendant.
- Drivers bas niveau (ADC, I2C, GPIO)
- Fonctions haut niveau (détection, normalisation)
- Interface simple pour le Core

Actuators/
Tout ce qui agit sur le robot.
- Contrôle moteur (PWM, direction)
- Fonctions haut niveau :
- forward()
- turnLeft()
- stop()

Communication/
Gestion des interfaces externes.
- BLE
- WiFi
- Interface commune ICommInterface
- CommandParser pour interpréter les commandes reçues

RTOS/
Gestion du temps réel.
- Tâches périodiques (SensorsTask, MotorTask, CommTask…)
- Queues pour échanger des messages
- Priorités, timing, synchronisation

config/
Centralise toutes les constantes du robot.
- Pins GPIO
- Seuils capteurs
- Paramètres moteurs
- Paramètres RTOS
- Paramètres communication

🚀 Démarrage rapide
1. Initialiser les modules
Dans main.cpp :
RobotContext robot;

robot.initSensors();
robot.initActuators();
robot.initCommunication();
robot.initRTOS();


2. Lancer les tâches RTOS
startTasks(robot);
vTaskStartScheduler();



🧪 Tests
Le dossier test/ contient des tests unitaires pour les moteurs, capteurs ou modules critiques.

📌 Objectifs du projet
- Architecture modulaire, propre et évolutive
- Séparation claire hardware / logique / RTOS
- Code réutilisable pour d’autres robots
- Base solide pour compétitions MiniSumo