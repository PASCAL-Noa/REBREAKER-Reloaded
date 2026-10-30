#pragma once

#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Events/ScopedSubscription.h"
#include "Events/GameplayEvents.h"
#include "UI/TextFeedback.h"
#include "LeaderboardManager.h"
#include "LevelThumbnail.h"
#include "Core/InputActions.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

class GameScene;

class GameUIController
{
public:
    GameUIController(Registry& registry, GameContext& context, GameScene& scene);
    ~GameUIController() = default;

    GameUIController(const GameUIController&) = delete;
    GameUIController& operator=(const GameUIController&) = delete;

    void OnInit(uint32_t fontId, uint32_t heartTexId, uint32_t fireTexId, uint32_t bounceSfxId, uint32_t scoreRecordedSfxId);
    void OnUpdate(float dt);

    void ShowState(GameUIState state);

    // Callbacks from StateMachine actions
    void SetupGameOver(uint32_t finalScore, int finalLevel);
    void SetupVictory(uint32_t finalScore);

    // Multi-tier heart HUD (Green, Yellow, Red)
    void UpdateHearts(int lives);
    void TriggerLifeLostAnimation(int currentLives);
    void SetHeartActive(int index, bool active);
    size_t GetHeartCount() const noexcept { return m_heartEntities.size(); }

    // Leaderboard UI
    void RefreshLeaderboardUI();
    void SubmitHighScore(const std::string& name, uint32_t score, int levelNumber);
    [[nodiscard]] bool IsTypingName() const;
    [[nodiscard]] bool HasEnteredPlayerName() const { return !m_enteredPlayerName.empty(); }
    bool TryConfirmNameBeforeProceeding();
    void PromptEnterName();

    // Power-Up Tester
    void SetPowerUpTesterActive(bool active);
    [[nodiscard]] bool IsPowerUpTesterActive() const noexcept { return m_powerUpTesterActive; }
    void TogglePowerUpTester();
    void UpdatePowerUpTesterStatus(const std::string& statusText);
    void NavigatePowerUpTester(int delta);
    void ActivatePowerUpTesterSelected();
    [[nodiscard]] int GetPowerUpTesterIndex() const noexcept { return m_powerUpTesterIndex; }

    // Text feedback
    void SpawnComboText(const Vector2f& worldPos, int comboMultiplier, const Camera2D& camera);
    void SpawnPowerUpText(PowerUpType type, const Vector2f& worldPos, const Camera2D& camera);


    void OnRender(GameContext& context);
    void RenderCountdown(GameContext& context);

    [[nodiscard]] bool IsModalOrSubMenuOpen() const;
    [[nodiscard]] bool IsRebinding() const noexcept { return m_isRebinding; }
    void CloseSettingsLayout();
    void OpenLevelSelectModal(Entity sourceCanvas);
    void CloseLevelSelectModal();

    // Access to UI canvas
    [[nodiscard]] Entity GetMainUICanvas() const noexcept { return m_uiCanvas; }

private:
    void SetupSubscriptions();

    // UI Builders
    void CreateHUD();
    void CreatePauseMenu();
    void CreateSettingsLayout();
    void CreateAudioTab();
    void CreateRenderTab();
    void CreateInputsTab();
    void CreateGamerulesTab();
    void CreateCheatsTab();
    void CreateLevelSelectModal();
    void RebuildLevelSelectCards();
    void ClearLevelSelectCards();
    void CreatePowerUpTesterUI();
    void CreateGameOverMenu();
    void CreateLevelClearMenu();
    void CreateVictoryMenu();

    void OpenSettingsTab(Entity targetCanvas);
    void UpdateVolumeBars(const std::vector<Entity>& bars, float volume);
    void UpdateInputButtons();

    Registry&       m_registry;
    GameContext&    m_context;
    GameScene&      m_scene;

    uint32_t        m_fontId = 0;
    uint32_t        m_heartTexId = 0;
    uint32_t        m_heartYellowTexId = 0;
    uint32_t        m_heartGreenTexId = 0;
    uint32_t        m_fireTexId = 0;
    uint32_t        m_bounceSfxId = 0;
    uint32_t        m_scoreRecordedSfxId = 0;

    // Canvases
    Entity          m_uiCanvas = NULL_ENTITY;
    Entity          m_pauseCanvas = NULL_ENTITY;
    Entity          m_settingsLayoutCanvas = NULL_ENTITY;
    Entity          m_audioCanvas = NULL_ENTITY;
    Entity          m_renderCanvas = NULL_ENTITY;
    Entity          m_inputsCanvas = NULL_ENTITY;
    Entity          m_gamerulesCanvas = NULL_ENTITY;
    Entity          m_cheatsCanvas = NULL_ENTITY;
    Entity          m_activeTabCanvas = NULL_ENTITY;
    Entity          m_gameOverCanvas = NULL_ENTITY;
    Entity          m_levelClearCanvas = NULL_ENTITY;
    Entity          m_victoryCanvas = NULL_ENTITY;
    Entity          m_powerUpTesterCanvas = NULL_ENTITY;

    // HUD Entities
    Entity          m_scoreTextEntity = NULL_ENTITY;
    Entity          m_explodingHeart = NULL_ENTITY;
    std::vector<Entity> m_heartEntities;

    // Settings widgets
    Entity          m_fsBtn = NULL_ENTITY;
    Entity          m_shaderBtn = NULL_ENTITY;
    Entity          m_particlesBtn = NULL_ENTITY;
    Entity          m_cheatsTabBtn = NULL_ENTITY;
    Entity          m_audioDeviceBtn = NULL_ENTITY;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;

    // Inputs rebinding widgets
    Entity          m_inputDeviceBtn = NULL_ENTITY;
    Entity          m_leftRebindBtn = NULL_ENTITY;
    Entity          m_rightRebindBtn = NULL_ENTITY;
    Entity          m_launchRebindBtn = NULL_ENTITY;
    Entity          m_pauseRebindBtn = NULL_ENTITY;
    Entity          m_resetDefaultsBtn = NULL_ENTITY;
    Entity          m_rebindStatusText = NULL_ENTITY;
    InputAction     m_rebindAction = InputAction::Count;
    bool            m_isRebinding = false;
    bool            m_rebindGamepadMode = false;
    float           m_rebindTimer = 0.0f;

    // PowerUp tester widgets
    bool            m_powerUpTesterActive = false;
    Entity          m_powerUpStatusText = NULL_ENTITY;
    std::vector<Entity> m_powerUpTesterButtons;
    int             m_powerUpTesterIndex = 0;

    // Game Over & Victory widgets
    Entity          m_gameOverTitleText = NULL_ENTITY;
    Entity          m_gameOverScoreText = NULL_ENTITY;
    Entity          m_gameOverStatusText = NULL_ENTITY;
    Entity          m_nameInputEntity = NULL_ENTITY;
    Entity          m_submitNameBtn = NULL_ENTITY;
    Entity          m_replayBtn = NULL_ENTITY;
    Entity          m_gameOverLevelSelectBtn = NULL_ENTITY;
    Entity          m_gameOverMainMenuBtn = NULL_ENTITY;
    std::vector<Entity> m_leaderboardRowTexts;
    std::string     m_enteredPlayerName = "PLAYER";

    // Level Clear widgets
    Entity          m_levelClearTitleText = NULL_ENTITY;
    Entity          m_levelClearStatsText = NULL_ENTITY;
    Entity          m_levelClearTimerText = NULL_ENTITY;
    Entity          m_victoryStatsText = NULL_ENTITY;

    // Level Select Modal widgets
    Entity          m_levelSelectCanvas = NULL_ENTITY;
    Entity          m_levelSelectSourceCanvas = NULL_ENTITY;
    std::vector<Entity> m_levelSelectEntities;
    std::vector<LevelThumbnailData> m_levelThumbnails;
    int             m_levelSelectPage = 0;
    int             m_hoveredLevelCard = -1;
    int             m_selectedLevelCard = 0;
    float           m_levelSelectAnimTime = 0.0f;

    // Text Feedback
    std::unique_ptr<TextFeedback> m_textFeedback;

    // State Tracking
    GameUIState     m_currentState = GameUIState::Playing;

    // Scoped event subscriptions
    ScopedSubscription m_stateSub;
    ScopedSubscription m_scoreSub;
    ScopedSubscription m_livesSub;
};
