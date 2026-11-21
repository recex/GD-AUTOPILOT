#pragma once

#include "core/Types.hpp"
#include "modules/LevelAnalyzer.hpp"
#include "modules/PhysicsEngine.hpp"

#include <array>
#include <memory>
#include <vector>

namespace gdautopilot {

// Base interface for mode-specific gameplay controllers
class IModeController {
public:
    virtual ~IModeController() = default;

    [[nodiscard]] virtual GameMode mode() const noexcept = 0;

    // Generates specialized candidate InputPlans tailored to this game mode's mechanics
    virtual void generateCandidates(
        PlayerState const& state,
        CorridorSample const& aheadCorridor,
        ObstaclePattern const& upcomingPattern,
        int totalRolloutSteps,
        float learningHoldBias,
        float learningYBias,
        std::vector<InputPlan>& outPlans
    ) const = 0;

    // Computes a fast mode-specific reactive preference score bonus for a given candidate result
    [[nodiscard]] virtual float evaluateModeHeuristic(
        PlayerState const& startState,
        InputPlan const& plan,
        SimulationResult const& result,
        CorridorSample const& aheadCorridor,
        float learningYBias
    ) const = 0;

    // Indicates whether this mode requires an explicit release tick before re-triggering (tap-based modes)
    [[nodiscard]] virtual bool prefersTapRelease(PlayerState const& state) const noexcept {
        (void)state;
        return false;
    }
};

class CubeController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Cube; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
};

class ShipController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Ship; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
};

class BallController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Ball; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
    [[nodiscard]] bool prefersTapRelease(PlayerState const& state) const noexcept override { return !state.isOnGround; }
};

class UFOController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::UFO; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
    [[nodiscard]] bool prefersTapRelease(PlayerState const& state) const noexcept override { return state.isHolding; }
};

class WaveController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Wave; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
};

class RobotController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Robot; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
};

class SpiderController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Spider; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
    [[nodiscard]] bool prefersTapRelease(PlayerState const& state) const noexcept override { return state.isHolding; }
};

class SwingController final : public IModeController {
public:
    [[nodiscard]] GameMode mode() const noexcept override { return GameMode::Swing; }
    void generateCandidates(PlayerState const&, CorridorSample const&, ObstaclePattern const&, int, float, float, std::vector<InputPlan>&) const override;
    [[nodiscard]] float evaluateModeHeuristic(PlayerState const&, InputPlan const&, SimulationResult const&, CorridorSample const&, float) const override;
    [[nodiscard]] bool prefersTapRelease(PlayerState const& state) const noexcept override { return state.isHolding; }
};

// Orchestrates all 8 mode controllers and manages physical input edge transitions
class GameController {
public:
    GameController();

    [[nodiscard]] IModeController const& getController(GameMode mode) const;

    // Resolves whether an actual press/release event should be dispatched to Geometry Dash
    // Returns true if button state changed and outNewButtonDown contains the new state
    bool computeInputDispatch(ActionDecision const& decision, GameMode mode, bool& outNewButtonDown);

    void resetInputState() noexcept;
    [[nodiscard]] bool isButtonCurrentlyDown() const noexcept { return m_buttonDown; }

private:
    std::array<std::unique_ptr<IModeController>, 8> m_controllers;
    bool m_buttonDown = false;
    bool m_pendingPulseReengage = false;
};

} // namespace gdautopilot
