#pragma once
#include <string>
#include <string_view>
#include <cstdint>

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

enum class InputAction : uint8_t
{
    MoveLeft,
    MoveRight,
    LaunchBall,
    TriggerStorm,
    Pause,
    Count,
    FireLaser = TriggerStorm
};

namespace KeyCodeUtils
{
    std::string_view ToString(KeyCode key);
    KeyCode FromString(std::string_view name);
    std::string_view ActionToString(InputAction action);
}
