#pragma once
#include "PlaylistManager.h"
#include "../../../Engine/include/ECS/Systems/UISystem.h"
#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Systems/TweenSystem.h"
#include "ScoreManager.h"
#include "UI/TextFeedback.h"
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include "Events/EventDispatcher.h"
#include "StateMachine/StateMachine.h"
#include "Generators/ILevelGenerator.h"
#include "ECS/Components/PowerUpComponent.h"
#include "LevelManager.h"
#include "LeaderboardManager.h"
#include <vector>

struct Color;

enum class SceneState : int
{
    Playing = 0,
    Paused = 1,
    GameOver = 2,
    Victory = 3,
    LevelTransition = 4
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

    [[nodiscard]] const GameContext*    GetContext() const { return mp_context; }
    [[nodiscard]] int     GetLives() const { return m_lives; }
    [[nodiscard]] int     GetBrickCount() const { return m_brickCount; }
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

    // State Machine Lifecycle Hooks
    void    OnGameOverEnter();
    void    OnGameOverUpdate();
    void    OnGameOverExit();
    void    OnVictoryEnter();
    void    OnVictoryUpdate();
    void    OnVictoryExit();

    // Leaderboard
    [[nodiscard]] LeaderboardManager&       GetLeaderboard() { return m_leaderboard; }
    [[nodiscard]] const LeaderboardManager& GetLeaderboard() const { return m_leaderboard; }
    [[nodiscard]] bool                      IsTypingName();
    void    RefreshLeaderboardUI();
    void    SubmitHighScore(const std::string& name);

    void    SetPowerUpTesterActive(bool active);
    [[nodiscard]] bool IsPowerUpTesterActive() const { return m_powerUpTesterActive; }
    void    TogglePowerUpTester();

protected:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);
    void    ResetBallAndPaddle(bool smooth = false);
    void    CreatePowerUpTesterUI(const GameContext& context);
    void    SpawnAllPowerUps();
    void    RespawnBricks();

    void    HandleDeath();
    void    HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY);
    void    HandlePaddleCollision(Entity ballEntity);
    void    HandleBallBottomCollision(Entity ballEntity);

    void    ExplodeFireBall(Entity ballEntity, const Vector2f& explosionCenter);
    void    SpawnFireTrailParticle(const Vector2f& position, const Vector2f& ballVelocity, bool isFuseActive);
    void    SpawnFireExplosionParticles(const Vector2f& position, float radius, int count = 45);

    Entity  CreateBall(const Vector2f& position, const Vector2f& velocity);
    void    SpawnPowerUp(const Vector2f& position);
    void    ApplyPowerUp(PowerUpType type);
    void    FireLasers();
    static Color GetPowerUpColor(PowerUpType type);

    void    SpawnExplosionParticles(const Vector2f& position, const Color& color, int count = 20);
    void    SpawnBleedParticles(const Vector2f& position);
    void    UpdatePowerUpTimers(float dt);

    void    CreateUILayout(GameContext& context);
    void    CreatePauseMenu(const GameContext& context);
    void    CreateSettingsLayout(const GameContext& context);
    void    CreateAudioTab(const GameContext& context);
    void    CreateRenderTab(const GameContext& context);
    void    CreateInputsTab(const GameContext& context);
    void    CreateGamerulesTab(const GameContext& context);
    void    CreateCheatsTab(const GameContext& context);
    void    OpenSettingsTab(Entity targetCanvas);

    void    UpdateVolumeBars(const std::vector<Entity>& bars, float volume);
    void    CreateGameOverMenu(const GameContext& context);
    void    CreateLevelClearMenu(const GameContext& context);
    void    CreateVictoryMenu(const GameContext& context);

    std::unique_ptr<StateMachine<GameScene>> mp_state_machine;
    Entity  m_ball{};
    Entity  m_paddle{};
    Entity  m_explodingHeart{};
    Entity  m_bottomWall{};

    uint32_t    m_shaderId = 0;
    uint32_t    m_crackShaderId = 0;
    uint32_t    m_ballTexId = 0;
    uint32_t    m_paddleTexId = 0;
    uint32_t    m_brickTexId = 0;
    uint32_t    m_brickCrackTexId = 0;
    uint32_t    m_bounceSfxId = 0;
    uint32_t    m_despawnSfxId = 0;
    uint32_t    m_explosionSfxId = 0;
    uint32_t    m_fireTexId = 0;
    uint32_t    m_heartTexId = 0;
    uint32_t    m_gameOverSfxId = 0;
    uint32_t    m_levelClearSfxId = 0;
    uint32_t    m_victorySfxId = 0;
    uint32_t    m_scoreRecordedSfxId = 0;

    UISystem        m_uiSystem;
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

    Entity          m_scoreTextEntity = NULL_ENTITY;

    std::vector<Entity> m_heartEntities;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;
    std::vector<Entity> m_leaderboardRowTexts;

    Entity          m_gameOverTitleText = NULL_ENTITY;
    Entity          m_gameOverScoreText = NULL_ENTITY;
    Entity          m_gameOverStatusText = NULL_ENTITY;
    Entity          m_nameInputEntity = NULL_ENTITY;
    Entity          m_submitNameBtn = NULL_ENTITY;
    Entity          m_replayBtn = NULL_ENTITY;
    bool            m_hasSubmittedScore = false;
    std::string     m_enteredPlayerName = "PLAYER";

    Entity          m_levelClearTitleText = NULL_ENTITY;
    Entity          m_levelClearStatsText = NULL_ENTITY;
    Entity          m_levelClearTimerText = NULL_ENTITY;
    Entity          m_victoryStatsText = NULL_ENTITY;

    LeaderboardManager m_leaderboard;

    BallState   m_ballState = BallState::Spawning;

    uint32_t    m_score = 0;
    uint32_t    m_highScore = 0;
    int     m_lives = 3;
    int     m_brickCount = 0;
    float   m_heartsAlpha = 0.0f;
    Entity  m_fsBtn = NULL_ENTITY;
    Entity  m_shaderBtn = NULL_ENTITY;
    Entity  m_particlesBtn = NULL_ENTITY;
    Entity  m_cheatsTabBtn = NULL_ENTITY;

    std::unique_ptr<TextFeedback> m_textFeedback;

    std::unique_ptr<ILevelGenerator> mp_levelGenerator;
    ScoreManager       m_scoreManager{"save.dat"};
    uint32_t    m_combo = 0;
    PlaylistManager     m_playlist;
    float m_cheatTimer;

    ScopedSubscription m_collisionSub;
    ScopedSubscription m_powerUpSub;
    ScopedSubscription m_nextLevelSub;

    LevelManager m_levelManager;
    float   m_levelTransitionTimer = 0.0f;
    uint32_t m_levelStartScore = 0;

    std::vector<Entity> m_balls;
    std::vector<Entity> m_lasers;
    bool    m_powerUpTesterActive = false;
    Entity  m_powerUpTesterCanvas = NULL_ENTITY;

    float   m_paddleSizeDuration = 0.0f;
    float   m_laserDuration = 0.0f;
    float   m_tempoBallDuration = 0.0f;
    int     m_tempoBallStacks = 0;
    float   m_bigBallDuration = 0.0f;
    Entity  m_powerUpStatusText = NULL_ENTITY;
};