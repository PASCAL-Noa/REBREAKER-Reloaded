#pragma once

#include <SFML/Graphics.hpp>
#include "WindowConfig.h"

class Window
{
public:
    Window(const WindowConfig& config);
    ~Window();

    bool                IsOpen() const;
    void                Close();
    void                Clear();
    void                Display();

    void                ApplyConfig(const WindowConfig& config);
    sf::RenderWindow&   GetNative();

private:
    sf::RenderWindow    m_window;
    WindowConfig        m_config;
};