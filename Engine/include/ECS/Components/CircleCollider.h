#pragma once
#include "Math/Vector2.h"

struct CircleCollider
{
    float       Radius = 0.0f;
    Vector2f    Offset;
    bool        IsColliding = false;
    bool        IsTrigger = false;

    float GetEffectiveRadius(float scale = 1.0f) const
    {
        return Radius * scale;
    }

    float GetEffectiveRadius(const Vector2f& scale) const
    {
        return Radius * scale.X;
    }
};