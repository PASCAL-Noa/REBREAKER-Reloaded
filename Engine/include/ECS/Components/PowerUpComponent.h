#pragma once
#include <cstdint>

enum class PowerUpType : uint8_t
{
    MultiBall,
    ExpandPaddle,
    ShrinkPaddle,
    LaserPaddle,
    SlowBall,
    ExtraLife
};

struct PowerUpComponent
{
    PowerUpType Type = PowerUpType::MultiBall;
    float       FallSpeed = 200.0f;
    bool        IsCollected = false;
};
