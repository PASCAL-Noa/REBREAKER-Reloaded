#include <doctest/doctest.h>
#include "Math/Vector2.h"
#include "Math/Physics.h"
#include "Math/EasingFunctions.h"
#include "ECS/Components/Transform2D.h"

#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"

TEST_SUITE("Math Utilities Tests")
{
    TEST_CASE("Vector2f basic arithmetic operations")
    {
        Vector2f a{3.0f, 4.0f};
        Vector2f b{1.0f, 2.0f};

        Vector2f sum = a + b;
        CHECK(sum.X == doctest::Approx(4.0f));
        CHECK(sum.Y == doctest::Approx(6.0f));

        Vector2f diff = a - b;
        CHECK(diff.X == doctest::Approx(2.0f));
        CHECK(diff.Y == doctest::Approx(2.0f));

        Vector2f scaled = a * 2.0f;
        CHECK(scaled.X == doctest::Approx(6.0f));
        CHECK(scaled.Y == doctest::Approx(8.0f));

        Vector2f divided = a / 2.0f;
        CHECK(divided.X == doctest::Approx(1.5f));
        CHECK(divided.Y == doctest::Approx(2.0f));

        CHECK(a == Vector2f{3.0f, 4.0f});
        CHECK(a != b);
    }

    TEST_CASE("Vector2f length, dot product and normalization")
    {
        Vector2f v{3.0f, 4.0f};
        CHECK(v.LengthSquared() == doctest::Approx(25.0f));
        CHECK(v.Length() == doctest::Approx(5.0f));

        Vector2f norm = v.Normalized();
        CHECK(norm.Length() == doctest::Approx(1.0f));
        CHECK(norm.X == doctest::Approx(0.6f));
        CHECK(norm.Y == doctest::Approx(0.8f));

        Vector2f other{4.0f, -3.0f}; // Perpendicular vector
        CHECK(v.Dot(other) == doctest::Approx(0.0f));

        Vector2f sameDir{6.0f, 8.0f};
        CHECK(v.Dot(sameDir) == doctest::Approx(50.0f));
    }

    TEST_CASE("Vector2i arithmetic and comparison")
    {
        Vector2i a{10, 20};
        Vector2i b{3, 4};

        CHECK((a + b) == Vector2i{13, 24});
        CHECK((a - b) == Vector2i{7, 16});
        CHECK((a * 2) == Vector2i{20, 40});
        CHECK((a / 2) == Vector2i{5, 10});
    }

    TEST_CASE("Physics::Clamp utility")
    {
        CHECK(Physics::Clamp(5.0f, 0.0f, 10.0f) == doctest::Approx(5.0f));
        CHECK(Physics::Clamp(-5.0f, 0.0f, 10.0f) == doctest::Approx(0.0f));
        CHECK(Physics::Clamp(15.0f, 0.0f, 10.0f) == doctest::Approx(10.0f));
    }

    TEST_CASE("EasingFunctions interpolation routines")
    {
        auto linear = EasingFunctions::GetFunction(EasingFunctions::EasingType::Linear);
        CHECK(linear(0.0f) == doctest::Approx(0.0f));
        CHECK(linear(0.5f) == doctest::Approx(0.5f));
        CHECK(linear(1.0f) == doctest::Approx(1.0f));

        auto quadIn = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseInQuad);
        CHECK(quadIn(0.0f) == doctest::Approx(0.0f));
        CHECK(quadIn(0.5f) == doctest::Approx(0.25f));
        CHECK(quadIn(1.0f) == doctest::Approx(1.0f));

        auto quadOut = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseOutQuad);
        CHECK(quadOut(0.0f) == doctest::Approx(0.0f));
        CHECK(quadOut(0.5f) == doctest::Approx(0.75f));
        CHECK(quadOut(1.0f) == doctest::Approx(1.0f));

        auto cubicIn = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseInCubic);
        CHECK(cubicIn(0.0f) == doctest::Approx(0.0f));
        CHECK(cubicIn(1.0f) == doctest::Approx(1.0f));

        auto bounceOut = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseOutBounce);
        CHECK(bounceOut(0.0f) == doctest::Approx(0.0f));
        CHECK(bounceOut(1.0f) == doctest::Approx(1.0f));
    }
}


TEST_SUITE("Collision Detection Algorithms Tests")
{
    TEST_CASE("AABB vs AABB Collision - Intersecting, Touching, and Separated")
    {
        BoxCollider boxA{Vector2f{100.0f, 40.0f}};
        BoxCollider boxB{Vector2f{100.0f, 40.0f}};

        SUBCASE("Overlapping boxes along X axis")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            Transform2D transB{Vector2f{80.0f, 0.0f}}; // Distance = 80, SumHalfExtents = 100 -> overlap 20

            CollisionManifold m = Physics::IntersectAABB(boxA, transA, boxB, transB);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(20.0f));
            CHECK(m.Normal.X == doctest::Approx(-1.0f));
            CHECK(m.Normal.Y == doctest::Approx(0.0f));
        }

        SUBCASE("Overlapping boxes along Y axis")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            Transform2D transB{Vector2f{0.0f, 30.0f}}; // Distance = 30, SumHalfExtents = 40 -> overlap 10

            CollisionManifold m = Physics::IntersectAABB(boxA, transA, boxB, transB);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(10.0f));
            CHECK(m.Normal.X == doctest::Approx(0.0f));
            CHECK(m.Normal.Y == doctest::Approx(-1.0f));
        }

        SUBCASE("Separated boxes")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            Transform2D transB{Vector2f{150.0f, 0.0f}}; // Distance = 150 > 100

            CollisionManifold m = Physics::IntersectAABB(boxA, transA, boxB, transB);
            CHECK_FALSE(m.IsColliding);
        }

        SUBCASE("Scaled boxes")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            transA.Scale = Vector2f{2.0f, 1.0f}; // boxA effective width = 200 (half = 100)
            Transform2D transB{Vector2f{130.0f, 0.0f}}; // boxB half = 50. Sum = 150 > 130 -> overlap 20

            CollisionManifold m = Physics::IntersectAABB(boxA, transA, boxB, transB);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(20.0f));
        }
    }

    TEST_CASE("Circle vs Circle Collision")
    {
        CircleCollider circleA{20.0f};
        CircleCollider circleB{30.0f};

        SUBCASE("Intersecting circles")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            Transform2D transB{Vector2f{40.0f, 0.0f}}; // Distance = 40, SumRadii = 50 -> penetration = 10

            CollisionManifold m = Physics::IntersectCircle(circleA, transA, circleB, transB);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(10.0f));
            CHECK(m.Normal.X == doctest::Approx(-1.0f));
            CHECK(m.Normal.Y == doctest::Approx(0.0f));
        }

        SUBCASE("Separated circles")
        {
            Transform2D transA{Vector2f{0.0f, 0.0f}};
            Transform2D transB{Vector2f{60.0f, 0.0f}}; // Distance = 60 > 50

            CollisionManifold m = Physics::IntersectCircle(circleA, transA, circleB, transB);
            CHECK_FALSE(m.IsColliding);
        }

        SUBCASE("Concentric circles (same center)")
        {
            Transform2D transA{Vector2f{10.0f, 10.0f}};
            Transform2D transB{Vector2f{10.0f, 10.0f}};

            CollisionManifold m = Physics::IntersectCircle(circleA, transA, circleB, transB);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(50.0f));
        }
    }

    TEST_CASE("Circle vs AABB Collision")
    {
        CircleCollider circle{15.0f};
        BoxCollider box{Vector2f{100.0f, 30.0f}}; // Half-extents: 50, 15

        SUBCASE("Circle hitting top of box")
        {
            Transform2D boxTrans{Vector2f{0.0f, 0.0f}};
            // Top face of box is at Y = -15. Circle at Y = -25 (Distance to edge = 10 < radius 15)
            Transform2D circleTrans{Vector2f{0.0f, -25.0f}};

            CollisionManifold m = Physics::IntersectCircleAABB(circle, circleTrans, box, boxTrans);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(5.0f));
            CHECK(m.Normal.X == doctest::Approx(0.0f));
            CHECK(m.Normal.Y == doctest::Approx(-1.0f));
        }

        SUBCASE("Circle hitting right of box")
        {
            Transform2D boxTrans{Vector2f{0.0f, 0.0f}};
            // Right edge of box is at X = 50. Circle at X = 60 (Distance = 10 < radius 15)
            Transform2D circleTrans{Vector2f{60.0f, 0.0f}};

            CollisionManifold m = Physics::IntersectCircleAABB(circle, circleTrans, box, boxTrans);
            CHECK(m.IsColliding);
            CHECK(m.Penetration == doctest::Approx(5.0f));
            CHECK(m.Normal.X == doctest::Approx(1.0f));
            CHECK(m.Normal.Y == doctest::Approx(0.0f));
        }

        SUBCASE("Circle completely outside box")
        {
            Transform2D boxTrans{Vector2f{0.0f, 0.0f}};
            Transform2D circleTrans{Vector2f{100.0f, 100.0f}};

            CollisionManifold m = Physics::IntersectCircleAABB(circle, circleTrans, box, boxTrans);
            CHECK_FALSE(m.IsColliding);
        }
    }
}
