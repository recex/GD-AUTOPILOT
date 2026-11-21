#pragma once

#include "core/Types.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace gdautopilot {

struct LevelAnalysisSummary {
    int totalObjects = 0;
    int solidCount = 0;
    int hazardCount = 0;
    int orbCount = 0;
    int padCount = 0;
    int portalCount = 0;
    int patternCount = 0;
    int corridorSamples = 0;
    float levelLength = 0.0f;
    float estimatedDifficulty = 0.0f;
    bool analyzed = false;
};

class LevelAnalyzer {
public:
    LevelAnalyzer() = default;

    // Classifies a raw Geometry Dash object using its canonical objectID and GameObjectType integer
    static bool classifyObject(
        int uniqueIndex,
        int objectID,
        int rawObjectType,
        Rect const& hitbox,
        float rotation,
        bool slopeUphill,
        bool isPassable,
        bool isDisabled,
        LevelObject& outObject
    );

    // Builds spatial indices, pattern map, mode transitions, and safe corridor representation
    void analyze(std::vector<LevelObject> objects, float levelLength);

    void clear();

    // Fast O(log N + k) spatial query for objects overlapping [minX, maxX]
    void queryRange(float minX, float maxX, std::vector<LevelObject const*>& outObjects) const;

    // Queries the closest safe corridor sample at horizontal coordinate x
    [[nodiscard]] CorridorSample sampleCorridor(float x) const;

    // Returns the active or next upcoming obstacle pattern within lookahead distance
    [[nodiscard]] ObstaclePattern findUpcomingPattern(float x, float maxLookahead) const;

    // Finds the next mode transition ahead of x (returns false if none)
    [[nodiscard]] bool findNextModeTransition(float x, ModeTransitionInfo& outTransition) const;

    [[nodiscard]] LevelAnalysisSummary const& getSummary() const noexcept { return m_summary; }
    [[nodiscard]] std::vector<ObstaclePattern> const& getPatterns() const noexcept { return m_patterns; }
    [[nodiscard]] std::vector<ModeTransitionInfo> const& getModeTransitions() const noexcept { return m_modeTransitions; }
    [[nodiscard]] std::vector<CorridorSample> const& getCorridor() const noexcept { return m_corridor; }
    [[nodiscard]] bool isReady() const noexcept { return m_summary.analyzed; }

private:
    void buildCorridorMap();
    void detectPatterns();
    void smoothCorridorTargets();

    std::vector<LevelObject> m_objects;
    std::vector<CorridorSample> m_corridor;
    std::vector<ObstaclePattern> m_patterns;
    std::vector<ModeTransitionInfo> m_modeTransitions;

    LevelAnalysisSummary m_summary{};
    float m_maxObjectWidth = 30.0f;
    float m_corridorStepX = 15.0f;
    float m_corridorStartX = 0.0f;
};

} // namespace gdautopilot
