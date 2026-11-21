#include "modules/GameController.hpp"

#include <algorithm>
#include <cmath>

namespace gdautopilot {

// ============================================================================
// 1. CUBE CONTROLLER
// ============================================================================
void CubeController::generateCandidates(
    PlayerState const& state,
    CorridorSample const& aheadCorridor,
    ObstaclePattern const& upcomingPattern,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    (void)aheadCorridor;
    (void)upcomingPattern;

    // 1. Do nothing (stay grounded / let current arc finish)
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1, .pulseOnStart = false});
    // 2. Jump / activate orb immediately and release shortly after
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = false, .switchStep = std::max(2, totalRolloutSteps / 8), .pulseOnStart = state.inOrbRange});
    // 3. Continuous hold (chains jumps / dash orbs)
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = true, .switchStep = -1, .pulseOnStart = state.inOrbRange});
    // 4. Delayed jump (early / mid / late timing options)
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(2, totalRolloutSteps / 5), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(4, totalRolloutSteps / 3), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(6, totalRolloutSteps / 2), .pulseOnStart = false});
}

float CubeController::evaluateModeHeuristic(
    PlayerState const& startState,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& /*aheadCorridor*/,
    float /*learningYBias*/
) const {
    float bonus = 0.0f;
    // Prefer staying calm (not jumping needlessly) when both jump and no-jump survive equally well
    if (!plan.initialHold && result.survived) {
        bonus += 14.0f;
    }
    // Reward landing safely on ground/platform at end of arc
    if (result.finalState.isOnGround && result.survived) {
        bonus += 8.0f;
    }
    // Strongly reward activating an orb if we're in orb range and it survives
    if (startState.inOrbRange && result.triggeredOrb && result.survived) {
        bonus += 24.0f;
    }
    return bonus;
}

// ============================================================================
// 2. SHIP CONTROLLER
// ============================================================================
void ShipController::generateCandidates(
    PlayerState const& /*state*/,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    int shortStep = std::max(2, totalRolloutSteps / 14);
    int midStep   = std::max(4, totalRolloutSteps / 6);
    int halfStep  = std::max(6, totalRolloutSteps / 2);

    // Closed-loop PD corridor followers starting with either HOLD or RELEASE (eliminates overshoot!)
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = shortStep, .closedLoopCorridor = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = shortStep, .closedLoopCorridor = true});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = midStep,   .closedLoopCorridor = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = midStep,   .closedLoopCorridor = true});

    // Open-loop emergency climbs/dives for dodging sudden obstacles
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = true,  .switchStep = -1});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = halfStep});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = halfStep});
}

float ShipController::evaluateModeHeuristic(
    PlayerState const& startState,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& aheadCorridor,
    float learningYBias
) const {
    float targetY = aheadCorridor.targetY + learningYBias;
    float finalError = std::abs(result.finalY - targetY);

    // PD controller style damping: penalize extreme vertical speed near target
    float velPenalty = std::abs(result.finalState.velocityY) * 0.045f;
    float smoothBonus = (plan.initialHold == startState.isHolding) ? 3.0f : 0.0f;

    return smoothBonus - finalError * 0.35f - velPenalty;
}

// ============================================================================
// 3. BALL CONTROLLER
// ============================================================================
void BallController::generateCandidates(
    PlayerState const& state,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1});
    // Flip gravity immediately (if grounded or touching orb)
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = false, .switchStep = std::max(2, totalRolloutSteps / 8), .pulseOnStart = state.isOnGround || state.inOrbRange});
    // Wait then flip
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(2, totalRolloutSteps / 4)});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(4, totalRolloutSteps / 2)});
}

float BallController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& /*aheadCorridor*/,
    float /*learningYBias*/
) const {
    float bonus = 0.0f;
    if (!plan.initialHold && result.survived) bonus += 12.0f;
    if (result.finalState.isOnGround && result.survived) bonus += 10.0f;
    return bonus;
}

// ============================================================================
// 4. UFO CONTROLLER
// ============================================================================
void UFOController::generateCandidates(
    PlayerState const& /*state*/,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    // UFO operates on discrete taps (rising edges)
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1, .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = 2,  .pulseOnStart = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(3, totalRolloutSteps / 4), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(5, totalRolloutSteps / 2), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(7, (totalRolloutSteps * 3) / 4), .pulseOnStart = false});
}

float UFOController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& aheadCorridor,
    float learningYBias
) const {
    float targetY = aheadCorridor.targetY + learningYBias;
    float err = std::abs(result.finalY - targetY);
    // Avoid over-tapping if coasting stays close to corridor center
    float tapPenalty = plan.initialHold ? 5.0f : 0.0f;
    return -err * 0.30f - tapPenalty;
}

// ============================================================================
// 5. WAVE CONTROLLER
// ============================================================================
void WaveController::generateCandidates(
    PlayerState const& /*state*/,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    int s1 = std::max(1, totalRolloutSteps / 6);
    int s2 = std::max(2, totalRolloutSteps / 3);
    int s3 = std::max(3, totalRolloutSteps / 2);

    // Closed-loop corridor zig-zag followers + open-loop tight diagonals for wave tunnels
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = s1, .closedLoopCorridor = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = s1, .closedLoopCorridor = true});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = true,  .switchStep = -1});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = s1});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = s1});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = s2});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = s2});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = s3});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = s3});
}

float WaveController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& /*plan*/,
    SimulationResult const& result,
    CorridorSample const& aheadCorridor,
    float learningYBias
) const {
    float targetY = aheadCorridor.targetY + learningYBias;
    float err = std::abs(result.finalY - targetY);
    float wallClearance = std::min(result.minHazardDistance, 50.0f) * 0.6f;
    return wallClearance - err * 0.42f;
}

// ============================================================================
// 6. ROBOT CONTROLLER
// ============================================================================
void RobotController::generateCandidates(
    PlayerState const& state,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    // Robot supports variable jump heights based on hold duration
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1});
    // Micro hop
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = false, .switchStep = std::max(2, totalRolloutSteps / 10), .pulseOnStart = state.inOrbRange});
    // Medium hop
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = false, .switchStep = std::max(4, totalRolloutSteps / 4), .pulseOnStart = state.inOrbRange});
    // Full max-height jump
    outPlans.push_back(InputPlan{.initialHold = true, .secondHold = false, .switchStep = std::max(6, totalRolloutSteps / 2), .pulseOnStart = state.inOrbRange});
    // Delayed medium jump
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true, .switchStep = std::max(3, totalRolloutSteps / 4)});
}

float RobotController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& /*aheadCorridor*/,
    float /*learningYBias*/
) const {
    float bonus = 0.0f;
    if (!plan.initialHold && result.survived) bonus += 12.0f;
    if (result.finalState.isOnGround && result.survived) bonus += 7.0f;
    return bonus;
}

// ============================================================================
// 7. SPIDER CONTROLLER
// ============================================================================
void SpiderController::generateCandidates(
    PlayerState const& /*state*/,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    // Spider teleports on tap when grounded
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1, .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = 2,  .pulseOnStart = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(3, totalRolloutSteps / 4), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(5, totalRolloutSteps / 2), .pulseOnStart = false});
}

float SpiderController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& /*aheadCorridor*/,
    float /*learningYBias*/
) const {
    float bonus = 0.0f;
    if (!plan.initialHold && result.survived) bonus += 15.0f;
    if (result.finalState.isOnGround && result.survived) bonus += 6.0f;
    return bonus;
}

// ============================================================================
// 8. SWING CONTROLLER
// ============================================================================
void SwingController::generateCandidates(
    PlayerState const& /*state*/,
    CorridorSample const& /*aheadCorridor*/,
    ObstaclePattern const& /*upcomingPattern*/,
    int totalRolloutSteps,
    float /*learningHoldBias*/,
    float /*learningYBias*/,
    std::vector<InputPlan>& outPlans
) const {
    // Swing inverts gravity on every tap in mid-air
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = false, .switchStep = -1, .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = true,  .secondHold = false, .switchStep = 2,  .pulseOnStart = true});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(3, totalRolloutSteps / 4), .pulseOnStart = false});
    outPlans.push_back(InputPlan{.initialHold = false, .secondHold = true,  .switchStep = std::max(6, totalRolloutSteps / 2), .pulseOnStart = false});
}

float SwingController::evaluateModeHeuristic(
    PlayerState const& /*startState*/,
    InputPlan const& plan,
    SimulationResult const& result,
    CorridorSample const& aheadCorridor,
    float learningYBias
) const {
    float targetY = aheadCorridor.targetY + learningYBias;
    float err = std::abs(result.finalY - targetY);
    float velPenalty = std::abs(result.finalState.velocityY) * 0.04f;
    float flipPenalty = plan.initialHold ? 6.0f : 0.0f;
    return -err * 0.35f - velPenalty - flipPenalty;
}

// ============================================================================
// GAME CONTROLLER ORCHESTRATOR
// ============================================================================
GameController::GameController() {
    m_controllers[static_cast<size_t>(GameMode::Cube)]   = std::make_unique<CubeController>();
    m_controllers[static_cast<size_t>(GameMode::Ship)]   = std::make_unique<ShipController>();
    m_controllers[static_cast<size_t>(GameMode::Ball)]   = std::make_unique<BallController>();
    m_controllers[static_cast<size_t>(GameMode::UFO)]    = std::make_unique<UFOController>();
    m_controllers[static_cast<size_t>(GameMode::Wave)]   = std::make_unique<WaveController>();
    m_controllers[static_cast<size_t>(GameMode::Robot)]  = std::make_unique<RobotController>();
    m_controllers[static_cast<size_t>(GameMode::Spider)] = std::make_unique<SpiderController>();
    m_controllers[static_cast<size_t>(GameMode::Swing)]  = std::make_unique<SwingController>();
}

IModeController const& GameController::getController(GameMode mode) const {
    size_t idx = static_cast<size_t>(mode);
    if (idx >= m_controllers.size()) idx = 0;
    return *m_controllers[idx];
}

bool GameController::computeInputDispatch(ActionDecision const& decision, GameMode /*mode*/, bool& outNewButtonDown) {
    // Handle forced release tick for tap-based mechanics (UFO, Spider, Swing, Orbs)
    if (decision.forcePulseRelease && m_buttonDown) {
        m_buttonDown = false;
        m_pendingPulseReengage = decision.holdInput;
        outNewButtonDown = false;
        return true;
    }

    if (m_pendingPulseReengage) {
        m_pendingPulseReengage = false;
        if (!m_buttonDown) {
            m_buttonDown = true;
            outNewButtonDown = true;
            return true;
        }
    }

    if (decision.holdInput != m_buttonDown) {
        m_buttonDown = decision.holdInput;
        outNewButtonDown = m_buttonDown;
        return true;
    }

    return false;
}

void GameController::resetInputState() noexcept {
    m_buttonDown = false;
    m_pendingPulseReengage = false;
}

} // namespace gdautopilot
