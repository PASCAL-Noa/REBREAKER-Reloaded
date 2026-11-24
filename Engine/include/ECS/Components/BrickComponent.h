#pragma once
#include "ECS/Components/PowerUpComponent.h"
#include <cstdint>

enum class BrickType : uint32_t
{
    Light,
    Medium,
    Hard,
    Special,
    Metal,
    Bomb,
    Lantern
};

struct BrickComponent
{
    BrickType   Type = BrickType::Light;
    uint32_t    ScoreValue = 0;
    uint32_t    HitPoints = 1;
    uint32_t    MaxHitPoints = 1;
    uint8_t     ArmorTier = 0;
    bool        IsSpecial = false;
    bool        IsUnbreakable = false;
    bool        IsBomb = false;
    bool        IsLantern = false;
    bool        IsRegenerating = false;
    float       RegenTimer = 0.0f;
    float       RegenCooldown = 30.0f;

    // Optional forced power-up drop for Special bricks (customized in Editor)
    bool        HasForcedPowerUp = false;
    PowerUpType ForcedPowerUp = PowerUpType::MultiBall;

    // Smooth top-down spawn/wave descent animation state
    bool        IsDescending = false;
    float       DescendTargetY = 0.0f;
    float       DescendSpeed = 380.0f;
};