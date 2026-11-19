#pragma once
#include "Math/Vector2.h"
#include "Data/Color.h"

struct BossProjectileComponent
{
    float    Damage = 18.0f; // Durability damage dealt to paddle
    float    Radius = 12.0f;
    Vector2f Velocity{0.0f, 220.0f};
    bool     IsDeflected = false;
    float    Lifetime = 6.0f;
    Color    ColorTint{255, 60, 60, 255};
};
