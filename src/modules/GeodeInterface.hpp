#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <array>
#include <string>
#include <vector>

namespace gdautopilot {

enum class MenuTab : int {
    AIPlayer = 0,
    LevelAnalyzer = 1,
    PhysicsEngine = 2,
    AISettings = 3,
    Statistics = 4,
    DebugConsole = 5
};

// Modern 6-tab Geode v5 Control Suite Popup for GD AUTOPILOT
class AutopilotMenuPopup : public geode::Popup {
public:
    static AutopilotMenuPopup* create();

protected:
    bool init();
    void update(float dt) override;

private:
    void buildHeader();
    void buildTabBar();
    void buildFooterControls();
    void selectTab(MenuTab tab);
    void rebuildActiveTabContent();
    void refreshLiveWidgets();

    void buildAIPlayerTab(cocos2d::CCNode* container);
    void buildLevelAnalyzerTab(cocos2d::CCNode* container);
    void buildPhysicsEngineTab(cocos2d::CCNode* container);
    void buildAISettingsTab(cocos2d::CCNode* container);
    void buildStatisticsTab(cocos2d::CCNode* container);
    void buildDebugConsoleTab(cocos2d::CCNode* container);

    MenuTab m_activeTab = MenuTab::AIPlayer;
    float m_refreshAccum = 0.0f;

    cocos2d::CCNode* m_contentContainer = nullptr;
    std::array<CCMenuItemSpriteExtra*, 6> m_tabButtons{};

    // Live header indicators
    cocos2d::CCLayerColor* m_activityDot = nullptr;
    cocos2d::CCLabelBMFont* m_activityLabel = nullptr;
    cocos2d::CCLabelBMFont* m_modeBadgeLabel = nullptr;

    // Live widgets inside AI Player tab
    cocos2d::CCLayerColor* m_progressFill = nullptr;
    cocos2d::CCLabelBMFont* m_progressText = nullptr;
    cocos2d::CCLabelBMFont* m_liveStatusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_liveDecisionLabel = nullptr;
    cocos2d::CCLabelBMFont* m_livePerfLabel = nullptr;

    // Live multi-line text labels for dynamic tabs
    cocos2d::CCLabelBMFont* m_dynamicBodyLabel = nullptr;
};

// Compact live HUD overlay displayed during PlayLayer sessions
class AutopilotHudOverlay : public cocos2d::CCNode {
public:
    static AutopilotHudOverlay* create();

protected:
    bool init() override;
    void update(float dt) override;

private:
    cocos2d::CCLayerColor* m_bg = nullptr;
    cocos2d::CCLayerColor* m_statusDot = nullptr;
    cocos2d::CCLayerColor* m_progressBg = nullptr;
    cocos2d::CCLayerColor* m_progressFill = nullptr;
    cocos2d::CCLabelBMFont* m_headerLabel = nullptr;
    cocos2d::CCLabelBMFont* m_detailLabel = nullptr;
    cocos2d::CCLabelBMFont* m_statsLabel = nullptr;
    float m_barMaxWidth = 148.0f;
};

// Helper utilities for attaching GD AUTOPILOT buttons to GD layers
class GeodeInterface {
public:
    static void openMainMenu();
    static void attachPauseLayerButton(PauseLayer* pauseLayer);
    static void attachMenuLayerButton(MenuLayer* menuLayer);
    static void attachPlayLayerHud(PlayLayer* playLayer);
};

} // namespace gdautopilot
