#pragma once
#include "Data/Color.h"
#include <cstdint>

struct PanelComponent
{
    Color   Tint = Colors::White;
    uint32_t    TextureId = 0;
};
