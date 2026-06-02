#pragma once

#include <SFML/Graphics.hpp>

#include "Core/InputManager.h"
#include "Data/WindowConfig.h"

class Window
{
public:
    Window(const WindowConfig& config = WindowConfig{});
    ~Window();

    bool                IsOpen() const;
    void                Close();
    void                Clear();
    void                Display();

    bool                PollEvents(InputManager& input);
    void                ApplyConfig(const WindowConfig& config);

    const WindowConfig& GetConfig() const;
    sf::RenderWindow&   GetNative();
    void                SetVSync(bool enabled);

private:
    sf::RenderWindow    m_window;
    WindowConfig        m_config;
};