#pragma once
#include "ECS/Component.h"

struct Camera2D : public Component
{
    float X = 0.0f;
    float Y = 0.0f;
    float Zoom = 1.0f;
    float Rotation = 0.0f;

    Camera2D() = default;

    Camera2D(Entity owner, float x, float y, float zoom = 1.0f, float rotation = 0.0f)
        : Component(owner), X(x), Y(y), Zoom(zoom), Rotation(rotation) {}
};