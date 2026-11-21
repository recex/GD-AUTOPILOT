#include "modules/AIPlayer.hpp"
#include "core/DebugLog.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace geode::prelude;

namespace gdautopilot {

namespace {

GameMode detectGameMode(PlayerObject* p) noexcept {
    if (!p) return GameMode::Cube;
    if (p->m_isShip)   return GameMode::Ship;
    if (p->m_isBall)   return GameMode::Ball;
    if (p->m_isBird)   return GameMode::UFO;
    if (p->m_isDart)   return GameMode::Wave;
    if (p->m_isRobot)  return GameMode::Robot;
    if (p->m_isSpider) return GameMode::Spider;
    if (p->m_isSwing)  return GameMode::Swing;
    return GameMode::Cube;
}

float computeSafeProgress(PlayLayer* pl, PlayerObject* p) noexcept {
    if (pl) {
        float pct = pl->getCurrentPercent();
        if (std::isfinite(pct) && pct >= 0.0f) {
            return std::clamp(pct, 0.0f, 100.0f);
        }
        if (p && pl->m_levelLength > 1.0f) {
            return std::clamp((p->m_position.x / pl->m_levelLength) * 100.0f, 0.0f, 100.0f);
        }
    }
    return 0.0f;
}

} // namespace

AIPlayer& AIPlayer::get() {
    static AIPlayer instance;
    return instance;
}

void AIPlayer::initializeFromModSettings() {
    auto* mod = Mod::get();
    if (!mod) return;

    m_config.enabled = mod->getSettingValue<bool>("auto-enable-on-enter");
    m_config.showHudOverlay = mod->getSettingValue<bool>("show-hud-overlay");
    m_config.learningEnabled = mod->getSettingValue<bool>("learning-enabled");
    m_config.debugLogging = mod->getSettingValue<bool>("debug-logging");

    int speedLevel = static_cast<int>(mod->getSettingValue<int64_t>("analysis-speed"));
    m_config.applyAnalysisSpeedPreset(speedLevel);

    DebugLog::get().setVerbose(m_config.debugLogging);
    m_learning.setEnabled(m_config.learningEnabled);
    m_learning.setStorageDirectory(mod->getSaveDir() / "learning");
    m_engine.setConfig(m_config);
    m_physics.mutableParams().hazardSafetyMargin = m_config.safetyMargin;

    DebugLog::get().info(
        "AIPlayer",
        "Initialized GD AUTOPILOT (speed=" + std::to_string(m_config.analysisSpeedLevel) +
        ", learning=" + std::string(m_config.learningEnabled ? "ON" : "OFF") + ")"
    );
}

void AIPlayer::syncConfigToModSettings() {
    auto* mod = Mod::get();
    if (!mod) return;

    mod->setSettingValue<bool>("show-hud-overlay", m_config.showHudOverlay);
    mod->setSettingValue<bool>("learning-enabled", m_config.learningEnabled);
    mod->setSettingValue<bool>("debug-logging", m_config.debugLogging);
    mod->setSettingValue<int64_t>("analysis-speed", static_cast<int64_t>(m_config.analysisSpeedLevel));
}

void AIPlayer::applyConfig(AIConfig const& cfg) {
    bool wasEnabled = m_config.enabled;
    m_config = cfg;
    m_config.applyAnalysisSpeedPreset(m_config.analysisSpeedLevel);

    DebugLog::get().setVerbose(m_config.debugLogging);
    m_learning.setEnabled(m_config.learningEnabled);
    m_physics.mutableParams().hazardSafetyMargin = m_config.safetyMargin;
    m_engine.setConfig(m_config);

    if (wasEnabled && !m_config.enabled) {
        releaseInputIfHeld(m_activePlayLayer);
        m_state = AIState::Idle;
    } else if (!wasEnabled && m_config.enabled) {
        m_state = m_activePlayLayer ? AIState::Running : AIState::Idle;
    }

    syncConfigToModSettings();
}

void AIPlayer::start() {
    m_config.enabled = true;
    m_engine.setConfig(m_config);
    m_state = (m_activePlayLayer && m_analyzer.isReady()) ? AIState::Running : AIState::Analyzing;
    DebugLog::get().info("AIPlayer", "Autopilot STARTED.");
}

void AIPlayer::stop() {
    m_config.enabled = false;
    m_engine.setConfig(m_config);
    releaseInputIfHeld(m_activePlayLayer);
    m_state = AIState::Idle;
    DebugLog::get().info("AIPlayer", "Autopilot STOPPED.");
}

void AIPlayer::toggle() {
    if (m_config.enabled) {
        stop();
    } else {
        start();
    }
}

void AIPlayer::reanalyzeActiveLevel() {
    if (!m_activePlayLayer) {
        DebugLog::get().warn("AIPlayer", "Cannot re-analyze: no active PlayLayer.");
        return;
    }
    onLevelObjectsReady(m_activePlayLayer);
}

void AIPlayer::clearActiveLevelLearning() {
    m_learning.resetLevelMemory();
    m_stats.onLearningCountsUpdated(
        static_cast<int>(m_learning.getFailureZones().size()),
        static_cast<int>(m_learning.getLearnedActions().size())
    );
}

void AIPlayer::onLevelEntered(PlayLayer* playLayer) {
    m_activePlayLayer = playLayer;
    m_controller.resetInputState();
    m_engine.reset();
    m_hasPrevPlayerX = false;
    m_deathRecordedThisAttempt = false;
    m_state = AIState::Analyzing;

    auto* mod = Mod::get();
    if (mod && mod->getSettingValue<bool>("auto-enable-on-enter")) {
        m_config.enabled = true;
    }

    int levelID = 0;
    std::string levelName = "Custom Level";
    if (playLayer && playLayer->m_level) {
        levelID = playLayer->m_level->m_levelID.value();
        if (!playLayer->m_level->m_levelName.empty()) {
            levelName = playLayer->m_level->m_levelName;
        }
    }

    m_stats.onLevelEntered(levelID, levelName);
    m_stats.onAttemptStart();
    m_learning.loadForLevel(levelID, levelName);
    m_stats.onLearningCountsUpdated(
        static_cast<int>(m_learning.getFailureZones().size()),
        static_cast<int>(m_learning.getLearnedActions().size())
    );

    DebugLog::get().info("AIPlayer", "Entered level '" + levelName + "' (ID " + std::to_string(levelID) + ").");
}

void AIPlayer::onLevelObjectsReady(PlayLayer* playLayer) {
    if (!playLayer) return;
    m_activePlayLayer = playLayer;
    m_state = AIState::Analyzing;

    std::vector<LevelObject> extracted;
    if (playLayer->m_objects) {
        auto objects = CCArrayExt<GameObject*>(playLayer->m_objects);
        extracted.reserve(objects.size() / 2 + 16);

        int idx = 0;
        for (auto* obj : objects) {
            ++idx;
            if (!obj) continue;
            if (obj->m_isDecoration || obj->m_isHide || obj->m_isDisabled) continue;

            CCRect r = obj->getObjectRect();
            Rect box{
                .minX = r.getMinX(),
                .minY = r.getMinY(),
                .maxX = r.getMaxX(),
                .maxY = r.getMaxY()
            };

            LevelObject parsed{};
            if (LevelAnalyzer::classifyObject(
                    idx,
                    obj->m_objectID,
                    static_cast<int>(obj->m_objectType),
                    box,
                    obj->getRotation(),
                    obj->m_slopeUphill,
                    obj->m_isPassable,
                    obj->m_isDisabled,
                    parsed
                )) {
                extracted.push_back(parsed);
            }
        }
    }

    m_analyzer.analyze(std::move(extracted), playLayer->m_levelLength);
    m_state = m_config.enabled ? AIState::Running : AIState::Idle;
}

PlayerState AIPlayer::extractPlayerState(GJBaseGameLayer* gameLayer, PlayerObject* player, float dt) {
    PlayerState s{};
    if (!player) return s;

    s.x = player->m_position.x;
    s.y = player->m_position.y;
    s.mode = detectGameMode(player);
    s.speedTier = playerSpeedFactorToTier(player->m_playerSpeed);
    s.playerSpeed = player->m_playerSpeed;
    s.vehicleSize = player->m_vehicleSize > 0.1f ? player->m_vehicleSize : 1.0f;
    s.gravityMod = player->m_gravityMod > 0.05f ? player->m_gravityMod : 1.0f;
    s.isUpsideDown = player->m_isUpsideDown;
    s.isOnGround = player->m_isOnGround;
    s.isOnSlope = player->m_isOnSlope;
    s.isDashing = player->m_isDashing;
    s.isDead = player->m_isDead;
    s.isGoingLeft = player->m_isGoingLeft;
    s.isPlatformer = player->m_isPlatformer;
    s.isHolding = m_controller.isButtonCurrentlyDown();
    s.inOrbRange = (player->m_touchingRings != nullptr && player->m_touchingRings->count() > 0);

    // Measure actual horizontal velocity smoothly to handle speed portals and timewarp accurately
    float nominalVx = speedTierToVelocity(s.speedTier);
    if (m_hasPrevPlayerX && dt > 1e-5f) {
        float measuredVx = std::abs(s.x - m_prevPlayerX) / dt;
        if (measuredVx > 60.0f && measuredVx < 1200.0f) {
            m_smoothedVelX = 0.80f * m_smoothedVelX + 0.20f * measuredVx;
        } else {
            m_smoothedVelX = nominalVx;
        }
    } else {
        m_smoothedVelX = nominalVx;
    }
    m_prevPlayerX = s.x;
    m_hasPrevPlayerX = true;

    s.velocityX = m_smoothedVelX;

    // GD stores m_yVelocity in internal units (~1/60th scale); convert to units/sec
    s.velocityY = static_cast<float>(player->m_yVelocity) * 60.0f;

    // Estimate ceiling/floor bounds from active corridor
    CorridorSample corr = m_analyzer.sampleCorridor(s.x);
    s.floorY = 0.0f;
    s.ceilingY = std::max(390.0f, corr.ceilingY + 40.0f);
    if (gameLayer && gameLayer->m_maxGameplayY > 240.0f) {
        s.ceilingY = std::max(s.ceilingY, gameLayer->m_maxGameplayY);
    }

    return s;
}

void AIPlayer::dispatchButton(GJBaseGameLayer* gameLayer, bool down) {
    if (!gameLayer) return;
    m_isDispatchingInput = true;
    gameLayer->handleButton(down, 1, true);
    m_isDispatchingInput = false;
}

void AIPlayer::releaseInputIfHeld(GJBaseGameLayer* gameLayer) {
    if (m_controller.isButtonCurrentlyDown()) {
        m_controller.resetInputState();
        if (gameLayer) {
            dispatchButton(gameLayer, false);
        }
    }
}

void AIPlayer::onPhysicsTick(GJBaseGameLayer* gameLayer, float dt, bool isHalfTick) {
    if (!gameLayer || isHalfTick) return;
    auto* playLayer = typeinfo_cast<PlayLayer*>(gameLayer);
    if (!playLayer) return;

    m_activePlayLayer = playLayer;
    auto* player = playLayer->m_player1;
    if (!player) return;

    if (playLayer->m_isPaused) {
        m_state = AIState::Paused;
        return;
    }

    if (playLayer->m_hasCompletedLevel) {
        m_state = AIState::Completed;
        return;
    }

    if (player->m_isDead || playLayer->m_playerDied) {
        releaseInputIfHeld(playLayer);
        return;
    }

    PlayerState currentState = extractPlayerState(playLayer, player, dt);
    m_lastPlayerState = currentState;

    float progress = computeSafeProgress(playLayer, player);

    if (!m_config.enabled) {
        m_state = AIState::Idle;
        m_stats.onTick(progress, currentState.mode, m_state, dt);
        return;
    }

    // Platformer mode guard (GD AUTOPILOT targets classic auto-scrolling levels primarily)
    if (currentState.isPlatformer) {
        m_state = AIState::Idle;
        m_stats.onTick(progress, currentState.mode, m_state, dt);
        return;
    }

    if (!m_analyzer.isReady()) {
        onLevelObjectsReady(playLayer);
    }

    EngineDecisionTelemetry telemetry{};
    ActionDecision decision = m_engine.evaluate(
        currentState,
        m_physics,
        m_analyzer,
        m_controller,
        m_learning,
        telemetry
    );

    m_state = telemetry.emergencyFallback ? AIState::Recovering : AIState::Running;

    bool newButtonDown = false;
    if (m_controller.computeInputDispatch(decision, currentState.mode, newButtonDown)) {
        dispatchButton(playLayer, newButtonDown);
    }

    m_learning.recordAttemptAction(currentState.x, m_controller.isButtonCurrentlyDown(), currentState.mode);
    if (telemetry.candidatesEvaluated > 0) {
        m_stats.onDecisionComputed(telemetry.computeTimeMs, telemetry.candidatesEvaluated);
    }
    m_stats.onTick(progress, currentState.mode, m_state, dt);
}

void AIPlayer::onResetLevel(PlayLayer* playLayer) {
    m_activePlayLayer = playLayer;
    releaseInputIfHeld(playLayer);
    m_controller.resetInputState();
    m_engine.reset();
    m_learning.clearCurrentAttemptTrace();
    m_hasPrevPlayerX = false;
    m_deathRecordedThisAttempt = false;
    m_stats.onAttemptStart();
    m_state = m_config.enabled ? AIState::Running : AIState::Idle;
}

void AIPlayer::onPlayerDeath(PlayLayer* playLayer, PlayerObject* player) {
    if (!playLayer || !player) return;
    if (player != playLayer->m_player1) return;
    if (m_deathRecordedThisAttempt) return;
    m_deathRecordedThisAttempt = true;

    releaseInputIfHeld(playLayer);

    float progress = computeSafeProgress(playLayer, player);
    GameMode mode = detectGameMode(player);
    auto snap = m_stats.getSnapshot();

    m_state = AIState::Learning;
    m_stats.onDeath(progress, mode);

    if (m_config.enabled && m_config.learningEnabled) {
        m_learning.recordDeath(
            player->m_position.x,
            player->m_position.y,
            progress,
            mode,
            snap.currentAttemptTimeSec
        );
        m_stats.onLearningCountsUpdated(
            static_cast<int>(m_learning.getFailureZones().size()),
            static_cast<int>(m_learning.getLearnedActions().size())
        );
    }
}

void AIPlayer::onLevelComplete(PlayLayer* playLayer) {
    releaseInputIfHeld(playLayer);
    auto snap = m_stats.getSnapshot();
    m_stats.onLevelComplete();
    if (m_config.enabled) {
        m_learning.recordCompletion(snap.currentAttemptTimeSec);
        m_stats.onLearningCountsUpdated(
            static_cast<int>(m_learning.getFailureZones().size()),
            static_cast<int>(m_learning.getLearnedActions().size())
        );
    }
    m_state = AIState::Completed;
    DebugLog::get().info("AIPlayer", "Level completed by GD AUTOPILOT!");
}

void AIPlayer::onQuitLevel() {
    releaseInputIfHeld(m_activePlayLayer);
    m_learning.save();
    m_analyzer.clear();
    m_activePlayLayer = nullptr;
    m_hasPrevPlayerX = false;
    m_state = AIState::Idle;
    DebugLog::get().info("AIPlayer", "Exited level and saved learning memory.");
}

} // namespace gdautopilot
