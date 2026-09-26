#include "Core/InputManager.h"

void InputManager::Update()
{
    m_previousKeys = m_currentKeys;
    m_previousMouse = m_currentMouse;
    m_mouseWheelDelta = 0.0f;
    m_enteredText.clear();
}

void InputManager::SetKeyState(const KeyCode key, const bool isDown)
{
    m_currentKeys[static_cast<size_t>(key)] = isDown;
}

bool InputManager::IsKeyDown(const KeyCode key) const
{
    return m_currentKeys[static_cast<size_t>(key)];
}

bool InputManager::IsKeyPress(const KeyCode key) const
{
    const size_t index = static_cast<size_t>(key);
    return m_currentKeys[index] && !m_previousKeys[index];
}

bool InputManager::IsKeyRelease(const KeyCode key) const
{
    const size_t index = static_cast<size_t>(key);
    return !m_currentKeys[index] && m_previousKeys[index];
}


void InputManager::SetMouseButtonState(const MouseButton button, const bool isDown)
{
    m_currentMouse[static_cast<size_t>(button)] = isDown;
}

bool InputManager::IsMouseButtonDown(const MouseButton button) const
{
    return m_currentMouse[static_cast<size_t>(button)];
}

bool InputManager::IsMouseButtonPress(const MouseButton button) const
{
    const size_t index = static_cast<size_t>(button);
    return m_currentMouse[index] && !m_previousMouse[index];
}

bool InputManager::IsMouseButtonRelease(const MouseButton button) const
{
    const size_t index = static_cast<size_t>(button);
    return !m_currentMouse[index] && m_previousMouse[index];
}

void InputManager::SetMousePosition(const float x, const float y)
{
    m_mousePosition.first = x;
    m_mousePosition.second = y;
}

void InputManager::AppendEnteredText(uint32_t unicode)
{
    if (unicode >= 32 && unicode < 128)
    {
        m_enteredText += static_cast<char>(unicode);
    }
}

const std::string& InputManager::GetEnteredText() const
{
    return m_enteredText;
}

const std::pair<float, float>& InputManager::GetMousePosition() const
{
    return m_mousePosition;
}

void InputManager::SetMouseWheelDelta(float delta)
{
    m_mouseWheelDelta = delta;
}

float InputManager::GetMouseWheelDelta() const
{
    return m_mouseWheelDelta;
}