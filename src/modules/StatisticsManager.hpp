#pragma once

#include "core/Types.hpp"

#include <chrono>
#include <deque>
#include <string>
#include <vector>

namespace gdautopilot {

class StatisticsManager {
public:
    StatisticsManager();

    void onLevelEntered(int levelID, std::string const& levelName);
    void onAttemptStart();
    void onTick(float progressPercent, GameMode mode, AIState state, double dtSec);
    void onDecisionComputed(double decisionMs, int simulationsRun);
    void onDeath(float progressPercent, GameMode mode);
    void onLevelComplete();
    void onLearningCountsUpdated(int failureZones, int learnedActions);

    void resetSession();

    [[nodiscard]] StatisticsSnapshot getSnapshot() const;
    [[nodiscard]] std::vector<AttemptRecord> const& getRecentAttempts() const noexcept { return m_recentAttempts; }

private:
    StatisticsSnapshot m_snapshot{};
    std::vector<AttemptRecord> m_recentAttempts;
    double m_decisionTimeSumMs = 0.0;
    uint64_t m_decisionSamples = 0;
};

} // namespace gdautopilot
