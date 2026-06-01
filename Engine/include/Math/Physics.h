#pragma once
#include "Math/Vector2.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/Transform2D.h"
#include <cmath>

struct CollisionManifold
{
    bool        IsColliding = false;
    float       Penetration = 0.0f;
    Vector2f    Normal;
};

class Physics
{
public:
    static float Clamp(float value, float min, float max);

    static CollisionManifold IntersectCircleAABB(
        const CircleCollider& circle, const Transform2D& circleTransform,
        const BoxCollider& box, const Transform2D& boxTransform);

    static CollisionManifold IntersectAABB(
        const BoxCollider& boxA, const Transform2D& transformA,
        const BoxCollider& boxB, const Transform2D& transformB);

    static CollisionManifold IntersectCircle(
        const CircleCollider& circleA, const Transform2D& transformA,
        const CircleCollider& circleB, const Transform2D& transformB);
};