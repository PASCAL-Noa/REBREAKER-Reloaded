#include "Scenes/Sandbox.h"
#include <random>
#include <string>
#include "Core/GameData.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/ColorComponent.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "Core/GameContext.h"
#include "Events/EventDispatcher.h"
#include "Events/CollisionEvent.h"
#include "AudioMixer.h"
#include "Core/Debug.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Components/SpriteComponent.h"

void SandBox::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");
    m_bounceSfxId = context.Resources.LoadResource("Resources/Audio/Sound/ball_hit.wav");
    m_debugTexId = context.Resources.LoadResource("Resources/images/debug.jpg");

    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);

    context.Events.Subscribe<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        Debug::Info("Collision detectee entre {} et {}", e.EntityA, e.EntityB);
        context.Audio.PlaySfx(m_bounceSfxId, 50.0f);
    });

    InitPhysicsWorld(context);
    InitParticles();

    m_systemManager.OnInit();
}

void SandBox::InitPhysicsWorld(GameContext& context)
{
    m_player = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_player, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<BoxCollider>(m_player, BoxCollider{Vector2f{40.0f, 40.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<RigidBody>(m_player, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 0.0f, false});

    CreateWall(0.0f, -380.0f, 1200.0f, 40.0f);
    CreateWall(0.0f, 380.0f, 1200.0f, 40.0f);
    CreateWall(-580.0f, 0.0f, 40.0f, 718.0f);
    CreateWall(580.0f, 0.0f, 40.0f, 718.0f);

    Entity bounceBall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(bounceBall, Transform2D{Vector2f{100.0f, 100.0f}});
    m_registry.AddComponent<CircleCollider>(bounceBall, CircleCollider{30.0f, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(bounceBall, RigidBody{Vector2f{450.0f, 350.0f}, 1.0f, 1.0f, false});

    Entity bounceBall2 = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(bounceBall2, Transform2D{Vector2f{-100.0f, -100.0f}});
    m_registry.AddComponent<CircleCollider>(bounceBall2, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(bounceBall2, RigidBody{Vector2f{-300.0f, -500.0f}, 1.0f, 1.0f, false});

    m_registry.AddComponent<SpriteComponent>(m_player, SpriteComponent{m_debugTexId});
}

void SandBox::InitParticles()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-500.0f, 500.0f);
    std::uniform_real_distribution<float> velDist(-50.0f, 50.0f);
    std::uniform_int_distribution<int> colDist(100, 255);

    for (int i = 0; i < 100000; ++i)
    {
        Entity e = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(e, Transform2D{Vector2f{posDist(rng), posDist(rng)}});
        m_registry.AddComponent<RigidBody>(e, RigidBody{Vector2f{velDist(rng), velDist(rng)}, 1.0f, 0.0f, true});
        m_registry.AddComponent<ColorComponent>(e, Color{static_cast<uint8_t>(colDist(rng)), 50, 200, 255});
    }
}

void SandBox::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    HandleInput(dt, context);
    m_systemManager.OnUpdate(dt);
}

void SandBox::HandleInput(float dt, const GameContext& context)
{
    const InputManager& input = context.Input;

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
    DefaultScene::OnRender(context);
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

    std::string instructions = "Toggle Shader 'F'\nMove 'Z' 'Q' 'S' 'D' | Rotate 'Space' | Zoom 'E' / 'A' | Spawn 'C'";
    DrawDefaultUI(context, "SANDBOX", instructions);
}

void SandBox::OnDestroy(GameContext& context)
{
    context.Events.Clear();
    DefaultScene::OnDestroy(context);
}

uint32_t SandBox::GetPostProcessShader() const
{
    return m_enableShader ? m_shaderId : 0;
}

void SandBox::CreateWall(float x, float y, float w, float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
}