#pragma once
#include "Math/Vector2.h"

struct CircleCollider
{
    float       Radius = 0.0f;
    Vector2f    Offset;
    bool        IsColliding = false;
    bool        IsTrigger = false;
};