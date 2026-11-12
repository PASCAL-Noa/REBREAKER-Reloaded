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

enum class ChannelType : int
{
    Campaign = 0,
    LevelSelect,
    TetrisEndless,
    Workshop,
    Leaderboard,
    Achievements,
    Sandbox,
    Settings,
    Count
};

struct TVChannel
{
    ChannelType Type = ChannelType::Campaign;
    std::string ChannelNumber;  // e.g. "CH-01"
    std::string Title;          // e.g. "CAMPAIGN"
    std::string Subtitle;       // e.g. "STAGE RUN"
    std::string Description;    // e.g. "Classic Breakout adventure with dynamic arena events"
    Color       ThemeColor{80, 220, 255, 255};
    Color       CardBgColor{20, 26, 40, 245};
    Vector2f    Position{0.0f, 0.0f};  // Center coordinates in logical view
    Vector2f    Size{280.0f, 185.0f};
    Vector2f    BasePosition{0.0f, 0.0f};
    Vector2f    BaseSize{280.0f, 185.0f};
    float       CurrentScale = 1.0f;
    float       TargetScale = 1.0f;
};

struct TVLayout
{
    float Scale = 1.0f;
    Vector2f CabinetCenter{960.0f, 540.0f};
    Vector2f GlassCenter{800.0f, 540.0f};
    Vector2f GlassSize{1390.0f, 910.0f};
    std::array<float, 4> GlassRectUV{0.0f, 0.0f, 0.0f, 0.0f};

    Vector2f ToScreenPos(const Vector2f& basePos) const
    {
        return CabinetCenter + (basePos - Vector2f{960.0f, 540.0f}) * Scale;
    }

    Vector2f ToBasePos(const Vector2f& screenPos) const
    {
        return Vector2f{960.0f, 540.0f} + (screenPos - CabinetCenter) / (Scale > 0.0001f ? Scale : 1.0f);
    }

    float ToScreenDim(float baseDim) const
    {
        return baseDim * Scale;
    }

    Vector2f ToScreenSize(const Vector2f& baseSize) const
    {
        return baseSize * Scale;
    }
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
    Entity CategoryText = NULL_ENTITY;
    Entity DescText = NULL_ENTITY;
    Entity CurTierText = NULL_ENTITY;
    Entity NextTierText = NULL_ENTITY;
    Entity CostText = NULL_ENTITY;
    Entity UpgradeBtn = NULL_ENTITY;
    Entity EquipBtn = NULL_ENTITY;
    Entity DescStatsBtn = NULL_ENTITY;
    std::array<Entity, 5> LevelPipButtons{};
    UltimateAbilityType Type = UltimateAbilityType::NovaShockwave;
    int InspectedLevel = 1;
    float SwitchTimer = 0.0f;
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

    static void SetReturnWithDezoom(bool enable) { s_returnWithDezoom = enable; }
    static bool IsReturnWithDezoom() { return s_returnWithDezoom; }

protected:
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    void    InitRetroBackground(const GameContext& context);
    void    UpdateRetroBackground(float dt, const GameContext& context);
    void    RenderRetroBackground(const GameContext& context) const;

    enum class ChannelZoomState
    {
        Grid,
        ZoomingIn,
        ZoomedView,
        ZoomingOut,
        LaunchingFullScreen,
        ReturningFromFullScreen
    };

    void StartDezoomToGrid();

    struct CampaignPreviewBrick
    {
        float RelX = 0.0f;
        float RelY = 0.0f;
        Color BaseColor{80, 200, 255, 255};
        bool Alive = true;
    };

    struct MusicNoteParticle
    {
        Vector2f BasePos;
        Vector2f Velocity;
        float Lifetime = 0.0f;
        float MaxLifetime = 2.5f;
        float Scale = 1.0f;
        Color NoteColor{120, 220, 255, 255};
        int NoteType = 0; // 0 = single note, 1 = beamed double note
        float WobblePhase = 0.0f;
        float WobbleSpeed = 3.0f;
    };

    void    InitTVChannels();
    void    InitCampaignPreviewLevel();
    void    UpdateCampaignPreviewSimulation(float dt);
    void    UpdateMusicParticles(float dt);
    void    RenderCampaignPreview(GameContext& context, const Vector2f& pos, float sc, bool isZoomed) const;
    void    RenderTwoGears(GameContext& context, const Vector2f& pos, float sc) const;
    void    RenderMusicParticles(GameContext& context) const;
    void    RenderWiiZoomedChannel(GameContext& context);
    void    UpdateTVLayout(const GameContext& context);
    void    UpdateTVControls(float dt, GameContext& context);
    void    CycleChannel(int step, GameContext& context);
    void    LaunchChannel(int channelIndex, GameContext& context);
    void    RenderRetroTVChassis(GameContext& context) const;
    void    RenderCRTBezelAndGlass(GameContext& context) const;
    void    RenderChannelGrid(GameContext& context);
    void    RenderTVScreenContent(GameContext& context);
    void    RenderChannelVignette(GameContext& context, const TVChannel& ch, const Vector2f& pos, float scale) const;
    void    RenderTVOSD(GameContext& context) const;
    void    RenderPowerOffDialog(GameContext& context);
    void    RenderPowerOffAnimation(GameContext& context) const;

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
    void    ResetSaveData();
    void    RenderWorkshopBackgroundVisuals(GameContext& context);
    void    RenderWorkshopForegroundVisuals(GameContext& context);
    [[nodiscard]] uint32_t GetWorkshopBadgeTexture(PowerUpType type) const;
    void    RenderPowerUpSimulation(GameContext& context, PowerUpType type, const Vector2f& center, const Vector2f& size);
    void    RenderUltimateSimulation(GameContext& context, UltimateAbilityType type, int level, const Vector2f& center, const Vector2f& size, float switchTimer);
    void    RenderPlasmaOrbPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime);
    void    RenderDiscoBallPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime);
    void    RenderPowerUpRainPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime);
    void    TriggerSimSfx(GameContext& context, uint32_t sfxId, int eventId, float volume = 65.0f);
    void    TriggerPreviewChannelSwitch();

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
    Entity  m_resetSaveBtn = NULL_ENTITY;
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
    bool    m_descModalForUltimate = false;
    UltimateAbilityType m_descModalUltimate = UltimateAbilityType::PlasmaOrb;
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
    std::vector<Entity> m_masterVolumeBars;
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
    uint32_t            m_workshopBadgeTexIds[16] = {0};

    int                 m_simLastLoop = -1;
    uint32_t            m_simAudioTriggerMask = 0;

    std::unique_ptr<PreviewScene> m_previewScene;
    std::unique_ptr<sf::RenderTexture> m_previewTexture;
    std::unique_ptr<sf::RenderTexture> m_ultimatePreviewTexture;
    std::unique_ptr<PreviewScene> m_ultimatePreviewScenes[3];
    std::unique_ptr<sf::RenderTexture> m_ultimateCardTextures[3];
    PowerUpType         m_lastPreviewPowerUp = PowerUpType::MultiBall;
    UltimateAbilityType m_lastInspectedUltimate = UltimateAbilityType::PlasmaOrb;
    UltimateAbilityType m_lastEquippedUltimate = UltimateAbilityType::PlasmaOrb;
    int                 m_lastPreviewLevel = 0;
    float               m_previewSwitchTimer = 0.0f;
    uint32_t            m_previewShaderId = 0;
    uint32_t            m_luckyShaderId = 0;
    uint32_t            m_plasmaShaderId = 0;
    uint32_t            m_channelSwitchSfxId = 0;

    std::vector<StarParticle> m_stars;
    float               m_gridAnimOffset = 0.0f;
    float               m_animTime = 0.0f;

    // Interactive Retro CRT TV & Wii-style Channel Grid (#90)
    TVLayout            m_tvLayout;
    std::unique_ptr<sf::RenderTexture> m_tvScreenTexture;
    std::vector<TVChannel> m_tvChannels;
    int                 m_selectedChannel = 0;
    int                 m_hoveredChannel = -1;
    float               m_navDebounceTimer = 0.0f;
    float               m_channelSwitchGlitch = 0.0f;
    float               m_osdTimer = 0.0f;
    std::string         m_osdText;
    float               m_volumeKnobAngle = 45.0f;
    float               m_masterVolume = 100.0f;
    bool                m_showPowerOffDialog = false;
    int                 m_powerDialogChoice = 0; // 0 = Cancel, 1 = Standby, 2 = Power Off
    bool                m_isPoweringOff = false;
    float               m_powerOffTimer = 0.0f;
    bool                m_isStandby = false;

    std::vector<MusicNoteParticle> m_musicParticles;
    float               m_musicParticleTimer = 0.0f;

    uint32_t            m_tvStaticSfxId = 0;
    uint32_t            m_tvSwitchSfxId = 0;
    uint32_t            m_tvHoverSfxId = 0;

    // Wii Channel Selection & Zoom Interaction
    ChannelZoomState    m_zoomState = ChannelZoomState::Grid;
    float               m_zoomProgress = 0.0f;
    float               m_zoomTimer = 0.0f;
    int                 m_zoomChoice = 1; // 0 = Retour, 1 = Entrer
    float               m_launchTimer = 0.0f;
    float               m_launchProgress = 0.0f;
    float               m_returnTimer = 0.0f;
    float               m_returnProgress = 0.0f;
    inline static bool  s_returnWithDezoom = false;

    // Campaign Gameplay Preview (10s live loop with current level layout)
    float               m_campaignPreviewTimer = 0.0f;
    int                 m_campaignLoadedLevel = -1;
    std::vector<CampaignPreviewBrick> m_campaignBricks;
    Vector2f            m_campaignBallPos{0.0f, 0.35f};
    Vector2f            m_campaignBallVel{0.48f, -0.62f};
    float               m_campaignPaddleX = 0.0f;
};