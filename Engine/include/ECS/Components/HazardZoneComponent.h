#pragma once
#include "Math/Vector2.h"
#include <cstdint>

enum class HazardEffectType : uint8_t
{
    AllMaluses = 0,
    ScreenGlitch = 1,
    CameraTilt = 2,
    CameraZoomPulse = 3,
    TemporalAudioWarp = 4
};

struct HazardZoneComponent
{
    Vector2f         Size{180.0f, 60.0f};
    bool             AffectsBall = true;
    bool             AffectsPaddle = true;
    HazardEffectType Effect = HazardEffectType::AllMaluses;
    float            Duration = 3.5f;
    float            Intensity = 1.0f;
    float            Cooldown = 0.8f;
    float            CooldownTimer = 0.0f;
    bool             IsActive = true;
};
