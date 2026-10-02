#pragma once
#include <cstdint>

enum class PowerUpType : uint8_t
{
    MultiBall,
    ExpandPaddle,
    ShrinkPaddle,
    LaserPaddle,
    TempoBall,
    ExtraLife,
    BigBall,
    FireBall
};

struct PowerUpComponent
{
    PowerUpType Type = PowerUpType::MultiBall;
    float       FallSpeed = 200.0f;
    bool        IsCollected = false;
};
