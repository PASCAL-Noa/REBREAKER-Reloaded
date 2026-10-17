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

    const int mappedBtn = InputRemapManager::Get().GetGamepadBinding(action);
    bool isMappedBtnDown = false;
    if (mappedBtn >= 0 && static_cast<unsigned int>(mappedBtn) < MAX_JOYSTICK_BUTTONS)
    {
        isMappedBtnDown = IsJoystickButtonDown(id, static_cast<unsigned int>(mappedBtn));
    }
    else if (mappedBtn == 100) // D-Pad Left
    {
        isMappedBtnDown = (m_joystickAxes[id][6] < -35.0f);
    }
    else if (mappedBtn == 101) // D-Pad Right
    {
        isMappedBtnDown = (m_joystickAxes[id][6] > 35.0f);
    }
    else if (mappedBtn == 102) // D-Pad Up
    {
        isMappedBtnDown = (m_joystickAxes[id][7] > 35.0f);
    }
    else if (mappedBtn == 103) // D-Pad Down
    {
        isMappedBtnDown = (m_joystickAxes[id][7] < -35.0f);
    }
    else if (mappedBtn == 104) // Stick Left
    {
        isMappedBtnDown = (m_joystickAxes[id][0] < -35.0f);
    }
    else if (mappedBtn == 105) // Stick Right
    {
        isMappedBtnDown = (m_joystickAxes[id][0] > 35.0f);
    }
    else if (mappedBtn == 106) // Stick Up
    {
        isMappedBtnDown = (m_joystickAxes[id][1] < -35.0f);
    }
    else if (mappedBtn == 107) // Stick Down
    {
        isMappedBtnDown = (m_joystickAxes[id][1] > 35.0f);
    }
    else if (mappedBtn == 108) // LT
    {
        isMappedBtnDown = (m_joystickAxes[id][2] < -35.0f);
    }
    else if (mappedBtn == 109) // RT
    {
        isMappedBtnDown = (m_joystickAxes[id][2] > 35.0f);
    }

    switch (action)
    {
        case InputAction::MoveLeft:
            return GetGamepadMoveX(id) < -0.3f || isMappedBtnDown;
        case InputAction::MoveRight:
            return GetGamepadMoveX(id) > 0.3f || isMappedBtnDown;
        case InputAction::LaunchBall:
        case InputAction::FireLaser:
            // Bound button OR A Button (0) OR Right Trigger (Axis 2)
            return isMappedBtnDown || IsJoystickButtonDown(id, 0) || (m_joystickAxes[id][2] > 40.0f);
        case InputAction::Pause:
            // Bound button OR Start / Menu button (7)
            return isMappedBtnDown || IsJoystickButtonDown(id, 7);
        default:
            return isMappedBtnDown;
    }
}

bool InputManager::IsGamepadActionTriggered(InputAction action, unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return false;
    }

    const int mappedBtn = InputRemapManager::Get().GetGamepadBinding(action);
    bool isMappedBtnPress = false;
    if (mappedBtn >= 0 && static_cast<unsigned int>(mappedBtn) < MAX_JOYSTICK_BUTTONS)
    {
        isMappedBtnPress = IsJoystickButtonPress(id, static_cast<unsigned int>(mappedBtn));
    }
    else if (mappedBtn == 100) // D-Pad Left
    {
        isMappedBtnPress = (m_joystickAxes[id][6] < -35.0f) && !(m_previousJoyAxes[id][6] < -35.0f);
    }
    else if (mappedBtn == 101) // D-Pad Right
    {
        isMappedBtnPress = (m_joystickAxes[id][6] > 35.0f) && !(m_previousJoyAxes[id][6] > 35.0f);
    }
    else if (mappedBtn == 102) // D-Pad Up
    {
        isMappedBtnPress = (m_joystickAxes[id][7] > 35.0f) && !(m_previousJoyAxes[id][7] > 35.0f);
    }
    else if (mappedBtn == 103) // D-Pad Down
    {
        isMappedBtnPress = (m_joystickAxes[id][7] < -35.0f) && !(m_previousJoyAxes[id][7] < -35.0f);
    }
    else if (mappedBtn == 104) // Stick Left
    {
        isMappedBtnPress = (m_joystickAxes[id][0] < -35.0f) && !(m_previousJoyAxes[id][0] < -35.0f);
    }
    else if (mappedBtn == 105) // Stick Right
    {
        isMappedBtnPress = (m_joystickAxes[id][0] > 35.0f) && !(m_previousJoyAxes[id][0] > 35.0f);
    }
    else if (mappedBtn == 106) // Stick Up
    {
        isMappedBtnPress = (m_joystickAxes[id][1] < -35.0f) && !(m_previousJoyAxes[id][1] < -35.0f);
    }
    else if (mappedBtn == 107) // Stick Down
    {
        isMappedBtnPress = (m_joystickAxes[id][1] > 35.0f) && !(m_previousJoyAxes[id][1] > 35.0f);
    }
    else if (mappedBtn == 108) // LT
    {
        isMappedBtnPress = (m_joystickAxes[id][2] < -35.0f) && !(m_previousJoyAxes[id][2] < -35.0f);
    }
    else if (mappedBtn == 109) // RT
    {
        isMappedBtnPress = (m_joystickAxes[id][2] > 35.0f) && !(m_previousJoyAxes[id][2] > 35.0f);
    }

    switch (action)
    {
        case InputAction::MoveLeft:
            return GetGamepadMoveX(id) < -0.4f || isMappedBtnPress;
        case InputAction::MoveRight:
            return GetGamepadMoveX(id) > 0.4f || isMappedBtnPress;
        case InputAction::LaunchBall:
        case InputAction::FireLaser:
            return isMappedBtnPress || IsJoystickButtonPress(id, 0);
        case InputAction::Pause:
            return isMappedBtnPress || IsJoystickButtonPress(id, 7);
        default:
            return isMappedBtnPress;
    }
}

int InputManager::GetAnyGamepadInputTriggered(unsigned int id) const
{
    if (id >= MAX_JOYSTICKS || !m_joystickConnected[id])
    {
        return -1;
    }

    // 1. Buttons 0 to 15
    for (unsigned int btn = 0; btn < 16; ++btn)
    {
        if (IsJoystickButtonPress(id, btn))
        {
            return static_cast<int>(btn);
        }
    }

    constexpr float threshold = 40.0f;

    // 2. D-Pad directions (Axis 6: POV X, Axis 7: POV Y)
    if (m_joystickAxes[id][6] < -threshold && !(m_previousJoyAxes[id][6] < -threshold))
        return 100; // D-Pad Left
    if (m_joystickAxes[id][6] > threshold && !(m_previousJoyAxes[id][6] > threshold))
        return 101; // D-Pad Right
    if (m_joystickAxes[id][7] > threshold && !(m_previousJoyAxes[id][7] > threshold))
        return 102; // D-Pad Up
    if (m_joystickAxes[id][7] < -threshold && !(m_previousJoyAxes[id][7] < -threshold))
        return 103; // D-Pad Down

    // 3. Left Stick directions (Axis 0: X, Axis 1: Y)
    if (m_joystickAxes[id][0] < -threshold && !(m_previousJoyAxes[id][0] < -threshold))
        return 104; // Stick Left
    if (m_joystickAxes[id][0] > threshold && !(m_previousJoyAxes[id][0] > threshold))
        return 105; // Stick Right
    if (m_joystickAxes[id][1] < -threshold && !(m_previousJoyAxes[id][1] < -threshold))
        return 106; // Stick Up
    if (m_joystickAxes[id][1] > threshold && !(m_previousJoyAxes[id][1] > threshold))
        return 107; // Stick Down

    // 4. Triggers (Axis 2)
    if (m_joystickAxes[id][2] < -threshold && !(m_previousJoyAxes[id][2] < -threshold))
        return 108; // LT
    if (m_joystickAxes[id][2] > threshold && !(m_previousJoyAxes[id][2] > threshold))
        return 109; // RT

    return -1;
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