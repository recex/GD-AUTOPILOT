#include "modules/GeodeInterface.hpp"
#include "core/DebugLog.hpp"
#include "modules/AIPlayer.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/MenuLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>

using namespace geode::prelude;

namespace gdautopilot {

namespace {

std::string formatFloat(float val, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, val);
    return std::string(buf);
}

std::string formatDouble(double val, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, val);
    return std::string(buf);
}

std::string formatDuration(double seconds) {
    int total = static_cast<int>(std::max(0.0, seconds));
    int mins = total / 60;
    int secs = total % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    return std::string(buf);
}

char const* analysisSpeedLabel(int level) noexcept {
    switch (level) {
        case 1: return "1 - ECO (60Hz / 0.45s)";
        case 2: return "2 - BALANCED (60Hz / 0.65s)";
        case 3: return "3 - AGGRESSIVE (120Hz / 0.85s)";
        case 4: return "4 - ULTRA (120Hz / 1.05s)";
        default: return "2 - BALANCED";
    }
}

ccColor3B colorForAIState(AIState state, bool running) {
    if (!running && state == AIState::Idle) return ccc3(230, 75, 75);
    switch (state) {
        case AIState::Running:    return ccc3(65, 235, 115);
        case AIState::Recovering: return ccc3(255, 195, 55);
        case AIState::Learning:   return ccc3(120, 185, 255);
        case AIState::Analyzing:  return ccc3(255, 230, 80);
        case AIState::Completed:  return ccc3(80, 245, 235);
        case AIState::Paused:     return ccc3(185, 185, 185);
        case AIState::Error:      return ccc3(255, 60, 60);
        case AIState::Idle:
        default:                  return ccc3(230, 75, 75);
    }
}

CCMenuItemSpriteExtra* makeStyledButton(
    std::string const& text,
    char const* bgTexture,
    float scale,
    std::function<void(CCMenuItemSpriteExtra*)> callback
) {
    auto* spr = ButtonSprite::create(text.c_str(), "goldFont.fnt", bgTexture, 0.8f);
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(spr, std::move(callback));
}

} // namespace

// ============================================================================
// AUTOPILOT MENU POPUP (GEODE v5 Popup)
// ============================================================================
AutopilotMenuPopup* AutopilotMenuPopup::create() {
    auto* ret = new AutopilotMenuPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AutopilotMenuPopup::init() {
    if (!Popup::init(460.0f, 300.0f)) {
        return false;
    }

    this->setTitle("GD AUTOPILOT");
    this->setID("gd-autopilot-popup"_spr);

    buildHeader();
    buildTabBar();
    buildFooterControls();

    // Content container panel
    auto* panelBg = CCLayerColor::create(ccc4(16, 20, 32, 210), 430.0f, 168.0f);
    panelBg->setPosition(ccp(15.0f, 52.0f));
    m_mainLayer->addChild(panelBg, 1);

    m_contentContainer = CCNode::create();
    m_contentContainer->setPosition(ccp(15.0f, 52.0f));
    m_contentContainer->setContentSize(CCSizeMake(430.0f, 168.0f));
    m_mainLayer->addChild(m_contentContainer, 2);

    selectTab(MenuTab::AIPlayer);
    this->scheduleUpdate();
    return true;
}

void AutopilotMenuPopup::buildHeader() {
    // Activity indicator bar right under title
    auto* statusBg = CCLayerColor::create(ccc4(10, 14, 24, 195), 430.0f, 22.0f);
    statusBg->setPosition(ccp(15.0f, 248.0f));
    m_mainLayer->addChild(statusBg, 1);

    m_activityDot = CCLayerColor::create(ccc4(230, 75, 75, 255), 10.0f, 10.0f);
    m_activityDot->setPosition(ccp(24.0f, 254.0f));
    m_mainLayer->addChild(m_activityDot, 2);

    m_activityLabel = CCLabelBMFont::create("STATUS: STOPPED (IDLE)", "bigFont.fnt");
    m_activityLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    m_activityLabel->setScale(0.36f);
    m_activityLabel->setPosition(ccp(40.0f, 259.0f));
    m_mainLayer->addChild(m_activityLabel, 2);

    m_modeBadgeLabel = CCLabelBMFont::create("MODE: Cube", "goldFont.fnt");
    m_modeBadgeLabel->setAnchorPoint(ccp(1.0f, 0.5f));
    m_modeBadgeLabel->setScale(0.45f);
    m_modeBadgeLabel->setPosition(ccp(438.0f, 259.0f));
    m_mainLayer->addChild(m_modeBadgeLabel, 2);
}

void AutopilotMenuPopup::buildTabBar() {
    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    m_mainLayer->addChild(menu, 3);

    static constexpr std::array<char const*, 6> kTabNames = {
        "AI PLAYER",
        "LEVEL ANALYZER",
        "PHYSICS ENGINE",
        "AI SETTINGS",
        "STATISTICS",
        "DEBUG CONSOLE"
    };

    float startX = 51.0f;
    float stepX = 71.5f;
    for (int i = 0; i < 6; ++i) {
        auto tabEnum = static_cast<MenuTab>(i);
        auto* btn = makeStyledButton(
            kTabNames[static_cast<size_t>(i)],
            "GJ_button_04.png",
            0.34f,
            [this, tabEnum](CCMenuItemSpriteExtra*) {
                this->selectTab(tabEnum);
            }
        );
        btn->setPosition(ccp(startX + stepX * static_cast<float>(i), 234.0f));
        menu->addChild(btn);
        m_tabButtons[static_cast<size_t>(i)] = btn;
    }
}

void AutopilotMenuPopup::buildFooterControls() {
    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    m_mainLayer->addChild(menu, 4);

    auto* startBtn = makeStyledButton("START", "GJ_button_01.png", 0.58f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().start();
        this->refreshLiveWidgets();
    });
    startBtn->setPosition(ccp(115.0f, 27.0f));
    menu->addChild(startBtn);

    auto* stopBtn = makeStyledButton("STOP", "GJ_button_06.png", 0.58f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().stop();
        this->refreshLiveWidgets();
    });
    stopBtn->setPosition(ccp(230.0f, 27.0f));
    menu->addChild(stopBtn);

    auto* reanalyzeBtn = makeStyledButton("RE-ANALYZE", "GJ_button_02.png", 0.52f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().reanalyzeActiveLevel();
        this->rebuildActiveTabContent();
    });
    reanalyzeBtn->setPosition(ccp(352.0f, 27.0f));
    menu->addChild(reanalyzeBtn);
}

void AutopilotMenuPopup::selectTab(MenuTab tab) {
    m_activeTab = tab;
    for (int i = 0; i < 6; ++i) {
        if (m_tabButtons[static_cast<size_t>(i)]) {
            bool active = (i == static_cast<int>(tab));
            m_tabButtons[static_cast<size_t>(i)]->setOpacity(active ? 255 : 170);
        }
    }
    rebuildActiveTabContent();
    refreshLiveWidgets();
}

void AutopilotMenuPopup::rebuildActiveTabContent() {
    if (!m_contentContainer) return;
    m_contentContainer->removeAllChildren();
    m_progressFill = nullptr;
    m_progressText = nullptr;
    m_liveStatusLabel = nullptr;
    m_liveDecisionLabel = nullptr;
    m_livePerfLabel = nullptr;
    m_dynamicBodyLabel = nullptr;

    switch (m_activeTab) {
        case MenuTab::AIPlayer:      buildAIPlayerTab(m_contentContainer); break;
        case MenuTab::LevelAnalyzer: buildLevelAnalyzerTab(m_contentContainer); break;
        case MenuTab::PhysicsEngine: buildPhysicsEngineTab(m_contentContainer); break;
        case MenuTab::AISettings:    buildAISettingsTab(m_contentContainer); break;
        case MenuTab::Statistics:    buildStatisticsTab(m_contentContainer); break;
        case MenuTab::DebugConsole:  buildDebugConsoleTab(m_contentContainer); break;
    }
}

void AutopilotMenuPopup::buildAIPlayerTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("AI PLAYER — REAL-TIME CONTROL", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_liveStatusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_liveStatusLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_liveStatusLabel->setScale(0.76f);
    m_liveStatusLabel->setPosition(ccp(12.0f, 138.0f));
    container->addChild(m_liveStatusLabel);

    m_liveDecisionLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_liveDecisionLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_liveDecisionLabel->setScale(0.72f);
    m_liveDecisionLabel->setColor(ccc3(160, 235, 255));
    m_liveDecisionLabel->setPosition(ccp(12.0f, 102.0f));
    container->addChild(m_liveDecisionLabel);

    // Visual progress bar
    auto* progLabel = CCLabelBMFont::create("LEVEL PROGRESS", "bigFont.fnt");
    progLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    progLabel->setScale(0.32f);
    progLabel->setPosition(ccp(12.0f, 70.0f));
    container->addChild(progLabel);

    auto* barBg = CCLayerColor::create(ccc4(45, 52, 70, 255), 340.0f, 14.0f);
    barBg->setPosition(ccp(12.0f, 48.0f));
    container->addChild(barBg);

    m_progressFill = CCLayerColor::create(ccc4(65, 230, 125, 255), 1.0f, 14.0f);
    m_progressFill->setPosition(ccp(12.0f, 48.0f));
    container->addChild(m_progressFill);

    m_progressText = CCLabelBMFont::create("0.0%", "bigFont.fnt");
    m_progressText->setAnchorPoint(ccp(0.0f, 0.5f));
    m_progressText->setScale(0.38f);
    m_progressText->setPosition(ccp(360.0f, 55.0f));
    container->addChild(m_progressText);

    m_livePerfLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_livePerfLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    m_livePerfLabel->setScale(0.68f);
    m_livePerfLabel->setColor(ccc3(210, 215, 230));
    m_livePerfLabel->setPosition(ccp(12.0f, 24.0f));
    container->addChild(m_livePerfLabel);
}

void AutopilotMenuPopup::buildLevelAnalyzerTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("LEVEL ANALYZER — STRUCTURE & PATTERNS", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_dynamicBodyLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_dynamicBodyLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_dynamicBodyLabel->setScale(0.68f);
    m_dynamicBodyLabel->setPosition(ccp(12.0f, 138.0f));
    container->addChild(m_dynamicBodyLabel);
}

void AutopilotMenuPopup::buildPhysicsEngineTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("PHYSICS ENGINE — 240Hz KINEMATICS", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_dynamicBodyLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_dynamicBodyLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_dynamicBodyLabel->setScale(0.68f);
    m_dynamicBodyLabel->setPosition(ccp(12.0f, 138.0f));
    container->addChild(m_dynamicBodyLabel);

    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    container->addChild(menu);

    auto* minusMargin = makeStyledButton("MARGIN -", "GJ_button_04.png", 0.44f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.safetyMargin = std::clamp(cfg.safetyMargin - 1.0f, 0.0f, 14.0f);
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    minusMargin->setPosition(ccp(78.0f, 22.0f));
    menu->addChild(minusMargin);

    auto* plusMargin = makeStyledButton("MARGIN +", "GJ_button_01.png", 0.44f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.safetyMargin = std::clamp(cfg.safetyMargin + 1.0f, 0.0f, 14.0f);
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    plusMargin->setPosition(ccp(180.0f, 22.0f));
    menu->addChild(plusMargin);

    auto* resetPhys = makeStyledButton("RESET PHYSICS", "GJ_button_06.png", 0.44f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().getPhysicsEngine().setParams(PhysicsParams{});
        auto cfg = AIPlayer::get().getConfig();
        cfg.safetyMargin = 4.0f;
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    resetPhys->setPosition(ccp(322.0f, 22.0f));
    menu->addChild(resetPhys);
}

void AutopilotMenuPopup::buildAISettingsTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("AI SETTINGS — SPEED & LEARNING", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_dynamicBodyLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_dynamicBodyLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_dynamicBodyLabel->setScale(0.68f);
    m_dynamicBodyLabel->setPosition(ccp(12.0f, 138.0f));
    container->addChild(m_dynamicBodyLabel);

    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    container->addChild(menu);

    auto* speedDown = makeStyledButton("SPEED -", "GJ_button_04.png", 0.43f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.applyAnalysisSpeedPreset(cfg.analysisSpeedLevel - 1);
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    speedDown->setPosition(ccp(62.0f, 56.0f));
    menu->addChild(speedDown);

    auto* speedUp = makeStyledButton("SPEED +", "GJ_button_01.png", 0.43f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.applyAnalysisSpeedPreset(cfg.analysisSpeedLevel + 1);
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    speedUp->setPosition(ccp(148.0f, 56.0f));
    menu->addChild(speedUp);

    auto* toggleLearn = makeStyledButton("TOGGLE LEARNING", "GJ_button_02.png", 0.42f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.learningEnabled = !cfg.learningEnabled;
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    toggleLearn->setPosition(ccp(280.0f, 56.0f));
    menu->addChild(toggleLearn);

    auto* toggleHud = makeStyledButton("TOGGLE HUD", "GJ_button_04.png", 0.42f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.showHudOverlay = !cfg.showHudOverlay;
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    toggleHud->setPosition(ccp(82.0f, 22.0f));
    menu->addChild(toggleHud);

    auto* toggleVerbose = makeStyledButton("VERBOSE LOGS", "GJ_button_04.png", 0.42f, [this](CCMenuItemSpriteExtra*) {
        auto cfg = AIPlayer::get().getConfig();
        cfg.debugLogging = !cfg.debugLogging;
        AIPlayer::get().applyConfig(cfg);
        this->refreshLiveWidgets();
    });
    toggleVerbose->setPosition(ccp(212.0f, 22.0f));
    menu->addChild(toggleVerbose);

    auto* clearMem = makeStyledButton("CLEAR MEMORY", "GJ_button_06.png", 0.42f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().clearActiveLevelLearning();
        this->refreshLiveWidgets();
    });
    clearMem->setPosition(ccp(348.0f, 22.0f));
    menu->addChild(clearMem);
}

void AutopilotMenuPopup::buildStatisticsTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("STATISTICS — PERFORMANCE TELEMETRY", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_dynamicBodyLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_dynamicBodyLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_dynamicBodyLabel->setScale(0.68f);
    m_dynamicBodyLabel->setPosition(ccp(12.0f, 138.0f));
    container->addChild(m_dynamicBodyLabel);

    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    container->addChild(menu);

    auto* resetBtn = makeStyledButton("RESET SESSION STATS", "GJ_button_06.png", 0.44f, [this](CCMenuItemSpriteExtra*) {
        AIPlayer::get().getStatistics().resetSession();
        this->refreshLiveWidgets();
    });
    resetBtn->setPosition(ccp(332.0f, 22.0f));
    menu->addChild(resetBtn);
}

void AutopilotMenuPopup::buildDebugConsoleTab(CCNode* container) {
    auto* title = CCLabelBMFont::create("DEBUG CONSOLE — LIVE ENGINE LOGS", "goldFont.fnt");
    title->setAnchorPoint(ccp(0.0f, 1.0f));
    title->setScale(0.46f);
    title->setPosition(ccp(12.0f, 160.0f));
    container->addChild(title);

    m_dynamicBodyLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_dynamicBodyLabel->setAnchorPoint(ccp(0.0f, 1.0f));
    m_dynamicBodyLabel->setScale(0.56f);
    m_dynamicBodyLabel->setColor(ccc3(185, 240, 200));
    m_dynamicBodyLabel->setPosition(ccp(12.0f, 140.0f));
    container->addChild(m_dynamicBodyLabel);

    auto* menu = CCMenu::create();
    menu->setPosition(ccp(0.0f, 0.0f));
    container->addChild(menu);

    auto* clearBtn = makeStyledButton("CLEAR CONSOLE", "GJ_button_06.png", 0.42f, [this](CCMenuItemSpriteExtra*) {
        DebugLog::get().clear();
        this->refreshLiveWidgets();
    });
    clearBtn->setPosition(ccp(348.0f, 20.0f));
    menu->addChild(clearBtn);
}

void AutopilotMenuPopup::refreshLiveWidgets() {
    auto& ai = AIPlayer::get();
    auto snap = ai.getStatistics().getSnapshot();
    auto const& cfg = ai.getConfig();
    auto const& ps = ai.getLastPlayerState();
    auto const& dec = ai.getLastDecision();
    auto const& tel = ai.getLastTelemetry();

    // Update header indicators
    ccColor3B stColor = colorForAIState(ai.getState(), ai.isRunning());
    if (m_activityDot) {
        m_activityDot->setColor(stColor);
    }
    if (m_activityLabel) {
        std::string txt = std::string(ai.isRunning() ? "ACTIVE" : "STOPPED") +
            "  |  STATE: " + aiStateToString(ai.getState());
        m_activityLabel->setString(txt.c_str());
        m_activityLabel->setColor(stColor);
    }
    if (m_modeBadgeLabel) {
        std::string modeTxt = std::string("MODE: ") + gameModeToString(snap.currentMode);
        m_modeBadgeLabel->setString(modeTxt.c_str());
    }

    // Update active tab widgets
    switch (m_activeTab) {
        case MenuTab::AIPlayer: {
            if (m_liveStatusLabel) {
                std::string s =
                    "Level: " + snap.activeLevelName + " (ID " + std::to_string(snap.activeLevelID) + ")\n" +
                    "Mode: " + std::string(gameModeToString(snap.currentMode)) +
                    "   |   Gravity: " + std::string(ps.isUpsideDown ? "INVERTED" : "NORMAL") +
                    "   |   Size: " + std::string(ps.isMini() ? "MINI" : "NORMAL");
                m_liveStatusLabel->setString(s.c_str());
            }
            if (m_liveDecisionLabel) {
                std::string d =
                    "Decision: " + (dec.reason.empty() ? std::string("Standby") : dec.reason) +
                    "   |   Conf: " + std::to_string(static_cast<int>(dec.confidence * 100.0f)) + "%";
                m_liveDecisionLabel->setString(d.c_str());
            }
            if (m_progressFill) {
                float w = std::max(1.0f, 340.0f * (std::clamp(snap.currentProgress, 0.0f, 100.0f) / 100.0f));
                m_progressFill->setContentSize(CCSizeMake(w, 14.0f));
            }
            if (m_progressText) {
                m_progressText->setString((formatFloat(snap.currentProgress, 1) + "%").c_str());
            }
            if (m_livePerfLabel) {
                std::string p =
                    "Attempts: " + std::to_string(snap.totalAttempts) +
                    "  |  Best: " + formatFloat(snap.bestProgress, 1) + "%" +
                    "  |  Eval: " + formatDouble(tel.computeTimeMs, 2) + " ms" +
                    "  |  Speed Preset: L" + std::to_string(cfg.analysisSpeedLevel);
                m_livePerfLabel->setString(p.c_str());
            }
            break;
        }

        case MenuTab::LevelAnalyzer: {
            if (m_dynamicBodyLabel) {
                auto const& sum = ai.getLevelAnalyzer().getSummary();
                std::ostringstream oss;
                if (!sum.analyzed) {
                    oss << "No level currently analyzed.\n"
                        << "Enter any Geometry Dash level or click RE-ANALYZE.\n\n"
                        << "Analyzer capabilities:\n"
                        << "- Classifies blocks, slopes, spikes, orbs, pads & portals\n"
                        << "- Computes navigable Safe Corridor Map\n"
                        << "- Detects obstacle patterns & mode transitions";
                } else {
                    ModeTransitionInfo nextMode{};
                    bool hasNext = ai.getLevelAnalyzer().findNextModeTransition(ps.x, nextMode);
                    oss << "Objects Indexed: " << sum.totalObjects
                        << "  (Solids: " << sum.solidCount << ", Hazards: " << sum.hazardCount << ")\n"
                        << "Interactives: Orbs=" << sum.orbCount
                        << "  Pads=" << sum.padCount
                        << "  Portals=" << sum.portalCount << "\n"
                        << "Patterns Detected: " << sum.patternCount
                        << "  |  Corridor Samples: " << sum.corridorSamples << "\n"
                        << "Level Length: " << static_cast<int>(sum.levelLength) << " u"
                        << "  |  Est. Difficulty: " << formatFloat(sum.estimatedDifficulty, 1) << "/10\n"
                        << "Next Mode Portal: "
                        << (hasNext ? (std::string(gameModeToString(nextMode.mode)) + " @ X=" + std::to_string(static_cast<int>(nextMode.x)))
                                    : std::string("None ahead"))
                        << "\nActive Pattern: " << patternTypeToString(dec.activePattern);
                }
                m_dynamicBodyLabel->setString(oss.str().c_str());
            }
            break;
        }

        case MenuTab::PhysicsEngine: {
            if (m_dynamicBodyLabel) {
                auto const& pp = ai.getPhysicsEngine().getParams();
                std::ostringstream oss;
                oss << "Position: X=" << formatFloat(ps.x, 1) << "  Y=" << formatFloat(ps.y, 1)
                    << "  |  OnGround: " << (ps.isOnGround ? "YES" : "NO") << "\n"
                    << "Velocity: Vx=" << formatFloat(ps.velocityX, 1) << " u/s"
                    << "  Vy=" << formatFloat(ps.velocityY, 1) << " u/s\n"
                    << "Gravity: " << (ps.isUpsideDown ? "INVERTED" : "NORMAL")
                    << " (mod " << formatFloat(ps.gravityMod, 2) << "x)"
                    << "  |  Base G=" << static_cast<int>(pp.baseGravity) << "\n"
                    << "Cube Jump=" << static_cast<int>(pp.cubeJumpVelocity)
                    << "  |  Ship Thrust=" << static_cast<int>(pp.shipThrustAccel)
                    << "  |  UFO Jump=" << static_cast<int>(pp.ufoJumpVelocity) << "\n"
                    << "Hazard Safety Margin: " << formatFloat(pp.hazardSafetyMargin, 1) << " u"
                    << "  |  Orb Range: " << (ps.inOrbRange ? "YES" : "NO");
                m_dynamicBodyLabel->setString(oss.str().c_str());
            }
            break;
        }

        case MenuTab::AISettings: {
            if (m_dynamicBodyLabel) {
                std::ostringstream oss;
                oss << "Analysis Speed: " << analysisSpeedLabel(cfg.analysisSpeedLevel) << "\n"
                    << "Adaptive Learning: " << (cfg.learningEnabled ? "ENABLED" : "DISABLED")
                    << "  (Failure Zones: " << snap.learnedZonesCount
                    << ", Strategies: " << snap.learnedStrategiesCount << ")\n"
                    << "In-Game HUD Overlay: " << (cfg.showHudOverlay ? "VISIBLE" : "HIDDEN") << "\n"
                    << "Verbose Debug Logging: " << (cfg.debugLogging ? "ON" : "OFF");
                m_dynamicBodyLabel->setString(oss.str().c_str());
            }
            break;
        }

        case MenuTab::Statistics: {
            if (m_dynamicBodyLabel) {
                std::ostringstream oss;
                oss << "Total Attempts: " << snap.totalAttempts
                    << "   |   Completions: " << snap.completedRuns << "\n"
                    << "Current Progress: " << formatFloat(snap.currentProgress, 1) << "%"
                    << "   |   Best Progress: " << formatFloat(snap.bestProgress, 1) << "%\n"
                    << "Session Runtime: " << formatDuration(snap.sessionRunTimeSec)
                    << "   |   Attempt Time: " << formatDuration(snap.currentAttemptTimeSec) << "\n"
                    << "Current Mode: " << gameModeToString(snap.currentMode)
                    << "   |   AI State: " << aiStateToString(snap.aiState) << "\n"
                    << "Decisions: " << snap.totalDecisions
                    << "   |   Simulations: " << snap.totalSimulations
                    << "   |   Avg: " << formatDouble(snap.avgDecisionTimeMs, 2) << " ms";
                m_dynamicBodyLabel->setString(oss.str().c_str());
            }
            break;
        }

        case MenuTab::DebugConsole: {
            if (m_dynamicBodyLabel) {
                auto entries = DebugLog::get().getRecentEntries(8);
                std::ostringstream oss;
                if (entries.empty()) {
                    oss << "[Console empty — start AI or enter a level to view live logs]";
                } else {
                    for (auto const& e : entries) {
                        char const* lvl = "INF";
                        if (e.level == LogLevel::Debug) lvl = "DBG";
                        else if (e.level == LogLevel::Warn) lvl = "WRN";
                        else if (e.level == LogLevel::Error) lvl = "ERR";

                        std::string msg = e.message;
                        if (msg.size() > 64) {
                            msg = msg.substr(0, 61) + "...";
                        }
                        oss << "[" << lvl << "][" << e.module << "] " << msg << "\n";
                    }
                }
                m_dynamicBodyLabel->setString(oss.str().c_str());
            }
            break;
        }
    }
}

void AutopilotMenuPopup::update(float dt) {
    m_refreshAccum += dt;
    if (m_refreshAccum >= 0.10f) {
        m_refreshAccum = 0.0f;
        refreshLiveWidgets();
    }
}

// ============================================================================
// IN-GAME LIVE HUD OVERLAY
// ============================================================================
AutopilotHudOverlay* AutopilotHudOverlay::create() {
    auto* ret = new AutopilotHudOverlay();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AutopilotHudOverlay::init() {
    if (!CCNode::init()) return false;

    this->setID("gd-autopilot-hud"_spr);
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    this->setPosition(ccp(10.0f, winSize.height - 70.0f));

    m_bg = CCLayerColor::create(ccc4(10, 14, 24, 165), 166.0f, 58.0f);
    this->addChild(m_bg, 0);

    m_statusDot = CCLayerColor::create(ccc4(230, 75, 75, 255), 7.0f, 7.0f);
    m_statusDot->setPosition(ccp(8.0f, 45.0f));
    this->addChild(m_statusDot, 1);

    m_headerLabel = CCLabelBMFont::create("AUTOPILOT: OFF", "bigFont.fnt");
    m_headerLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    m_headerLabel->setScale(0.28f);
    m_headerLabel->setPosition(ccp(20.0f, 48.5f));
    this->addChild(m_headerLabel, 1);

    m_detailLabel = CCLabelBMFont::create("Mode: Cube | IDLE", "chatFont.fnt");
    m_detailLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    m_detailLabel->setScale(0.56f);
    m_detailLabel->setPosition(ccp(8.0f, 34.0f));
    this->addChild(m_detailLabel, 1);

    m_progressBg = CCLayerColor::create(ccc4(45, 55, 75, 210), m_barMaxWidth, 6.0f);
    m_progressBg->setPosition(ccp(8.0f, 20.0f));
    this->addChild(m_progressBg, 1);

    m_progressFill = CCLayerColor::create(ccc4(70, 235, 130, 255), 1.0f, 6.0f);
    m_progressFill->setPosition(ccp(8.0f, 20.0f));
    this->addChild(m_progressFill, 2);

    m_statsLabel = CCLabelBMFont::create("Att: 0 | Best: 0.0%", "chatFont.fnt");
    m_statsLabel->setAnchorPoint(ccp(0.0f, 0.5f));
    m_statsLabel->setScale(0.52f);
    m_statsLabel->setColor(ccc3(205, 215, 235));
    m_statsLabel->setPosition(ccp(8.0f, 10.0f));
    this->addChild(m_statsLabel, 1);

    this->scheduleUpdate();
    return true;
}

void AutopilotHudOverlay::update(float /*dt*/) {
    auto& ai = AIPlayer::get();
    bool visible = ai.getConfig().showHudOverlay;
    this->setVisible(visible);
    if (!visible) return;

    auto snap = ai.getStatistics().getSnapshot();
    ccColor3B stColor = colorForAIState(ai.getState(), ai.isRunning());

    m_statusDot->setColor(stColor);
    m_headerLabel->setString(ai.isRunning() ? "GD AUTOPILOT: ON" : "GD AUTOPILOT: OFF");
    m_headerLabel->setColor(stColor);

    std::string det = std::string(gameModeToString(snap.currentMode)) +
        " | " + aiStateToString(snap.aiState) +
        " | " + formatFloat(snap.currentProgress, 1) + "%";
    m_detailLabel->setString(det.c_str());

    float fillW = std::max(1.0f, m_barMaxWidth * (std::clamp(snap.currentProgress, 0.0f, 100.0f) / 100.0f));
    m_progressFill->setContentSize(CCSizeMake(fillW, 6.0f));

    std::string st = "Att: " + std::to_string(snap.totalAttempts) +
        " | Best: " + formatFloat(snap.bestProgress, 1) + "%" +
        " | " + formatDuration(snap.currentAttemptTimeSec);
    m_statsLabel->setString(st.c_str());
}

// ============================================================================
// GEODE INTERFACE ENTRY HELPERS
// ============================================================================
void GeodeInterface::openMainMenu() {
    auto* popup = AutopilotMenuPopup::create();
    if (popup) {
        popup->show();
    }
}

void GeodeInterface::attachPauseLayerButton(PauseLayer* pauseLayer) {
    if (!pauseLayer) return;

    auto* btn = makeStyledButton("GD AUTOPILOT", "GJ_button_01.png", 0.55f, [](CCMenuItemSpriteExtra*) {
        GeodeInterface::openMainMenu();
    });
    btn->setID("gd-autopilot-pause-btn"_spr);

    if (auto* rightMenu = pauseLayer->getChildByID("right-button-menu")) {
        rightMenu->addChild(btn);
        rightMenu->updateLayout();
        return;
    }

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto* fallbackMenu = CCMenu::create();
    fallbackMenu->setID("gd-autopilot-pause-menu"_spr);
    fallbackMenu->setPosition(ccp(winSize.width - 75.0f, winSize.height - 34.0f));
    fallbackMenu->addChild(btn);
    pauseLayer->addChild(fallbackMenu, 20);
}

void GeodeInterface::attachMenuLayerButton(MenuLayer* menuLayer) {
    if (!menuLayer) return;

    auto* btn = makeStyledButton("AI", "GJ_button_01.png", 0.72f, [](CCMenuItemSpriteExtra*) {
        GeodeInterface::openMainMenu();
    });
    btn->setID("gd-autopilot-menu-btn"_spr);

    if (auto* bottomMenu = menuLayer->getChildByID("bottom-menu")) {
        bottomMenu->addChild(btn);
        bottomMenu->updateLayout();
        return;
    }

    auto* fallbackMenu = CCMenu::create();
    fallbackMenu->setID("gd-autopilot-main-menu"_spr);
    fallbackMenu->setPosition(ccp(40.0f, 40.0f));
    fallbackMenu->addChild(btn);
    menuLayer->addChild(fallbackMenu, 10);
}

void GeodeInterface::attachPlayLayerHud(PlayLayer* playLayer) {
    if (!playLayer) return;
    if (playLayer->getChildByID("gd-autopilot-hud"_spr)) return;

    auto* hud = AutopilotHudOverlay::create();
    if (hud) {
        playLayer->addChild(hud, 999);
    }
}

} // namespace gdautopilot
