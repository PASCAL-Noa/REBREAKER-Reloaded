#include "Window.h"

Window::Window(const WindowConfig& config)
{
    ApplyConfig(config);
}

Window::~Window()
{
    if (m_window.isOpen()) m_window.close();
}

bool Window::IsOpen() const
{
    return m_window.isOpen();
}

void Window::Close()
{
    m_window.close();
}

void Window::Clear()
{
    m_window.clear();
}

void Window::Display()
{
    m_window.display();
}

void Window::ApplyConfig(const WindowConfig &config)
{
    m_config = config;

    if (m_window.isOpen())
        m_window.close();

    uint32_t sfStyle = sf::Style::Default;
    sf::State sfState = sf::State::Windowed;

    switch (m_config.Mode)
    {
        case WindowMode::Windowed:
            sfStyle = sf::Style::Default;
            sfState = sf::State::Windowed;
            break;
        case WindowMode::Fullscreen:
            sfStyle = sf::Style::Default;
            sfState = sf::State::Fullscreen;
            break;
        case WindowMode::Borderless:
            sfStyle = sf::Style::None;
            sfState = sf::State::Windowed;
            break;
    }

    m_window.create(sf::VideoMode({m_config.Width, m_config.Height}), m_config.Title, sfStyle, sfState);
    m_window.setFramerateLimit(m_config.MaxFPS);
}

sf::RenderWindow& Window::GetNative()
{
    return m_window;
}