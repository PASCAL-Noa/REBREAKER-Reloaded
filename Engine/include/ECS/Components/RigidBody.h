#pragma once
#include "Math/Vector2.h"

struct RigidBody
{
    Vector2f    Velocity;
    float       Mass = 1.0f;
    float       Restitution = 1.0f;
    bool        IsKinematic = false;
};