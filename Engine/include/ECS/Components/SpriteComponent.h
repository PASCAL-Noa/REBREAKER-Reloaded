#pragma once
#include <cstdint>
#include "Data/Color.h"

struct SpriteComponent
{
    uint32_t TextureId = 0;
    Color Tint = Colors::White;
};