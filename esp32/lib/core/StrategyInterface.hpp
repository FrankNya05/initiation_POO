#pragma once
#include "RobotConstants.hpp"

// ═══════════════════════════════════════════════════════════════
//  StrategyInterface.hpp — Interface abstraite des stratégies
//
//  OOP appliqué :
//  - Classe abstraite pure (aucune implémentation concrète ici)
//  - Polymorphisme : StrategyManager manipule uniquement ce type
//  - Destructeur virtuel : requis pour delete via pointeur de base
//
//  Utilisation du forward declare :
//  - RobotContext est forward-déclaré (pas besoin de l'include complet)
//    → évite la dépendance circulaire
//    → execute() prend RobotContext& (référence → pointeur opaque ok)
// ═══════════════════════════════════════════════════════════════

// Forward declaration — évite d'inclure RobotContext.hpp ici
// (RobotContext.hpp inclura StrategyInterface indirectement)
class RobotContext;

class StrategyInterface {
public:
    virtual ~StrategyInterface() = default;

    // ── Méthode principale ────────────────────────────────────
    //  Reçoit l'état complet du robot, retourne une commande moteur.
    //  Appelée par StrategyManager::executeStrategy() depuis taskStrategy.
    virtual RobotConstants::ActionCommand execute(RobotContext& ctx) = 0;
  

    // ── Nom de la stratégie ───────────────────────────────────
    //  Utilisé pour le debug et le LOG.
    virtual const char* name() const = 0;
};
