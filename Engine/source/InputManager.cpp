#include "Core/InputManager.h"
#include "Core/InputRemapManager.h"
#include <cmath>
#include <algorithm>

void InputManager::Update()
{
    m_previousKeys = m_currentKeys;
    m_previousMouse = m_currentMouse;
    m_previousJoyButtons = m_currentJoyButtons;
    m_previousJoyAxes = m_joystickAxes;
    m_mouseWheelDelta = 0.0f;
    m_enteredText.clear();
    m_lastKeyPressed = KeyCode::Count;
}

void InputManager::SetKeyState(const KeyCode key, const bool isDown)
{
    const size_t index = static_cast<size_t>(key);
    if (index < KEY_COUNT)
    {
        m_currentKeys[index] = isDown;
        if (isDown)
        {
            m_lastKeyPressed = key;
        }
    }
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

void InputManager::SetJoystickConnected(unsigned int id, bool connected)
{
    if (id < MAX_JOYSTICKS)
    {
        m_joystickConnected[id] = connected;
    }
}

bool InputManager::IsJoystickConnected(unsigned int id) const
{
    if (id < MAX_JOYSTICKS)
    {
        return m_joystickConnected[id];
    }
    return false;
}

void InputManager::SetJoystickAxis(unsigned int id, unsigned int axis, float position)
{
    if (id < MAX_JOYSTICKS && axis < MAX_JOYSTICK_AXES)
    {
        m_joystickAxes[id][axis] = position;
    }
}

float InputManager::GetJoystickAxis(unsigned int id, unsigned int axis) const
{
    if (id < MAX_JOYSTICKS && axis < MAX_JOYSTICK_AXES)
    {
        return m_joystickAxes[id][axis];
    }
    return 0.0f;
}

void InputManager::SetJoystickButton(unsigned int id, unsigned int button, bool isDown)
{
    if (id < MAX_JOYSTICKS && button < MAX_JOYSTICK_BUTTONS)
    {
        m_currentJoyButtons[id][button] = isDown;
    }
}

bool InputManager::IsJoystickButtonDown(unsigned int id, unsigned int button) const
{
    if (id < MAX_JOYSTICKS && button < MAX_JOYSTICK_BUTTONS)
    {
        return m_currentJoyButtons[id][button];
    }
    return false;
}

bool InputManager::IsJoystickButtonPress(unsigned int id, unsigned int button) const
{
    if (id < MAX_JOYSTICKS && button < MAX_JOYSTICK_BUTTONS)
    {
        return m_currentJoyButtons[id][button] && !m_previousJoyButtons[id][button];
    }
    return false;
}

bool InputManager::IsJoystickButtonRelease(unsigned int id, unsigned int button) const
{
    if (id < MAX_JOYSTICKS && button < MAX_JOYSTICK_BUTTONS)
    {
        return !m_currentJoyButtons[id][button] && m_previousJoyButtons[id][button];
    }
    return false;
}

float InputManager::GetGamepadMoveX(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return 0.0f;
    }

    // Axis 0 = Left Stick X (sf::Joystick::Axis::X)
    float stickX = m_joystickAxes[id][0]; // range [-100 .. 100]
    constexpr float deadzone = 15.0f;

    float result = 0.0f;
    if (std::abs(stickX) > deadzone)
    {
        result = (stickX > 0.0f) ? (stickX - deadzone) / (100.0f - deadzone)
                                 : (stickX + deadzone) / (100.0f - deadzone);
    }

    // Axis 6 = PovX (D-Pad horizontal in SFML)
    float povX = m_joystickAxes[id][6];
    if (std::abs(povX) > deadzone)
    {
        result = (povX > 0.0f) ? 1.0f : -1.0f;
    }

    return std::clamp(result, -1.0f, 1.0f);
}

float InputManager::GetGamepadMoveY(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return 0.0f;
    }

    // Axis 1 = Left Stick Y (sf::Joystick::Axis::Y: -100 up, +100 down)
    float stickY = m_joystickAxes[id][1];
    constexpr float deadzone = 15.0f;

    float result = 0.0f;
    if (std::abs(stickY) > deadzone)
    {
        result = (stickY > 0.0f) ? (stickY - deadzone) / (100.0f - deadzone)
                                 : (stickY + deadzone) / (100.0f - deadzone);
    }

    // Axis 7 = PovY (D-Pad vertical in SFML: +100 up, -100 down)
    float povY = m_joystickAxes[id][7];
    if (std::abs(povY) > deadzone)
    {
        result = (povY > 0.0f) ? -1.0f : 1.0f; // -1.0 is up, +1.0 is down to match stick
    }

    return std::clamp(result, -1.0f, 1.0f);
}

bool InputManager::IsGamepadDirectionActive(GamepadDirection dir, unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return false;
    }

    constexpr float threshold = 40.0f;
    switch (dir)
    {
        case GamepadDirection::Up:
            return m_joystickAxes[id][1] < -threshold || m_joystickAxes[id][7] > threshold;
        case GamepadDirection::Down:
            return m_joystickAxes[id][1] > threshold || m_joystickAxes[id][7] < -threshold;
        case GamepadDirection::Left:
            return m_joystickAxes[id][0] < -threshold || m_joystickAxes[id][6] < -threshold;
        case GamepadDirection::Right:
            return m_joystickAxes[id][0] > threshold || m_joystickAxes[id][6] > threshold;
    }
    return false;
}

bool InputManager::IsGamepadDirectionTriggered(GamepadDirection dir, unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return false;
    }

    constexpr float threshold = 40.0f;
    switch (dir)
    {
        case GamepadDirection::Up:
        {
            bool curr = m_joystickAxes[id][1] < -threshold || m_joystickAxes[id][7] > threshold;
            bool prev = m_previousJoyAxes[id][1] < -threshold || m_previousJoyAxes[id][7] > threshold;
            return curr && !prev;
        }
        case GamepadDirection::Down:
        {
            bool curr = m_joystickAxes[id][1] > threshold || m_joystickAxes[id][7] < -threshold;
            bool prev = m_previousJoyAxes[id][1] > threshold || m_previousJoyAxes[id][7] < -threshold;
            return curr && !prev;
        }
        case GamepadDirection::Left:
        {
            bool curr = m_joystickAxes[id][0] < -threshold || m_joystickAxes[id][6] < -threshold;
            bool prev = m_previousJoyAxes[id][0] < -threshold || m_previousJoyAxes[id][6] < -threshold;
            return curr && !prev;
        }
        case GamepadDirection::Right:
        {
            bool curr = m_joystickAxes[id][0] > threshold || m_joystickAxes[id][6] > threshold;
            bool prev = m_previousJoyAxes[id][0] > threshold || m_previousJoyAxes[id][6] > threshold;
            return curr && !prev;
        }
    }
    return false;
}

bool InputManager::IsMenuUpTriggered() const
{
    return IsKeyPress(KeyCode::Up) || IsGamepadDirectionTriggered(GamepadDirection::Up);
}

bool InputManager::IsMenuDownTriggered() const
{
    return IsKeyPress(KeyCode::Down) || IsGamepadDirectionTriggered(GamepadDirection::Down);
}

bool InputManager::IsMenuLeftTriggered() const
{
    return IsKeyPress(KeyCode::Left) || IsGamepadDirectionTriggered(GamepadDirection::Left);
}

bool InputManager::IsMenuRightTriggered() const
{
    return IsKeyPress(KeyCode::Right) || IsGamepadDirectionTriggered(GamepadDirection::Right);
}

bool InputManager::IsMenuSelectTriggered() const
{
    return IsKeyPress(KeyCode::Enter) || IsKeyPress(KeyCode::Space) || IsJoystickButtonPress(0, 0);
}

bool InputManager::IsMenuCancelTriggered() const
{
    return IsKeyPress(KeyCode::Escape) || IsJoystickButtonPress(0, 1);
}

bool InputManager::IsGamepadActionActive(InputAction action, unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return false;
    }

    switch (action)
    {
        case InputAction::MoveLeft:
            return GetGamepadMoveX(id) < -0.3f;
        case InputAction::MoveRight:
            return GetGamepadMoveX(id) > 0.3f;
        case InputAction::LaunchBall:
        case InputAction::FireLaser:
            // A Button (0), X Button (2), Right Trigger (Axis 2)
            return IsJoystickButtonDown(id, 0) || IsJoystickButtonDown(id, 2) || (m_joystickAxes[id][2] > 40.0f);
        case InputAction::Pause:
            // Start / Menu button (7 or 6)
            return IsJoystickButtonDown(id, 7) || IsJoystickButtonDown(id, 6);
        default:
            return false;
    }
}

bool InputManager::IsGamepadActionTriggered(InputAction action, unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return false;
    }

    switch (action)
    {
        case InputAction::MoveLeft:
            return GetGamepadMoveX(id) < -0.4f;
        case InputAction::MoveRight:
            return GetGamepadMoveX(id) > 0.4f;
        case InputAction::LaunchBall:
        case InputAction::FireLaser:
            return IsJoystickButtonPress(id, 0) || IsJoystickButtonPress(id, 2);
        case InputAction::Pause:
            return IsJoystickButtonPress(id, 7) || IsJoystickButtonPress(id, 6);
        default:
            return false;
    }
}

float InputManager::GetGamepadRightStickY(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return 0.0f;
    }
    constexpr float deadzone = 25.0f;
    float stickY = m_joystickAxes[id][5];
    if (std::abs(stickY) < deadzone && std::abs(m_joystickAxes[id][3]) > deadzone)
    {
        stickY = m_joystickAxes[id][3];
    }
    if (std::abs(stickY) > deadzone)
    {
        return (stickY > 0.0f) ? (stickY - deadzone) / (100.0f - deadzone)
                               : (stickY + deadzone) / (100.0f - deadzone);
    }
    return 0.0f;
}

bool InputManager::IsGamepadRightStickUpTriggered(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id]) return false;
    constexpr float threshold = 40.0f;
    bool curr = (m_joystickAxes[id][5] < -threshold) || (m_joystickAxes[id][3] < -threshold);
    bool prev = (m_previousJoyAxes[id][5] < -threshold) || (m_previousJoyAxes[id][3] < -threshold);
    return curr && !prev;
}

bool InputManager::IsGamepadRightStickDownTriggered(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id]) return false;
    constexpr float threshold = 40.0f;
    bool curr = (m_joystickAxes[id][5] > threshold) || (m_joystickAxes[id][3] > threshold);
    bool prev = (m_previousJoyAxes[id][5] > threshold) || (m_previousJoyAxes[id][3] > threshold);
    return curr && !prev;
}

bool InputManager::IsGamepadLeftBumperTriggered(unsigned int id) const
{
    return IsJoystickButtonPress(id, 4);
}

bool InputManager::IsGamepadRightBumperTriggered(unsigned int id) const
{
    return IsJoystickButtonPress(id, 5);
}

bool InputManager::IsActionActive(InputAction action) const
{
    if (InputRemapManager::Get().IsActionActive(action, *this))
    {
        return true;
    }
    return IsGamepadActionActive(action);
}

bool InputManager::IsActionTriggered(InputAction action) const
{
    if (InputRemapManager::Get().IsActionTriggered(action, *this))
    {
        return true;
    }
    return IsGamepadActionTriggered(action);
}