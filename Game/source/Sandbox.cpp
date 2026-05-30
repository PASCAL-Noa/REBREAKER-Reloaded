#include "Scenes/Sandbox.h"

void SandBox::OnInit(GameContext& context)
{
    Scene::OnInit(context);

    m_fontId = context.Resources.LoadResource("Resources/font/arial.ttf");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");

    m_systemManager.AddSystem<ParticleMovementSystem>(m_registry);
    m_systemManager.AddSystem<ParticleRenderSystem>(m_registry, context.Render, 100000);

    InitParticles();
    m_systemManager.OnInit();
    Debug::Info("SandBox Scene Initialized");
}

void SandBox::OnUpdate(float dt, GameContext& context)
{
    Scene::OnUpdate(dt, context);

    HandleInput(dt, context.Input);
    m_systemManager.OnUpdate(dt);
}

void SandBox::OnRender(GameContext& context)
{
    Scene::OnRender(context);

    context.Render.SetCamera(m_camera);
    context.Render.DrawRectangle(50.0f, 50.0f, Transform2D{0, -25.0f, -25.0f}, Colors::Red);

    m_systemManager.OnRender();

    DrawLights(context.Render);
    context.Render.DrawCircle(30.0f, m_player, Colors::Green);

    context.Render.ResetCamera();
    DrawUI(context);
}

void SandBox::OnDestroy(GameContext& context)
{
    Scene::OnDestroy(context);

    Debug::Info("SandBox Scene Destroyed");
}

uint32_t SandBox::GetPostProcessShader() const
{
    return m_enableShader ? m_shaderId : 0;
}

void SandBox::InitParticles()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-800.0f, 800.0f);
    std::uniform_real_distribution<float> velDist(-150.0f, 150.0f);
    std::uniform_int_distribution<int> colDist(100, 255);

    for (int i = 0; i < 100000; ++i)
    {
        Entity e = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(e, posDist(rng), posDist(rng));
        m_registry.AddComponent<Velocity2D>(e, velDist(rng), velDist(rng));
        m_registry.AddComponent<ColorComponent>(e, Color{ static_cast<uint8_t>(colDist(rng)), static_cast<uint8_t>(colDist(rng)), 255, 255 });
    }
}

void SandBox::HandleInput(float dt, const InputManager& input)
{
    const float speed = 600.0f * dt;

    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))    m_player.Y -= speed;
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))  m_player.Y += speed;
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))  m_player.X -= speed;
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D)) m_player.X += speed;

    if (input.IsKeyDown(KeyCode::Space)) m_camera.Rotation += 1.0f;
    else m_camera.Rotation = 0.0f;

    if (input.IsKeyDown(KeyCode::A)) m_camera.Zoom += 0.02f;
    if (input.IsKeyDown(KeyCode::E)) m_camera.Zoom -= 0.02f;
    if (m_camera.Zoom < 0.1f) m_camera.Zoom = 0.1f;

    m_camera.X += (m_player.X - m_camera.X) * 0.1f;
    m_camera.Y += (m_player.Y - m_camera.Y) * 0.1f;

    m_enableShader = input.IsKeyDown(KeyCode::F);

    if (input.IsKeyDown(KeyCode::C)) SpawnParticles(500, m_player.X, m_player.Y);
}

void SandBox::SpawnParticles(int count, float x, float y)
{
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> velDist(-300.0f, 300.0f);
    std::uniform_int_distribution<int> colDist(150, 255);

    for (int i = 0; i < count; ++i)
    {
        Entity e = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(e, x, y);
        m_registry.AddComponent<Velocity2D>(e, velDist(rng), velDist(rng));
        m_registry.AddComponent<ColorComponent>(e, Color{ static_cast<uint8_t>(colDist(rng)), static_cast<uint8_t>(colDist(rng)), 255, 255 });
    }
}

void SandBox::DrawLights(Renderer& renderer)
{
    renderer.DrawCircle(80.0f, Transform2D{0, 0.0f, -20.0f}, { 255, 0, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, Transform2D{0, -20.0f, 20.0f}, { 0, 255, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, Transform2D{0, 20.0f, 20.0f}, { 0, 0, 255, 150 }, BlendMode::Add);
}

void SandBox::DrawUI(GameContext& context)
{
    std::string debugText = "FPS : " + std::to_string(context.Data.FPS) + "\n";
    debugText += "Active Particles : " + std::to_string(m_registry.GetActiveEntityCount()) + "\n";
    debugText += "Toggle shader 'F' \n";
    debugText += "Move 'Z' 'Q' 'S' 'D' | Rotate 'Space' | Zoom 'E' / 'A' | Spawn 'C'";

    context.Render.DrawText(debugText, m_fontId, 24.0f, Transform2D{0, 10.0f, 10.0f}, Colors::Yellow);
}
