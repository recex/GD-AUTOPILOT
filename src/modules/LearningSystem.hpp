#pragma once

#include "core/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace gdautopilot {

struct AdaptiveGuidance {
    bool hasFailureZone = false;
    bool hasLearnedAction = false;
    float timingOffsetBias = 0.0f;
    float preferHoldBias = 0.0f;
    float targetYBias = 0.0f;
    bool recommendedHold = false;
    float learnedReliability = 0.0f;
    int strategyVariant = 0;
};

class LearningSystem {
public:
    LearningSystem() = default;

    // Configures persistence directory and active level memory file
    void setStorageDirectory(std::filesystem::path const& dir);
    void loadForLevel(int levelID, std::string const& levelName);
    void save() const;

    void setEnabled(bool enabled) noexcept { m_enabled = enabled; }
    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled; }

    // Records an action taken during the active attempt so surviving decisions can be promoted
    void recordAttemptAction(float x, bool hold, GameMode mode);

    // Called when the player dies: clusters failure zone and rotates adaptive strategy bias
    void recordDeath(float deathX, float deathY, float progressPercent, GameMode mode, double attemptDurationSec);

    // Called when the level is completed: promotes the full attempt's actions as high-reliability strategies
    void recordCompletion(double attemptDurationSec);

    // Queries learned biases and winning actions for the current horizontal position
    [[nodiscard]] AdaptiveGuidance queryGuidance(float x, GameMode mode) const;

    void clearCurrentAttemptTrace();
    void resetLevelMemory();

    [[nodiscard]] std::vector<FailureZone> const& getFailureZones() const noexcept { return m_failureZones; }
    [[nodiscard]] std::vector<LearnedAction> const& getLearnedActions() const noexcept { return m_learnedActions; }
    [[nodiscard]] std::vector<AttemptRecord> const& getAttemptHistory() const noexcept { return m_history; }
    [[nodiscard]] int getActiveLevelID() const noexcept { return m_levelID; }

private:
    struct TracePoint {
        float x = 0.0f;
        bool hold = false;
        GameMode mode = GameMode::Cube;
    };

    void promoteTraceUpTo(float safeMaxX, float reliabilityBoost);
    [[nodiscard]] std::filesystem::path memoryFilePath() const;

    bool m_enabled = true;
    int m_levelID = 0;
    std::string m_levelName;
    std::filesystem::path m_storageDir;

    std::vector<FailureZone> m_failureZones;
    std::vector<LearnedAction> m_learnedActions;
    std::vector<AttemptRecord> m_history;
    std::vector<TracePoint> m_currentAttemptTrace;
    float m_lastTraceX = -1000.0f;
    int m_attemptCounter = 0;
};

} // namespace gdautopilot
