#include <doctest/doctest.h>
#include "ECS/Registry.hpp"
#include "ECS/Systems/DynamicEventSystem.h"
#include "ECS/Components/EnvironmentalProjectileComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "Events/ArenaEvents.h"
#include <cmath>

TEST_SUITE("Dynamic Arena Event Engine Tests")
{
    TEST_CASE("Lifecycle State Machine Transitions (Idle -> Warning -> Active -> Cooldown -> Idle)")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.SetWarningDuration(2.0f);
        eventSystem.SetEventDuration(5.0f);
        eventSystem.SetCooldownDuration(3.0f);

        CHECK(eventSystem.GetState() == ArenaEventState::Idle);
        CHECK_FALSE(eventSystem.IsEventActive());
        CHECK_FALSE(eventSystem.IsWarningActive());

        // Trigger an event with warning
        eventSystem.TriggerEvent(ArenaEventType::GravityShiftLeft, false);

        CHECK(eventSystem.GetState() == ArenaEventState::Warning);
        CHECK(eventSystem.IsWarningActive());
        CHECK_FALSE(eventSystem.IsEventActive());
        CHECK(eventSystem.GetCurrentEventType() == ArenaEventType::GravityShiftLeft);
        CHECK(eventSystem.GetTimeRemaining() == doctest::Approx(2.0f));

        // Advance 1 second during warning
        eventSystem.UpdateLifecycle(1.0f);
        CHECK(eventSystem.GetState() == ArenaEventState::Warning);
        CHECK(eventSystem.GetTimeRemaining() == doctest::Approx(1.0f));

        // Advance remaining 1.1 seconds -> transitions to Active
        eventSystem.UpdateLifecycle(1.1f);
        CHECK(eventSystem.GetState() == ArenaEventState::Active);
        CHECK(eventSystem.IsEventActive());
        CHECK_FALSE(eventSystem.IsWarningActive());
        CHECK(eventSystem.GetCurrentEventType() == ArenaEventType::GravityShiftLeft);
        CHECK(eventSystem.GetGravityForce().X < 0.0f);

        // Advance 5.1 seconds -> transitions to Cooldown
        eventSystem.UpdateLifecycle(5.1f);
        CHECK(eventSystem.GetState() == ArenaEventState::Cooldown);
        CHECK_FALSE(eventSystem.IsEventActive());
        // Standard physics should be restored upon entering cooldown
        CHECK(eventSystem.GetGravityForce().X == doctest::Approx(0.0f));

        // Advance remaining cooldown 3.1 seconds -> transitions to Idle
        eventSystem.UpdateLifecycle(3.1f);
        CHECK(eventSystem.GetState() == ArenaEventState::Idle);
    }

    TEST_CASE("Skip Warning Immediate Triggering and Manual Stop")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.SetEventDuration(8.0f);

        // Immediate activation without warning
        eventSystem.TriggerEvent(ArenaEventType::GravityShiftLeft, true);

        CHECK(eventSystem.GetState() == ArenaEventState::Active);
        CHECK(eventSystem.IsEventActive());
        CHECK(eventSystem.GetCurrentEventType() == ArenaEventType::GravityShiftLeft);
        CHECK(eventSystem.GetGravityForce().X < 0.0f);
        CHECK(eventSystem.GetArenaTiltAngle() < 0.0f);

        // Stop active event manually -> enters Cooldown and restores physics
        eventSystem.StopActiveEvent();

        CHECK(eventSystem.GetState() == ArenaEventState::Cooldown);
        CHECK_FALSE(eventSystem.IsEventActive());
        CHECK(eventSystem.GetGravityForce().X == doctest::Approx(0.0f));
        CHECK(eventSystem.GetGravityForce().Y == doctest::Approx(0.0f));
        CHECK(eventSystem.GetArenaTiltAngle() == doctest::Approx(0.0f));

        // ResetLifecycle returns directly to Idle
        eventSystem.ResetLifecycle();
        CHECK(eventSystem.GetState() == ArenaEventState::Idle);
    }

    TEST_CASE("Physical Alterations: Time Dilation / Multi-Variation Temporal Flux (20s Slice Engine)")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.TriggerEvent(ArenaEventType::TemporalFlux, true);
        CHECK(eventSystem.IsEventActive());
        CHECK(eventSystem.GetStateDuration() == doctest::Approx(20.0f));
        CHECK(eventSystem.GetTimeRemaining() == doctest::Approx(20.0f));

        const auto& slices = eventSystem.GetTemporalSlices();
        CHECK(slices.size() >= 3);

        // Sum of all slice durations must be strictly equal to 20.0s
        float totalSliceDuration = 0.0f;
        for (const auto& slice : slices)
        {
            totalSliceDuration += slice.Duration;
            CHECK(slice.Duration >= 1.5f);
        }
        CHECK(totalSliceDuration == doctest::Approx(20.0f));

        // Slices must strictly alternate between slow and speed
        for (size_t i = 1; i < slices.size(); ++i)
        {
            CHECK(slices[i].IsSlow != slices[i - 1].IsSlow);
        }

        // Active time scale should be either 0.50f (slow) or 1.50f (speed)
        const float initialScale = eventSystem.GetTimeScale();
        CHECK((initialScale == doctest::Approx(0.50f) || initialScale == doctest::Approx(1.50f)));

        // Advance through first slice
        const float firstSliceDur = slices[0].Duration;
        eventSystem.UpdateLifecycle(firstSliceDur + 0.05f);

        // Should now be on second slice with inverted scale
        CHECK(eventSystem.GetCurrentTemporalSliceIndex() == 1);
        const float secondScale = eventSystem.GetTimeScale();
        CHECK((secondScale == doctest::Approx(0.50f) || secondScale == doctest::Approx(1.50f)));
        CHECK(secondScale != doctest::Approx(initialScale));

        // Fast-forward to end of 20s event -> clean restoration to 1.0f
        eventSystem.UpdateLifecycle(20.0f);
        CHECK(eventSystem.GetState() == ArenaEventState::Cooldown);
        CHECK(eventSystem.GetTimeScale() == doctest::Approx(1.0f));
    }

    TEST_CASE("Physical Alterations: Directional Gravity Shifts & Arena Tilt")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        // Gravity Left
        eventSystem.TriggerEvent(ArenaEventType::GravityShiftLeft, true);
        CHECK(eventSystem.GetGravityForce().X < -200.0f);
        CHECK(eventSystem.GetGravityForce().Y == doctest::Approx(0.0f));
        CHECK(eventSystem.GetArenaTiltAngle() < 0.0f);
        eventSystem.RestoreStandardPhysics();
        CHECK(eventSystem.GetGravityForce().X == doctest::Approx(0.0f));
        CHECK(eventSystem.GetArenaTiltAngle() == doctest::Approx(0.0f));

        // Gravity Right
        eventSystem.TriggerEvent(ArenaEventType::GravityShiftRight, true);
        CHECK(eventSystem.GetGravityForce().X > 200.0f);
        CHECK(eventSystem.GetGravityForce().Y == doctest::Approx(0.0f));
        CHECK(eventSystem.GetArenaTiltAngle() > 0.0f);
        eventSystem.RestoreStandardPhysics();
        CHECK(eventSystem.GetGravityForce().X == doctest::Approx(0.0f));
        CHECK(eventSystem.GetArenaTiltAngle() == doctest::Approx(0.0f));

        // Gravity Down
        eventSystem.TriggerEvent(ArenaEventType::GravityShiftDown, true);
        CHECK(eventSystem.GetGravityForce().X == doctest::Approx(0.0f));
        CHECK(eventSystem.GetGravityForce().Y > 200.0f);
        CHECK(eventSystem.GetArenaTiltAngle() == doctest::Approx(0.0f));
        eventSystem.RestoreStandardPhysics();
        CHECK(eventSystem.GetGravityForce().Y == doctest::Approx(0.0f));
    }

    TEST_CASE("Physical Alterations: Camera Earthquake & Ball Agitation")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        // Spawn a ball with Transform2D, RigidBody, and BallComponent
        Entity ball = registry.CreateEntity();
        auto& ballTransform = registry.AddComponent<Transform2D>(ball);
        ballTransform.Position = Vector2f(400.0f, 300.0f);
        registry.AddComponent<BallComponent>(ball);
        auto& ballRb = registry.AddComponent<RigidBody>(ball);
        ballRb.Velocity = Vector2f(0.0f, -400.0f);

        eventSystem.TriggerEvent(ArenaEventType::CameraEarthquake, true);

        // Simulate a frame of earthquake
        eventSystem.OnUpdate(0.05f);

        // Camera shake offset or angle should have activated
        CHECK((eventSystem.GetCameraShakeOffset().X != 0.0f ||
               eventSystem.GetCameraShakeOffset().Y != 0.0f ||
               eventSystem.GetCameraShakeAngle() != 0.0f));

        // Clean restoration
        eventSystem.StopActiveEvent();
        CHECK(eventSystem.GetCameraShakeOffset().X == doctest::Approx(0.0f));
        CHECK(eventSystem.GetCameraShakeOffset().Y == doctest::Approx(0.0f));
        CHECK(eventSystem.GetCameraShakeAngle() == doctest::Approx(0.0f));
    }

    TEST_CASE("Solar Flare / Blackout Zone Intensity and Restoration")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.TriggerEvent(ArenaEventType::SolarFlareBlackout, true);
        CHECK(eventSystem.GetBlackoutIntensity() > 0.8f);

        eventSystem.StopActiveEvent();
        CHECK(eventSystem.GetBlackoutIntensity() == doctest::Approx(0.0f));
    }

    TEST_CASE("Meteor Shower / Environmental Projectile Physics & Deflection")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        CHECK(eventSystem.GetActiveProjectileCount() == 0);

        // Spawn a meteor projectile
        Entity meteor = eventSystem.SpawnMeteor(Vector2f(200.0f, 100.0f), Vector2f(50.0f, 300.0f));
        REQUIRE(meteor != NULL_ENTITY);
        CHECK(eventSystem.GetActiveProjectileCount() == 1);

        REQUIRE(registry.HasComponent<EnvironmentalProjectileComponent>(meteor));
        auto& proj = registry.GetComponent<EnvironmentalProjectileComponent>(meteor);
        CHECK_FALSE(proj.IsDeflected);
        CHECK(proj.Lifetime > 0.0f);

        // Advance simulation
        eventSystem.OnUpdate(0.1f);
        auto& trans = registry.GetComponent<Transform2D>(meteor);
        // Position should have advanced
        CHECK(trans.Position.Y > 100.0f);

        // Clear projectiles cleanly
        eventSystem.ClearProjectiles();
        CHECK(eventSystem.GetActiveProjectileCount() == 0);
        CHECK_FALSE(registry.IsAlive(meteor));
    }

    TEST_CASE("Event Names and Color Lookups")
    {
        CHECK(std::string(GetArenaEventName(ArenaEventType::TemporalFlux)) == "TEMPORAL FLUX");
        CHECK(std::string(GetArenaEventName(ArenaEventType::GravityShiftLeft)) == "GRAVITY SHIFT (LEFT)");
        CHECK(std::string(GetArenaEventName(ArenaEventType::GravityShiftRight)) == "GRAVITY SHIFT (RIGHT)");
        CHECK(std::string(GetArenaEventName(ArenaEventType::GravityShiftDown)) == "GRAVITY SHIFT (DOWN)");
        CHECK(std::string(GetArenaEventName(ArenaEventType::CameraEarthquake)) == "CAMERA EARTHQUAKE");
        CHECK(std::string(GetArenaEventName(ArenaEventType::SolarFlareBlackout)) == "SOLAR FLARE / BLACKOUT");
        CHECK(std::string(GetArenaEventName(ArenaEventType::MeteorShower)) == "METEOR SHOWER");

        Color slowCol = GetArenaEventColor(ArenaEventType::TemporalFluxSlow);
        CHECK(slowCol.a == 255);
        Color meteorCol = GetArenaEventColor(ArenaEventType::MeteorShower);
        CHECK(meteorCol.r == 255);
    }

    TEST_CASE("Visual & Post-Processing Feedback: Alarm Intensity, Strobe, and Color Restoration")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.SetWarningDuration(2.0f);
        eventSystem.SetEventDuration(4.0f);

        CHECK(eventSystem.GetAlarmIntensity() == doctest::Approx(0.0f));

        // Start Warning
        eventSystem.TriggerEvent(ArenaEventType::CameraEarthquake, false);
        CHECK(eventSystem.IsWarningActive());
        CHECK(eventSystem.GetAlarmIntensity() > 0.0f);
        Color warnCol = eventSystem.GetAlarmColor();
        CHECK(warnCol.r > 200);

        // Update during warning
        eventSystem.UpdateLifecycle(0.5f);
        CHECK(eventSystem.GetAlarmIntensity() > 0.0f);

        // Advance into Active
        eventSystem.UpdateLifecycle(1.6f);
        CHECK(eventSystem.IsEventActive());
        CHECK(eventSystem.GetAlarmIntensity() > 0.0f);
        Color activeCol = eventSystem.GetAlarmColor();
        CHECK(activeCol.r == GetArenaEventColor(ArenaEventType::CameraEarthquake).r);

        // Restore standard physics clears alarm
        eventSystem.RestoreStandardPhysics();
        CHECK(eventSystem.GetAlarmIntensity() == doctest::Approx(0.0f));
    }

    TEST_CASE("TriggerRandomEvent and Customizable Intervals")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        eventSystem.SetIntervalRange(12.0f, 18.0f);
        CHECK(eventSystem.GetMinInterval() == doctest::Approx(12.0f));
        CHECK(eventSystem.GetMaxInterval() == doctest::Approx(18.0f));

        // Random trigger with warning
        eventSystem.TriggerRandomEvent(false);
        CHECK(eventSystem.IsWarningActive());
        CHECK(eventSystem.GetCurrentEventType() != ArenaEventType::None);
        CHECK(static_cast<int>(eventSystem.GetCurrentEventType()) < static_cast<int>(ArenaEventType::Count));

        // Random trigger immediate
        eventSystem.TriggerRandomEvent(true);
        CHECK(eventSystem.IsEventActive());
        CHECK(eventSystem.GetCurrentEventType() != ArenaEventType::None);
    }

    TEST_CASE("Meteor Deflection by Paddle and Destruction of Bricks via Callbacks")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        // Create paddle entity
        Entity paddle = registry.CreateEntity();
        registry.AddComponent<Transform2D>(paddle, Transform2D{Vector2f{200.0f, 300.0f}});
        registry.AddComponent<BoxCollider>(paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
        eventSystem.SetPaddleEntity(paddle);

        // Create brick entity
        Entity brick = registry.CreateEntity();
        registry.AddComponent<Transform2D>(brick, Transform2D{Vector2f{200.0f, 150.0f}});
        registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{60.0f, 25.0f}, Vector2f{0.0f, 0.0f}, false, false});
        registry.AddComponent<BrickComponent>(brick, BrickComponent{});

        uint32_t awardedScore = 0;
        eventSystem.SetScoreCallback([&](uint32_t s, uint32_t m) {
            awardedScore += s * m;
        });

        Entity hitBrickEntity = NULL_ENTITY;
        eventSystem.SetBrickCollisionCallback([&](Entity b, Entity) {
            hitBrickEntity = b;
        });

        // Spawn meteor falling downwards onto paddle
        Entity meteor = eventSystem.SpawnMeteor(Vector2f{200.0f, 285.0f}, Vector2f{0.0f, 300.0f});
        REQUIRE(registry.IsAlive(meteor));

        // Run system update -> should collide with paddle and deflect upward
        eventSystem.OnUpdate(0.016f);

        auto& epc = registry.GetComponent<EnvironmentalProjectileComponent>(meteor);
        auto& rb = registry.GetComponent<RigidBody>(meteor);
        CHECK(epc.IsDeflected);
        CHECK(rb.Velocity.Y < 0.0f); // Velocity inverted upwards!
        CHECK(awardedScore == 150);

        // Move meteor into the brick
        auto& trans = registry.GetComponent<Transform2D>(meteor);
        trans.Position = Vector2f{200.0f, 150.0f};

        // Run update -> should collide with brick and trigger callback
        eventSystem.OnUpdate(0.016f);

        CHECK(hitBrickEntity == brick);
        registry.ProcessDeferredCommands();
        CHECK_FALSE(registry.IsAlive(meteor));
    }
}
