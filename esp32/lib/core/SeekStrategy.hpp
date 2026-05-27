#pragma once
#include "StrategyInterface.hpp"
#include "RobotContext.hpp"

// ═══════════════════════════════════════════════════════════════
//  SeekStrategy — Approche l'objet le plus proche détecté par le lidar
//
//  États :
//  1. ATTACK : objet détecté dans l'arc avant (±90° autour de 270°)
//  2. SEARCH : rien détecté ou objet derrière → rotation sur place
//
//  Capteurs de ligne ignorés volontairement (stratégie de test).
// ═══════════════════════════════════════════════════════════════

class SeekStrategy : public StrategyInterface {
public:
    const char* name()  const override { return "Seek"; }
    LedColor    color() const override { return LedColor::YELLOW; }

    RobotConstants::ActionCommand execute(RobotContext& ctx) override {
        using AC = RobotConstants::ActionCommand;
        ctx.setLidarEnabled(true);

        SensorData lidar = ctx.getLidarData();
        bool attack = false;

        if (lidar.isValid) {
            float dist  = lidar.value.vector.x;
            float angle = lidar.value.vector.y;

            if (dist >= MIN_DIST_M && dist <= MAX_DIST_M) {
                float diff = angle - FRONT_DEG;
                if (diff >  180.0f) diff -= 360.0f;
                if (diff < -180.0f) diff += 360.0f;

                // Attaque seulement si l'objet est dans l'arc avant (±90°)
                if (fabsf(diff) <= FRONT_ARC_DEG) {
                    float diffNorm = constrain(diff / 90.0f, -1.0f, 1.0f);
                    int l = constrain((int)(SPEED_FWD * (1.0f + diffNorm)), 0, 255);
                    int r = constrain((int)(SPEED_FWD * (1.0f - diffNorm)), 0, 255);

                    // Correction encodeur quand le robot est aligné avec la cible
                    if (fabsf(diffNorm) < 0.15f) {
                        auto encL = ctx.getEncoderData(SensorPosition::LEFT);
                        auto encR = ctx.getEncoderData(SensorPosition::RIGHT);
                        if (encL.isValid && encR.isValid) {
                            int corr = constrain((int)((encL.rpm - encR.rpm) * ENC_KP), -30, 30);
                            l = constrain(l - corr, 0, 255);
                            r = constrain(r + corr, 0, 255);
                        }
                    }

                    ctx.setState(RobotConstants::State::ATTACK);
                    return AC{ l, r };
                }
            }
        }

        // ── Rotation de recherche ─────────────────────────────
        ctx.setState(RobotConstants::State::SEARCH);
        return AC{ SPEED_SEARCH, -SPEED_SEARCH };
    }

private:
    static constexpr float FRONT_DEG     = 270.0f; // avant du robot (mesuré empiriquement)
    static constexpr float FRONT_ARC_DEG =  90.0f; // demi-angle de l'arc avant
    static constexpr float MIN_DIST_M    =  0.15f;
    static constexpr float MAX_DIST_M    =  5.0f;
    static constexpr int   SPEED_FWD     = 180;
    static constexpr int   SPEED_SEARCH  = 150;
    static constexpr float ENC_KP        = 0.5f;
};
