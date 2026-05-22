#include "System/Window.h"
#include "InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "Components/Transform2D.h"
#include "Components/Camera2D.h"
#include "Timer.h"
#include <string>
#include <vector>
#include <random>


struct Particle
{
    float x, y;
    float vx, vy;
    Color color;
};

constexpr int NUM_PARTICLES = 1000000;
std::vector<Particle> particles;

static void InitParticles()
{
    particles.resize(NUM_PARTICLES);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-800.0f, 800.0f);
    std::uniform_real_distribution<float> velDist(-150.0f, 150.0f);
    std::uniform_int_distribution<int> colDist(100, 255);

    for (auto& p : particles)
    {
        p.x = posDist(rng);
        p.y = posDist(rng);
        p.vx = velDist(rng);
        p.vy = velDist(rng);
        p.color = { static_cast<uint8_t>(colDist(rng)), static_cast<uint8_t>(colDist(rng)), 255, 255 };
    }
}

static void UpdateParticles(float dt)
{
    for (auto& p : particles)
    {
        p.x += p.vx * dt;
        p.y += p.vy * dt;

        if (p.x < -1000.0f || p.x > 1000.0f) p.vx *= -1.0f;
        if (p.y < -1000.0f || p.y > 1000.0f) p.vy *= -1.0f;
    }
}

static void DrawParticles(Renderer& renderer)
{
    std::vector<Vertex> vertices;
    vertices.reserve(NUM_PARTICLES);
    for (const auto& p : particles)
    {
        vertices.push_back({ p.x, p.y, p.color, 0.0f, 0.0f });
    }

    renderer.DrawVertices(vertices, PrimitiveType::Points, 0, BlendMode::Add);
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
    renderer.DrawCircle(80.0f, { 0.0f, -20.0f, 0.0f, 1.0f, 1.0f }, { 255, 0, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, { -20.0f, 20.0f, 0.0f, 1.0f, 1.0f }, { 0, 255, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, { 20.0f, 20.0f, 0.0f, 1.0f, 1.0f }, { 0, 0, 255, 150 }, BlendMode::Add);
}

int main()
{
    Window window{};
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    const uint32_t fontDebugId = resourceManager.LoadResource("Resources/font/arial.ttf");
    const uint32_t shaderFxId = resourceManager.LoadResource("Resources/shaders/fx.frag");

    Transform2D playerTransform{0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    Camera2D camera{playerTransform.X, playerTransform.Y, 1.0f, 0.0f};

    InitParticles();

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
        UpdateParticles(dt);

        const bool enableShader = inputManager.IsKeyDown(KeyCode::F);

        renderer.BeginDraw();

        renderer.SetCamera(camera);
        renderer.DrawRectangle(50.0f, 50.0f, { -25.0f, -25.0f, 0.0f, 1.0f, 1.0f }, Colors::Red);
        DrawParticles(renderer);
        DrawLights(renderer);
        renderer.DrawCircle(30.0f, playerTransform, Colors::Green);

        renderer.ResetCamera();

        std::string debugText = "FPS : " + std::to_string(static_cast<int>(time.GetFPS())) + "\n";
        debugText += "Particles : " + std::to_string(NUM_PARTICLES) + "\n";
        debugText += "Toggle shader 'F' \n";
        debugText += "Move 'Z' 'Q' 'S' 'D' | Rotate 'Space' | Zoom in 'E' / Zoom out 'A')";

        renderer.DrawText(debugText, fontDebugId, 24.0f, {10.0f, 10.0f, 0.0f, 1.0f, 1.0f}, Colors::Yellow);

        renderer.EndDraw(enableShader ? shaderFxId : 0);
    }

    return 0;
}