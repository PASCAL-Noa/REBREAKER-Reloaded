#include "ECS/Systems/PhysicsSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/PaddleComponent.h"
#include "Events/EventDispatcher.h"
#include "Events/CollisionEvent.h"
#include <algorithm>

PhysicsSystem::PhysicsSystem(Registry& registry, EventDispatcher& events)
    : System(registry), m_events(events) {}

void PhysicsSystem::OnUpdate(float dt)
{
    ResetColliders();
    ApplyVelocity(dt);
    CheckAABBCollisions();
    CheckCircleAABBCollisions();
    CheckCircleCollisions();
    m_registry.ProcessDeferredCommands();
}

void PhysicsSystem::ResetColliders() const
{
    m_registry.View<BoxCollider>([](Entity, BoxCollider& box) { box.IsColliding = false; });
    m_registry.View<CircleCollider>([](Entity, CircleCollider& circle) { circle.IsColliding = false; });
}

void PhysicsSystem::ApplyVelocity(float dt) const
{
    m_registry.View<Transform2D, RigidBody>([dt](Entity, Transform2D& transform, RigidBody& rb)
    {
        if (!rb.IsKinematic) transform.Position += rb.Velocity * dt;
    });
}

void PhysicsSystem::CheckAABBCollisions() const
{
    struct MovingBox {
        Entity entity;
        Transform2D* transform;
        BoxCollider* collider;
    };

    std::vector<MovingBox> movingBoxes;
    movingBoxes.reserve(16);

    m_registry.View<Transform2D, BoxCollider>([this, &movingBoxes](Entity e, Transform2D& t, BoxCollider& b)
    {
        bool isDynamic = m_registry.HasComponent<RigidBody>(e) && !m_registry.GetComponent<RigidBody>(e).IsKinematic;
        bool isPaddle = m_registry.HasComponent<PaddleComponent>(e);
        if (isDynamic || isPaddle)
        {
            movingBoxes.push_back({e, &t, &b});
        }
    });

    if (movingBoxes.empty()) return;

    m_registry.View<Transform2D, BoxCollider>([this, &movingBoxes](Entity e2, Transform2D& t2, BoxCollider& b2)
    {
        bool isMover2 = (m_registry.HasComponent<RigidBody>(e2) && !m_registry.GetComponent<RigidBody>(e2).IsKinematic)
                     || m_registry.HasComponent<PaddleComponent>(e2);

        for (const auto& mover : movingBoxes)
        {
            Entity e1 = mover.entity;
            if (e1 == e2) continue;

            // When both are movers, test once (e1 < e2) to avoid duplicate collision events
            if (isMover2 && e1 >= e2) continue;

            CollisionManifold manifold = Physics::IntersectAABB(*mover.collider, *mover.transform, b2, t2);
            if (manifold.IsColliding)
            {
                mover.collider->IsColliding = true;
                b2.IsColliding = true;
                m_events.Publish(CollisionEvent{e1, e2});

                if (!mover.collider->IsTrigger && !b2.IsTrigger)
                {
                    ResolveCollision(e1, *mover.transform, e2, t2, manifold);
                }
            }
        }
    });
}

void PhysicsSystem::CheckCircleAABBCollisions() const
{
    m_registry.View<Transform2D, CircleCollider>([this](Entity e1, Transform2D& t1, CircleCollider& c1)
    {
        m_registry.View<Transform2D, BoxCollider>([&](Entity e2, Transform2D& t2, BoxCollider& b2)
        {
            // Triggers (bonus capsules, lasers) do not physically collide with balls
            if (b2.IsTrigger) return;

            CollisionManifold manifold = Physics::IntersectCircleAABB(c1, t1, b2, t2);
            if (manifold.IsColliding)
            {
                c1.IsColliding = true;
                b2.IsColliding = true;
                m_events.Publish(CollisionEvent{e1, e2});

                if (!c1.IsTrigger && !b2.IsTrigger) ResolveCollision(e1, t1, e2, t2, manifold);
            }
        });
    });
}

void PhysicsSystem::CheckCircleCollisions() const
{
    m_registry.View<Transform2D, CircleCollider>([this](Entity e1, Transform2D& t1, CircleCollider& c1)
    {
        m_registry.View<Transform2D, CircleCollider>([&](Entity e2, Transform2D& t2, CircleCollider& c2)
        {
            if (e1 >= e2) return;

            CollisionManifold manifold = Physics::IntersectCircle(c1, t1, c2, t2);
            if (manifold.IsColliding)
            {
                c1.IsColliding = true;
                c2.IsColliding = true;
                m_events.Publish(CollisionEvent{e1, e2});

                if (!c1.IsTrigger && !c2.IsTrigger) ResolveCollision(e1, t1, e2, t2, manifold);
            }
        });
    });
}

void PhysicsSystem::ResolveCollision(Entity e1, Transform2D& t1, Entity e2, Transform2D& t2, const CollisionManifold& manifold) const
{
    RigidBody* rb1 = m_registry.HasComponent<RigidBody>(e1) ? &m_registry.GetComponent<RigidBody>(e1) : nullptr;
    RigidBody* rb2 = m_registry.HasComponent<RigidBody>(e2) ? &m_registry.GetComponent<RigidBody>(e2) : nullptr;

    float invMass1 = (rb1 && !rb1->IsKinematic) ? 1.0f / rb1->Mass : 0.0f;
    float invMass2 = (rb2 && !rb2->IsKinematic) ? 1.0f / rb2->Mass : 0.0f;
    float sumInvMass = invMass1 + invMass2;

    if (sumInvMass == 0.0f) return;

    ResolvePenetration(t1, invMass1, t2, invMass2, manifold);
    ResolveImpulse(rb1, invMass1, rb2, invMass2, manifold);
}

void PhysicsSystem::ResolvePenetration(Transform2D& t1, float invMass1, Transform2D& t2, float invMass2, const CollisionManifold& manifold)
{
    float sumInvMass = invMass1 + invMass2;
    if (invMass1 > 0.0f) t1.Position += manifold.Normal * (manifold.Penetration * (invMass1 / sumInvMass));
    if (invMass2 > 0.0f) t2.Position -= manifold.Normal * (manifold.Penetration * (invMass2 / sumInvMass));
}

void PhysicsSystem::ResolveImpulse(RigidBody* rb1, float invMass1, RigidBody* rb2, float invMass2, const CollisionManifold& manifold)
{
    Vector2f v1 = rb1 ? rb1->Velocity : Vector2f{0.0f, 0.0f};
    Vector2f v2 = rb2 ? rb2->Velocity : Vector2f{0.0f, 0.0f};
    Vector2f relativeVelocity = v1 - v2;

    float velAlongNormal = relativeVelocity.Dot(manifold.Normal);
    if (velAlongNormal > 0.0f) return;

    float e = 1.0f;
    if (rb1 && rb2) e = std::min(rb1->Restitution, rb2->Restitution);
    else if (rb1) e = rb1->Restitution;
    else if (rb2) e = rb2->Restitution;

    float sumInvMass = invMass1 + invMass2;
    float j = -(1.0f + e) * velAlongNormal;
    j /= sumInvMass;

    Vector2f impulse = manifold.Normal * j;
    if (invMass1 > 0.0f) rb1->Velocity += impulse * invMass1;
    if (invMass2 > 0.0f) rb2->Velocity -= impulse * invMass2;
}