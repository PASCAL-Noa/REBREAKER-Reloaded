#include "Math/Physics.h"

float Physics::Clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

CollisionManifold Physics::IntersectCircleAABB(const CircleCollider& circle, const Transform2D& circleTransform,
                                               const BoxCollider& box, const Transform2D& boxTransform)
{
    CollisionManifold manifold;

    Vector2f circleCenter = circleTransform.Position + circle.Offset;
    Vector2f boxCenter = boxTransform.Position + box.Offset;

    Vector2f halfExtents = {
        (box.Size.X * boxTransform.Scale.X) / 2.0f,
        (box.Size.Y * boxTransform.Scale.Y) / 2.0f
    };

    Vector2f difference = circleCenter - boxCenter;

    Vector2f clamped = {
        Clamp(difference.X, -halfExtents.X, halfExtents.X),
        Clamp(difference.Y, -halfExtents.Y, halfExtents.Y)
    };

    Vector2f closestPoint = boxCenter + clamped;

    Vector2f differenceToClosest = circleCenter - closestPoint;
    float distanceSquared = differenceToClosest.LengthSquared();

    float radius = circle.Radius * circleTransform.Scale.X;

    if (distanceSquared > 0.0f && distanceSquared < (radius * radius))
    {
        manifold.IsColliding = true;

        float distance = std::sqrt(distanceSquared);
        manifold.Penetration = radius - distance;
        manifold.Normal = differenceToClosest / distance;
    }
    else if (distanceSquared == 0.0f)
    {
        manifold.IsColliding = true;
        manifold.Penetration = radius;

        if (std::abs(difference.X) > std::abs(difference.Y))
            manifold.Normal = { difference.X > 0 ? 1.0f : -1.0f, 0.0f };
        else
            manifold.Normal = { 0.0f, difference.Y > 0 ? 1.0f : -1.0f };
    }
    return manifold;
}

CollisionManifold Physics::IntersectAABB(const BoxCollider& boxA, const Transform2D& transformA,
    const BoxCollider& boxB, const Transform2D& transformB)
{
    CollisionManifold manifold;

    Vector2f centerA = transformA.Position + boxA.Offset;
    Vector2f centerB = transformB.Position + boxB.Offset;

    Vector2f halfExtentsA = { (boxA.Size.X * transformA.Scale.X) / 2.0f, (boxA.Size.Y * transformA.Scale.Y) / 2.0f };
    Vector2f halfExtentsB = { (boxB.Size.X * transformB.Scale.X) / 2.0f, (boxB.Size.Y * transformB.Scale.Y) / 2.0f };

    Vector2f distanceVec = centerA - centerB;

    float overlapX = (halfExtentsA.X + halfExtentsB.X) - std::abs(distanceVec.X);
    float overlapY = (halfExtentsA.Y + halfExtentsB.Y) - std::abs(distanceVec.Y);

    if (overlapX > 0.0f && overlapY > 0.0f)
    {
        manifold.IsColliding = true;

        if (overlapX < overlapY)
        {
            manifold.Penetration = overlapX;
            manifold.Normal = { distanceVec.X > 0 ? 1.0f : -1.0f, 0.0f };
        }
        else
        {
            manifold.Penetration = overlapY;
            manifold.Normal = { 0.0f, distanceVec.Y > 0 ? 1.0f : -1.0f };
        }
    }
    return manifold;
}

CollisionManifold Physics::IntersectCircle(const CircleCollider& circleA, const Transform2D& transformA,
    const CircleCollider& circleB, const Transform2D& transformB)
{
    CollisionManifold manifold;

    Vector2f centerA = transformA.Position + circleA.Offset;
    Vector2f centerB = transformB.Position + circleB.Offset;

    Vector2f distanceVec = centerA - centerB;
    float distanceSquared = distanceVec.LengthSquared();

    float radiusA = circleA.Radius * transformA.Scale.X;
    float radiusB = circleB.Radius * transformB.Scale.X;
    float sumRadii = radiusA + radiusB;

    if (distanceSquared > 0.0f && distanceSquared < (sumRadii * sumRadii))
    {
        manifold.IsColliding = true;
        float distance = std::sqrt(distanceSquared);
        manifold.Penetration = sumRadii - distance;
        manifold.Normal = distanceVec / distance;
    }
    else if (distanceSquared == 0.0f)
    {
        manifold.IsColliding = true;
        manifold.Penetration = sumRadii;
        manifold.Normal = { 1.0f, 0.0f };
    }

    return manifold;
}
