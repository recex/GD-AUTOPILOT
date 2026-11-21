#include "core/DebugLog.hpp"
#include "core/Types.hpp"
#include "modules/AIEngine.hpp"
#include "modules/GameController.hpp"
#include "modules/LearningSystem.hpp"
#include "modules/LevelAnalyzer.hpp"
#include "modules/PhysicsEngine.hpp"
#include "modules/StatisticsManager.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace gdautopilot;

namespace {

LevelObject makeSpike(int id, float x, float y, float w = 24.0f, float h = 28.0f) {
    LevelObject obj{};
    bool ok = LevelAnalyzer::classifyObject(
        id, 8, 2, Rect{x, y, x + w, y + h}, 0.0f, false, false, false, obj
    );
    assert(ok);
    return obj;
}

LevelObject makeSolid(int id, float x, float y, float w = 30.0f, float h = 30.0f) {
    LevelObject obj{};
    bool ok = LevelAnalyzer::classifyObject(
        id, 1, 0, Rect{x, y, x + w, y + h}, 0.0f, false, false, false, obj
    );
    assert(ok);
    return obj;
}

LevelObject makePortal(int id, int objectID, float x, float y) {
    LevelObject obj{};
    bool ok = LevelAnalyzer::classifyObject(
        id, objectID, 7, Rect{x, y, x + 30.0f, y + 90.0f}, 0.0f, false, false, false, obj
    );
    assert(ok);
    return obj;
}

LevelObject makeOrb(int id, int objectID, float x, float y) {
    LevelObject obj{};
    bool ok = LevelAnalyzer::classifyObject(
        id, objectID, 11, Rect{x, y, x + 30.0f, y + 30.0f}, 0.0f, false, false, false, obj
    );
    assert(ok);
    return obj;
}

LevelObject makePad(int id, int objectID, float x, float y) {
    LevelObject obj{};
    bool ok = LevelAnalyzer::classifyObject(
        id, objectID, 10, Rect{x, y, x + 30.0f, y + 10.0f}, 0.0f, false, false, false, obj
    );
    assert(ok);
    return obj;
}

void testLevelAnalyzerClassificationAndPatterns() {
    std::vector<LevelObject> objs;
    // Triple spike cluster
    objs.push_back(makeSpike(1, 300.0f, 0.0f));
    objs.push_back(makeSpike(2, 330.0f, 0.0f));
    objs.push_back(makeSpike(3, 360.0f, 0.0f));
    // Yellow orb + Ship portal + Speed 2x portal + Yellow pad
    objs.push_back(makeOrb(4, 36, 520.0f, 90.0f));
    objs.push_back(makePortal(5, 13, 640.0f, 60.0f));
    objs.push_back(makePortal(6, 202, 700.0f, 60.0f));
    objs.push_back(makePad(7, 35, 820.0f, 0.0f));
    // Corridor ceiling & floor blocks
    for (float x = 900.0f; x <= 1200.0f; x += 30.0f) {
        objs.push_back(makeSolid(100 + static_cast<int>(x), x, 0.0f, 30.0f, 60.0f));
        objs.push_back(makeSolid(500 + static_cast<int>(x), x, 145.0f, 30.0f, 60.0f));
    }

    LevelAnalyzer analyzer;
    analyzer.analyze(objs, 1500.0f);

    assert(analyzer.isReady());
    auto const& sum = analyzer.getSummary();
    assert(sum.hazardCount == 3);
    assert(sum.orbCount == 1);
    assert(sum.padCount == 1);
    assert(sum.portalCount == 2);
    assert(sum.patternCount >= 4);

    // Next mode transition should be Ship at ~655
    ModeTransitionInfo mt{};
    assert(analyzer.findNextModeTransition(100.0f, mt));
    assert(mt.mode == GameMode::Ship);

    // Safe corridor inside [900, 1200] should guide targetY inside open gap (60..145)
    CorridorSample corr = analyzer.sampleCorridor(1050.0f);
    assert(corr.targetY > 65.0f && corr.targetY < 140.0f);

    std::cout << "[PASS] testLevelAnalyzerClassificationAndPatterns\n";
}

void testAllEightGameModesSimulation() {
    PhysicsEngine physics;
    LevelAnalyzer analyzer;
    GameController controller;
    LearningSystem learning;
    AIEngine engine;

    AIConfig cfg{};
    cfg.enabled = true;
    cfg.applyAnalysisSpeedPreset(3);
    engine.setConfig(cfg);

    // Build a simple obstacle course with floor/ceiling clearance
    std::vector<LevelObject> objs;
    objs.push_back(makeSpike(1, 220.0f, 0.0f));
    objs.push_back(makeSpike(2, 460.0f, 0.0f));
    objs.push_back(makeSolid(3, 600.0f, 240.0f, 200.0f, 30.0f));
    analyzer.analyze(objs, 1000.0f);

    for (int m = 0; m < 8; ++m) {
        GameMode mode = static_cast<GameMode>(m);
        PlayerState state{};
        state.x = 60.0f;
        state.y = (mode == GameMode::Ship || mode == GameMode::Wave || mode == GameMode::UFO || mode == GameMode::Swing)
            ? 120.0f : 15.0f;
        state.mode = mode;
        state.isOnGround = (state.y <= 16.0f);
        state.velocityX = 311.58f;
        state.ceilingY = 360.0f;

        controller.resetInputState();
        engine.reset();

        bool survivedCourse = true;
        float dt = 1.0f / 120.0f;
        std::vector<LevelObject const*> nearby;

        for (int tick = 0; tick < 240; ++tick) {
            EngineDecisionTelemetry tel{};
            ActionDecision dec = engine.evaluate(state, physics, analyzer, controller, learning, tel);
            assert(tel.candidatesEvaluated >= 0);

            bool prevHold = controller.isButtonCurrentlyDown();
            bool newDown = prevHold;
            controller.computeInputDispatch(dec, mode, newDown);
            bool hold = controller.isButtonCurrentlyDown();
            bool justPressed = hold && !prevHold;

            analyzer.queryRange(state.x - 40.0f, state.x + 80.0f, nearby);
            bool hitH = false, hitW = false, hitO = false, hitP = false, chgM = false;
            float minD = 9999.0f;
            physics.step(state, hold, justPressed, dt, nearby, hitH, hitW, hitO, hitP, chgM, minD);

            if (hitH || hitW || state.isDead) {
                survivedCourse = false;
                break;
            }
        }

        assert(survivedCourse);
        assert(state.x > 600.0f);
    }

    std::cout << "[PASS] testAllEightGameModesSimulation (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing)\n";
}

void testCubeJumpsOverSpikesAndSurvives() {
    PhysicsEngine physics;
    LevelAnalyzer analyzer;
    GameController controller;
    LearningSystem learning;
    AIEngine engine;

    AIConfig cfg{};
    cfg.enabled = true;
    cfg.applyAnalysisSpeedPreset(4);
    engine.setConfig(cfg);

    std::vector<LevelObject> objs;
    objs.push_back(makeSpike(1, 240.0f, 0.0f));
    objs.push_back(makeSpike(2, 266.0f, 0.0f));
    objs.push_back(makeSpike(3, 520.0f, 0.0f));
    objs.push_back(makePad(4, 35, 720.0f, 0.0f));
    objs.push_back(makeSpike(5, 790.0f, 0.0f));
    objs.push_back(makeSpike(6, 820.0f, 0.0f));
    analyzer.analyze(objs, 1100.0f);

    PlayerState s{};
    s.x = 30.0f;
    s.y = 15.0f;
    s.mode = GameMode::Cube;
    s.isOnGround = true;

    float dt = 1.0f / 240.0f;
    std::vector<LevelObject const*> nearby;
    int jumpsPerformed = 0;

    for (int tick = 0; tick < 800 && s.x < 960.0f; ++tick) {
        EngineDecisionTelemetry tel{};
        ActionDecision dec = engine.evaluate(s, physics, analyzer, controller, learning, tel);
        bool prev = controller.isButtonCurrentlyDown();
        bool next = prev;
        if (controller.computeInputDispatch(dec, s.mode, next) && next) {
            ++jumpsPerformed;
        }
        bool hold = controller.isButtonCurrentlyDown();
        analyzer.queryRange(s.x - 40.0f, s.x + 80.0f, nearby);

        bool h = false, w = false, o = false, p = false, m = false;
        float d = 9999.0f;
        physics.step(s, hold, hold && !prev, dt, nearby, h, w, o, p, m, d);
        assert(!h && !w && !s.isDead);
    }

    assert(s.x >= 960.0f);
    assert(jumpsPerformed >= 2);
    std::cout << "[PASS] testCubeJumpsOverSpikesAndSurvives (jumps=" << jumpsPerformed << ")\n";
}

void testShipNavigatesNarrowCorridor() {
    PhysicsEngine physics;
    LevelAnalyzer analyzer;
    GameController controller;
    LearningSystem learning;
    AIEngine engine;

    AIConfig cfg{};
    cfg.enabled = true;
    cfg.applyAnalysisSpeedPreset(3);
    engine.setConfig(cfg);

    std::vector<LevelObject> objs;
    for (float x = 150.0f; x <= 900.0f; x += 30.0f) {
        float floorH = 60.0f + 40.0f * std::sin(x * 0.008f);
        float ceilY = floorH + 110.0f;
        objs.push_back(makeSpike(static_cast<int>(x), x, floorH - 25.0f, 30.0f, 25.0f));
        objs.push_back(makeSpike(static_cast<int>(x) + 10000, x, ceilY, 30.0f, 25.0f));
    }
    analyzer.analyze(objs, 1000.0f);

    PlayerState s{};
    s.x = 30.0f;
    s.y = 120.0f;
    s.mode = GameMode::Ship;
    s.isOnGround = false;

    float dt = 1.0f / 120.0f;
    std::vector<LevelObject const*> nearby;

    for (int tick = 0; tick < 360 && s.x < 920.0f; ++tick) {
        EngineDecisionTelemetry tel{};
        ActionDecision dec = engine.evaluate(s, physics, analyzer, controller, learning, tel);
        bool prev = controller.isButtonCurrentlyDown();
        bool next = prev;
        controller.computeInputDispatch(dec, s.mode, next);
        bool hold = controller.isButtonCurrentlyDown();
        analyzer.queryRange(s.x - 40.0f, s.x + 80.0f, nearby);

        bool h = false, w = false, o = false, p = false, m = false;
        float d = 9999.0f;
        physics.step(s, hold, hold && !prev, dt, nearby, h, w, o, p, m, d);
        assert(!h && !w && !s.isDead);
    }

    assert(s.x >= 920.0f);
    std::cout << "[PASS] testShipNavigatesNarrowCorridor\n";
}

void testLearningSystemPersistenceAndAdaptation() {
    auto tmpDir = std::filesystem::temp_directory_path() / "gd_autopilot_test_mem";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    {
        LearningSystem ls;
        ls.setStorageDirectory(tmpDir);
        ls.loadForLevel(424242, "Test Demon");
        ls.recordAttemptAction(100.0f, true, GameMode::Cube);
        ls.recordAttemptAction(220.0f, false, GameMode::Cube);
        ls.recordDeath(410.0f, 15.0f, 41.0f, GameMode::Cube, 2.5);

        auto g1 = ls.queryGuidance(370.0f, GameMode::Cube);
        assert(g1.hasFailureZone);
        int strat1 = g1.strategyVariant;

        // Second death in same area must rotate strategy variant
        ls.recordDeath(415.0f, 15.0f, 41.5f, GameMode::Cube, 2.6);
        auto g2 = ls.queryGuidance(370.0f, GameMode::Cube);
        assert(g2.hasFailureZone);
        assert(g2.strategyVariant != strat1);
    }

    // Reload from disk and verify persistence
    {
        LearningSystem ls2;
        ls2.setStorageDirectory(tmpDir);
        ls2.loadForLevel(424242, "Test Demon");
        assert(!ls2.getFailureZones().empty());
        assert(ls2.getFailureZones()[0].failureCount == 2);

        ls2.recordAttemptAction(370.0f, true, GameMode::Cube);
        ls2.recordCompletion(12.0);
        auto g3 = ls2.queryGuidance(370.0f, GameMode::Cube);
        assert(g3.hasLearnedAction);
        assert(g3.recommendedHold == true);
    }

    std::filesystem::remove_all(tmpDir, ec);
    std::cout << "[PASS] testLearningSystemPersistenceAndAdaptation\n";
}

void testStatisticsManager() {
    StatisticsManager stats;
    stats.onLevelEntered(101, "Stereo Madness");
    stats.onAttemptStart();
    stats.onTick(32.5f, GameMode::Cube, AIState::Running, 0.5);
    stats.onDecisionComputed(0.12, 6);
    stats.onDeath(32.5f, GameMode::Cube);

    stats.onAttemptStart();
    stats.onTick(100.0f, GameMode::Ship, AIState::Running, 1.0);
    stats.onLevelComplete();

    auto snap = stats.getSnapshot();
    assert(snap.totalAttempts == 2);
    assert(snap.completedRuns == 1);
    assert(snap.bestProgress >= 99.9f);
    assert(snap.totalDecisions == 1);
    assert(snap.totalSimulations == 6);
    assert(snap.aiState == AIState::Completed);

    std::cout << "[PASS] testStatisticsManager\n";
}

} // namespace

int main() {
    std::cout << "=== GD AUTOPILOT Core Engine Test Suite ===\n";
    testLevelAnalyzerClassificationAndPatterns();
    testAllEightGameModesSimulation();
    testCubeJumpsOverSpikesAndSurvives();
    testShipNavigatesNarrowCorridor();
    testLearningSystemPersistenceAndAdaptation();
    testStatisticsManager();
    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
