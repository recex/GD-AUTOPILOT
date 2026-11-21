#include "modules/LearningSystem.hpp"
#include "core/DebugLog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace gdautopilot {

void LearningSystem::setStorageDirectory(std::filesystem::path const& dir) {
    m_storageDir = dir;
    if (!m_storageDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(m_storageDir, ec);
    }
}

std::filesystem::path LearningSystem::memoryFilePath() const {
    if (m_storageDir.empty()) return {};
    return m_storageDir / ("level_" + std::to_string(m_levelID) + ".gda");
}

void LearningSystem::loadForLevel(int levelID, std::string const& levelName) {
    m_levelID = levelID;
    m_levelName = levelName;
    m_failureZones.clear();
    m_learnedActions.clear();
    m_history.clear();
    clearCurrentAttemptTrace();
    m_attemptCounter = 0;

    auto path = memoryFilePath();
    if (path.empty() || !std::filesystem::exists(path)) {
        DebugLog::get().info("LearningSystem", "Initialized fresh memory for level " + std::to_string(levelID));
        return;
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        DebugLog::get().warn("LearningSystem", "Unable to open memory file: " + path.string());
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "FZ") {
            FailureZone z{};
            int modeInt = 0;
            if (iss >> z.centerX >> z.radius >> z.failureCount >> modeInt
                    >> z.timingOffsetBias >> z.preferHoldBias >> z.targetYBias >> z.currentStrategyVariant) {
                z.mode = static_cast<GameMode>(std::clamp(modeInt, 0, 7));
                m_failureZones.push_back(z);
            }
        } else if (tag == "LA") {
            LearnedAction a{};
            int holdInt = 0;
            int modeInt = 0;
            if (iss >> a.triggerX >> holdInt >> modeInt >> a.successCount >> a.reliability) {
                a.hold = (holdInt != 0);
                a.mode = static_cast<GameMode>(std::clamp(modeInt, 0, 7));
                m_learnedActions.push_back(a);
            }
        }
    }

    DebugLog::get().info(
        "LearningSystem",
        "Loaded memory for level " + std::to_string(levelID) + ": " +
        std::to_string(m_failureZones.size()) + " failure zones, " +
        std::to_string(m_learnedActions.size()) + " learned actions."
    );
}

void LearningSystem::save() const {
    auto path = memoryFilePath();
    if (path.empty()) return;

    std::error_code ec;
    std::filesystem::create_directories(m_storageDir, ec);

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        DebugLog::get().error("LearningSystem", "Failed to save learning memory to " + path.string());
        return;
    }

    out << "# GD AUTOPILOT Learning Memory v1\n";
    out << "# LevelID " << m_levelID << "\n";

    for (auto const& z : m_failureZones) {
        out << "FZ "
            << z.centerX << ' '
            << z.radius << ' '
            << z.failureCount << ' '
            << static_cast<int>(z.mode) << ' '
            << z.timingOffsetBias << ' '
            << z.preferHoldBias << ' '
            << z.targetYBias << ' '
            << z.currentStrategyVariant << '\n';
    }

    for (auto const& a : m_learnedActions) {
        out << "LA "
            << a.triggerX << ' '
            << (a.hold ? 1 : 0) << ' '
            << static_cast<int>(a.mode) << ' '
            << a.successCount << ' '
            << a.reliability << '\n';
    }
}

void LearningSystem::clearCurrentAttemptTrace() {
    m_currentAttemptTrace.clear();
    m_lastTraceX = -1000.0f;
}

void LearningSystem::resetLevelMemory() {
    m_failureZones.clear();
    m_learnedActions.clear();
    m_history.clear();
    clearCurrentAttemptTrace();
    save();
    DebugLog::get().info("LearningSystem", "Cleared learned memory for level " + std::to_string(m_levelID));
}

void LearningSystem::recordAttemptAction(float x, bool hold, GameMode mode) {
    if (!m_enabled) return;
    // Sample state transitions or every 24 world units to keep memory compact
    bool stateChanged = m_currentAttemptTrace.empty() || (m_currentAttemptTrace.back().hold != hold);
    if (!stateChanged && (x - m_lastTraceX) < 24.0f) {
        return;
    }
    m_currentAttemptTrace.push_back(TracePoint{.x = x, .hold = hold, .mode = mode});
    m_lastTraceX = x;
}

void LearningSystem::promoteTraceUpTo(float safeMaxX, float reliabilityBoost) {
    for (auto const& pt : m_currentAttemptTrace) {
        if (pt.x > safeMaxX) break;

        // Find existing learned action within 20 units
        auto it = std::find_if(m_learnedActions.begin(), m_learnedActions.end(), [&](LearnedAction const& a) {
            return std::abs(a.triggerX - pt.x) <= 20.0f && a.mode == pt.mode;
        });

        if (it != m_learnedActions.end()) {
            if (it->hold == pt.hold) {
                ++it->successCount;
                it->reliability = std::min(1.0f, it->reliability + reliabilityBoost);
            } else if (reliabilityBoost >= 0.35f) {
                // Winning run overrides conflicting lower-confidence action
                it->hold = pt.hold;
                it->successCount = 1;
                it->reliability = std::clamp(reliabilityBoost, 0.35f, 1.0f);
            }
        } else {
            m_learnedActions.push_back(LearnedAction{
                .triggerX = pt.x,
                .hold = pt.hold,
                .mode = pt.mode,
                .successCount = 1,
                .reliability = std::clamp(reliabilityBoost, 0.25f, 1.0f)
            });
        }
    }

    // Cap stored actions for bounded memory
    if (m_learnedActions.size() > 2500) {
        std::sort(m_learnedActions.begin(), m_learnedActions.end(), [](LearnedAction const& a, LearnedAction const& b) {
            return a.reliability > b.reliability;
        });
        m_learnedActions.resize(2000);
    }
}

void LearningSystem::recordDeath(float deathX, float deathY, float progressPercent, GameMode mode, double attemptDurationSec) {
    ++m_attemptCounter;
    m_history.push_back(AttemptRecord{
        .attemptNumber = m_attemptCounter,
        .progressPercent = progressPercent,
        .deathX = deathX,
        .deathY = deathY,
        .deathMode = mode,
        .durationSec = attemptDurationSec,
        .completed = false,
        .strategyId = 0
    });
    if (m_history.size() > 100) {
        m_history.erase(m_history.begin());
    }

    if (!m_enabled) {
        clearCurrentAttemptTrace();
        return;
    }

    // Promote decisions well before the crash as validated segments
    if (deathX > 180.0f) {
        promoteTraceUpTo(deathX - 130.0f, 0.12f);
    }

    // Penalize any learned action right inside the crash window
    for (auto& act : m_learnedActions) {
        if (std::abs(act.triggerX - deathX) <= 65.0f) {
            act.reliability = std::max(0.0f, act.reliability - 0.35f);
        }
    }

    // Cluster into an existing FailureZone or create a new one centered slightly before deathX
    float zoneX = std::max(0.0f, deathX - 42.0f);
    FailureZone* zone = nullptr;
    for (auto& z : m_failureZones) {
        if (std::abs(z.centerX - zoneX) <= 60.0f && z.mode == mode) {
            zone = &z;
            break;
        }
    }

    if (!zone) {
        m_failureZones.push_back(FailureZone{
            .centerX = zoneX,
            .radius = 56.0f,
            .failureCount = 0,
            .mode = mode
        });
        zone = &m_failureZones.back();
    }

    ++zone->failureCount;
    zone->currentStrategyVariant = (zone->currentStrategyVariant + 1) % 6;

    // Cycle adaptive strategy parameters to break out of repetitive deaths
    switch (zone->currentStrategyVariant) {
        case 0:
            zone->timingOffsetBias = -0.04f;
            zone->preferHoldBias = +0.45f;
            zone->targetYBias = +18.0f;
            break;
        case 1:
            zone->timingOffsetBias = +0.04f;
            zone->preferHoldBias = -0.45f;
            zone->targetYBias = -18.0f;
            break;
        case 2:
            zone->timingOffsetBias = -0.08f;
            zone->preferHoldBias = +0.75f;
            zone->targetYBias = +32.0f;
            break;
        case 3:
            zone->timingOffsetBias = +0.08f;
            zone->preferHoldBias = -0.75f;
            zone->targetYBias = -32.0f;
            break;
        case 4:
            zone->timingOffsetBias = -0.02f;
            zone->preferHoldBias = +0.25f;
            zone->targetYBias = +10.0f;
            break;
        case 5:
        default:
            zone->timingOffsetBias = +0.02f;
            zone->preferHoldBias = -0.25f;
            zone->targetYBias = -10.0f;
            break;
    }

    DebugLog::get().info(
        "LearningSystem",
        "Recorded death at X=" + std::to_string(static_cast<int>(deathX)) +
        " (" + std::string(gameModeToString(mode)) + "), zone failures=" +
        std::to_string(zone->failureCount) + ", switching to strategy #" +
        std::to_string(zone->currentStrategyVariant)
    );

    clearCurrentAttemptTrace();
    save();
}

void LearningSystem::recordCompletion(double attemptDurationSec) {
    ++m_attemptCounter;
    m_history.push_back(AttemptRecord{
        .attemptNumber = m_attemptCounter,
        .progressPercent = 100.0f,
        .deathX = 0.0f,
        .deathY = 0.0f,
        .deathMode = GameMode::Cube,
        .durationSec = attemptDurationSec,
        .completed = true,
        .strategyId = 0
    });

    if (m_enabled) {
        promoteTraceUpTo(1e9f, 0.55f);
        save();
        DebugLog::get().info("LearningSystem", "Level completed! Promoted full winning trajectory to memory.");
    }
    clearCurrentAttemptTrace();
}

AdaptiveGuidance LearningSystem::queryGuidance(float x, GameMode mode) const {
    AdaptiveGuidance out{};
    if (!m_enabled) return out;

    // 1. Check failure zones
    for (auto const& z : m_failureZones) {
        if (z.mode == mode && std::abs(x - z.centerX) <= z.radius) {
            out.hasFailureZone = true;
            out.timingOffsetBias = z.timingOffsetBias;
            out.preferHoldBias = z.preferHoldBias;
            out.targetYBias = z.targetYBias;
            out.strategyVariant = z.currentStrategyVariant;
            break;
        }
    }

    // 2. Check high-reliability learned actions
    float bestDist = 26.0f;
    for (auto const& a : m_learnedActions) {
        if (a.mode != mode || a.reliability < 0.35f) continue;
        float dist = std::abs(x - a.triggerX);
        if (dist < bestDist) {
            bestDist = dist;
            out.hasLearnedAction = true;
            out.recommendedHold = a.hold;
            out.learnedReliability = a.reliability;
        }
    }

    return out;
}

} // namespace gdautopilot
