#include "ECS/Systems/PowerUpSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/PowerUpComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "Events/GameplayEvents.h"
#include "Math/Physics.h"
#include "PowerUps/PowerUpConfig.h"

PowerUpSystem::PowerUpSystem(Registry& registry, EventDispatcher& events)
    : System(registry), m_events(events)
{
}

void PowerUpSystem::OnUpdate(float dt)
{
    // 0. Magnet Paddle: Passively pull falling capsules towards the paddle center
    m_registry.View<Transform2D, PaddleComponent>(
        [this, dt](Entity, const Transform2D& paddleTrans, const PaddleComponent& paddleComp)
    {
        if (!paddleComp.HasMagnet) return;

        const auto& magnetCfg = PowerUpManager::Get().Magnet();
        const float magnetRadius = magnetCfg.MagnetRadius;
        const float pullSpeed = magnetCfg.GetEffectivePullSpeed();

        m_registry.View<Transform2D, PowerUpComponent>(
            [&](Entity, Transform2D& powerUpTrans, PowerUpComponent& powerUp)
        {
            if (powerUp.IsCollected) return;

            Vector2f toPaddle = paddleTrans.Position - powerUpTrans.Position;
            float dist = toPaddle.Length();
            if (dist > 1.0f && dist <= magnetRadius)
            {
                Vector2f dir = toPaddle / dist;
                powerUpTrans.Position.X += dir.X * pullSpeed * dt;
                powerUpTrans.Position.Y += std::max(0.0f, dir.Y) * (pullSpeed * 0.7f) * dt;
            }
        });
    });

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
