#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include "StateMachine/StateMachine.h"

enum class SceneState : int
{
    Playing = 0,
    Paused = 1,
    GameOver = 2,
    Victory = 3
};

class GameScene : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t  GetPostProcessShader() const override;

    [[nodiscard]] const GameContext* GetContext() const { return mp_context; }
    [[nodiscard]] int     GetLives() const { return m_lives; }
    [[nodiscard]] int     GetBrickCount() const { return m_brickCount; }
    void    FullReset();

private:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);
    void    CreateBrickGrid();
    void    ResetBallAndPaddle();
    void    HandleDeath();
    void    CreateBrick(float x, float y, BrickType type, bool isSpecial);
    void    HandleBrickCollision(Entity entity);
    void    HandlePaddleCollision();

    void    LoadHighScore();
    void    SaveHighScore();

    StateMachine<GameScene>*    mp_state_machine = nullptr;
    GameContext*    mp_context = nullptr;

    Entity  m_ball{};
    Entity  m_paddle{};
    Entity  m_bottomWall{};

    uint32_t    m_shaderId = 0;
    uint32_t    m_ballTexId = 0;
    uint32_t    m_paddleTexId = 0;
    uint32_t    m_bounceSfxId = 0;
    uint32_t    m_brickTexId = 0;

    uint32_t    m_score = 0;
    uint32_t    m_highScore = 0;
    int     m_lives = 3;
    int     m_brickCount = 0;
};