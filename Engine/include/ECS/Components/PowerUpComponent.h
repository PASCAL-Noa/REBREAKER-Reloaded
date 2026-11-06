#pragma once
#include <cstdint>

enum class PowerUpType : uint8_t
{
    MultiBall,
    ExpandPaddle,
    ShrinkPaddle,
    Storm,
    TempoBall,
    ExtraLife,
    BigBall,
    FireBall,
    StickyPaddle,
    BottomBarrier,
    MagnetPaddle,
    GhostBall,
    ItemRegen,
    LaserPaddle = Storm,
    GlitchMalus = 14
};

struct PowerUpComponent
{
    PowerUpType Type = PowerUpType::MultiBall;
    float       FallSpeed = 200.0f;
    bool        IsCollected = false;
    bool        IsDebuff = false;
};
