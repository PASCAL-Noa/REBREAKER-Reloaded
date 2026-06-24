#include "System/Window.h"
#include "System/InputTranslator.h"
#include <optional>


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

bool Window::PollEvents(InputManager& input)
{
    const auto handleKey = [&input](auto sfKey, bool isPressed)
    {
        if (const KeyCode code = InputTranslator::TranslateKey(sfKey); code != KeyCode::Count)
            input.SetKeyState(code, isPressed);
    };

    const auto handleMouse = [&input](auto sfButton, bool isPressed)
    {
        if (const MouseButton btn = InputTranslator::TranslateMouseButton(sfButton); btn != MouseButton::Count)
            input.SetMouseButtonState(btn, isPressed);
    };

    while (const std::optional<sf::Event> event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
            return false;

        if (const auto* e = event->getIf<sf::Event::KeyPressed>())
            handleKey(e->code, true);
        else if (const auto* e = event->getIf<sf::Event::KeyReleased>())
            handleKey(e->code, false);
        else if (const auto* e = event->getIf<sf::Event::MouseButtonPressed>())
            handleMouse(e->button, true);
        else if (const auto* e = event->getIf<sf::Event::MouseButtonReleased>())
            handleMouse(e->button, false);
        else if (const auto* e = event->getIf<sf::Event::MouseWheelScrolled>())
            input.SetMouseWheelDelta(e->delta);
    }

    sf::Vector2i mousePos = sf::Mouse::getPosition(m_window);
    input.SetMousePosition(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

    return true;
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
    m_window.setFramerateLimit(0);
    m_window.setVerticalSyncEnabled(m_config.VSync);
}

const WindowConfig& Window::GetConfig() const
{
    return m_config;
}

sf::RenderWindow& Window::GetNative()
{
    return m_window;
}

void Window::SetVSync(bool enabled)
{
    if (enabled) {
        m_window.setFramerateLimit(0);
    }
    m_window.setVerticalSyncEnabled(enabled);
}

std::vector<Resolution> Window::GetSupportedResolutions()
{
    std::vector<Resolution> resolutions;
    std::vector<sf::VideoMode> modes = sf::VideoMode::getFullscreenModes();
    
    for (const auto& mode : modes)
    {
        if (mode.bitsPerPixel != 32) continue;
        
        bool duplicate = false;
        for (const auto& r : resolutions)
        {
            if (r.Width == mode.size.x && r.Height == mode.size.y)
            {
                duplicate = true;
                break;
            }
        }
        
        if (!duplicate)
        {
            resolutions.push_back(Resolution{mode.size.x, mode.size.y});
        }
    }
    
    return resolutions;
}
