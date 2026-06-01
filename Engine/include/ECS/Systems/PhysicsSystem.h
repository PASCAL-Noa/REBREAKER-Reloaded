#pragma once
#include "ECS/System.h"
#include "Math/Physics.h"
#include "ECS/Entity.h"
#include "ECS/Components/RigidBody.h"

class PhysicsSystem : public System
{
public:
    using System::System;

    void            OnUpdate(float dt) override;

private:
    void            ResetColliders() const;
    void            ApplyVelocity(float dt) const;
    void            CheckAABBCollisions() const;
    void            CheckCircleAABBCollisions() const;
    void            CheckCircleCollisions() const;

    void            ResolveCollision(Entity e1, Transform2D& t1, Entity e2, Transform2D& t2,
                                        const CollisionManifold& manifold) const;
    static void     ResolvePenetration(Transform2D& t1, float invMass1, Transform2D& t2, float invMass2,
                                        const CollisionManifold& manifold);
    static void     ResolveImpulse(RigidBody* rb1, float invMass1, RigidBody* rb2, float invMass2,
                                        const CollisionManifold& manifold);
};