#pragma once
#include <string>
#include <functional>
#include "Data/Color.h"

struct TextInputComponent
{
    std::string     Text = "";
    std::string     Placeholder = "";
    bool    IsFocused = false;
    Color   DefaultColor = Color{50, 50, 50, 255};
    Color   FocusedColor = Color{80, 80, 80, 255};
    Color   TextColor = Colors::White;
    std::function<void(const std::string&)>     OnSubmit = nullptr;
    size_t  MaxLength = 20;
    uint32_t    FontId = 0;
    float   FontSize = 30.0f;
};
