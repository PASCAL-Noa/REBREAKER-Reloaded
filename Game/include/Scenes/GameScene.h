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
#include <vector>
#include <memory>

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
    void    ResumeGame();
    void    OnReplayRequested();

    // Power-Up Tester callbacks
    void    ApplyPowerUp(PowerUpType type);
    void    SpawnPowerUpAtPaddle();
    void    SpawnAllPowerUps();
    void    RespawnBricks();
    void    ResetBallAndPaddle(bool smooth = false);

protected:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);

    void    HandleDeath();
    void    HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false);
    void    DetonateBombBrick(Entity bombEntity);
    void    HandlePaddleCollision(Entity ballEntity);
    void    HandleBallBottomCollision(Entity ballEntity);

    void    ExplodeFireBall(Entity ballEntity, const Vector2f& explosionCenter);
    void    SpawnFireTrailParticle(const Vector2f& position, const Vector2f& ballVelocity, bool isFuseActive);
    void    SpawnFireExplosionParticles(const Vector2f& position, float radius, int count = 45);

    Entity  CreateBall(const Vector2f& position, const Vector2f& velocity);
    void    SpawnPowerUp(const Vector2f& position);
    void    FireLasers();
    static Color GetPowerUpColor(PowerUpType type);

    void    SpawnExplosionParticles(const Vector2f& position, const Color& color, int count = 20);
    void    SpawnBleedParticles(const Vector2f& position);
    void    UpdatePowerUpTimers(float dt);

    std::unique_ptr<StateMachine<GameScene>> mp_state_machine;
    Entity  m_ball{};
    Entity  m_paddle{};
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
    uint32_t    m_fireballBurnSfxId = 0;
    uint32_t    m_fireballExplosionSfxId = 0;
    uint32_t    m_fireTexId = 0;
    uint32_t    m_heartTexId = 0;
    uint32_t    m_gameOverSfxId = 0;
    uint32_t    m_levelClearSfxId = 0;
    uint32_t    m_victorySfxId = 0;
    uint32_t    m_scoreRecordedSfxId = 0;

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
    float   m_laserDuration = 0.0f;
    float   m_tempoBallDuration = 0.0f;
    int     m_tempoBallStacks = 0;
    float   m_bigBallDuration = 0.0f;

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
};