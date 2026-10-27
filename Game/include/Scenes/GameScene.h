#pragma once
#include "PlaylistManager.h"
#include "ECS/Systems/PaddleSystem.h"
#include "ECS/Systems/BallSystem.h"
#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Systems/TweenSystem.h"
#include "ScoreManager.h"
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include "Events/EventDispatcher.h"
#include "StateMachine/StateMachine.h"
#include "Generators/ILevelGenerator.h"
#include "ECS/Components/PowerUpComponent.h"
#include "LevelManager.h"
#include "LeaderboardManager.h"
#include "UI/GameUIController.h"
#include "AmbientBackground.h"
#include "Data/PostProcessParams.h"
#include "ECS/Components/LightSourceComponent.h"
#include <vector>
#include <memory>

struct Color;

enum class SceneState : int
{
    Playing = 0,
    Paused = 1,
    GameOver = 2,
    Victory = 3,
    LevelTransition = 4,
    Countdown = 5
};

enum class BallState
{
    Active,
    Dying,
    Spawning,
    Attached
};

class GameScene : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t  GetPostProcessShader() const override;
    [[nodiscard]] PostProcessParams GetPostProcessParams() const override;
    void    TriggerPostProcessFlash(float caIntensity, float vignetteIntensity, const Color& vignetteColor, float flashIntensity = 0.0f, const Color& flashColor = Colors::White);
    void    UpdateComboBackgroundColor(uint32_t comboMultiplier);
    void    ApplyLevelTheme(int levelNumber);
    [[nodiscard]] const LevelTheme& GetCurrentTheme() const { return m_currentTheme; }

    [[nodiscard]] const GameContext*    GetContext() const { return mp_context; }
    [[nodiscard]] int     GetLives() const { return m_lives; }
    [[nodiscard]] int     GetBrickCount() const { return m_brickCount; }
    [[nodiscard]] bool    CanUnpause() const;
    void    FullReset();

    // Level Management & Progression
    [[nodiscard]] LevelManager&       GetLevelManager() { return m_levelManager; }
    [[nodiscard]] const LevelManager& GetLevelManager() const { return m_levelManager; }
    [[nodiscard]] int                 GetCurrentLevel() const;
    [[nodiscard]] int                 GetLevelCount() const;
    [[nodiscard]] bool                HasNextLevel() const;
    void    AdvanceToNextLevel();
    void    LoadLevel(int levelIndex, bool preserveStats = false);
    void    StartLevelTransition();
    void    UpdateLevelTransition();
    void    CompleteLevelTransition();
    [[nodiscard]] bool                IsLevelTransitionComplete() const;
    [[nodiscard]] float               GetTransitionTimer() const { return m_levelTransitionTimer; }
    void    SetTransitionTimer(float timer) { m_levelTransitionTimer = timer; }

    // Countdown & Ready Sequence
    void    StartCountdown();
    [[nodiscard]] bool  IsCountdownActive() const noexcept { return m_countdownActive; }
    [[nodiscard]] bool  IsCountdownComplete() const noexcept { return !m_countdownActive; }
    [[nodiscard]] float GetCountdownTimer() const noexcept { return m_countdownTimer; }
    [[nodiscard]] float GetCountdownGoTimer() const noexcept { return m_countdownGoTimer; }
    [[nodiscard]] int   GetCountdownSecond() const noexcept { return m_countdownLastSecond; }

    // State Machine Lifecycle Hooks
    void    OnGameOverEnter();
    void    OnGameOverUpdate();
    void    OnGameOverExit();
    void    OnVictoryEnter();
    void    OnVictoryUpdate();
    void    OnVictoryExit();

    // Leaderboard & UI Interaction
    [[nodiscard]] LeaderboardManager&       GetLeaderboard() { return m_leaderboard; }
    [[nodiscard]] const LeaderboardManager& GetLeaderboard() const { return m_leaderboard; }
    [[nodiscard]] ScoreManager&             GetScoreManager() { return m_scoreManager; }
    [[nodiscard]] const ScoreManager&       GetScoreManager() const { return m_scoreManager; }
    [[nodiscard]] bool                      IsTypingName();
    [[nodiscard]] bool                      TryConfirmNameBeforeProceeding();
    void                                    PromptEnterName();
    void    ResumeGame();
    void    OnReplayRequested();

    // Power-Up Tester callbacks
    virtual void ApplyPowerUp(PowerUpType type);
    void    SpawnPowerUpAtPaddle();
    void    SpawnAllPowerUps();
    void    RespawnBricks();
    void    ResetBallAndPaddle(bool smooth = false);

protected:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);

    void    HandleDeath();
    void    HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false, int damage = 1);
    void    DetonateBombBrick(Entity bombEntity);
    void    HandlePaddleCollision(Entity ballEntity);
    void    HandleBallBottomCollision(Entity ballEntity);

    void    ExplodeFireBall(Entity ballEntity, const Vector2f& explosionCenter);
    void    SpawnFireTrailParticle(const Vector2f& position, const Vector2f& ballVelocity, bool isFuseActive);
    void    SpawnFireExplosionParticles(const Vector2f& position, float radius, int count = 45);

    Entity  CreateBall(const Vector2f& position, const Vector2f& velocity);
    virtual void SpawnPowerUp(const Vector2f& position);
    void    TriggerStormStrike();
    void    FireLasers();
    static Color GetPowerUpColor(PowerUpType type);

    void    SpawnExplosionParticles(const Vector2f& position, const Color& color, int count = 20);
    void    SpawnBleedParticles(const Vector2f& position);
    void    UpdatePowerUpTimers(float dt);
    void    UpdatePaddleSpeed();

    std::unique_ptr<StateMachine<GameScene>> mp_state_machine;
    Entity  m_ball{};
    Entity  m_paddle{};
    Entity  m_bottomWall{};

    uint32_t    m_shaderId = 0;
    uint32_t    m_crackShaderId = 0;
    uint32_t    m_fontId = 0;
    uint32_t    m_ballTexId = 0;
    uint32_t    m_paddleTexId = 0;
    uint32_t    m_brickTexId = 0;
    uint32_t    m_brickCrackTexId = 0;
    uint32_t    m_bounceSfxId = 0;
    uint32_t    m_despawnSfxId = 0;
    uint32_t    m_explosionSfxId = 0;
    uint32_t    m_fireballBurnSfxId = 0;
    uint32_t    m_fireballExplosionSfxId = 0;
    uint32_t    m_lightningSfxId = 0;
    uint32_t    m_conductionSfxId = 0;
    uint32_t    m_fireTexId = 0;
    uint32_t    m_heartTexId = 0;
    uint32_t    m_gameOverSfxId = 0;
    uint32_t    m_levelClearSfxId = 0;
    uint32_t    m_victorySfxId = 0;
    uint32_t    m_scoreRecordedSfxId = 0;
    uint32_t    m_countdownReadySfxId = 0;
    uint32_t    m_countdown3SfxId = 0;
    uint32_t    m_countdown2SfxId = 0;
    uint32_t    m_countdown1SfxId = 0;
    uint32_t    m_countdownGoSfxId = 0;
    uint32_t    m_countdownWhooshSfxId = 0;

    float       m_countdownTimer = 0.0f;
    float       m_countdownGoTimer = 0.0f;
    int         m_countdownLastSecond = 0;
    int         m_countdownWhooshPlayedForSecond = 0;
    bool        m_countdownActive = false;

    LeaderboardManager m_leaderboard;
    ScoreManager       m_scoreManager;

    BallState   m_ballState = BallState::Spawning;

    int     m_lives = 3;
    int     m_brickCount = 0;

    std::unique_ptr<ILevelGenerator> mp_levelGenerator;
    PlaylistManager     m_playlist;

    ScopedSubscription m_collisionSub;
    ScopedSubscription m_powerUpSub;
    ScopedSubscription m_nextLevelSub;

    LevelManager m_levelManager;
    float   m_levelTransitionTimer = 0.0f;
    uint32_t m_levelStartScore = 0;

    PaddleSystem* mp_paddleSystem = nullptr;
    BallSystem*   mp_ballSystem = nullptr;

    float   m_paddleSizeDuration = 0.0f;
    bool    m_isShrinkPaddle = false;
    float   m_stormDuration = 0.0f;
    int     m_stormCharges = 0;
    float   m_laserDuration = 0.0f;
    float   m_tempoBallDuration = 0.0f;
    int     m_tempoBallStacks = 0;
    float   m_bigBallDuration = 0.0f;
    float   m_stickyPaddleDuration = 0.0f;
    float   m_bottomBarrierDuration = 0.0f;
    int     m_bottomBarrierCharges = 0;
    bool    m_bottomBarrierActive = false;
    float   m_magnetDuration = 0.0f;
    float   m_ghostBallDuration = 0.0f;
    bool    m_ghostBallPendingExpiry = false;
    float   m_ghostBallExpiryGraceTimer = 0.0f;

    struct LightningSegment
    {
        Vector2f Start;
        Vector2f End;
    };

    struct ActiveLightningBolt
    {
        std::vector<LightningSegment> Segments;
        std::vector<Vector2f> Nodes;
        float Lifetime = 0.22f;
        float MaxLifetime = 0.22f;
    };
    std::vector<ActiveLightningBolt> m_activeLightningBolts;

    struct ElectrifiedBrickEffect
    {
        Vector2f Position;
        float Lifetime = 0.35f;
        float MaxLifetime = 0.35f;
    };
    std::vector<ElectrifiedBrickEffect> m_electrifiedBricks;

    struct StuckBall
    {
        Entity Ball = NULL_ENTITY;
        float  RelativeOffsetX = 0.0f;
        float  HoldTimer = 0.0f;
    };
    std::vector<StuckBall> m_stuckBalls;

    ParticleSystem* m_particleSystem = nullptr;

    // Dedicated UI Controller
    std::unique_ptr<GameUIController> m_uiController;

    // Ambient background & dynamic shader effects
    AmbientBackground   m_ambientBackground;
    LevelTheme          m_currentTheme{};
    PostProcessParams   m_postProcessParams;
    Entity              m_bgTweenEntity = NULL_ENTITY;
    uint32_t            m_currentComboMultiplier = 0;
    float               m_gameplayTime = 0.0f;

    // Dynamic decaying light flashes (bombs, fireball explosions)
    struct LightFlash
    {
        Vector2f Position;
        float Radius = 550.0f;
        Color LightColor{255, 210, 120, 255};
        float Intensity = 2.0f;
        float InitialIntensity = 2.0f;
        float Lifetime = 0.45f;
        float InitialLifetime = 0.45f;
    };
    std::vector<LightFlash> m_lightFlashes;
    void SpawnLightFlash(const Vector2f& position, float radius = 550.0f, const Color& color = Color{255, 210, 120, 255}, float intensity = 2.0f, float duration = 0.45f);
};