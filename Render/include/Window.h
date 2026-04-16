#pragma once

#include <SFML/Graphics.hpp>

#include "InputManager.h"
#include "WindowConfig.h"

class Window
{
public:
    Window(const WindowConfig& config = WindowConfig());
    ~Window();

    bool                IsOpen() const;
    void                Close();
    void                Clear();
    void                Display();

    bool                PollEvents(InputManager& input);
    void                ApplyConfig(const WindowConfig& config);
    sf::RenderWindow&   GetNative();

private:
    sf::RenderWindow    m_window;
    WindowConfig        m_config;
};