#include "modules/LevelAnalyzer.hpp"
#include "core/DebugLog.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdautopilot {

bool LevelAnalyzer::classifyObject(
    int uniqueIndex,
    int objectID,
    int rawObjectType,
    Rect const& hitbox,
    float rotation,
    bool slopeUphill,
    bool isPassable,
    bool isDisabled,
    LevelObject& outObject
) {
    if (isDisabled) return false;
    if (hitbox.width() <= 0.01f && hitbox.height() <= 0.01f) return false;

    outObject = LevelObject{};
    outObject.uniqueIndex = uniqueIndex;
    outObject.objectID = objectID;
    outObject.hitbox = hitbox;
    outObject.rotation = rotation;
    outObject.slopeUphill = slopeUphill;
    outObject.isPassable = isPassable;
    outObject.isDisabled = isDisabled;

    // 1. Canonical Geometry Dash 2.2 Object IDs for interactive mechanics (portals, orbs, pads, speeds)
    switch (objectID) {
        // Mode Portals
        case 12:   outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Cube;   return true;
        case 13:   outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Ship;   return true;
        case 47:   outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Ball;   return true;
        case 111:  outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::UFO;    return true;
        case 660:  outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Wave;   return true;
        case 745:  outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Robot;  return true;
        case 1331: outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Spider; return true;
        case 1933: outObject.category = ObjectCategory::PortalMode; outObject.targetMode = GameMode::Swing;  return true;

        // Gravity Portals
        case 10:   outObject.category = ObjectCategory::PortalGravity; outObject.gravityDirection = +1; return true; // Normal
        case 11:   outObject.category = ObjectCategory::PortalGravity; outObject.gravityDirection = -1; return true; // Upside down
        case 2926: outObject.category = ObjectCategory::PortalGravity; outObject.gravityDirection = 2;  return true; // Green toggle

        // Size Portals
        case 99:   outObject.category = ObjectCategory::PortalSize; outObject.setsNormalSize = true; return true;
        case 101:  outObject.category = ObjectCategory::PortalSize; outObject.setsMini = true; outObject.targetMini = true; return true;

        // Dual Portals
        case 286:
        case 287:
            outObject.category = ObjectCategory::PortalDual;
            return true;

        // Speed Portals
        case 200:  outObject.category = ObjectCategory::PortalSpeed; outObject.targetSpeed = SpeedTier::Slow05x;   return true;
        case 201:  outObject.category = ObjectCategory::PortalSpeed; outObject.targetSpeed = SpeedTier::Normal1x;  return true;
        case 202:  outObject.category = ObjectCategory::PortalSpeed; outObject.targetSpeed = SpeedTier::Fast2x;    return true;
        case 203:  outObject.category = ObjectCategory::PortalSpeed; outObject.targetSpeed = SpeedTier::Faster3x;  return true;
        case 1334: outObject.category = ObjectCategory::PortalSpeed; outObject.targetSpeed = SpeedTier::Fastest4x; return true;

        // Jump Pads
        case 35:   outObject.category = ObjectCategory::Pad; outObject.padType = PadType::Yellow; return true;
        case 67:   outObject.category = ObjectCategory::Pad; outObject.padType = PadType::Blue;   return true;
        case 140:  outObject.category = ObjectCategory::Pad; outObject.padType = PadType::Pink;   return true;
        case 1332: outObject.category = ObjectCategory::Pad; outObject.padType = PadType::Red;    return true;
        case 3005: outObject.category = ObjectCategory::Pad; outObject.padType = PadType::Spider; return true;

        // Jump Orbs / Rings
        case 36:   outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Yellow;    return true;
        case 84:   outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Blue;      return true;
        case 141:  outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Pink;      return true;
        case 1022: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Green;     return true;
        case 1333: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Red;       return true;
        case 1594: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Black;     return true;
        case 1704: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::DashGreen; return true;
        case 1751: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::DashPink;  return true;
        case 3004: outObject.category = ObjectCategory::Orb; outObject.orbType = OrbType::Spider;    return true;

        default:
            break;
    }

    // 2. Fallback classification via Geometry Dash GameObjectType integer values:
    //    Solid = 0, Hazard = 2, Slope = 5, Breakable = 37, AnimatedHazard = 39
    switch (rawObjectType) {
        case 0:  // Solid
        case 37: // Breakable solid
            outObject.category = ObjectCategory::Solid;
            return true;
        case 5:  // Slope
            outObject.category = ObjectCategory::Slope;
            return true;
        case 2:  // Hazard
        case 39: // AnimatedHazard
            outObject.category = ObjectCategory::Hazard;
            return true;
        default:
            return false;
    }
}

void LevelAnalyzer::clear() {
    m_objects.clear();
    m_corridor.clear();
    m_patterns.clear();
    m_modeTransitions.clear();
    m_summary = LevelAnalysisSummary{};
    m_maxObjectWidth = 30.0f;
}

void LevelAnalyzer::analyze(std::vector<LevelObject> objects, float levelLength) {
    clear();

    m_objects = std::move(objects);
    std::sort(m_objects.begin(), m_objects.end(), [](LevelObject const& a, LevelObject const& b) {
        return a.hitbox.minX < b.hitbox.minX;
    });

    m_summary.totalObjects = static_cast<int>(m_objects.size());
    float maxObservedX = levelLength;

    for (auto const& obj : m_objects) {
        m_maxObjectWidth = std::max(m_maxObjectWidth, obj.hitbox.width());
        maxObservedX = std::max(maxObservedX, obj.hitbox.maxX);

        switch (obj.category) {
            case ObjectCategory::Solid:
            case ObjectCategory::Slope:
                ++m_summary.solidCount;
                break;
            case ObjectCategory::Hazard:
                ++m_summary.hazardCount;
                break;
            case ObjectCategory::Orb:
                ++m_summary.orbCount;
                break;
            case ObjectCategory::Pad:
                ++m_summary.padCount;
                break;
            case ObjectCategory::PortalMode:
                ++m_summary.portalCount;
                m_modeTransitions.push_back(ModeTransitionInfo{
                    .x = obj.hitbox.centerX(),
                    .y = obj.hitbox.centerY(),
                    .mode = obj.targetMode
                });
                break;
            case ObjectCategory::PortalGravity:
            case ObjectCategory::PortalSpeed:
            case ObjectCategory::PortalSize:
            case ObjectCategory::PortalDual:
                ++m_summary.portalCount;
                break;
            default:
                break;
        }
    }

    m_summary.levelLength = std::max(maxObservedX, 300.0f);

    buildCorridorMap();
    smoothCorridorTargets();
    detectPatterns();

    m_summary.corridorSamples = static_cast<int>(m_corridor.size());
    m_summary.patternCount = static_cast<int>(m_patterns.size());

    float lengthUnits = std::max(m_summary.levelLength / 300.0f, 1.0f);
    float hazardRatio = static_cast<float>(m_summary.hazardCount) / lengthUnits;
    float portalRatio = static_cast<float>(m_summary.portalCount) / lengthUnits;
    m_summary.estimatedDifficulty = std::clamp(hazardRatio * 1.4f + portalRatio * 2.5f, 1.0f, 10.0f);
    m_summary.analyzed = true;

    DebugLog::get().info(
        "LevelAnalyzer",
        "Analyzed " + std::to_string(m_summary.totalObjects) + " objects (" +
        std::to_string(m_summary.hazardCount) + " hazards, " +
        std::to_string(m_summary.orbCount) + " orbs, " +
        std::to_string(m_summary.portalCount) + " portals, " +
        std::to_string(m_summary.patternCount) + " patterns)."
    );
}

void LevelAnalyzer::queryRange(float minX, float maxX, std::vector<LevelObject const*>& outObjects) const {
    outObjects.clear();
    if (m_objects.empty()) return;

    float searchStart = minX - m_maxObjectWidth;
    auto it = std::lower_bound(
        m_objects.begin(),
        m_objects.end(),
        searchStart,
        [](LevelObject const& obj, float val) { return obj.hitbox.minX < val; }
    );

    for (; it != m_objects.end() && it->hitbox.minX <= maxX; ++it) {
        if (!it->isDisabled && it->hitbox.maxX >= minX) {
            outObjects.push_back(&(*it));
        }
    }
}

void LevelAnalyzer::buildCorridorMap() {
    m_corridorStartX = 0.0f;
    int numSamples = static_cast<int>(std::ceil(m_summary.levelLength / m_corridorStepX)) + 2;
    numSamples = std::clamp(numSamples, 1, 20000);
    m_corridor.resize(static_cast<size_t>(numSamples));

    std::vector<LevelObject const*> sliceObjs;
    sliceObjs.reserve(32);

    for (int i = 0; i < numSamples; ++i) {
        float x = m_corridorStartX + static_cast<float>(i) * m_corridorStepX;
        queryRange(x - 18.0f, x + 18.0f, sliceObjs);

        CorridorSample sample{};
        sample.x = x;
        sample.floorY = 0.0f;
        sample.ceilingY = 390.0f;

        std::vector<std::pair<float, float>> blockedIntervals;
        blockedIntervals.reserve(16);

        float orbY = -1.0f;
        float portalY = -1.0f;
        int hazardsHere = 0;

        for (auto const* obj : sliceObjs) {
            if (obj->category == ObjectCategory::Hazard) {
                ++hazardsHere;
                blockedIntervals.emplace_back(obj->hitbox.minY - 8.0f, obj->hitbox.maxY + 8.0f);
            } else if (obj->category == ObjectCategory::Solid || obj->category == ObjectCategory::Slope) {
                blockedIntervals.emplace_back(obj->hitbox.minY, obj->hitbox.maxY);
            } else if (obj->category == ObjectCategory::Orb) {
                sample.hasOrb = true;
                orbY = obj->hitbox.centerY();
            } else if (obj->category == ObjectCategory::PortalMode || obj->category == ObjectCategory::PortalGravity) {
                portalY = obj->hitbox.centerY();
            }
        }

        sample.hazardDensity = static_cast<float>(hazardsHere);

        // Merge blocked Y intervals to discover the widest open vertical passage
        if (!blockedIntervals.empty()) {
            std::sort(blockedIntervals.begin(), blockedIntervals.end());
            std::vector<std::pair<float, float>> merged;
            for (auto const& iv : blockedIntervals) {
                if (merged.empty() || iv.first > merged.back().second) {
                    merged.push_back(iv);
                } else {
                    merged.back().second = std::max(merged.back().second, iv.second);
                }
            }

            // Evaluate candidate open gaps between [0, 420]
            float prevTop = 0.0f;
            float bestGapCenter = 120.0f;
            float bestGapScore = -1e9f;
            float bestFloor = 0.0f;
            float bestCeiling = 390.0f;

            auto evalGap = [&](float lo, float hi, bool boundedBelow, bool boundedAbove) {
                float gapHeight = hi - lo;
                if (gapHeight < 28.0f) return;
                float center = 0.5f * (lo + hi);
                // Cap height reward so infinite sky above a roof doesn't overpower real enclosed corridors
                float effectiveHeight = std::min(gapHeight, 130.0f);
                float score = effectiveHeight;
                if (boundedBelow && boundedAbove && gapHeight >= 40.0f) {
                    score += 85.0f;
                }
                if (portalY >= 0.0f) {
                    score -= std::abs(center - portalY) * 0.55f;
                }
                if (orbY >= 0.0f) {
                    score -= std::abs(center - orbY) * 0.45f;
                }
                if (i > 0) {
                    score -= std::abs(center - m_corridor[static_cast<size_t>(i - 1)].targetY) * 0.65f;
                }
                if (score > bestGapScore) {
                    bestGapScore = score;
                    bestGapCenter = center;
                    bestFloor = lo;
                    bestCeiling = hi;
                }
            };

            for (auto const& iv : merged) {
                evalGap(prevTop, iv.first, prevTop > 0.0f, true);
                prevTop = std::max(prevTop, iv.second);
            }
            evalGap(prevTop, 420.0f, prevTop > 0.0f, false);

            sample.floorY = bestFloor;
            sample.ceilingY = bestCeiling;
            sample.targetY = bestGapCenter;
        } else {
            sample.targetY = (i > 0) ? m_corridor[static_cast<size_t>(i - 1)].targetY : 90.0f;
            if (portalY >= 0.0f) sample.targetY = portalY;
            if (orbY >= 0.0f) sample.targetY = orbY;
        }

        // Detect floor gap if hazards sit at ground level
        if (sample.floorY <= 2.0f && hazardsHere > 0) {
            sample.hasFloorGap = true;
        }

        m_corridor[static_cast<size_t>(i)] = sample;
    }
}

void LevelAnalyzer::smoothCorridorTargets() {
    if (m_corridor.size() < 5) return;

    std::vector<float> smoothed(m_corridor.size(), 0.0f);
    for (size_t i = 0; i < m_corridor.size(); ++i) {
        float sum = 0.0f;
        float weightSum = 0.0f;
        for (int k = -2; k <= 2; ++k) {
            int idx = static_cast<int>(i) + k;
            if (idx < 0 || idx >= static_cast<int>(m_corridor.size())) continue;
            float w = (k == 0) ? 3.0f : (std::abs(k) == 1 ? 2.0f : 1.0f);
            sum += m_corridor[static_cast<size_t>(idx)].targetY * w;
            weightSum += w;
        }
        float avg = (weightSum > 0.0f) ? (sum / weightSum) : m_corridor[i].targetY;
        // Clamp smoothed target inside actual open gap
        float safeLo = m_corridor[i].floorY + 18.0f;
        float safeHi = m_corridor[i].ceilingY - 18.0f;
        if (safeLo < safeHi) {
            avg = std::clamp(avg, safeLo, safeHi);
        }
        smoothed[i] = avg;
    }

    for (size_t i = 0; i < m_corridor.size(); ++i) {
        m_corridor[i].targetY = smoothed[i];
    }
}

void LevelAnalyzer::detectPatterns() {
    if (m_objects.empty()) return;

    std::vector<bool> visited(m_objects.size(), false);

    // 1. Detect spike clusters, single spikes, orb chains, pads, gravity & mode transitions
    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (visited[i]) continue;
        auto const& obj = m_objects[i];

        if (obj.category == ObjectCategory::Hazard) {
            visited[i] = true;
            float startX = obj.hitbox.minX;
            float endX = obj.hitbox.maxX;
            float refY = obj.hitbox.centerY();
            int count = 1;

            for (size_t j = i + 1; j < m_objects.size() && m_objects[j].hitbox.minX - endX <= 42.0f; ++j) {
                if (!visited[j] && m_objects[j].category == ObjectCategory::Hazard &&
                    std::abs(m_objects[j].hitbox.centerY() - refY) <= 45.0f) {
                    visited[j] = true;
                    endX = std::max(endX, m_objects[j].hitbox.maxX);
                    ++count;
                }
            }

            m_patterns.push_back(ObstaclePattern{
                .type = (count >= 2) ? PatternType::SpikeCluster : PatternType::SingleSpike,
                .startX = startX,
                .endX = endX,
                .referenceY = refY,
                .objectCount = count,
                .difficultyScore = std::min(10.0f, static_cast<float>(count) * 2.2f)
            });
        } else if (obj.category == ObjectCategory::Orb) {
            visited[i] = true;
            float startX = obj.hitbox.minX;
            float endX = obj.hitbox.maxX;
            float refY = obj.hitbox.centerY();
            int count = 1;

            for (size_t j = i + 1; j < m_objects.size() && m_objects[j].hitbox.minX - endX <= 150.0f; ++j) {
                if (!visited[j] && m_objects[j].category == ObjectCategory::Orb) {
                    visited[j] = true;
                    endX = std::max(endX, m_objects[j].hitbox.maxX);
                    ++count;
                }
            }

            m_patterns.push_back(ObstaclePattern{
                .type = PatternType::OrbChain,
                .startX = startX,
                .endX = endX,
                .referenceY = refY,
                .objectCount = count,
                .difficultyScore = std::min(10.0f, 2.0f + static_cast<float>(count) * 1.8f)
            });
        } else if (obj.category == ObjectCategory::Pad) {
            visited[i] = true;
            m_patterns.push_back(ObstaclePattern{
                .type = PatternType::PadLaunch,
                .startX = obj.hitbox.minX,
                .endX = obj.hitbox.maxX,
                .referenceY = obj.hitbox.centerY(),
                .objectCount = 1,
                .difficultyScore = 1.5f
            });
        } else if (obj.category == ObjectCategory::PortalGravity) {
            visited[i] = true;
            m_patterns.push_back(ObstaclePattern{
                .type = PatternType::GravityTransition,
                .startX = obj.hitbox.minX,
                .endX = obj.hitbox.maxX,
                .referenceY = obj.hitbox.centerY(),
                .objectCount = 1,
                .difficultyScore = 3.0f
            });
        } else if (obj.category == ObjectCategory::PortalMode) {
            visited[i] = true;
            m_patterns.push_back(ObstaclePattern{
                .type = PatternType::ModeTransition,
                .startX = obj.hitbox.minX,
                .endX = obj.hitbox.maxX,
                .referenceY = obj.hitbox.centerY(),
                .objectCount = 1,
                .difficultyScore = 3.5f
            });
        }
    }

    // 2. Detect narrow corridors from the safe corridor map
    bool inNarrow = false;
    float narrowStart = 0.0f;
    float narrowAvgY = 0.0f;
    int narrowCount = 0;

    for (auto const& sample : m_corridor) {
        float gap = sample.ceilingY - sample.floorY;
        if (gap < 110.0f && gap > 25.0f) {
            if (!inNarrow) {
                inNarrow = true;
                narrowStart = sample.x;
                narrowAvgY = sample.targetY;
                narrowCount = 1;
            } else {
                narrowAvgY += sample.targetY;
                ++narrowCount;
            }
        } else if (inNarrow) {
            if (narrowCount >= 3) {
                m_patterns.push_back(ObstaclePattern{
                    .type = PatternType::NarrowCorridor,
                    .startX = narrowStart,
                    .endX = sample.x,
                    .referenceY = narrowAvgY / static_cast<float>(narrowCount),
                    .objectCount = narrowCount,
                    .difficultyScore = 5.5f
                });
            }
            inNarrow = false;
        }
    }

    std::sort(m_patterns.begin(), m_patterns.end(), [](ObstaclePattern const& a, ObstaclePattern const& b) {
        return a.startX < b.startX;
    });
}

CorridorSample LevelAnalyzer::sampleCorridor(float x) const {
    if (m_corridor.empty()) {
        return CorridorSample{.x = x, .floorY = 0.0f, .ceilingY = 390.0f, .targetY = 90.0f};
    }

    int idx = static_cast<int>(std::round((x - m_corridorStartX) / m_corridorStepX));
    idx = std::clamp(idx, 0, static_cast<int>(m_corridor.size()) - 1);
    return m_corridor[static_cast<size_t>(idx)];
}

ObstaclePattern LevelAnalyzer::findUpcomingPattern(float x, float maxLookahead) const {
    if (m_patterns.empty()) return ObstaclePattern{};

    auto it = std::lower_bound(
        m_patterns.begin(),
        m_patterns.end(),
        x - 20.0f,
        [](ObstaclePattern const& p, float val) { return p.endX < val; }
    );

    if (it != m_patterns.end() && it->startX <= x + maxLookahead) {
        return *it;
    }
    return ObstaclePattern{};
}

bool LevelAnalyzer::findNextModeTransition(float x, ModeTransitionInfo& outTransition) const {
    for (auto const& t : m_modeTransitions) {
        if (t.x >= x) {
            outTransition = t;
            return true;
        }
    }
    return false;
}

} // namespace gdautopilot
