#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"
#include "RTOSConfig.hpp"

// ═══════════════════════════════════════════════════════════════
//  StrategyManager.hpp — Gestionnaire de stratégies
//
//  OOP appliqué :
//  - Composition    : contient un pointeur vers StrategyInterface
//  - Polymorphisme  : executeStrategy() appelle la bonne execute()
//                     sans connaître la stratégie concrète
//  - Encapsulation  : le changement de stratégie est contrôlé
//
//  Utilisation dans l'architecture FreeRTOS :
//  - taskStrategy appelle executeStrategy() toutes les 50 ms
//  - Le résultat (ActionCommand) est envoyé à taskMotors via Queue
// ═══════════════════════════════════════════════════════════════

class StrategyManager {
public:

    StrategyManager() = default;

    // ── Changer de stratégie ──────────────────────────────────
    //  Appelé avant le match ou dynamiquement selon l'état.
    //  Ownership : le caller garde la responsabilité des objets.
    void setStrategy(StrategyInterface* strategy) {
        _current = strategy;
        if (_current) {
            LOGF("[StrategyManager] Stratégie active : %s\n", _current->name());
        }
    }

    // ── Exécuter la stratégie courante ────────────────────────
    //  Retourne STOP si aucune stratégie n'est définie.
    RobotConstants::ActionCommand executeStrategy(RobotContext& ctx) {
        if (!_current) {
            return RobotConstants::ActionCommand{};   // stop par défaut
        }
        return _current->execute(ctx);
    }

    // ── Accesseurs debug ──────────────────────────────────────
    const char* currentName() const {
        return _current ? _current->name() : "none";
    }

    bool hasStrategy() const {
        return _current != nullptr;
    }

private:
    StrategyInterface* _current = nullptr;  // observateur — pas propriétaire
};
