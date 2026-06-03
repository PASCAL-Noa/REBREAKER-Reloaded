#include "Scenes/GameScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/InputManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "Graphics/Renderer.h"
#include "Events/EventDispatcher.h"
#include "Events/CollisionEvent.h"
#include <string>
#include "AudioMixer.h"

void GameScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");

    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);

    context.Events.Subscribe<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        context.Audio.PlaySfx(m_bounceSfxId, 50.0f);
    });

    CreateWall(0.0f, -470.0f, 1600.0f, 40.0f);
    CreateWall(0.0f, 470.0f, 1600.0f, 40.0f);
    CreateWall(-820.0f, 0.0f, 40.0f, 900.0f);
    CreateWall(820.0f, 0.0f, 40.0f, 900.0f);

    m_ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_ball, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<CircleCollider>(m_ball, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false});
    m_registry.AddComponent<RigidBody>(m_ball, RigidBody{Vector2f{500.0f, 400.0f}, 1.0f, 1.0f, false});
    m_registry.AddComponent<SpriteComponent>(m_ball, SpriteComponent{m_ballTexId});

    m_paddle = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_paddle, Transform2D{Vector2f{0.0f, 400.0f}});
    m_registry.AddComponent<BoxCollider>(m_paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_paddle, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_paddle, SpriteComponent{m_paddleTexId});

    m_systemManager.OnInit();
}

void GameScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    HandleInput(dt, context);
    m_systemManager.OnUpdate(dt);
}

void GameScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_camera);

    m_systemManager.OnRender();

    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{800.0f, -450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, 450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{-800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{800.0f, -450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);

    if (m_showDebug)
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

    std::string debugState = m_showDebug ? "ON" : "OFF";
    std::string shaderState = m_enableShader ? "ON" : "OFF";
    std::string stats = "Debug (G): " + debugState + "\nShader (F): " + shaderState;
    DrawDefaultUI(context, "BREAKOUT", stats);
}

uint32_t GameScene::GetPostProcessShader() const
{
    return m_enableShader ? m_shaderId : 0;
}

void GameScene::CreateWall(float x, float y, float w, float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
}

void GameScene::OnDestroy(GameContext& context)
{
    context.Events.Clear();
    DefaultScene::OnDestroy(context);
}

void GameScene::HandleInput(float dt, const GameContext& context)
{
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    constexpr float speed = 700.0f;

    if (context.Input.IsKeyPress(KeyCode::F)) m_enableShader = !m_enableShader;
    if (context.Input.IsKeyPress(KeyCode::G)) m_showDebug = !m_showDebug;

    if (context.Input.IsKeyDown(KeyCode::Q)) paddleTransform.Position.X -= speed * dt;
    if (context.Input.IsKeyDown(KeyCode::D)) paddleTransform.Position.X += speed * dt;

    constexpr float limitX = 740.0f;

    if (paddleTransform.Position.X < -limitX)
    {
        paddleTransform.Position.X = -limitX;
    }
    else if (paddleTransform.Position.X > limitX)
    {
        paddleTransform.Position.X = limitX;
    }
}