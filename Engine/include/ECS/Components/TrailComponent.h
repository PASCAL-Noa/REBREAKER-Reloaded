#pragma once
#include "Math/Vector2.h"
#include "Data/Color.h"
#include "Data/BlendMode.h"
#include <vector>
#include <deque>
#include <cmath>
#include <cstdint>
#include <algorithm>

enum class TrailShape : uint8_t
{
    Ribbon,    // Continuous smooth mesh along trajectory
    Particles  // Discrete fading particle streak
};

enum class TrailCosmeticId : uint8_t
{
    DefaultNeon,    // Pure white neon matching the ball
    ElectricCyan,   // Classic electric cyan/blue neon
    FireStreak,     // Burning flame trail (used for FireBall & cosmetic)
    ElectricViolet, // High-voltage magenta/purple plasma
    EmeraldLaser,   // Cyber green streak
    GoldenSun,      // Radiant gold/amber blaze
    Rainbow,        // Shifting spectrum rainbow
    Custom
};

struct TrailPoint
{
    Vector2f Position{0.0f, 0.0f};
    float    Age = 0.0f;       // Seconds since emission
    float    Width = 18.0f;    // Base width at emission
    Color    ColorTint = Colors::White;
};

struct TrailConfig
{
    TrailCosmeticId CosmeticId = TrailCosmeticId::DefaultNeon;
    TrailShape      Shape = TrailShape::Ribbon;

    float     Lifetime = 0.22f;          // Point duration in seconds
    float     StartWidth = 18.0f;        // Head ribbon width
    float     EndWidth = 2.0f;           // Tail tip width
    float     MinPointDistance = 4.0f;   // Min distance between samples
    float     MaxPointDistance = 45.0f;  // Subdivide distance for smooth high-speed sampling

    Color     StartColor{255, 255, 255, 230}; // Pure white matching the ball at head
    Color     EndColor{255, 255, 255, 0};      // Faded white at tail

    BlendMode Blend = BlendMode::Add;  // Additive for vibrant neon glow
    bool      FadeWidth = true;             // Taper ribbon width
    bool      FadeAlpha = true;             // Fade alpha to 0 at tail
};

namespace TrailPresets
{
    inline Color HsvToRgb(float h, float s, float v)
    {
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        if (h < 60.0f)       { r = c; g = x; }\
        else if (h < 120.0f) { r = x; g = c; }\
        else if (h < 180.0f) { g = c; b = x; }\
        else if (h < 240.0f) { g = x; b = c; }\
        else if (h < 300.0f) { r = x; b = c; }\
        else                 { r = c; b = x; }\

        return Color{
            static_cast<uint8_t>((r + m) * 255.0f),
            static_cast<uint8_t>((g + m) * 255.0f),
            static_cast<uint8_t>((b + m) * 255.0f),
            255
        };
    }

    inline TrailConfig GetPreset(TrailCosmeticId id)
    {
        TrailConfig cfg;
        cfg.CosmeticId = id;
        switch (id)
        {
            case TrailCosmeticId::DefaultNeon:
                cfg.StartColor = Color{255, 255, 255, 230};
                cfg.EndColor = Color{255, 255, 255, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.22f;
                cfg.StartWidth = 18.0f;
                cfg.EndWidth = 2.0f;
                break;
            case TrailCosmeticId::ElectricCyan:
                cfg.StartColor = Color{0, 220, 255, 210};
                cfg.EndColor = Color{0, 80, 255, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.22f;
                cfg.StartWidth = 18.0f;
                cfg.EndWidth = 2.0f;
                break;
            case TrailCosmeticId::FireStreak:
                cfg.StartColor = Color{255, 140, 20, 240};
                cfg.EndColor = Color{255, 30, 0, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.28f;
                cfg.StartWidth = 22.0f;
                cfg.EndWidth = 3.0f;
                break;
            case TrailCosmeticId::ElectricViolet:
                cfg.StartColor = Color{210, 80, 255, 220};
                cfg.EndColor = Color{100, 20, 220, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.24f;
                cfg.StartWidth = 18.0f;
                cfg.EndWidth = 2.0f;
                break;
            case TrailCosmeticId::EmeraldLaser:
                cfg.StartColor = Color{50, 255, 120, 220};
                cfg.EndColor = Color{0, 180, 60, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.22f;
                cfg.StartWidth = 18.0f;
                cfg.EndWidth = 2.0f;
                break;
            case TrailCosmeticId::GoldenSun:
                cfg.StartColor = Color{255, 215, 60, 220};
                cfg.EndColor = Color{255, 110, 0, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.25f;
                cfg.StartWidth = 18.0f;
                cfg.EndWidth = 2.0f;
                break;
            case TrailCosmeticId::Rainbow:
                cfg.StartColor = Color{255, 50, 150, 220};
                cfg.EndColor = Color{50, 200, 255, 0};
                cfg.Blend = BlendMode::Add;
                cfg.Lifetime = 0.30f;
                cfg.StartWidth = 20.0f;
                cfg.EndWidth = 3.0f;
                break;
            default:
                break;
        }
        return cfg;
    }
}

struct TrailComponent
{
    TrailConfig Config{};
    std::deque<TrailPoint> Points{};
    Vector2f LastRecordedPos{0.0f, 0.0f};
    bool HasRecordedFirst = false;

    void Clear()
    {
        Points.clear();
        HasRecordedFirst = false;
        LastRecordedPos = Vector2f{0.0f, 0.0f};
    }
};
