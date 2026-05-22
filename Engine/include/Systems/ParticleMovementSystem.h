#pragma once
#include "ECS/System.h"
#include "Components/Transform2D.h"
#include "Components/Velocity2D.h"
#include "ECS/Registry.hpp"

class ParticleMovementSystem : public System
{
public:
    using System::System;

    void OnUpdate(float dt) override
    {
        m_registry.View<Transform2D, Velocity2D>([dt](Entity e, Transform2D& transform, Velocity2D& velocity) 
        {
            transform.X += velocity.X * dt;
            transform.Y += velocity.Y * dt;

            if (transform.X < -1000.0f || transform.X > 1000.0f) velocity.X *= -1.0f;
            if (transform.Y < -1000.0f || transform.Y > 1000.0f) velocity.Y *= -1.0f;
        });
    }
};
