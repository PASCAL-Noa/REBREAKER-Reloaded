#pragma once
#include "Math/Vector2.h"

struct BoxCollider
{
    Vector2f    Size;
    Vector2f    Offset;
    bool        IsColliding = false;
    bool        IsTrigger = false;

    Vector2f GetEffectiveSize(const Vector2f& scale = {1.0f, 1.0f}) const
    {
        return { Size.X * scale.X, Size.Y * scale.Y };
    }
};