#include "Window.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "Transform2D.h"
#include <string>

int main()
{
    Window window;
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    const uint32_t FONT_DEBUG = resourceManager.LoadResource("Resources/font/arial.ttf");
    const uint32_t TEX_LOGO = resourceManager.LoadResource("Resources/images/debug.jpg");

    Transform2D playerTransform{590.0f, 310.0f, 0.0f, 1.0f, 1.0f};
    Transform2D obstacleTransform{100.0f, 100.0f, 45.0f, 1.0f, 1.0f};
    Transform2D spriteTransform{900.0f, 50.0f, 0.0f, 0.5f, 0.5f};
    Transform2D textTransform{10.0f, 10.0f, 0.0f, 1.0f, 1.0f};

    const float speed = 10.0f;

    while (window.IsOpen())
    {
        inputManager.Update();

        if (!window.PollEvents(inputManager))
        {
            window.Close();
            break;
        }

        if (inputManager.IsKeyDown(KeyCode::Up) || inputManager.IsKeyDown(KeyCode::Z) || inputManager.IsKeyDown(KeyCode::W))
            playerTransform.Y -= speed;
        if (inputManager.IsKeyDown(KeyCode::Down) || inputManager.IsKeyDown(KeyCode::S))
            playerTransform.Y += speed;
        if (inputManager.IsKeyDown(KeyCode::Left) || inputManager.IsKeyDown(KeyCode::Q) || inputManager.IsKeyDown(KeyCode::A))
            playerTransform.X -= speed;
        if (inputManager.IsKeyDown(KeyCode::Right) || inputManager.IsKeyDown(KeyCode::D))
            playerTransform.X += speed;

        obstacleTransform.Rotation += 1.0f;

        std::string debugText = "Debug UI | Player X: " + std::to_string(static_cast<int>(playerTransform.X)) +
                                " Y: " + std::to_string(static_cast<int>(playerTransform.Y));

        renderer.BeginDraw();

        renderer.DrawRectangle(150.0f, 50.0f, obstacleTransform, Colors::Red);
        renderer.DrawSprite(TEX_LOGO, spriteTransform, Colors::White);
        renderer.DrawCircle(50.0f, playerTransform, Colors::Green);
        renderer.DrawText(debugText, FONT_DEBUG, 24.0f, textTransform, Colors::Yellow);

        renderer.EndDraw();
    }

    return 0;
}