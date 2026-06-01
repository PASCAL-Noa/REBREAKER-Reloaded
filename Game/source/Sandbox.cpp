#include "Scenes/Sandbox.h"
#include <random>
#include <string>
#include "Core/Debug.h"
#include "Core/GameData.h"
#include "Core/SceneManager.h"
#include "InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/ColorComponent.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"

void SandBox::OnInit(GameContext& context)
{
    Scene::OnInit(context);

    m_fontId = context.Resources.LoadResource("Resources/font/arial.ttf");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");

    m_systemManager.AddSystem<PhysicsSystem>(m_registry);
    m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);

    InitPhysicsWorld();
    InitParticles();

    m_systemManager.OnInit();
}

void SandBox::InitPhysicsWorld()
{
    m_player = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_player, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<BoxCollider>(m_player, BoxCollider{Vector2f{40.0f, 40.0f}, Vector2f{20.0f, 20.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_player, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 0.0f, false});

    CreateWall(-600.0f, -400.0f, 1200.0f, 40.0f);
    CreateWall(-600.0f, 360.0f, 1200.0f, 40.0f);
    CreateWall(-600.0f, -360.0f, 40.0f, 720.0f);
    CreateWall(560.0f, -360.0f, 40.0f, 720.0f);

    Entity bounceBall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(bounceBall, Transform2D{Vector2f{100.0f, 100.0f}});
    m_registry.AddComponent<CircleCollider>(bounceBall, CircleCollider{30.0f, Vector2f{30.0f, 30.0f}, false, false});
    m_registry.AddComponent<RigidBody>(bounceBall, RigidBody{Vector2f{450.0f, 350.0f}, 1.0f, 1.0f, false});

    Entity bounceBall2 = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(bounceBall2, Transform2D{Vector2f{-100.0f, -100.0f}});
    m_registry.AddComponent<CircleCollider>(bounceBall2, CircleCollider{20.0f, Vector2f{20.0f, 20.0f}, false, false});
    m_registry.AddComponent<RigidBody>(bounceBall2, RigidBody{Vector2f{-300.0f, -500.0f}, 1.0f, 1.0f, false});
}

void SandBox::InitParticles()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-500.0f, 500.0f);
    std::uniform_real_distribution<float> velDist(-50.0f, 50.0f);
    std::uniform_int_distribution<int> colDist(100, 255);

    for (int i = 0; i < 20000; ++i)
    {
        Entity e = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(e, Transform2D{Vector2f{posDist(rng), posDist(rng)}});
        m_registry.AddComponent<RigidBody>(e, RigidBody{Vector2f{velDist(rng), velDist(rng)}, 1.0f, 0.0f, true});
        m_registry.AddComponent<ColorComponent>(e, Color{static_cast<uint8_t>(colDist(rng)), 50, 200, 255});
    }
}

void SandBox::OnUpdate(float dt, GameContext& context)
{
    Scene::OnUpdate(dt, context);
    HandleInput(dt, context);
    m_systemManager.OnUpdate(dt);
}

void SandBox::HandleInput(float dt, const GameContext& context)
{
    const InputManager& input = context.Input;
    if (input.IsKeyDown(KeyCode::Num0)) context.Scenes.LoadScene<MenuScene>();

    auto& playerRb = m_registry.GetComponent<RigidBody>(m_player);
    auto& playerTransform = m_registry.GetComponent<Transform2D>(m_player);
    const float speed = 400.0f;

    playerRb.Velocity.X = 0.0f;
    playerRb.Velocity.Y = 0.0f;

    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))    playerRb.Velocity.Y = -speed;
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))  playerRb.Velocity.Y = speed;
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))  playerRb.Velocity.X = -speed;
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D)) playerRb.Velocity.X = speed;

    if (input.IsKeyDown(KeyCode::Space)) m_camera.Rotation += 1.0f;
    else m_camera.Rotation = 0.0f;

    if (input.IsKeyDown(KeyCode::A)) m_camera.Zoom += 0.02f;
    if (input.IsKeyDown(KeyCode::E)) m_camera.Zoom -= 0.02f;
    if (m_camera.Zoom < 0.1f) m_camera.Zoom = 0.1f;

    m_camera.Position.X += (playerTransform.Position.X - m_camera.Position.X) * 0.1f;
    m_camera.Position.Y += (playerTransform.Position.Y - m_camera.Position.Y) * 0.1f;

    m_enableShader = input.IsKeyDown(KeyCode::F);

    if (input.IsKeyDown(KeyCode::C)) SpawnParticles(50, playerTransform.Position.X, playerTransform.Position.Y);
}

void SandBox::SpawnParticles(int count, float x, float y)
{
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> velDist(-300.0f, 300.0f);
    std::uniform_int_distribution<int> colDist(150, 255);

    for (int i = 0; i < count; ++i)
    {
        Entity e = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(e, Transform2D{Vector2f{x, y}});
        m_registry.AddComponent<RigidBody>(e, RigidBody{Vector2f{velDist(rng), velDist(rng)}, 1.0f, 0.0f, true});
        m_registry.AddComponent<ColorComponent>(e, Color{255, static_cast<uint8_t>(colDist(rng)), 50, 255});
    }
}

void SandBox::OnRender(GameContext& context)
{
    Scene::OnRender(context);
    context.Render.SetCamera(m_camera);

    m_systemManager.OnRender();

    m_registry.View<Transform2D, BoxCollider>([&](Entity, Transform2D& transform, BoxCollider& box)
    {
        Color color = box.IsColliding ? Colors::Red : (box.IsTrigger ? Colors::Yellow : Colors::Green);
        context.Render.DrawRectangleOutline(box.Size.X, box.Size.Y, transform, color, -2.0f);
    });

    m_registry.View<Transform2D, CircleCollider>([&](Entity, Transform2D& transform, CircleCollider& circle)
    {
        Color color = circle.IsColliding ? Colors::Red : Colors::Green;
        context.Render.DrawCircleOutline(circle.Radius, transform, color, -2.0f);
    });

    context.Render.ResetCamera();
    DrawUI(context);
}

void SandBox::OnDestroy(GameContext& context)
{
    Scene::OnDestroy(context);
}

uint32_t SandBox::GetPostProcessShader() const
{
    return m_enableShader ? m_shaderId : 0;
}

void SandBox::DrawUI(const GameContext& context) const
{
    std::string debugText = "FPS : " + std::to_string(context.Data.FPS) + "\n";
    debugText += "Active Entities : " + std::to_string(m_registry.GetActiveEntityCount()) + "\n";
    debugText += "Toggle Shader 'F' | Menu '0' \n";
    debugText += "Move 'Z' 'Q' 'S' 'D' | Rotate 'Space' | Zoom 'E' / 'A' | Spawn 'C'";

    context.Render.DrawText(debugText, m_fontId, 24.0f, Transform2D{Vector2f{10.0f, 10.0f}}, Colors::Yellow);
}

void SandBox::CreateWall(float x, float y, float w, float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{w / 2.0f, h / 2.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
}

