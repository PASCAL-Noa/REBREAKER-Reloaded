#pragma once
#include <cstdint>
#include "Data/Color.h"

struct SpriteComponent
{
    uint32_t TextureId = 0;
    Color Tint = Colors::White;
    uint32_t ShaderId = 0;
    uint32_t OverlayTextureId = 0;
    float ShaderValue = 0.0f;
};