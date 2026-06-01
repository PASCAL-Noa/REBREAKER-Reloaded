#pragma once
#include "Math/Vector2.h"

struct Camera2D
{
    Vector2f    Position;
    float       Zoom = 1.0f;
    float       Rotation = 0.0f;
};