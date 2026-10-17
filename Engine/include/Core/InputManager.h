#pragma once
#include <cstdint>
#include <array>
#include <string>

#include "Core/InputActions.h"

enum class GamepadDirection : uint8_t
{
    Up,
    Down,
    Left,
    Right
};

enum class MouseButton : uint8_t
{
    Left, Right, Middle, Extra1, Extra2,
    Count
};

class InputManager
{
public:
    InputManager() = default;
    ~InputManager() = default;

    void Update();

    void SetKeyState(KeyCode key, bool isDown);
    [[nodiscard]] bool IsKeyDown(KeyCode key) const;
    [[nodiscard]] bool IsKeyPress(KeyCode key) const;
    [[nodiscard]] bool IsKeyRelease(KeyCode key) const;
    [[nodiscard]] KeyCode GetLastKeyPressed() const { return m_lastKeyPressed; }

    void SetMouseButtonState(MouseButton button, bool isDown);
    [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonPress(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonRelease(MouseButton button) const;

    void SetMousePosition(float x, float y);
    [[nodiscard]] const std::pair<float, float>& GetMousePosition() const;

    void SetMouseWheelDelta(float delta);
    [[nodiscard]] float GetMouseWheelDelta() const;

    void AppendEnteredText(uint32_t unicode);
    [[nodiscard]] const std::string& GetEnteredText() const;

    // Joystick / Gamepad support
    void SetJoystickConnected(unsigned int id, bool connected);
    [[nodiscard]] bool IsJoystickConnected(unsigned int id = 0) const;
    void SetJoystickAxis(unsigned int id, unsigned int axis, float position);
    [[nodiscard]] float GetJoystickAxis(unsigned int id, unsigned int axis) const;
    void SetJoystickButton(unsigned int id, unsigned int button, bool isDown);
    [[nodiscard]] bool IsJoystickButtonDown(unsigned int id, unsigned int button) const;
    [[nodiscard]] bool IsJoystickButtonPress(unsigned int id, unsigned int button) const;
    [[nodiscard]] bool IsJoystickButtonRelease(unsigned int id, unsigned int button) const;

    // High level helpers for gameplay gamepad input
    [[nodiscard]] float GetGamepadMoveX(unsigned int id = 0) const; // [-1.0 .. 1.0] from Left Stick or D-Pad
    [[nodiscard]] float GetGamepadMoveY(unsigned int id = 0) const; // [-1.0 .. 1.0] from Left Stick or D-Pad
    [[nodiscard]] bool IsGamepadDirectionActive(GamepadDirection dir, unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadDirectionTriggered(GamepadDirection dir, unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadActionTriggered(InputAction action, unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadActionActive(InputAction action, unsigned int id = 0) const;
    [[nodiscard]] int GetAnyGamepadInputTriggered(unsigned int id = 0) const;

    // Unified menu navigation triggers (Keyboard + Gamepad)
    [[nodiscard]] bool IsMenuUpTriggered() const;
    [[nodiscard]] bool IsMenuDownTriggered() const;
    [[nodiscard]] bool IsMenuLeftTriggered() const;
    [[nodiscard]] bool IsMenuRightTriggered() const;
    [[nodiscard]] bool IsMenuSelectTriggered() const;
    [[nodiscard]] bool IsMenuCancelTriggered() const;

    // Independent secondary controls for overlays/testers (Right stick & Bumpers)
    [[nodiscard]] float GetGamepadRightStickY(unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadRightStickUpTriggered(unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadRightStickDownTriggered(unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadLeftBumperTriggered(unsigned int id = 0) const;
    [[nodiscard]] bool IsGamepadRightBumperTriggered(unsigned int id = 0) const;

    // Unified action polling combining keyboard bindings and gamepad
    [[nodiscard]] bool IsActionActive(InputAction action) const;
    [[nodiscard]] bool IsActionTriggered(InputAction action) const;

private:
    static constexpr size_t KEY_COUNT = static_cast<size_t>(KeyCode::Count);
    static constexpr size_t MOUSE_COUNT = static_cast<size_t>(MouseButton::Count);
    static constexpr size_t MAX_JOYSTICKS = 8;
    static constexpr size_t MAX_JOYSTICK_BUTTONS = 32;
    static constexpr size_t MAX_JOYSTICK_AXES = 8;

    std::array<bool, KEY_COUNT> m_currentKeys{false};
    std::array<bool, KEY_COUNT> m_previousKeys{false};
    KeyCode m_lastKeyPressed = KeyCode::Count;

    std::array<bool, MOUSE_COUNT>   m_currentMouse{false};
    std::array<bool, MOUSE_COUNT>   m_previousMouse{false};
    std::pair<float, float> m_mousePosition{0.0f, 0.0f};
    float m_mouseWheelDelta = 0.0f;
    std::string m_enteredText;

    std::array<bool, MAX_JOYSTICKS> m_joystickConnected{false};
    std::array<std::array<bool, MAX_JOYSTICK_BUTTONS>, MAX_JOYSTICKS> m_currentJoyButtons{};
    std::array<std::array<bool, MAX_JOYSTICK_BUTTONS>, MAX_JOYSTICKS> m_previousJoyButtons{};
    std::array<std::array<float, MAX_JOYSTICK_AXES>, MAX_JOYSTICKS> m_joystickAxes{};
    std::array<std::array<float, MAX_JOYSTICK_AXES>, MAX_JOYSTICKS> m_previousJoyAxes{};
};