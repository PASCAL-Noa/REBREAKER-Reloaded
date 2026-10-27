#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "LeaderboardManager.h"
#include "LevelManager.h"
#include "Core/InputActions.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"

namespace sf { class RenderTexture; }
class PreviewScene;

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
    Entity CardBtn = NULL_ENTITY;
    Entity TitleText = NULL_ENTITY;
    Entity CategoryText = NULL_ENTITY;
    Entity LevelText = NULL_ENTITY;
    Entity CostText = NULL_ENTITY;
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
    MenuScene();
    ~MenuScene() override;

    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t GetPostProcessShader() const override;
    [[nodiscard]] PostProcessParams GetPostProcessParams() const override;

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
    void    RenderWorkshopCustomVisuals(GameContext& context);
    void    RenderPowerUpSimulation(GameContext& context, PowerUpType type, const Vector2f& center, const Vector2f& size);
    void    RenderUltimateSimulation(GameContext& context, UltimateAbilityType type, const Vector2f& center, const Vector2f& size);
    void    TriggerSimSfx(GameContext& context, uint32_t sfxId, int eventId, float volume = 65.0f);

    void    PreloadGameAssets(GameContext& context);

    void    OpenModal(Entity modalCanvas);
    void    CloseModals();
    void    OpenDescriptionModal();
    void    CloseDescriptionModal();

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
    Entity  m_workshopPlayerText = NULL_ENTITY;
    Entity  m_workshopPageText = NULL_ENTITY;
    Entity  m_workshopProfileNameText = NULL_ENTITY;
    Entity  m_workshopProfileStatsText = NULL_ENTITY;
    Entity  m_activeModal = NULL_ENTITY;

    // Workshop Inspector Entities (Right Column)
    Entity  m_inspectorTitleText = NULL_ENTITY;
    Entity  m_inspectorCategoryText = NULL_ENTITY;
    Entity  m_inspectorDescText = NULL_ENTITY;
    Entity  m_inspectorCurTierText = NULL_ENTITY;
    Entity  m_inspectorArrowText = NULL_ENTITY;
    Entity  m_inspectorNextTierText = NULL_ENTITY;
    Entity  m_inspectorUpgradeBtn = NULL_ENTITY;
    Entity  m_inspectorDescBtn = NULL_ENTITY;
    Entity  m_inspectorTierCostText = NULL_ENTITY;

    // Workshop Description & Stats Popup Modal Entities
    Entity  m_workshopDescCanvas = NULL_ENTITY;
    Entity  m_workshopDescBackdrop = NULL_ENTITY;
    Entity  m_descModalCloseBtn = NULL_ENTITY;
    Entity  m_descModalXBtn = NULL_ENTITY;
    Entity  m_descModalTitleText = NULL_ENTITY;
    Entity  m_descModalCategoryText = NULL_ENTITY;
    Entity  m_descModalDescText = NULL_ENTITY;
    std::array<Entity, 5> m_descModalTierPanels{};
    std::array<Entity, 5> m_descModalTierTags{};
    std::array<Entity, 5> m_descModalTierTexts{};

    std::vector<UpgradeCardWidgets> m_upgradeCards;
    std::vector<UltimateCardWidgets> m_ultimateCards;
    PowerUpType m_selectedPowerUp = PowerUpType::Storm;
    int     m_workshopCurrentPage = 0;
    int     m_workshopScrollRow = 0;
    int     m_inspectedLevel = 1;
    int     m_workshopPreviewLevel = 1;
    bool    m_showDescriptionModal = false;
    float   m_previewAnimTime = 0.0f;
    std::vector<Entity> m_levelPipButtons;
    Entity  m_workshopScrollUpBtn = NULL_ENTITY;
    Entity  m_workshopScrollDownBtn = NULL_ENTITY;

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
    uint32_t            m_brickHitSfxId = 0;
    uint32_t            m_brickDestroySfxId = 0;
    uint32_t            m_lightningSfxId = 0;
    uint32_t            m_conductionSfxId = 0;
    uint32_t            m_fireballBurnSfxId = 0;
    uint32_t            m_fireballExplosionSfxId = 0;
    uint32_t            m_laserSfxId = 0;
    uint32_t            m_powerupSfxId = 0;
    uint32_t            m_powerupBuffSfxId = 0;
    uint32_t            m_shaderId = 0;
    uint32_t            m_paddleTexId = 0;
    uint32_t            m_ballTexId = 0;
    uint32_t            m_brickTexId = 0;
    uint32_t            m_brickCrackTexId = 0;
    uint32_t            m_fireTexId = 0;
    uint32_t            m_heartTexId = 0;

    int                 m_simLastLoop = -1;
    uint32_t            m_simAudioTriggerMask = 0;

    std::unique_ptr<PreviewScene> m_previewScene;
    std::unique_ptr<sf::RenderTexture> m_previewTexture;
    PowerUpType         m_lastPreviewPowerUp = PowerUpType::MultiBall;
    int                 m_lastPreviewLevel = 0;

    std::vector<StarParticle> m_stars;
    float               m_gridAnimOffset = 0.0f;
    float               m_animTime = 0.0f;
};