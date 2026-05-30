#pragma once
#include "ECS/Component.h"

struct Velocity2D : public Component
{
    float X = 0.0f;
    float Y = 0.0f;

    Velocity2D() = default;
    
    Velocity2D(Entity owner, float x, float y) 
        : Component(owner), X(x), Y(y) {}
};