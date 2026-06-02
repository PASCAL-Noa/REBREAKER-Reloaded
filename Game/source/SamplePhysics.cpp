#include "Scenes/SamplePhysics.h"
#include "Core/GameContext.h"
#include "InputManager.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Systems/PhysicsSystem.h"

void SamplePhysics::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_systemManager.AddSystem<PhysicsSystem>(m_registry);

    CreateBox(-300.0f, -200.0f, 100.0f, 100.0f, 0.0f, 0.0f, false, true);
    CreateBox(-250.0f, -150.0f, 100.0f, 100.0f, 0.0f, 0.0f, false, true);

    CreateBox(100.0f, -200.0f, 50.0f, 50.0f, 100.0f, 0.0f, true, false);
    CreateBox(300.0f, -200.0f, 50.0f, 50.0f, -100.0f, 0.0f, true, false);

    CreateBox(-200.0f, 150.0f, 50.0f, 50.0f, 150.0f, 0.0f, false, false);
    CreateBox(0.0f, 150.0f, 50.0f, 50.0f, -150.0f, 0.0f, false, false);

    CreateBox(200.0f, 50.0f, 200.0f, 20.0f, 0.0f, 0.0f, false, true);
    CreateBox(200.0f, 250.0f, 200.0f, 20.0f, 0.0f, 0.0f, false, true);

    Entity ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(ball, Transform2D{Vector2f{280.0f, 100.0f}});
    m_registry.AddComponent<CircleCollider>(ball, CircleCollider{20.0f, Vector2f{20.0f, 20.0f}, false});
    m_registry.AddComponent<RigidBody>(ball, RigidBody{Vector2f{0.0f, 250.0f}, 1.0f, 1.0f, false});

    m_player = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_player, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<BoxCollider>(m_player, BoxCollider{Vector2f{40.0f, 40.0f}, Vector2f{20.0f, 20.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_player, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 0.0f, false});

    m_systemManager.OnInit();
}

void SamplePhysics::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    HandleInput(dt, context);
    m_systemManager.OnUpdate(dt);
}

void SamplePhysics::HandleInput(float dt, const GameContext& context)
{
    const InputManager& input = context.Input;
    auto& rb = m_registry.GetComponent<RigidBody>(m_player);
    const float speed = 400.0f;

    rb.Velocity.X = 0.0f;
    rb.Velocity.Y = 0.0f;

    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))    rb.Velocity.Y = -speed;
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))  rb.Velocity.Y = speed;
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))  rb.Velocity.X = -speed;
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D)) rb.Velocity.X = speed;
}

void SamplePhysics::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_camera);

    m_registry.View<Transform2D, BoxCollider>([&](Entity, Transform2D& transform, BoxCollider& box)
    {
        Color color = box.IsColliding ? Colors::Red : Colors::Green;
        context.Render.DrawRectangleOutline(box.Size.X, box.Size.Y, transform, color, -2.0f);
    });

    m_registry.View<Transform2D, CircleCollider>([&](Entity, Transform2D& transform, CircleCollider& circle)
    {
        Color color = circle.IsColliding ? Colors::Red : Colors::Green;
        context.Render.DrawCircleOutline(circle.Radius, transform, color, -2.0f);
    });

    context.Render.ResetCamera();

    DrawDefaultUI(context, "SAMPLE PHYSICS", "Mouvement : Z Q S D / Fleches");
}

void SamplePhysics::CreateBox(float x, float y, float w, float h, float vx, float vy, bool isTrigger, bool isKinematic)
{
    Entity e = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(e, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(e, BoxCollider{Vector2f{w, h}, Vector2f{w / 2.0f, h / 2.0f}, false, isTrigger});

    if (!isKinematic || vx != 0.0f || vy != 0.0f)
    {
        m_registry.AddComponent<RigidBody>(e, RigidBody{Vector2f{vx, vy}, 1.0f, 1.0f, isKinematic});
    }
}