#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include "StateMachine/StateMachine.h"

enum class SceneState : int
{
    Playing = 0,
    Paused = 1
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

private:
    Entity  CreateWall(float x, float y, float w, float h);
    void    HandleInput(float dt, const GameContext& context);
    void    CreateBrickGrid();
    void    ResetBallAndPaddle();
    void    HandleDeath();
    void    CreateBrick(float x, float y, BrickType type, bool isSpecial);
    void    HandleBrickCollision(Entity entity);
    void    HandlePaddleCollision();

    StateMachine<GameScene>*    mp_state_machine = nullptr;
    const GameContext*      mp_context = nullptr;

    Entity  m_ball{};
    Entity  m_paddle{};
    Entity  m_bottomWall{};

    uint32_t    m_shaderId = 0;
    uint32_t    m_ballTexId = 0;
    uint32_t    m_paddleTexId = 0;
    uint32_t    m_bounceSfxId = 0;
    uint32_t    m_brickTexId = 0;

    bool    m_enableShader = false;
    bool    m_showDebug = false;

    uint32_t    m_score = 0;
    int     m_lives = 3;
};