#include "modules/AIEngine.hpp"
#include "core/DebugLog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace gdautopilot {

void AIEngine::reset() noexcept {
    m_lastDecision = ActionDecision{};
    m_lastTelemetry = EngineDecisionTelemetry{};
    m_ticksSinceFullEval = 0;
}

ActionDecision AIEngine::evaluate(
    PlayerState const& state,
    PhysicsEngine const& physics,
    LevelAnalyzer const& analyzer,
    GameController const& controller,
    LearningSystem const& learning,
    EngineDecisionTelemetry& outTelemetry
) {
    auto startClock = std::chrono::steady_clock::now();
    outTelemetry = EngineDecisionTelemetry{};

    // Respect decision stride unless player is in an orb or imminent danger
    bool urgentContext = state.inOrbRange || state.mode == GameMode::Wave || state.mode == GameMode::Ship;
    if (!urgentContext && m_ticksSinceFullEval + 1 < std::max(1, m_config.decisionStrideTicks)) {
        ++m_ticksSinceFullEval;
        outTelemetry = m_lastTelemetry;
        outTelemetry.computeTimeMs = 0.0;
        outTelemetry.candidatesEvaluated = 0;
        return m_lastDecision;
    }
    m_ticksSinceFullEval = 0;

    // 1. Gather level context & adaptive learning guidance
    float lookaheadSec = std::clamp(m_config.lookaheadSeconds, 0.25f, 1.4f);
    float simDt = (m_config.analysisSpeedLevel >= 3) ? (1.0f / 120.0f) : (1.0f / 60.0f);
    int totalSteps = std::max(6, static_cast<int>(std::round(lookaheadSec / simDt)));

    float aheadX = state.x + std::abs(state.velocityX) * (lookaheadSec * 0.6f);
    CorridorSample aheadCorridor = analyzer.sampleCorridor(aheadX);
    ObstaclePattern upcomingPattern = analyzer.findUpcomingPattern(state.x, std::abs(state.velocityX) * lookaheadSec);
    AdaptiveGuidance guidance = learning.queryGuidance(state.x, state.mode);

    outTelemetry.usedLearningBias = guidance.hasFailureZone || guidance.hasLearnedAction;

    // 2. Ask the active mode controller to generate physics-aware candidate plans
    IModeController const& modeCtrl = controller.getController(state.mode);
    m_candidateBuffer.clear();
    m_candidateBuffer.reserve(12);
    modeCtrl.generateCandidates(
        state,
        aheadCorridor,
        upcomingPattern,
        totalSteps,
        guidance.preferHoldBias,
        guidance.targetYBias,
        m_candidateBuffer
    );

    // If adaptive learning has a strong timing offset in a failure zone, inject an extra shifted candidate
    if (guidance.hasFailureZone && std::abs(guidance.timingOffsetBias) > 0.005f) {
        int shiftedSwitch = std::clamp(
            static_cast<int>(std::round((0.12f + guidance.timingOffsetBias) / simDt)),
            1,
            std::max(1, totalSteps - 1)
        );
        m_candidateBuffer.push_back(InputPlan{
            .initialHold = (guidance.preferHoldBias > 0.0f),
            .secondHold = !(guidance.preferHoldBias > 0.0f),
            .switchStep = shiftedSwitch,
            .pulseOnStart = state.inOrbRange
        });
    }

    // 3. Evaluate all candidate trajectories through PhysicsEngine
    float bestScore = -std::numeric_limits<float>::infinity();
    InputPlan bestPlan{};
    SimulationResult bestResult{};
    bool anySurvived = false;

    for (auto const& plan : m_candidateBuffer) {
        SimulationResult sim = physics.simulateRollout(
            state,
            plan,
            lookaheadSec,
            simDt,
            analyzer,
            m_config.corridorWeight
        );

        float modeBonus = modeCtrl.evaluateModeHeuristic(
            state,
            plan,
            sim,
            aheadCorridor,
            guidance.targetYBias
        );

        float learningBonus = 0.0f;
        if (m_config.learningEnabled) {
            if (guidance.hasLearnedAction && plan.initialHold == guidance.recommendedHold) {
                learningBonus += 32.0f * guidance.learnedReliability * m_config.learningInfluence;
            }
            if (guidance.hasFailureZone) {
                float holdSign = plan.initialHold ? 1.0f : -1.0f;
                learningBonus += holdSign * guidance.preferHoldBias * 26.0f * m_config.learningInfluence;
            }
        }

        // Pattern-aware tuning
        float patternBonus = 0.0f;
        if (upcomingPattern.type == PatternType::OrbChain && sim.triggeredOrb && sim.survived) {
            patternBonus += 28.0f;
        } else if (upcomingPattern.type == PatternType::NarrowCorridor && sim.survived) {
            patternBonus -= sim.corridorDeviation * 0.25f;
        }

        float totalScore = sim.score + modeBonus + learningBonus + patternBonus;
        if (sim.survived) anySurvived = true;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestPlan = plan;
            bestResult = sim;
        }
        ++outTelemetry.candidatesEvaluated;
    }

    // 4. Construct final ActionDecision
    ActionDecision decision{};
    decision.holdInput = bestPlan.initialHold;
    decision.activePattern = upcomingPattern.type;
    decision.predictedSurvivalSec = static_cast<float>(bestResult.stepsSurvived) * simDt;

    // Tap-based modes & orbs require a clean release edge when re-tapping
    if (bestPlan.pulseOnStart && state.isHolding &&
        (modeCtrl.prefersTapRelease(state) || state.inOrbRange)) {
        decision.forcePulseRelease = true;
    }

    if (anySurvived) {
        decision.confidence = std::clamp(0.55f + (bestResult.minHazardDistance / 80.0f) * 0.45f, 0.55f, 1.0f);
        decision.reason = std::string(gameModeToString(state.mode)) +
            (decision.holdInput ? " HOLD" : " RELEASE") +
            " [" + patternTypeToString(upcomingPattern.type) + "]";
    } else {
        outTelemetry.emergencyFallback = true;
        decision.confidence = std::clamp(decision.predictedSurvivalSec / lookaheadSec, 0.05f, 0.50f);
        decision.reason = "RECOVERY (" + std::string(gameModeToString(state.mode)) + ")";
    }

    auto endClock = std::chrono::steady_clock::now();
    outTelemetry.computeTimeMs =
        std::chrono::duration<double, std::milli>(endClock - startClock).count();
    outTelemetry.bestScore = bestScore;

    m_lastDecision = decision;
    m_lastTelemetry = outTelemetry;

    if (m_config.debugLogging) {
        DebugLog::get().debug(
            "AIEngine",
            decision.reason +
            " | conf=" + std::to_string(static_cast<int>(decision.confidence * 100.0f)) + "%" +
            " | sim=" + std::to_string(outTelemetry.candidatesEvaluated) +
            " | score=" + std::to_string(static_cast<int>(bestScore))
        );
    }

    return decision;
}

} // namespace gdautopilot
