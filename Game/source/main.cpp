#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include "System/Window.h"
#include "InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "ECS/Registry.hpp"
#include "ECS/SystemManager.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/Velocity2D.h"
#include "ECS/Components/ColorComponent.h"
#include "ECS/Systems/ParticleMovementSystem.h"
#include "ECS/Systems/ParticleRenderSystem.h"
#include "Timer.h"
#include "Core/Debug.h"
#include <string>
#include <random>

constexpr int NUM_PARTICLES = 100000;

static void InitECSParticles(Registry& registry)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-800.0f, 800.0f);
    std::uniform_real_distribution<float> velDist(-150.0f, 150.0f);
    std::uniform_int_distribution<int> colDist(100, 255);

    for (int i = 0; i < NUM_PARTICLES; ++i)
    {
        Entity e = registry.CreateEntity();

        registry.AddComponent<Transform2D>(e, posDist(rng), posDist(rng));
        registry.AddComponent<Velocity2D>(e, velDist(rng), velDist(rng));

        Color color = { static_cast<uint8_t>(colDist(rng)), static_cast<uint8_t>(colDist(rng)), 255, 255 };
        registry.AddComponent<ColorComponent>(e, color);
    }
}

static void HandlePlayerInput(const InputManager& input, Transform2D& player, float speed)
{
    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))    player.Y -= speed;
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))  player.Y += speed;
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))  player.X -= speed;
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D)) player.X += speed;
}

static void HandleCameraInput(const InputManager& input, Camera2D& camera)
{
    if (input.IsKeyDown(KeyCode::Space)) camera.Rotation += 1.0f;
    else camera.Rotation = 0.0f;

    if (input.IsKeyDown(KeyCode::A)) camera.Zoom += 0.02f;
    if (input.IsKeyDown(KeyCode::E)) camera.Zoom -= 0.02f;
    if (camera.Zoom < 0.1f) camera.Zoom = 0.1f;
}

static void UpdateCamera(Camera2D& camera, const Transform2D& target)
{
    camera.X += (target.X - camera.X) * 0.1f;
    camera.Y += (target.Y - camera.Y) * 0.1f;
}

static void DrawLights(Renderer& renderer)
{
    renderer.DrawCircle(80.0f, Transform2D{0, 0.0f, -20.0f}, { 255, 0, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, Transform2D{0, -20.0f, 20.0f}, { 0, 255, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, Transform2D{0, 20.0f, 20.0f}, { 0, 0, 255, 150 }, BlendMode::Add);
}

static void SpawnParticles(Registry& registry, int count, float x, float y)
{
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> velDist(-300.0f, 300.0f);
    std::uniform_int_distribution<int> colDist(150, 255);

    for (int i = 0; i < count; ++i)
    {
        Entity e = registry.CreateEntity();

        registry.AddComponent<Transform2D>(e, x, y);
        registry.AddComponent<Velocity2D>(e, velDist(rng), velDist(rng));

        Color color = { static_cast<uint8_t>(colDist(rng)), static_cast<uint8_t>(colDist(rng)), 255, 255 };
        registry.AddComponent<ColorComponent>(e, color);
    }
}

int main()
{
    Debug::Init();
    Debug::Info("Engine initialized successfully.");

    Window window{};

    Debug::Warning("Testing warnings... Max FPS set to {}", window.GetConfig().MaxFPS);

    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    Registry registry;
    SystemManager systemManager;

    systemManager.AddSystem<ParticleMovementSystem>(registry);
    systemManager.AddSystem<ParticleRenderSystem>(registry, renderer, NUM_PARTICLES);

    const uint32_t fontDebugId = resourceManager.LoadResource("Resources/font/arial.ttf");
    const uint32_t shaderFxId = resourceManager.LoadResource("Resources/shaders/fx.frag");

    Transform2D playerTransform{0, 0.0f, 0.0f};
    Camera2D camera{0, 0.0f, 0.0f};

    InitECSParticles(registry);
    systemManager.OnInit();

    Timer time(window.GetConfig().VSync ? 0 : window.GetConfig().MaxFPS);

    while (window.IsOpen())
    {
        time.Update();

        inputManager.Update();
        if (!window.PollEvents(inputManager)) break;

        const float dt = time.GetDeltaTime();
        const float playerSpeed = 600.0f * dt;

        HandlePlayerInput(inputManager, playerTransform, playerSpeed);
        HandleCameraInput(inputManager, camera);
        UpdateCamera(camera, playerTransform);

        if (inputManager.IsKeyDown(KeyCode::C))
        {
            SpawnParticles(registry, 500, playerTransform.X, playerTransform.Y);
            Debug::Info("Spawned 500 particles at X:{}, Y:{}", playerTransform.X, playerTransform.Y);
        }

        const bool enableShader = inputManager.IsKeyDown(KeyCode::F);

        renderer.BeginDraw();
        renderer.SetCamera(camera);

        renderer.DrawRectangle(50.0f, 50.0f, Transform2D{0, -25.0f, -25.0f}, Colors::Red);

        systemManager.OnUpdate(dt);

        DrawLights(renderer);
        renderer.DrawCircle(30.0f, playerTransform, Colors::Green);

        renderer.ResetCamera();

        std::string debugText = "FPS : " + std::to_string(static_cast<int>(time.GetFPS())) + "\n";
        debugText += "Active Particles : " + std::to_string(registry.GetActiveEntityCount()) + "\n";
        debugText += "Toggle shader 'F' \n";
        debugText += "Move 'Z' 'Q' 'S' 'D' | Rotate 'Space' | Zoom 'E' / 'A' | Spawn 'C'";

        renderer.DrawText(debugText, fontDebugId, 24.0f, Transform2D{0, 10.0f, 10.0f}, Colors::Yellow);

        renderer.EndDraw(enableShader ? shaderFxId : 0);
    }

    return 0;
}