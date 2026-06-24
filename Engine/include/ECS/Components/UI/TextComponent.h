#pragma once
#include "Data/Color.h"
#include "Math/Vector2.h"
#include <string>
#include <cstdint>

struct TextComponent
{
    std::string     Text = "";
    uint32_t    FontId = 0;
    float   FontSize = 24.0f;
    Color   Tint = Colors::White;
    bool    TextCenter = true;
    Vector2f    Offset = {0.0f, 0.0f};
};
