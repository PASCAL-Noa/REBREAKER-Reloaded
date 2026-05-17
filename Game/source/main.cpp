#include "System/Window.h"
#include "InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "Transform2D.h"
#include "Camera2D.h"
#include <string>
#include <cmath>

static void HandlePlayerInput(const InputManager& input, Transform2D& player, float speed)
{
    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))    player.Y -= speed;
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))  player.Y += speed;
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))  player.X -= speed;
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D)) player.X += speed;
}

static void HandleCameraInput(const InputManager& input, Camera2D& camera)
{
    if (input.IsKeyDown(KeyCode::Space)) camera.Rotation += 2.0f;
    else camera.Rotation = 0.0f;

    if (input.IsKeyDown(KeyCode::A)) camera.Zoom += 0.01f;
    if (input.IsKeyDown(KeyCode::E)) camera.Zoom -= 0.01f;
    if (camera.Zoom < 0.1f) camera.Zoom = 0.1f;
}

static void UpdateCamera(Camera2D& camera, const Transform2D& target)
{
    camera.X += (target.X - camera.X) * 0.1f;
    camera.Y += (target.Y - camera.Y) * 0.1f;
}

static void DrawGrid(Renderer& renderer)
{
    for (int x = -1000; x <= 1000; x += 200)
    {
        for (int y = -1000; y <= 1000; y += 200)
        {
            renderer.DrawCircle(5.0f, {static_cast<float>(x), static_cast<float>(y), 0.0f, 1.0f, 1.0f}, Colors::Blue);
        }
    }
}

static void DrawDebugUI(Renderer& renderer, const Camera2D& camera, uint32_t fontId)
{
    std::string debugText = "Controles: ZQSD (Mouvement) | E/A (Zoom) | Espace (Rotation)\n";
    debugText += "Camera X: " + std::to_string(static_cast<int>(camera.X)) +
                 " Y: " + std::to_string(static_cast<int>(camera.Y)) +
                 " Zoom: " + std::to_string(camera.Zoom);

    renderer.ResetCamera();
    renderer.DrawText(debugText, fontId, 20.0f, {10.0f, 10.0f, 0.0f, 1.0f, 1.0f}, Colors::Yellow);
}

static void DrawLights(Renderer& renderer)
{
    renderer.DrawCircle(80.0f, { 0.0f, -20.0f, 0.0f, 1.0f, 1.0f }, { 255, 0, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, { -20.0f, 20.0f, 0.0f, 1.0f, 1.0f }, { 0, 255, 0, 150 }, BlendMode::Add);
    renderer.DrawCircle(80.0f, { 20.0f, 20.0f, 0.0f, 1.0f, 1.0f }, { 0, 0, 255, 150 }, BlendMode::Add);
}

static void DrawBurst(Renderer& renderer)
{
    std::vector<Vertex> lines;
    lines.reserve(72);

    for (int i = 0; i < 360; i += 10)
    {
        const float rad = i * 3.14159f / 180.0f;
        lines.push_back({0.0f, 0.0f, Colors::White, 0.0f, 0.0f});
        lines.push_back({std::cos(rad) * 400.0f, std::sin(rad) * 400.0f, {255, 0, 0, 0}, 0.0f, 0.0f});
    }

    renderer.DrawVertices(lines, PrimitiveType::Lines, 0, BlendMode::Add);
}

static void RenderScene(Renderer& renderer, const Camera2D& camera, const Transform2D& player, const Transform2D& obstacle, uint32_t fontId, uint32_t shaderId, bool useShader)
{
    renderer.BeginDraw();

    renderer.SetCamera(camera);
    DrawGrid(renderer);
    DrawBurst(renderer);
    DrawLights(renderer);
    renderer.DrawRectangle(150.0f, 50.0f, obstacle, Colors::Red);
    renderer.DrawCircle(50.0f, player, Colors::Green);

    DrawDebugUI(renderer, camera, fontId);

    renderer.EndDraw(useShader ? shaderId : 0);
}

int main()
{
    Window window;
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    const uint32_t fontDebugId = resourceManager.LoadResource("Resources/font/arial.ttf");
    const uint32_t shaderFxId = resourceManager.LoadResource("Resources/shaders/fx.frag");

    Transform2D playerTransform{0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    Transform2D obstacleTransform{300.0f, 200.0f, 45.0f, 1.0f, 1.0f};
    Camera2D camera{playerTransform.X, playerTransform.Y, 1.0f, 0.0f};

    constexpr float speed = 10.0f;

    while (window.IsOpen())
    {
        inputManager.Update();

        if (!window.PollEvents(inputManager))
        {
            window.Close();
            break;
        }

        HandlePlayerInput(inputManager, playerTransform, speed);
        HandleCameraInput(inputManager, camera);
        UpdateCamera(camera, playerTransform);

        const bool enableShader = inputManager.IsKeyDown(KeyCode::F);

        RenderScene(renderer, camera, playerTransform, obstacleTransform, fontDebugId, shaderFxId, enableShader);
    }

    return 0;
}