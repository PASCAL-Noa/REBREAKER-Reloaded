#pragma once
#include <cstdint>

enum class BrickType : uint32_t
{
    Light,
    Medium,
    Hard,
    Special,
    Metal,
    Bomb
};

struct BrickComponent
{
    BrickType   Type = BrickType::Light;
    uint32_t    ScoreValue = 0;
    uint32_t    HitPoints = 1;
    uint32_t    MaxHitPoints = 1;
    bool        IsSpecial = false;
    bool        IsUnbreakable = false;
    bool        IsBomb = false;
};