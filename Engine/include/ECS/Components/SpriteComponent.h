#pragma once
#include <cstdint>
#include "Data/Color.h"
#include "Math/Rect.h"
#include "Math/Vector2.h"

#include <optional>

struct ShaderParams
{
    uint32_t    ShaderId = 0;
    uint32_t    OverlayTextureId = 0;
    float    ShaderValue = 0.0f;
};

struct SpriteComponent
{
    uint32_t    TextureId = 0;
    Color    Tint = Colors::White;
    ShaderParams    Shader;
    std::optional<IntRect>  TextureRect = std::nullopt;
    std::optional<Vector2f>     Origin = std::nullopt;
};