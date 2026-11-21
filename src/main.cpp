#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "core/DebugLog.hpp"
#include "modules/AIPlayer.hpp"
#include "modules/GeodeInterface.hpp"

using namespace geode::prelude;
using namespace gdautopilot;

$on_mod(Loaded) {
    AIPlayer::get().initializeFromModSettings();
    DebugLog::get().info("Main", "GD AUTOPILOT v1.0.0 loaded successfully.");
}

class $modify(AutopilotBaseGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        AIPlayer::get().onPhysicsTick(this, dt, isHalfTick);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(AutopilotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) {
            return false;
        }
        AIPlayer::get().onLevelEntered(this);
        GeodeInterface::attachPlayLayerHud(this);
        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        AIPlayer::get().onLevelObjectsReady(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        AIPlayer::get().onResetLevel(this);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        AIPlayer::get().onPlayerDeath(this, player);
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        AIPlayer::get().onLevelComplete(this);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        AIPlayer::get().onQuitLevel();
        PlayLayer::onQuit();
    }
};

class $modify(AutopilotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        GeodeInterface::attachPauseLayerButton(this);
    }
};

class $modify(AutopilotMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        GeodeInterface::attachMenuLayerButton(this);
        return true;
    }
};
