#pragma once
#include "ECS/Component.h"

struct Transform2D : public Component
{
    float X = 0.0f;
    float Y = 0.0f;
    float Rotation = 0.0f;
    float ScaleX = 1.0f;
    float ScaleY = 1.0f;

    Transform2D() = default;

    Transform2D(Entity owner, float x, float y, float rotation = 0.0f, float scaleX = 1.0f, float scaleY = 1.0f)
        : Component(owner), X(x), Y(y), Rotation(rotation), ScaleX(scaleX), ScaleY(scaleY) {}
};