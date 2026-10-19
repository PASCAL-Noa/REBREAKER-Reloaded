#include <doctest/doctest.h>
#include "ECS/Registry.hpp"
#include "ECS/SparseSet.hpp"
#include <string>

struct Position
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity
{
    float vx = 0.0f;
    float vy = 0.0f;
};

struct Name
{
    std::string value;
};

TEST_SUITE("SparseSet Tests")
{
    TEST_CASE("SparseSet basic insert, get, contains, and remove")
    {
        SparseSet<Position> set;
        CHECK(set.Size() == 0);

        Entity e1 = MakeEntity(10, 1);
        Entity e2 = MakeEntity(42, 1);
        Entity e3 = MakeEntity(5000, 1); // Crosses page boundary (PAGE_SIZE = 4096)

        CHECK_FALSE(set.Contains(e1));
        CHECK_FALSE(set.Contains(e2));
        CHECK_FALSE(set.Contains(e3));

        set.Insert(e1, Position{100.0f, 200.0f});
        set.Insert(e2, Position{300.0f, 400.0f});
        set.Insert(e3, Position{500.0f, 600.0f});

        CHECK(set.Size() == 3);
        CHECK(set.Contains(e1));
        CHECK(set.Contains(e2));
        CHECK(set.Contains(e3));

        CHECK(set.Get(e1).x == doctest::Approx(100.0f));
        CHECK(set.Get(e1).y == doctest::Approx(200.0f));
        CHECK(set.Get(e2).x == doctest::Approx(300.0f));
        CHECK(set.Get(e3).x == doctest::Approx(500.0f));

        // Test TryGet
        CHECK(set.TryGet(e1) != nullptr);
        CHECK(set.TryGet(e1)->y == doctest::Approx(200.0f));
        Entity nonExistent = MakeEntity(999, 1);
        CHECK(set.TryGet(nonExistent) == nullptr);

        // Remove element from middle
        set.Remove(e2);
        CHECK(set.Size() == 2);
        CHECK(set.Contains(e1));
        CHECK_FALSE(set.Contains(e2));
        CHECK(set.Contains(e3));
        CHECK(set.Get(e1).x == doctest::Approx(100.0f));
        CHECK(set.Get(e3).x == doctest::Approx(500.0f));

        // Remove remaining
        set.Remove(e1);
        CHECK(set.Size() == 1);
        CHECK_FALSE(set.Contains(e1));
        CHECK(set.Contains(e3));

        set.Remove(e3);
        CHECK(set.Size() == 0);
        CHECK_FALSE(set.Contains(e3));
    }

    TEST_CASE("SparseSet version mismatch handling")
    {
        SparseSet<int> set;
        Entity v1 = MakeEntity(5, 1);
        Entity v2 = MakeEntity(5, 2);

        set.Insert(v1, 42);
        CHECK(set.Contains(v1));
        // v2 shares the index but has different version
        CHECK_FALSE(set.Contains(v2));

        // Removal with wrong version does not remove v1
        set.Remove(v2);
        CHECK(set.Contains(v1));
        CHECK(set.Get(v1) == 42);
    }

    TEST_CASE("SparseSet clear and memory cleanup")
    {
        SparseSet<int> set;
        for (uint32_t i = 0; i < 100; ++i)
        {
            set.Insert(MakeEntity(i, 1), static_cast<int>(i * 10));
        }
        CHECK(set.Size() == 100);

        set.Clear();
        CHECK(set.Size() == 0);
        CHECK_FALSE(set.Contains(MakeEntity(0, 1)));
        CHECK_FALSE(set.Contains(MakeEntity(99, 1)));
    }
}

TEST_SUITE("Registry Lifecycle & Entity Recycling Tests")
{
    TEST_CASE("Entity creation and version assignment")
    {
        Registry registry;
        CHECK(registry.GetActiveEntityCount() == 0);

        Entity e0 = registry.CreateEntity();
        Entity e1 = registry.CreateEntity();
        Entity e2 = registry.CreateEntity();

        CHECK(registry.GetActiveEntityCount() == 3);
        CHECK(registry.IsAlive(e0));
        CHECK(registry.IsAlive(e1));
        CHECK(registry.IsAlive(e2));

        CHECK(GetEntityIndex(e0) == 0);
        CHECK(GetEntityIndex(e1) == 1);
        CHECK(GetEntityIndex(e2) == 2);

        CHECK(GetEntityVersion(e0) == 1);
        CHECK(GetEntityVersion(e1) == 1);
        CHECK(GetEntityVersion(e2) == 1);
    }

    TEST_CASE("Entity destruction and index recycling with incremented version")
    {
        Registry registry;
        Entity e0 = registry.CreateEntity();
        Entity e1 = registry.CreateEntity();

        CHECK(registry.IsAlive(e0));
        CHECK(registry.IsAlive(e1));

        registry.AddComponent<Position>(e0, Position{10.0f, 20.0f});
        CHECK(registry.HasComponent<Position>(e0));

        // Destroy e0
        registry.DestroyEntity(e0);
        CHECK_FALSE(registry.IsAlive(e0));
        CHECK(registry.IsAlive(e1));
        CHECK(registry.GetActiveEntityCount() == 1);


        // Position component should be removed from destroyed entity
        CHECK_FALSE(registry.HasComponent<Position>(e0));

        // Recycle: next entity created should reuse index 0 with version 2
        Entity e0_recycled = registry.CreateEntity();
        CHECK(GetEntityIndex(e0_recycled) == 0);
        CHECK(GetEntityVersion(e0_recycled) == 2);
        CHECK(registry.IsAlive(e0_recycled));
        CHECK_FALSE(registry.IsAlive(e0)); // Old handle remains dead
        CHECK(registry.GetActiveEntityCount() == 2);

        // Recycled entity does not have old components
        CHECK_FALSE(registry.HasComponent<Position>(e0_recycled));
    }

    TEST_CASE("Registry component management: Add, Get, Has, Remove")
    {
        Registry registry;
        Entity e = registry.CreateEntity();

        CHECK_FALSE(registry.HasComponent<Position>(e));
        CHECK_FALSE(registry.HasComponent<Velocity>(e));

        registry.AddComponent<Position>(e, Position{5.0f, -10.0f});
        CHECK(registry.HasComponent<Position>(e));
        CHECK(registry.GetComponent<Position>(e).x == doctest::Approx(5.0f));
        CHECK(registry.GetComponent<Position>(e).y == doctest::Approx(-10.0f));

        // Modify in-place
        registry.GetComponent<Position>(e).x = 99.0f;
        CHECK(registry.GetComponent<Position>(e).x == doctest::Approx(99.0f));

        registry.AddComponent<Velocity>(e, Velocity{1.0f, 2.0f});
        CHECK(registry.HasComponent<Position>(e));
        CHECK(registry.HasComponent<Velocity>(e));

        registry.RemoveComponent<Position>(e);
        CHECK_FALSE(registry.HasComponent<Position>(e));
        CHECK(registry.HasComponent<Velocity>(e));
    }

    TEST_CASE("Registry multi-component View iteration")
    {
        Registry registry;

        Entity e1 = registry.CreateEntity(); // Pos + Vel
        Entity e2 = registry.CreateEntity(); // Pos only
        Entity e3 = registry.CreateEntity(); // Pos + Vel
        Entity e4 = registry.CreateEntity(); // Vel only

        registry.AddComponent<Position>(e1, Position{1.0f, 1.0f});
        registry.AddComponent<Velocity>(e1, Velocity{10.0f, 10.0f});

        registry.AddComponent<Position>(e2, Position{2.0f, 2.0f});

        registry.AddComponent<Position>(e3, Position{3.0f, 3.0f});
        registry.AddComponent<Velocity>(e3, Velocity{30.0f, 30.0f});

        registry.AddComponent<Velocity>(e4, Velocity{40.0f, 40.0f});

        // Single component view
        int posCount = 0;
        registry.View<Position>([&](Entity, Position&) {
            posCount++;
        });
        CHECK(posCount == 3);

        int velCount = 0;
        registry.View<Velocity>([&](Entity, Velocity&) {
            velCount++;
        });
        CHECK(velCount == 3);

        // Multi component view: only e1 and e3 have both Position and Velocity
        int multiCount = 0;
        float totalX = 0.0f;
        registry.View<Position, Velocity>([&](Entity, Position& p, Velocity& v) {
            multiCount++;
            totalX += p.x + v.vx;
        });
        CHECK(multiCount == 2);
        CHECK(totalX == doctest::Approx(1.0f + 10.0f + 3.0f + 30.0f));
    }

    TEST_CASE("Registry deferred destruction and command buffer execution")
    {
        Registry registry;
        Entity e1 = registry.CreateEntity();
        Entity e2 = registry.CreateEntity();

        registry.AddComponent<Position>(e1, Position{1.0f, 1.0f});
        registry.AddComponent<Position>(e2, Position{2.0f, 2.0f});

        registry.DestroyEntityDeferred(e1);
        CHECK(registry.IsPendingDestroy(e1));
        CHECK_FALSE(registry.IsAlive(e1));
        CHECK(registry.IsAlive(e2));

        // While pending destroy, Views filter e1 out
        int seen = 0;
        registry.View<Position>([&](Entity e, Position&) {
            seen++;
            CHECK(e == e2);
        });
        CHECK(seen == 1);

        // Executing deferred commands flushes destruction
        registry.ProcessDeferredCommands();
        CHECK_FALSE(registry.IsAlive(e1));
        CHECK_FALSE(registry.IsPendingDestroy(e1));
        CHECK(registry.GetActiveEntityCount() == 1);
    }
}
