#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "LeaderboardManager.h"
#include "LevelManager.h"
#include "Core/InputActions.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"

struct StarParticle
{
    float X = 0.0f;
    float Y = 0.0f;
    float Speed = 20.0f;
    float Size = 2.0f;
    float Alpha = 180.0f;
    float TwinkleFreq = 2.0f;
};

struct UpgradeCardWidgets
{
    Entity RootPanel = NULL_ENTITY;
    Entity TitleText = NULL_ENTITY;
    Entity DescText = NULL_ENTITY;
    Entity CurrentTierText = NULL_ENTITY;
    Entity NextTierText = NULL_ENTITY;
    Entity UpgradeBtn = NULL_ENTITY;
    PowerUpType Type = PowerUpType::MultiBall;
};

struct UltimateCardWidgets
{
    Entity RootPanel = NULL_ENTITY;
    Entity TitleText = NULL_ENTITY;
    Entity DescText = NULL_ENTITY;
    Entity EquipBtn = NULL_ENTITY;
    UltimateAbilityType Type = UltimateAbilityType::NovaShockwave;
};

class MenuScene : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t GetPostProcessShader() const override;

protected:
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    void    InitRetroBackground(const GameContext& context);
    void    UpdateRetroBackground(float dt, const GameContext& context);
    void    RenderRetroBackground(const GameContext& context) const;

    void    CreateMainMenuUI(const GameContext& context);
    void    CreateSettingsLayout(const GameContext& context);
    void    CreateAudioTab(const GameContext& context);
    void    CreateRenderTab(const GameContext& context);
    void    CreateInputsTab(const GameContext& context);
    void    CreateGamerulesTab(const GameContext& context);
    void    CreateCheatsTab(const GameContext& context);
    void    OpenSettingsTab(Entity targetCanvas);
    void    UpdateInputButtons();

    void    CreateLeaderboardModal(const GameContext& context);
    void    CreateDebugModal(const GameContext& context);

    void    CreateWorkshopModal(const GameContext& context);
    void    OpenWorkshopTab(Entity targetCanvas);
    void    RefreshWorkshopUI();

    void    PreloadGameAssets(GameContext& context);

    void    OpenModal(Entity modalCanvas);
    void    CloseModals();

    void    UpdateVolumeBars(const std::vector<Entity>& bars, float volume);

    Entity  m_mainCanvas = NULL_ENTITY;
    Entity  m_settingsCanvas = NULL_ENTITY;
    Entity  m_audioCanvas = NULL_ENTITY;
    Entity  m_renderCanvas = NULL_ENTITY;
    Entity  m_inputsCanvas = NULL_ENTITY;
    Entity  m_gamerulesCanvas = NULL_ENTITY;
    Entity  m_cheatsCanvas = NULL_ENTITY;
    Entity  m_activeTabCanvas = NULL_ENTITY;
    Entity  m_cheatsTabBtn = NULL_ENTITY;
    Entity  m_leaderboardCanvas = NULL_ENTITY;
    Entity  m_debugCanvas = NULL_ENTITY;
    Entity  m_workshopCanvas = NULL_ENTITY;
    Entity  m_workshopUpgradesCanvas = NULL_ENTITY;
    Entity  m_workshopUltimatesCanvas = NULL_ENTITY;
    Entity  m_workshopProfileCanvas = NULL_ENTITY;
    Entity  m_workshopActiveTabCanvas = NULL_ENTITY;
    Entity  m_workshopBankText = NULL_ENTITY;
    Entity  m_workshopPageText = NULL_ENTITY;
    Entity  m_workshopProfileNameText = NULL_ENTITY;
    Entity  m_workshopProfileStatsText = NULL_ENTITY;
    Entity  m_activeModal = NULL_ENTITY;

    std::vector<UpgradeCardWidgets> m_upgradeCards;
    std::vector<UltimateCardWidgets> m_ultimateCards;
    int     m_workshopCurrentPage = 0;

    // Settings widgets
    Entity  m_fsBtn = NULL_ENTITY;
    Entity  m_vsyncBtn = NULL_ENTITY;
    Entity  m_shaderBtn = NULL_ENTITY;
    Entity  m_particlesBtn = NULL_ENTITY;
    Entity  m_audioDeviceBtn = NULL_ENTITY;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;

    // Inputs rebinding widgets
    Entity  m_inputDeviceBtn = NULL_ENTITY;
    Entity  m_leftRebindBtn = NULL_ENTITY;
    Entity  m_rightRebindBtn = NULL_ENTITY;
    Entity  m_launchRebindBtn = NULL_ENTITY;
    Entity  m_pauseRebindBtn = NULL_ENTITY;
    Entity  m_resetDefaultsBtn = NULL_ENTITY;
    Entity  m_rebindStatusText = NULL_ENTITY;
    InputAction m_rebindAction = InputAction::Count;
    bool    m_isRebinding = false;
    bool    m_rebindGamepadMode = false;
    float   m_rebindTimer = 0.0f;

    LeaderboardManager  m_leaderboard;
    LevelManager        m_levelManager;

    uint32_t            m_clickSfxId = 0;
    uint32_t            m_bounceSfxId = 0;
    uint32_t            m_shaderId = 0;

    std::vector<StarParticle> m_stars;
    float               m_gridAnimOffset = 0.0f;
    float               m_animTime = 0.0f;
};