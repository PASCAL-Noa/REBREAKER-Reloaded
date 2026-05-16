#include "Window.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "Transform2D.h"
#include "Camera2D.h"
#include <string>

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
            renderer.DrawCircle(5.0f, {static_cast<float>(x), static_cast<float>(y), 0, 1, 1}, Colors::Blue);
        }
    }
}

static void DrawDebugUI(Renderer& renderer, const Camera2D& camera, uint32_t fontId)
{
    std::string debugText = "Controles: ZQSD (Mouvement) | E/A (Zoom) | Espace (Rotation)\n";
    debugText += "Camera X: " + std::to_string(static_cast<int>(camera.X)) +
                 " Y: " + std::to_string(static_cast<int>(camera.Y)) +
                 " Zoom: " + std::to_string(camera.Zoom);

    Transform2D textTransform{10.0f, 10.0f, 0.0f, 1.0f, 1.0f};

    renderer.ResetCamera();
    renderer.DrawText(debugText, fontId, 20.0f, textTransform, Colors::Yellow);
}

int main()
{
    Window window;
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    const uint32_t fontDebugId = resourceManager.LoadResource("Resources/font/arial.ttf");

    Transform2D playerTransform{0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    Camera2D camera{playerTransform.X, playerTransform.Y, 1.0f, 0.0f};

    while (window.IsOpen())
    {
        constexpr float speed = 10.0f;
        Transform2D obstacleTransform{300.0f, 200.0f, 45.0f, 1.0f, 1.0f};
        inputManager.Update();

        if (!window.PollEvents(inputManager))
        {
            window.Close();
            break;
        }

        HandlePlayerInput(inputManager, playerTransform, speed);
        HandleCameraInput(inputManager, camera);
        UpdateCamera(camera, playerTransform);

        renderer.BeginDraw();

        renderer.SetCamera(camera);
        DrawGrid(renderer);
        renderer.DrawRectangle(150.0f, 50.0f, obstacleTransform, Colors::Red);
        renderer.DrawCircle(50.0f, playerTransform, Colors::Green);

        DrawDebugUI(renderer, camera, fontDebugId);

        renderer.EndDraw();
    }

    return 0;
}