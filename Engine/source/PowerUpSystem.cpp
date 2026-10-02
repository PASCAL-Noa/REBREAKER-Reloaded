#include "ECS/Systems/PowerUpSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/PowerUpComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "Events/GameplayEvents.h"
#include "Math/Physics.h"

PowerUpSystem::PowerUpSystem(Registry& registry, EventDispatcher& events)
    : System(registry), m_events(events)
{
}

void PowerUpSystem::OnUpdate(float dt)
{
    // 1. Check collision between falling bonus capsules and the paddle
    m_registry.View<Transform2D, BoxCollider, PaddleComponent>(
        [this](Entity paddleEntity, Transform2D& paddleTrans, BoxCollider& paddleBox, PaddleComponent&)
    {
        (void)paddleEntity;
        m_registry.View<Transform2D, BoxCollider, PowerUpComponent>(
            [&](Entity powerUpEntity, Transform2D& powerUpTrans, BoxCollider& powerUpBox, PowerUpComponent& powerUp)
        {
            if (powerUp.IsCollected) return;

            CollisionManifold manifold = Physics::IntersectAABB(powerUpBox, powerUpTrans, paddleBox, paddleTrans);
            if (manifold.IsColliding)
            {
                powerUp.IsCollected = true;
                m_events.Publish(PowerUpEvent(powerUpEntity, powerUp.Type));
                m_registry.DestroyEntityDeferred(powerUpEntity);
            }
        });
    });

    // 2. Move uncollected capsules downwards and despawn if past the play area
    m_registry.View<Transform2D, PowerUpComponent>(
        [this, dt](Entity entity, Transform2D& transform, PowerUpComponent& powerUp)
    {
        if (powerUp.IsCollected) return;

        transform.Position.Y += powerUp.FallSpeed * dt;

        // Despawn as soon as it passes below the play area (paddle is at Y = 300, bottom boundary at 450)
        if (transform.Position.Y > 480.0f)
        {
            m_registry.DestroyEntityDeferred(entity);
        }
    });
}
