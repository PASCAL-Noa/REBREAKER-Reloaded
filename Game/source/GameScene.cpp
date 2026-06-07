#include "Scenes/GameScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/InputManager.h"
#include "Core/GameRules.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "Graphics/Renderer.h"
#include "Events/EventDispatcher.h"
#include "Events/CollisionEvent.h"
#include "StateMachine/Transition.h"
#include "Conditions/KeyPressCondition.h"
#include "Conditions/GameConditions.h"
#include "Actions/ResetGameAction.h"
#include "AudioMixer.h"
#include <string>
#include <cmath>
#include <fstream>

void GameScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    mp_context = &context;

    context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", false, RuleAccess::Public);
    context.Rules.RegisterRule(Rule::Debug::ShowCollider, "Toogle Hitbox", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::Invincible, "God mod", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::InfiniteLives, "Infinite lives", false, RuleAccess::Private);

    m_camera.Zoom = 0.75f;

    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_brickTexId = context.Resources.LoadResource("Resources/sprite/brick.png");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");

    LoadHighScore();

    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);

    context.Events.Subscribe<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        context.Audio.PlaySfx(m_bounceSfxId, 50.0f);
        HandleBrickCollision(e.EntityA);
        HandleBrickCollision(e.EntityB);

        if ((e.EntityA == m_ball && e.EntityB == m_paddle) ||
            (e.EntityB == m_ball && e.EntityA == m_paddle))
        {
            HandlePaddleCollision();
        }
        else if (e.EntityA == m_bottomWall || e.EntityB == m_bottomWall)
        {
            if (!context.Rules.GetRule(Rule::Gameplay::Invincible))
            {
                HandleDeath();
            }
        }
    });

    CreateWall(0.0f, -550.0f, 1600.0f, 200.0f);
    CreateWall(-900.0f, 0.0f, 200.0f, 900.0f);
    CreateWall(900.0f, 0.0f, 200.0f, 900.0f);
    m_bottomWall = CreateWall(0.0f, 550.0f, 1600.0f, 200.0f);

    m_ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_ball, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<CircleCollider>(m_ball, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false});
    m_registry.AddComponent<RigidBody>(m_ball, RigidBody{Vector2f{500.0f, 400.0f}, 1.0f, 1.0f, false});
    m_registry.AddComponent<SpriteComponent>(m_ball, SpriteComponent{m_ballTexId});

    m_paddle = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_paddle, Transform2D{Vector2f{0.0f, 300.0f}});
    m_registry.AddComponent<BoxCollider>(m_paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_paddle, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_paddle, SpriteComponent{m_paddleTexId});

    CreateBrickGrid();
    m_systemManager.OnInit();

    mp_state_machine = new StateMachine<GameScene>(this, 4);

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new VictoryCondition<GameScene>(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new ResetGameAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Space), static_cast<int>(SceneState::Playing)));

    State<GameScene>* victoryState = mp_state_machine->CreateState(static_cast<int>(SceneState::Victory));
    victoryState->AddAction(new ResetGameAction<GameScene>());
    victoryState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Space), static_cast<int>(SceneState::Playing)));

    mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
}

void GameScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;

    if (mp_state_machine)
    {
        mp_state_machine->Update();
    }

    if (mp_state_machine && mp_state_machine->GetCurrentState() == static_cast<int>(SceneState::Playing))
    {
        HandleInput(dt, context);
        m_systemManager.OnUpdate(dt);
    }
}

void GameScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_camera);

    m_systemManager.OnRender();

    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{800.0f, -450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{-800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{800.0f, -450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, 450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);

    bool showDebug = context.Rules.GetRule(Rule::Debug::ShowCollider);
    if (showDebug)
    {
        m_registry.View<Transform2D, BoxCollider>([&](Entity, Transform2D& t, BoxCollider& b) {
            Color color = b.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, color, -2.0f);
        });

        m_registry.View<Transform2D, CircleCollider>([&](Entity, Transform2D& t, CircleCollider& c) {
            Color color = c.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawCircleOutline(c.Radius, t, color, -2.0f);
        });
    }

    context.Render.ResetCamera();

    std::string gameStateStr = "PLAY";
    if (mp_state_machine)
    {
        int state = mp_state_machine->GetCurrentState();
        if (state == static_cast<int>(SceneState::Paused)) gameStateStr = "PAUSE";
        else if (state == static_cast<int>(SceneState::GameOver)) gameStateStr = "GAME OVER - ESPACE POUR REJOUER";
        else if (state == static_cast<int>(SceneState::Victory)) gameStateStr = "VICTOIRE - ESPACE POUR REJOUER";
    }

    std::string debugStr = showDebug ? "ON" : "OFF";
    std::string shaderStr = context.Rules.GetRule(Rule::Graphics::EnableShader) ? "ON" : "OFF";
    std::string invStr = context.Rules.GetRule(Rule::Gameplay::Invincible) ? "ON" : "OFF";
    std::string infLivesStr = context.Rules.GetRule(Rule::Gameplay::InfiniteLives) ? "ON" : "OFF";

    std::string stats = "HP : " + std::to_string(m_lives) + " | Score : " + std::to_string(m_score) + " | Record : " + std::to_string(m_highScore);
    stats += "\nDebug (G) : " + debugStr + " | Shader (F) : " + shaderStr;
    stats += "\nGod mod (I) : " + invStr + " | Infinite lives (L) : " + infLivesStr;
    stats += "\nState : " + gameStateStr;

    DrawDefaultUI(context, "REBREAKER", stats);
}

uint32_t GameScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
}

Entity GameScene::CreateWall(float x, float y, float w, float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});

    return wall;
}

void GameScene::OnDestroy(GameContext& context)
{
    delete mp_state_machine;
    mp_state_machine = nullptr;

    context.Events.Clear();
    DefaultScene::OnDestroy(context);
}

void GameScene::LoadHighScore()
{
    std::ifstream file("save.dat");
    if (file.is_open())
    {
        file >> m_highScore;
        file.close();
    }
}

void GameScene::SaveHighScore()
{
    std::ofstream file("save.dat");
    if (file.is_open())
    {
        file << m_highScore;
        file.close();
    }
}

void GameScene::FullReset()
{
    if (m_score > m_highScore)
    {
        m_highScore = m_score;
        SaveHighScore();
    }

    m_score = 0;
    m_lives = 3;
    m_brickCount = 0;

    m_registry.View<BrickComponent>([this](Entity e, BrickComponent&) {
        m_registry.DestroyEntity(e);
    });

    CreateBrickGrid();
    ResetBallAndPaddle();
}

void GameScene::HandleInput(float dt, const GameContext& context)
{
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    const float speed = 700.0f;

    if (context.Input.IsKeyPress(KeyCode::F))
    {
        bool shader = context.Rules.GetRule(Rule::Graphics::EnableShader);
        context.Rules.SetRule(Rule::Graphics::EnableShader, !shader);
    }

    if (context.Input.IsKeyPress(KeyCode::G))
    {
        bool debug = context.Rules.GetRule(Rule::Debug::ShowCollider);
        context.Rules.SetRule(Rule::Debug::ShowCollider, !debug);
    }

    if (context.Input.IsKeyPress(KeyCode::I))
    {
        bool invincible = context.Rules.GetRule(Rule::Gameplay::Invincible);
        context.Rules.SetRule(Rule::Gameplay::Invincible, !invincible);
    }

    if (context.Input.IsKeyPress(KeyCode::L))
    {
        bool infLives = context.Rules.GetRule(Rule::Gameplay::InfiniteLives);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, !infLives);
    }

    if (context.Input.IsKeyDown(KeyCode::Q)) paddleTransform.Position.X -= speed * dt;
    if (context.Input.IsKeyDown(KeyCode::D)) paddleTransform.Position.X += speed * dt;

    const float limitX = 740.0f;

    if (paddleTransform.Position.X < -limitX)
    {
        paddleTransform.Position.X = -limitX;
    }
    else if (paddleTransform.Position.X > limitX)
    {
        paddleTransform.Position.X = limitX;
    }
}

void GameScene::CreateBrickGrid()
{
    const int rows = 5;
    const int cols = 12;
    const float bWidth = 100.0f;
    const float bHeight = 30.0f;
    const float startX = -605.0f;
    const float startY = -350.0f;
    const float pad = 10.0f;

    for (int r = 0; r < rows; ++r)
    {
        BrickType rowType = BrickType::Light;
        if (r == 0) rowType = BrickType::Hard;
        else if (r == 1 || r == 2) rowType = BrickType::Medium;
        else if (r == 3) rowType = BrickType::Special;

        for (int c = 0; c < cols; ++c)
        {
            float x = startX + c * (bWidth + pad);
            float y = startY + r * (bHeight + pad);

            bool isSpecial = (r == 0 && c == 5);

            CreateBrick(x, y, rowType, isSpecial);
        }
    }
}

void GameScene::ResetBallAndPaddle()
{
    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);

    ballTransform.Position = Vector2f{0.0f, 0.0f};
    ballRb.Velocity = Vector2f{500.0f, 400.0f};

    paddleTransform.Position = Vector2f{0.0f, 300.0f};
}

void GameScene::HandleDeath()
{
    bool infiniteLives = mp_context ? mp_context->Rules.GetRule(Rule::Gameplay::InfiniteLives) : false;

    if (!infiniteLives)
    {
        m_lives--;
    }

    if (m_lives > 0 || infiniteLives)
    {
        ResetBallAndPaddle();
    }
}

void GameScene::HandleBrickCollision(Entity entity)
{
    if (!m_registry.HasComponent<BrickComponent>(entity)) return;

    auto& brick = m_registry.GetComponent<BrickComponent>(entity);
    brick.HitPoints--;

    if (brick.HitPoints <= 0)
    {
        m_score += brick.ScoreValue;
        m_brickCount--;
        m_registry.DestroyEntity(entity);
    }
}

void GameScene::HandlePaddleCollision()
{
    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);

    float paddleHalfWidth = paddleCollider.Size.X * 0.5f;
    float offset = ballTransform.Position.X - paddleTransform.Position.X;

    float hitFactor = offset / paddleHalfWidth;
    if (hitFactor < -1.0f) hitFactor = -1.0f;
    if (hitFactor > 1.0f) hitFactor = 1.0f;

    float speed = std::sqrt(ballRb.Velocity.X * ballRb.Velocity.X + ballRb.Velocity.Y * ballRb.Velocity.Y);
    float maxAngle = 60.0f * 3.14159265f / 180.0f;
    float bounceAngle = hitFactor * maxAngle;

    ballRb.Velocity.X = speed * std::sin(bounceAngle);
    ballRb.Velocity.Y = -speed * std::cos(bounceAngle);
}

void GameScene::CreateBrick(float x, float y, BrickType type, bool isSpecial)
{
    Entity brick = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(brick, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{100.0f, 30.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});

    BrickComponent comp;
    comp.Type = type;
    comp.IsSpecial = isSpecial;

    Color tint = Colors::White;

    switch (type)
    {
    case BrickType::Light:
        comp.HitPoints = 1;
        comp.ScoreValue = 50;
        tint = Colors::Green;
        break;
    case BrickType::Medium:
        comp.HitPoints = 2;
        comp.ScoreValue = 100;
        tint = Colors::Yellow;
        break;
    case BrickType::Hard:
        comp.HitPoints = 3;
        comp.ScoreValue = 200;
        tint = Colors::Red;
        break;
    case BrickType::Special:
        comp.HitPoints = 1;
        comp.ScoreValue = 500;
        comp.IsSpecial = true;
        tint = Color{255, 0, 255, 255};
        break;
    default:
        tint = Colors::White;
    }

    m_brickCount++;
    m_registry.AddComponent<BrickComponent>(brick, comp);
    m_registry.AddComponent<SpriteComponent>(brick, SpriteComponent{m_brickTexId, tint});
}