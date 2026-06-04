#pragma once
#include <cstdint>

enum class BrickType : uint32_t
{
    Light,
    Medium,
    Hard,
    Special
};

struct BrickComponent
{
    BrickType   Type = BrickType::Light;
    uint32_t    ScoreValue = 0;
    uint32_t    HitPoints = 1;
    bool        IsSpecial = false;
};