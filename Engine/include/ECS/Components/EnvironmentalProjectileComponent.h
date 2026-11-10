#pragma once
#include "Math/Vector2.h"
#include "Data/Color.h"

struct EnvironmentalProjectileComponent
{
    float Radius = 16.0f;
    int   Damage = 1;
    bool  IsDeflected = false;
    float Lifetime = 7.0f;
    Color TrailColor{255, 130, 30, 255};
};
