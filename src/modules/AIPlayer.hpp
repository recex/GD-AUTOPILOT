#pragma once

#include "core/Types.hpp"
#include "modules/AIEngine.hpp"
#include "modules/GameController.hpp"
#include "modules/LearningSystem.hpp"
#include "modules/LevelAnalyzer.hpp"
#include "modules/PhysicsEngine.hpp"
#include "modules/StatisticsManager.hpp"

#include <string>

class PlayLayer;
class GJBaseGameLayer;
class PlayerObject;

namespace gdautopilot {

class AIPlayer {
public:
    static AIPlayer& get();

    void initializeFromModSettings();
    void syncConfigToModSettings();

    // Gameplay controls
    void start();
    void stop();
    void toggle();
    void reanalyzeActiveLevel();
    void clearActiveLevelLearning();

    [[nodiscard]] bool isRunning() const noexcept { return m_config.enabled; }
    [[nodiscard]] AIState getState() const noexcept { return m_state; }
    [[nodiscard]] bool isDispatchingSyntheticInput() const noexcept { return m_isDispatchingInput; }

    // Geode hook callbacks
    void onLevelEntered(PlayLayer* playLayer);
    void onLevelObjectsReady(PlayLayer* playLayer);
    void onPhysicsTick(GJBaseGameLayer* gameLayer, float dt, bool isHalfTick);
    void onResetLevel(PlayLayer* playLayer);
    void onPlayerDeath(PlayLayer* playLayer, PlayerObject* player);
    void onLevelComplete(PlayLayer* playLayer);
    void onQuitLevel();

    // Module accessors for GeodeInterface & diagnostics
    [[nodiscard]] AIConfig const& getConfig() const noexcept { return m_config; }
    void applyConfig(AIConfig const& cfg);

    [[nodiscard]] PlayerState const& getLastPlayerState() const noexcept { return m_lastPlayerState; }
    [[nodiscard]] ActionDecision const& getLastDecision() const noexcept { return m_engine.getLastDecision(); }
    [[nodiscard]] EngineDecisionTelemetry const& getLastTelemetry() const noexcept { return m_engine.getLastTelemetry(); }

    [[nodiscard]] LevelAnalyzer const& getLevelAnalyzer() const noexcept { return m_analyzer; }
    [[nodiscard]] PhysicsEngine& getPhysicsEngine() noexcept { return m_physics; }
    [[nodiscard]] PhysicsEngine const& getPhysicsEngine() const noexcept { return m_physics; }
    [[nodiscard]] LearningSystem const& getLearningSystem() const noexcept { return m_learning; }
    [[nodiscard]] StatisticsManager& getStatistics() noexcept { return m_stats; }
    [[nodiscard]] StatisticsManager const& getStatistics() const noexcept { return m_stats; }

private:
    AIPlayer() = default;

    [[nodiscard]] PlayerState extractPlayerState(GJBaseGameLayer* gameLayer, PlayerObject* player, float dt);
    void dispatchButton(GJBaseGameLayer* gameLayer, bool down);
    void releaseInputIfHeld(GJBaseGameLayer* gameLayer);

    AIConfig m_config{};
    AIState m_state = AIState::Idle;
    bool m_isDispatchingInput = false;

    PlayLayer* m_activePlayLayer = nullptr;
    PlayerState m_lastPlayerState{};
    float m_prevPlayerX = 0.0f;
    bool m_hasPrevPlayerX = false;
    float m_smoothedVelX = 311.58f;
    bool m_deathRecordedThisAttempt = false;

    LevelAnalyzer m_analyzer;
    PhysicsEngine m_physics;
    GameController m_controller;
    LearningSystem m_learning;
    AIEngine m_engine;
    StatisticsManager m_stats;
};

} // namespace gdautopilot
