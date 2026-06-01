#pragma once
#include "Math/Vector2.h"

struct Transform2D
{
    Vector2f    Position;
    float       Rotation = 0.0f;
    Vector2f    Scale{1.0f, 1.0f};
};