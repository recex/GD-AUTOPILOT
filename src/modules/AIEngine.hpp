#pragma once

#include "core/Types.hpp"
#include "modules/GameController.hpp"
#include "modules/LearningSystem.hpp"
#include "modules/LevelAnalyzer.hpp"
#include "modules/PhysicsEngine.hpp"

#include <vector>

namespace gdautopilot {

struct EngineDecisionTelemetry {
    double computeTimeMs = 0.0;
    int candidatesEvaluated = 0;
    float bestScore = 0.0f;
    bool usedLearningBias = false;
    bool emergencyFallback = false;
};

class AIEngine {
public:
    AIEngine() = default;

    void setConfig(AIConfig const& config) noexcept { m_config = config; }
    [[nodiscard]] AIConfig const& getConfig() const noexcept { return m_config; }
    [[nodiscard]] AIConfig& mutableConfig() noexcept { return m_config; }

    // Main real-time decision step
    [[nodiscard]] ActionDecision evaluate(
        PlayerState const& state,
        PhysicsEngine const& physics,
        LevelAnalyzer const& analyzer,
        GameController const& controller,
        LearningSystem const& learning,
        EngineDecisionTelemetry& outTelemetry
    );

    [[nodiscard]] ActionDecision const& getLastDecision() const noexcept { return m_lastDecision; }
    [[nodiscard]] EngineDecisionTelemetry const& getLastTelemetry() const noexcept { return m_lastTelemetry; }

    void reset() noexcept;

private:
    AIConfig m_config{};
    ActionDecision m_lastDecision{};
    EngineDecisionTelemetry m_lastTelemetry{};
    std::vector<InputPlan> m_candidateBuffer;
    int m_ticksSinceFullEval = 0;
};

} // namespace gdautopilot
