#include "Window.h"
#include "InputManager.h"
#include <SFML/Graphics.hpp>

int main()
{
    Window window{};
    InputManager inputManager;

    sf::CircleShape player(50.0f);
    player.setFillColor(sf::Color::Green);

    player.setPosition({590.0f, 310.0f});

    const float speed = 10.0f;

    while (window.IsOpen())
    {
        inputManager.Update();

        if (!window.PollEvents(inputManager))
        {
            window.Close();
            break;
        }

        sf::Vector2f movement(0.0f, 0.0f);

        if (inputManager.IsKeyDown(KeyCode::Up) || inputManager.IsKeyDown(KeyCode::Z) || inputManager.IsKeyDown(KeyCode::W))
            movement.y -= speed;
        if (inputManager.IsKeyDown(KeyCode::Down) || inputManager.IsKeyDown(KeyCode::S))
            movement.y += speed;
        if (inputManager.IsKeyDown(KeyCode::Left) || inputManager.IsKeyDown(KeyCode::Q) || inputManager.IsKeyDown(KeyCode::A))
            movement.x -= speed;
        if (inputManager.IsKeyDown(KeyCode::Right) || inputManager.IsKeyDown(KeyCode::D))
            movement.x += speed;

        player.move(movement);

        window.Clear();
        window.GetNative().draw(player);
        window.Display();
    }

    return 0;
}