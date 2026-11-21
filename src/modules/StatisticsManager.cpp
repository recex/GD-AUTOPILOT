#include "modules/StatisticsManager.hpp"

#include <algorithm>

namespace gdautopilot {

StatisticsManager::StatisticsManager() {
    resetSession();
}

void StatisticsManager::resetSession() {
    m_snapshot = StatisticsSnapshot{};
    m_recentAttempts.clear();
    m_decisionTimeSumMs = 0.0;
    m_decisionSamples = 0;
}

void StatisticsManager::onLevelEntered(int levelID, std::string const& levelName) {
    if (m_snapshot.activeLevelID != levelID) {
        m_snapshot.activeLevelID = levelID;
        m_snapshot.activeLevelName = levelName.empty() ? "Unnamed Level" : levelName;
        m_snapshot.bestProgress = 0.0f;
        m_snapshot.totalAttempts = 0;
        m_snapshot.completedRuns = 0;
        m_recentAttempts.clear();
    }
    m_snapshot.currentProgress = 0.0f;
    m_snapshot.currentAttemptTimeSec = 0.0;
}

void StatisticsManager::onAttemptStart() {
    ++m_snapshot.totalAttempts;
    m_snapshot.currentProgress = 0.0f;
    m_snapshot.currentAttemptTimeSec = 0.0;
}

void StatisticsManager::onTick(float progressPercent, GameMode mode, AIState state, double dtSec) {
    m_snapshot.currentProgress = std::clamp(progressPercent, 0.0f, 100.0f);
    m_snapshot.bestProgress = std::max(m_snapshot.bestProgress, m_snapshot.currentProgress);
    m_snapshot.currentMode = mode;
    m_snapshot.aiState = state;
    if (dtSec > 0.0 && dtSec < 0.5) {
        m_snapshot.sessionRunTimeSec += dtSec;
        m_snapshot.currentAttemptTimeSec += dtSec;
    }
}

void StatisticsManager::onDecisionComputed(double decisionMs, int simulationsRun) {
    ++m_snapshot.totalDecisions;
    m_snapshot.totalSimulations += static_cast<uint64_t>(std::max(0, simulationsRun));
    m_decisionTimeSumMs += decisionMs;
    ++m_decisionSamples;
    m_snapshot.avgDecisionTimeMs = m_decisionTimeSumMs / static_cast<double>(m_decisionSamples);
}

void StatisticsManager::onDeath(float progressPercent, GameMode mode) {
    float clamped = std::clamp(progressPercent, 0.0f, 100.0f);
    m_snapshot.bestProgress = std::max(m_snapshot.bestProgress, clamped);
    m_recentAttempts.push_back(AttemptRecord{
        .attemptNumber = m_snapshot.totalAttempts,
        .progressPercent = clamped,
        .deathMode = mode,
        .durationSec = m_snapshot.currentAttemptTimeSec,
        .completed = false
    });
    if (m_recentAttempts.size() > 50) {
        m_recentAttempts.erase(m_recentAttempts.begin());
    }
}

void StatisticsManager::onLevelComplete() {
    ++m_snapshot.completedRuns;
    m_snapshot.currentProgress = 100.0f;
    m_snapshot.bestProgress = 100.0f;
    m_snapshot.aiState = AIState::Completed;
    m_recentAttempts.push_back(AttemptRecord{
        .attemptNumber = m_snapshot.totalAttempts,
        .progressPercent = 100.0f,
        .deathMode = m_snapshot.currentMode,
        .durationSec = m_snapshot.currentAttemptTimeSec,
        .completed = true
    });
    if (m_recentAttempts.size() > 50) {
        m_recentAttempts.erase(m_recentAttempts.begin());
    }
}

void StatisticsManager::onLearningCountsUpdated(int failureZones, int learnedActions) {
    m_snapshot.learnedZonesCount = failureZones;
    m_snapshot.learnedStrategiesCount = learnedActions;
}

StatisticsSnapshot StatisticsManager::getSnapshot() const {
    return m_snapshot;
}

} // namespace gdautopilot
