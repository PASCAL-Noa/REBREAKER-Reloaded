#pragma once
#include "Data/Color.h"
#include <string>

enum class LightingMode : int
{
    Lit = 0,
    Atmospheric = 1,
    PitchBlack = 2
};

inline Color GetAmbientLightForLightingMode(LightingMode mode) noexcept
{
    switch (mode)
    {
    case LightingMode::Lit:         return Color{255, 255, 255, 255};
    case LightingMode::Atmospheric: return Color{45, 50, 75, 255};
    case LightingMode::PitchBlack:  return Color{0, 0, 0, 255};
    default:                        return Color{255, 255, 255, 255};
    }
}

inline const char* GetLightingModeName(LightingMode mode) noexcept
{
    switch (mode)
    {
    case LightingMode::Lit:         return "LIT";
    case LightingMode::Atmospheric: return "ATMOSPHERIC";
    case LightingMode::PitchBlack:  return "PITCH BLACK";
    default:                        return "LIT";
    }
}

struct LevelTheme
{
    std::string Name;
    Color BgBase1{8, 10, 18, 255};
    Color BgBase2{14, 16, 28, 255};
    Color GridColor{45, 75, 140, 24};
    Color DustColor{180, 215, 255, 45};
    float GridSpeed = 14.0f;
    float DustSpeedY = -15.0f;

    // Colors when combo multiplier reaches 1, 2, 3, 4, 5+
    Color ComboColors[5] = {
        Color{8, 12, 22, 255},
        Color{10, 24, 38, 255},
        Color{14, 36, 56, 255},
        Color{24, 28, 68, 255},
        Color{38, 20, 78, 255}
    };

    Color VignetteBaseColor{0, 0, 0, 255};
    float VignetteBaseIntensity = 0.25f;
    Color AmbientLight{255, 255, 255, 255};

    [[nodiscard]] bool HasLighting() const noexcept
    {
        return AmbientLight.r < 255 || AmbientLight.g < 255 || AmbientLight.b < 255;
    }
};

class LevelThemeCatalog
{
public:
    static const LevelTheme& GetThemeForLevel(int levelNumber);
};
