#pragma once
#include "Data/Color.h"
#include <functional>
#include <cstdint>

enum class ButtonState
{
    Normal,
    Hovered,
    Pressed
};

struct ButtonComponent
{
    ButtonState State = ButtonState::Normal;

    Color   DefaultColor = Colors::White;
    Color   HoverColor = Colors::LightGray;
    Color   PressedColor = Colors::DarkGray;

    uint32_t    DefaultTextureId = 0;
    uint32_t    HoverTextureId = 0;
    uint32_t    PressedTextureId = 0;

    std::function<void()>   OnClick = nullptr;
    std::function<void()>   OnHoverEnter = nullptr;
    std::function<void()>   OnHoverExit = nullptr;
};
