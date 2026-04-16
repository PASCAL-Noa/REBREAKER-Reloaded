#pragma once
#include <cstdint>
#include <array>

enum class KeyCode : uint8_t
{
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Left, Right, Up, Down,
    Escape, LControl, LShift, LAlt, LSystem, RControl, RShift, RAlt, RSystem, Menu,
    LBracket, RBracket, Semicolon, Comma, Period, Apostrophe, Slash, Backslash, Grave, Equal, Hyphen,
    Space, Enter, Backspace, Tab, PageUp, PageDown, End, Home, Insert, Delete, Pause,
    Add, Subtract, Multiply, Divide,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15,
    Count
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

    void SetMouseButtonState(MouseButton button, bool isDown);
    [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonPress(MouseButton button) const;
    [[nodiscard]] bool IsMouseButtonRelease(MouseButton button) const;

private:
    static constexpr size_t KEY_COUNT = static_cast<size_t>(KeyCode::Count);
    static constexpr size_t MOUSE_COUNT = static_cast<size_t>(MouseButton::Count);
    
    std::array<bool, KEY_COUNT> m_currentKeys{false};
    std::array<bool, KEY_COUNT> m_previousKeys{false};

    std::array<bool, MOUSE_COUNT>   m_currentMouse{false};
    std::array<bool, MOUSE_COUNT>   m_previousMouse{false};
};