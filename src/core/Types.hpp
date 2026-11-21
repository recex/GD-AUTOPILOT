#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace gdautopilot {

enum class GameMode : uint8_t {
    Cube = 0,
    Ship,
    Ball,
    UFO,
    Wave,
    Robot,
    Spider,
    Swing
};

enum class SpeedTier : uint8_t {
    Slow05x = 0,   // ~251.16 units/s
    Normal1x = 1,  // ~311.58 units/s
    Fast2x = 2,    // ~387.42 units/s
    Faster3x = 3,  // ~468.00 units/s
    Fastest4x = 4  // ~576.00 units/s
};

enum class ObjectCategory : uint8_t {
    Unknown = 0,
    Solid,
    Slope,
    Hazard,
    Orb,
    Pad,
    PortalMode,
    PortalGravity,
    PortalSpeed,
    PortalSize,
    PortalDual
};

enum class OrbType : uint8_t {
    None = 0,
    Yellow,
    Pink,
    Red,
    Blue,
    Green,
    Black,
    DashGreen,
    DashPink,
    Spider
};

enum class PadType : uint8_t {
    None = 0,
    Yellow,
    Pink,
    Red,
    Blue,
    Spider
};

enum class AIState : uint8_t {
    Idle = 0,
    Analyzing,
    Running,
    Recovering,
    Learning,
    Completed,
    Paused,
    Error
};

enum class PatternType : uint8_t {
    None = 0,
    SingleSpike,
    SpikeCluster,
    PlatformGap,
    OrbChain,
    PadLaunch,
    NarrowCorridor,
    GravityTransition,
    ModeTransition,
    WaveTunnel
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    [[nodiscard]] float width() const noexcept { return maxX - minX; }
    [[nodiscard]] float height() const noexcept { return maxY - minY; }
    [[nodiscard]] float centerX() const noexcept { return 0.5f * (minX + maxX); }
    [[nodiscard]] float centerY() const noexcept { return 0.5f * (minY + maxY); }

    [[nodiscard]] bool intersects(Rect const& o) const noexcept {
        return minX < o.maxX && maxX > o.minX && minY < o.maxY && maxY > o.minY;
    }

    [[nodiscard]] Rect expanded(float marginX, float marginY) const noexcept {
        return Rect{minX - marginX, minY - marginY, maxX + marginX, maxY + marginY};
    }
};

struct LevelObject {
    int uniqueIndex = 0;
    int objectID = 0;
    ObjectCategory category = ObjectCategory::Unknown;
    Rect hitbox{};
    float rotation = 0.0f;
    bool slopeUphill = true;
    bool isPassable = false;
    bool isDisabled = false;

    // Specialized metadata depending on category
    GameMode targetMode = GameMode::Cube;
    SpeedTier targetSpeed = SpeedTier::Normal1x;
    OrbType orbType = OrbType::None;
    PadType padType = PadType::None;
    int gravityDirection = 0; // +1 = normal, -1 = upsideDown, 2 = toggle
    bool targetMini = false;
    bool setsMini = false;
    bool setsNormalSize = false;
};

struct CorridorSample {
    float x = 0.0f;
    float floorY = 0.0f;
    float ceilingY = 360.0f;
    float targetY = 120.0f;
    float hazardDensity = 0.0f;
    bool hasFloorGap = false;
    bool hasOrb = false;
};

struct ObstaclePattern {
    PatternType type = PatternType::None;
    float startX = 0.0f;
    float endX = 0.0f;
    float referenceY = 0.0f;
    int objectCount = 0;
    float difficultyScore = 0.0f;
};

struct ModeTransitionInfo {
    float x = 0.0f;
    float y = 0.0f;
    GameMode mode = GameMode::Cube;
};

struct PlayerState {
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 311.58f;
    float velocityY = 0.0f;
    float gravityMod = 1.0f;
    float vehicleSize = 1.0f; // 1.0 normal, ~0.6 mini
    float playerSpeed = 0.9f; // GD m_playerSpeed factor
    GameMode mode = GameMode::Cube;
    SpeedTier speedTier = SpeedTier::Normal1x;
    bool isUpsideDown = false;
    bool isOnGround = true;
    bool isOnSlope = false;
    bool isHolding = false;
    bool isDashing = false;
    bool isDead = false;
    bool isGoingLeft = false;
    bool isPlatformer = false;
    bool inOrbRange = false;
    int activeOrbIndex = -1;
    double robotHoldTime = 0.0;
    double tickTime = 0.0;
    float floorY = 0.0f;
    float ceilingY = 390.0f;

    [[nodiscard]] bool isMini() const noexcept {
        return vehicleSize < 0.85f;
    }

    [[nodiscard]] float halfSize() const noexcept {
        return isMini() ? 9.0f : 15.0f;
    }

    [[nodiscard]] Rect hitbox() const noexcept {
        float h = halfSize();
        return Rect{x - h, y - h, x + h, y + h};
    }

    // Inner hazard hitbox in GD is smaller than outer solid hitbox
    [[nodiscard]] Rect hazardHitbox() const noexcept {
        float h = isMini() ? 5.5f : 9.0f;
        return Rect{x - h, y - h, x + h, y + h};
    }
};

struct PhysicsParams {
    float fixedStepDt = 1.0f / 240.0f;
    float baseGravity = 2660.0f;        // units/s^2 (scaled per mode)
    float cubeJumpVelocity = 620.0f;    // units/s
    float shipThrustAccel = 1850.0f;    // units/s^2
    float shipGravityAccel = 1480.0f;   // units/s^2
    float shipMaxVelY = 440.0f;
    float ufoJumpVelocity = 520.0f;
    float ufoGravityAccel = 2050.0f;
    float ballGravityAccel = 2450.0f;
    float waveSlopeFactor = 1.0f;       // 45 deg normal, ~63 deg mini
    float robotMinJumpVel = 460.0f;
    float robotMaxBoostAccel = 1650.0f;
    float robotMaxHoldSec = 0.28f;
    float swingGravityAccel = 1620.0f;
    float swingMaxVelY = 430.0f;
    float hazardSafetyMargin = 4.0f;
    float groundHeight = 0.0f;
};

struct SimulationResult {
    bool survived = true;
    bool hitHazard = false;
    bool hitWall = false;
    bool triggeredOrb = false;
    bool triggeredPad = false;
    bool changedMode = false;
    int stepsSurvived = 0;
    float finalX = 0.0f;
    float finalY = 0.0f;
    float minHazardDistance = 9999.0f;
    float corridorDeviation = 0.0f;
    float score = 0.0f;
    PlayerState finalState{};
};

struct ActionDecision {
    bool holdInput = false;
    bool forcePulseRelease = false;
    float confidence = 0.0f;
    float predictedSurvivalSec = 0.0f;
    PatternType activePattern = PatternType::None;
    std::string reason;
};

struct AIConfig {
    bool enabled = false;
    bool showHudOverlay = true;
    bool learningEnabled = true;
    bool debugLogging = false;
    int analysisSpeedLevel = 2;      // 1 = Eco, 2 = Balanced, 3 = Aggressive, 4 = Ultra
    float lookaheadSeconds = 0.65f;  // Dynamic by analysisSpeedLevel
    float safetyMargin = 4.0f;
    float corridorWeight = 1.25f;
    float learningInfluence = 0.75f;
    int decisionStrideTicks = 2;     // Evaluate full search every N physics ticks

    void applyAnalysisSpeedPreset(int level) noexcept {
        analysisSpeedLevel = std::clamp(level, 1, 4);
        switch (analysisSpeedLevel) {
            case 1: // Eco
                lookaheadSeconds = 0.45f;
                decisionStrideTicks = 4;
                break;
            case 2: // Balanced
                lookaheadSeconds = 0.65f;
                decisionStrideTicks = 2;
                break;
            case 3: // Aggressive
                lookaheadSeconds = 0.85f;
                decisionStrideTicks = 1;
                break;
            case 4: // Ultra
                lookaheadSeconds = 1.05f;
                decisionStrideTicks = 1;
                break;
        }
    }
};

struct AttemptRecord {
    int attemptNumber = 0;
    float progressPercent = 0.0f;
    float deathX = 0.0f;
    float deathY = 0.0f;
    GameMode deathMode = GameMode::Cube;
    double durationSec = 0.0;
    bool completed = false;
    int strategyId = 0;
};

struct FailureZone {
    float centerX = 0.0f;
    float radius = 48.0f;
    int failureCount = 0;
    GameMode mode = GameMode::Cube;
    float timingOffsetBias = 0.0f; // Seconds shifted earlier (-) or later (+)
    float preferHoldBias = 0.0f;   // [-1.0, +1.0] bias towards hold vs release
    float targetYBias = 0.0f;      // Vertical adjustment in flying modes
    int currentStrategyVariant = 0;
};

struct LearnedAction {
    float triggerX = 0.0f;
    bool hold = false;
    GameMode mode = GameMode::Cube;
    int successCount = 0;
    float reliability = 0.0f;
};

struct StatisticsSnapshot {
    int totalAttempts = 0;
    int completedRuns = 0;
    float currentProgress = 0.0f;
    float bestProgress = 0.0f;
    double sessionRunTimeSec = 0.0;
    double currentAttemptTimeSec = 0.0;
    double avgDecisionTimeMs = 0.0;
    uint64_t totalDecisions = 0;
    uint64_t totalSimulations = 0;
    GameMode currentMode = GameMode::Cube;
    AIState aiState = AIState::Idle;
    std::string activeLevelName = "None";
    int activeLevelID = 0;
    int learnedZonesCount = 0;
    int learnedStrategiesCount = 0;
};

inline char const* gameModeToString(GameMode mode) noexcept {
    switch (mode) {
        case GameMode::Cube:   return "Cube";
        case GameMode::Ship:   return "Ship";
        case GameMode::Ball:   return "Ball";
        case GameMode::UFO:    return "UFO";
        case GameMode::Wave:   return "Wave";
        case GameMode::Robot:  return "Robot";
        case GameMode::Spider: return "Spider";
        case GameMode::Swing:  return "Swing";
    }
    return "Unknown";
}

inline char const* aiStateToString(AIState state) noexcept {
    switch (state) {
        case AIState::Idle:       return "IDLE";
        case AIState::Analyzing:  return "ANALYZING";
        case AIState::Running:    return "RUNNING";
        case AIState::Recovering: return "RECOVERING";
        case AIState::Learning:   return "LEARNING";
        case AIState::Completed:  return "COMPLETED";
        case AIState::Paused:     return "PAUSED";
        case AIState::Error:      return "ERROR";
    }
    return "UNKNOWN";
}

inline char const* patternTypeToString(PatternType p) noexcept {
    switch (p) {
        case PatternType::None:              return "Clear";
        case PatternType::SingleSpike:       return "Single Spike";
        case PatternType::SpikeCluster:      return "Spike Cluster";
        case PatternType::PlatformGap:       return "Platform Gap";
        case PatternType::OrbChain:          return "Orb Chain";
        case PatternType::PadLaunch:         return "Pad Launch";
        case PatternType::NarrowCorridor:    return "Narrow Corridor";
        case PatternType::GravityTransition: return "Gravity Flip";
        case PatternType::ModeTransition:    return "Mode Portal";
        case PatternType::WaveTunnel:        return "Wave Tunnel";
    }
    return "Unknown";
}

inline float speedTierToVelocity(SpeedTier tier) noexcept {
    switch (tier) {
        case SpeedTier::Slow05x:   return 251.16f;
        case SpeedTier::Normal1x:  return 311.58f;
        case SpeedTier::Fast2x:    return 387.42f;
        case SpeedTier::Faster3x:  return 468.00f;
        case SpeedTier::Fastest4x: return 576.00f;
    }
    return 311.58f;
}

inline SpeedTier playerSpeedFactorToTier(float speedFactor) noexcept {
    if (speedFactor < 0.78f) return SpeedTier::Slow05x;
    if (speedFactor < 1.01f) return SpeedTier::Normal1x;
    if (speedFactor < 1.21f) return SpeedTier::Fast2x;
    if (speedFactor < 1.45f) return SpeedTier::Faster3x;
    return SpeedTier::Fastest4x;
}

} // namespace gdautopilot
