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

struct Color;

enum class SceneState : int
{
    Playing = 0,
    Paused = 1,
    GameOver = 2,
    Victory = 3
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

private:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);
    void    ResetBallAndPaddle();

    void    HandleDeath();
    void    HandleBrickCollision(Entity entity);
    void    HandlePaddleCollision();

    void    SpawnExplosionParticles(const Vector2f& position, const Color& color, int count = 20);
    void    SpawnBleedParticles(const Vector2f& position);

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
    uint32_t    m_fireTexId = 0;
    uint32_t    m_heartTexId = 0;

    
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

    Entity          m_scoreTextEntity = NULL_ENTITY;

    std::vector<Entity> m_heartEntities;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;

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

    EventDispatcher::SubscriptionID m_collisionSubId = 0;
    EventDispatcher::SubscriptionID m_uiEventSubId = 0;
};