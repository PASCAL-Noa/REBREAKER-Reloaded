#pragma once
#include "ECS/Components/PowerUpComponent.h"
#include "Events/ArenaEvents.h"
#include <cstdint>

enum class BrickType : uint32_t
{
    Light,
    Medium,
    Hard,
    Special,
    Metal,
    Bomb,
    Lantern,
    Event
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
    bool        IsEventBrick = false;
    bool        IsRegenerating = false;
    bool        RegenEnabled = true;
    float       RegenTimer = 0.0f;
    float       RegenCooldown = 30.0f;

    // Optional forced power-up drop for Special bricks (customized in Editor)
    bool        HasForcedPowerUp = false;
    PowerUpType ForcedPowerUp = PowerUpType::MultiBall;

    // Event brick configuration (extensible via ArenaEventType catalog)
    bool           RandomEvent = true;
    ArenaEventType TriggeredEvent = ArenaEventType::MeteorShower;
    bool           EventTriggerOnHit = false;
    bool           EventAlreadyTriggered = false;

    // Element-specific gameplay parameters
    int         BombBlastTier = 1;      // 0 = Small (1x1), 1 = Standard (3x3), 2 = Large (5x5)
    float       MetalBounceBoost = 1.0f;// Speed multiplier on Metal bounce (1.0 .. 1.4)

    // Smooth top-down spawn/wave descent animation state
    bool        IsDescending = false;
    float       DescendTargetY = 0.0f;
    float       DescendSpeed = 380.0f;
};