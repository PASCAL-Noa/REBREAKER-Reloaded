#pragma once
#include "ECS/System.h"
#include "Components/Transform2D.h"
#include "Components/Velocity2D.h"
#include <vector>

class ParticleMovementSystem : public System
{
public:
    using System::System;

    void OnUpdate(float dt) override
    {
        std::vector<Entity> toDestroy;

        m_registry.View<Transform2D, Velocity2D>([&](Entity e, Transform2D& transform, Velocity2D& velocity)
        {
            transform.X += velocity.X * dt;
            transform.Y += velocity.Y * dt;

            if (transform.X < -1000.0f || transform.X > 1000.0f ||
                transform.Y < -1000.0f || transform.Y > 1000.0f)
            {
                toDestroy.push_back(e);
            }
        });

        for (Entity e : toDestroy)
        {
            m_registry.DestroyEntity(e);
        }
    }
};