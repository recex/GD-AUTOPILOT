#pragma once

#include "core/Types.hpp"
#include "modules/LevelAnalyzer.hpp"

#include <functional>
#include <vector>

namespace gdautopilot {

// Describes a piecewise input plan over a simulation rollout
struct InputPlan {
    bool initialHold = false;
    bool secondHold = false;
    int switchStep = -1;          // If >= 0, switch from initialHold to secondHold at this step
    bool pulseOnStart = false;    // Force a rising edge on step 0 even if player was holding
    bool closedLoopCorridor = false; // After switchStep, use closed-loop PD tracking of corridor targetY
};

class PhysicsEngine {
public:
    PhysicsEngine() = default;

    void setParams(PhysicsParams const& params) noexcept { m_params = params; }
    [[nodiscard]] PhysicsParams const& getParams() const noexcept { return m_params; }
    [[nodiscard]] PhysicsParams& mutableParams() noexcept { return m_params; }

    // Advance a PlayerState by one physics step (dt seconds) given hold input and nearby level objects
    void step(
        PlayerState& state,
        bool holdInput,
        bool justPressed,
        float dt,
        std::vector<LevelObject const*> const& nearbyObjects,
        bool& outHitHazard,
        bool& outHitWall,
        bool& outTriggeredOrb,
        bool& outTriggeredPad,
        bool& outChangedMode,
        float& outMinHazardDist
    ) const;

    // Simulates an entire trajectory using an InputPlan and scores survival & corridor alignment
    [[nodiscard]] SimulationResult simulateRollout(
        PlayerState const& startState,
        InputPlan const& plan,
        float durationSec,
        float stepDt,
        LevelAnalyzer const& analyzer,
        float corridorWeight
    ) const;

    // Utility: compute spider teleport target Y given nearby solid objects
    [[nodiscard]] float computeSpiderTeleportY(
        PlayerState const& state,
        bool goingUp,
        std::vector<LevelObject const*> const& nearbyObjects
    ) const;

    // Utility: distance between two AABBs (0 if intersecting)
    [[nodiscard]] static float rectDistance(Rect const& a, Rect const& b) noexcept;

private:
    void applyModeKinematics(
        PlayerState& state,
        bool holdInput,
        bool justPressed,
        float dt,
        std::vector<LevelObject const*> const& nearbyObjects
    ) const;

    void resolveObjectInteractions(
        PlayerState& state,
        bool holdInput,
        bool justPressed,
        std::vector<LevelObject const*> const& nearbyObjects,
        bool& outHitHazard,
        bool& outHitWall,
        bool& outTriggeredOrb,
        bool& outTriggeredPad,
        bool& outChangedMode,
        float& outMinHazardDist
    ) const;

    void applyOrbImpulse(PlayerState& state, OrbType orb, std::vector<LevelObject const*> const& nearbyObjects) const;
    void applyPadImpulse(PlayerState& state, PadType pad, std::vector<LevelObject const*> const& nearbyObjects) const;

    PhysicsParams m_params{};
};

} // namespace gdautopilot
