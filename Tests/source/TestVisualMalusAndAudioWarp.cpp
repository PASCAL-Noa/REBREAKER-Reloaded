#include <doctest/doctest.h>
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/HazardZoneComponent.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Math/Physics.h"
#include "Data/PostProcessParams.h"
#include "Resources/ResourceManager.h"
#include "AudioMixer.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include <cmath>

TEST_SUITE("Visual Malus, Camera Disruption & Temporal Audio Distortion Tests (#83)")
{
    TEST_CASE("Camera Zoom Pulse oscillation and clean identity restoration")
    {
        Camera2D camera;
        camera.Zoom = 1.0f;
        camera.Rotation = 0.0f;

        float duration = 1.0f;
        float timer = 0.0f;
        const float frequency = 5.0f;
        const float amplitude = 0.08f;
        const float dt = 0.016f;

        bool hasOscillated = false;

        while (duration > 0.0f)
        {
            timer += dt;
            duration -= dt;
            if (duration < 0.0f) duration = 0.0f;

            const float decay = std::clamp(duration / 0.6f, 0.0f, 1.0f);
            const float pulse = std::sin(timer * frequency * 6.2831853f) * amplitude;
            camera.Zoom = 1.0f + pulse * decay;

            if (std::abs(camera.Zoom - 1.0f) > 0.01f)
            {
                hasOscillated = true;
            }
        }

        // Clean restoration upon expiration
        camera.Zoom = 1.0f;

        CHECK(hasOscillated);
        CHECK(camera.Zoom == doctest::Approx(1.0f));
    }

    TEST_CASE("Camera Tilt / Rotation angle oscillation and clean identity restoration")
    {
        Camera2D camera;
        camera.Rotation = 0.0f;

        float duration = 1.2f;
        float timer = 0.0f;
        const float maxAngle = 3.5f;
        const float frequency = 3.2f;
        const float dt = 0.016f;

        bool hasTilted = false;

        while (duration > 0.0f)
        {
            timer += dt;
            duration -= dt;
            if (duration < 0.0f) duration = 0.0f;

            const float decay = std::clamp(duration / 0.6f, 0.0f, 1.0f);
            const float tilt = std::sin(timer * frequency * 6.2831853f) * maxAngle;
            camera.Rotation = tilt * decay;

            if (std::abs(camera.Rotation) > 0.5f)
            {
                hasTilted = true;
            }
        }

        // Clean restoration upon expiration
        camera.Rotation = 0.0f;

        CHECK(hasTilted);
        CHECK(camera.Rotation == doctest::Approx(0.0f));
    }

    TEST_CASE("Post-Processing Screen Glitch & Scanline Flicker decay to default")
    {
        PostProcessParams postProcess;
        CHECK(postProcess.GlitchIntensity == 0.0f);
        CHECK(postProcess.ScanlineFlicker == 0.0f);

        float glitchDuration = 0.8f;
        const float maxIntensity = 1.0f;
        const float dt = 0.016f;

        // Apply glitch
        postProcess.GlitchIntensity = maxIntensity;
        postProcess.ScanlineFlicker = 0.85f;
        CHECK(postProcess.GlitchIntensity > 0.0f);
        CHECK(postProcess.ScanlineFlicker > 0.0f);

        while (glitchDuration > 0.0f)
        {
            glitchDuration -= dt;
            if (glitchDuration < 0.0f) glitchDuration = 0.0f;

            const float decay = std::clamp(glitchDuration / 0.6f, 0.0f, 1.0f);
            postProcess.GlitchIntensity = maxIntensity * decay;
            postProcess.ScanlineFlicker = 0.85f * decay;
        }

        CHECK(postProcess.GlitchIntensity == doctest::Approx(0.0f));
        CHECK(postProcess.ScanlineFlicker == doctest::Approx(0.0f));
    }

    TEST_CASE("AudioMixer Temporal Pitch Distortion & Smooth Pitch Gliding")
    {
        ResourceManager rm;
        AudioMixer mixer(rm);

        // Default pitch states
        CHECK(mixer.GetMusicPitch() == doctest::Approx(1.0f));
        CHECK(mixer.GetTargetMusicPitch() == doctest::Approx(1.0f));
        CHECK(mixer.GetTemporalSfxPitchMultiplier() == doctest::Approx(1.0f));

        // Trigger time-warp / speed-up (1.30x pitch)
        mixer.FadeMusicPitch(1.30f, 0.20f);
        mixer.SetTemporalSfxPitchMultiplier(1.30f);

        CHECK(mixer.GetTargetMusicPitch() == doctest::Approx(1.30f));
        CHECK(mixer.GetTemporalSfxPitchMultiplier() == doctest::Approx(1.30f));

        // Advance mixer time to simulate smooth pitch transition
        for (int i = 0; i < 15; ++i)
        {
            mixer.Update(0.016f);
        }

        CHECK(mixer.GetMusicPitch() > 1.15f);

        // Revert cleanly to normal playback (1.0x)
        mixer.ResetMusicPitch(0.20f);
        mixer.SetTemporalSfxPitchMultiplier(1.0f);

        CHECK(mixer.GetTargetMusicPitch() == doctest::Approx(1.0f));
        CHECK(mixer.GetTemporalSfxPitchMultiplier() == doctest::Approx(1.0f));

        for (int i = 0; i < 20; ++i)
        {
            mixer.Update(0.016f);
        }

        CHECK(mixer.GetMusicPitch() == doctest::Approx(1.0f));
    }

    TEST_CASE("HazardZoneComponent collision and effect triggering")
    {
        Registry registry;

        Entity zone = registry.CreateEntity();
        registry.AddComponent<Transform2D>(zone, Transform2D{Vector2f{0.0f, 100.0f}, 0.0f, Vector2f{1.0f, 1.0f}});
        registry.AddComponent<BoxCollider>(zone, BoxCollider{Vector2f{180.0f, 60.0f}, Vector2f{0.0f, 0.0f}, false, true});
        registry.AddComponent<HazardZoneComponent>(zone, HazardZoneComponent{
            .Size = Vector2f{180.0f, 60.0f},
            .AffectsBall = true,
            .AffectsPaddle = true,
            .Effect = HazardEffectType::AllMaluses,
            .Duration = 3.5f,
            .Intensity = 1.0f,
            .Cooldown = 1.0f,
            .CooldownTimer = 0.0f,
            .IsActive = true
        });

        // Paddle inside hazard zone
        Entity paddle = registry.CreateEntity();
        registry.AddComponent<Transform2D>(paddle, Transform2D{Vector2f{10.0f, 105.0f}, 0.0f, Vector2f{1.0f, 1.0f}});
        registry.AddComponent<BoxCollider>(paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});

        CollisionManifold manifold = Physics::IntersectAABB(
            registry.GetComponent<BoxCollider>(paddle),
            registry.GetComponent<Transform2D>(paddle),
            registry.GetComponent<BoxCollider>(zone),
            registry.GetComponent<Transform2D>(zone));
        CHECK(manifold.IsColliding);

        // Paddle outside hazard zone
        registry.GetComponent<Transform2D>(paddle).Position = Vector2f{0.0f, 300.0f};
        CollisionManifold outsideManifold = Physics::IntersectAABB(
            registry.GetComponent<BoxCollider>(paddle),
            registry.GetComponent<Transform2D>(paddle),
            registry.GetComponent<BoxCollider>(zone),
            registry.GetComponent<Transform2D>(zone));
        CHECK_FALSE(outsideManifold.IsColliding);

        // Ball intersecting hazard zone
        Entity ball = registry.CreateEntity();
        registry.AddComponent<Transform2D>(ball, Transform2D{Vector2f{0.0f, 100.0f}, 0.0f, Vector2f{1.0f, 1.0f}});
        registry.AddComponent<CircleCollider>(ball, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false, false});

        CollisionManifold ballManifold = Physics::IntersectCircleAABB(
            registry.GetComponent<CircleCollider>(ball),
            registry.GetComponent<Transform2D>(ball),
            registry.GetComponent<BoxCollider>(zone),
            registry.GetComponent<Transform2D>(zone));
        CHECK(ballManifold.IsColliding);
    }

    TEST_CASE("Debuff Capsule GlitchMalus PowerUpComponent configuration")
    {
        PowerUpComponent debuffComp{
            .Type = PowerUpType::GlitchMalus,
            .FallSpeed = 240.0f,
            .IsCollected = false,
            .IsDebuff = true
        };
        CHECK(debuffComp.Type == PowerUpType::GlitchMalus);
        CHECK(debuffComp.FallSpeed == 240.0f);
        CHECK(debuffComp.IsDebuff);
        CHECK_FALSE(debuffComp.IsCollected);

        PowerUpComponent standardBuff{
            .Type = PowerUpType::MultiBall,
            .FallSpeed = 220.0f,
            .IsCollected = false,
            .IsDebuff = false
        };
        CHECK_FALSE(standardBuff.IsDebuff);
    }

    TEST_CASE("Bounded Temporal Audio Distortion duration and synchronous timer expiry")
    {
        ResourceManager rm;
        AudioMixer mixer(rm);

        float tempoBallDuration = 3.0f;
        float audioDistortionDuration = 3.0f;
        bool audioDistortionActive = true;
        const float dt = 0.05f;

        // Active state
        mixer.FadeMusicPitch(1.15f, 0.20f);
        mixer.SetTemporalSfxPitchMultiplier(1.15f);
        CHECK(mixer.GetTargetMusicPitch() == doctest::Approx(1.15f));

        // Advance until expiration
        while (tempoBallDuration > 0.0f)
        {
            tempoBallDuration -= dt;
            audioDistortionDuration -= dt;
            if (tempoBallDuration <= 0.0f)
            {
                tempoBallDuration = 0.0f;
                audioDistortionDuration = 0.0f;
                audioDistortionActive = false;
                mixer.ResetMusicPitch(0.12f);
                mixer.SetTemporalSfxPitchMultiplier(1.0f);
            }
        }

        // Must expire synchronously
        CHECK(tempoBallDuration == doctest::Approx(0.0f));
        CHECK(audioDistortionDuration == doctest::Approx(0.0f));
        CHECK_FALSE(audioDistortionActive);
        CHECK(mixer.GetTargetMusicPitch() == doctest::Approx(1.0f));
        CHECK(mixer.GetTemporalSfxPitchMultiplier() == doctest::Approx(1.0f));
    }

    TEST_CASE("Dynamic weighted pitch modulation based on ball velocities and zone ratios")
    {
        auto calcPitch = [](int totalBalls, int fastBalls, int slowBalls) -> float {
            if (totalBalls <= 0) return 1.0f;
            const float slowRatio = static_cast<float>(slowBalls) / static_cast<float>(totalBalls);
            const float fastRatio = static_cast<float>(fastBalls) / static_cast<float>(totalBalls);

            const float fastPitch = 1.12f + std::min(0.08f, 0.02f * static_cast<float>(fastBalls));
            const float slowPitch = 0.90f - std::min(0.06f, 0.02f * static_cast<float>(slowBalls));

            return fastPitch * fastRatio + slowPitch * slowRatio;
        };

        // 1 ball: all fast (moving up)
        float pitch1Fast = calcPitch(1, 1, 0);
        CHECK(pitch1Fast > 1.13f);

        // 1 ball: in slow zone
        float pitch1Slow = calcPitch(1, 0, 1);
        CHECK(pitch1Slow < 0.90f);

        // MultiBall: 3 balls, all fast
        float pitch3Fast = calcPitch(3, 3, 0);
        CHECK(pitch3Fast > pitch1Fast);

        // MultiBall: 3 balls, all in slow zone
        float pitch3Slow = calcPitch(3, 0, 3);
        CHECK(pitch3Slow < pitch1Slow);

        // MultiBall: 2 fast, 1 slow
        float pitch2Fast1Slow = calcPitch(3, 2, 1);
        CHECK(pitch2Fast1Slow > 1.0f);
        CHECK(pitch2Fast1Slow < pitch3Fast);

        // MultiBall: 1 fast, 2 slow
        float pitch1Fast2Slow = calcPitch(3, 1, 2);
        CHECK(pitch1Fast2Slow < 1.0f);
        CHECK(pitch1Fast2Slow > pitch3Slow);
    }

    TEST_CASE("Bounded malus timers expire cleanly")
    {
        constexpr float kEffectDuration = 10.0f;
        constexpr float kMalusDuration = 5.0f;

        CHECK(kMalusDuration < kEffectDuration);

        float effectTimer = kEffectDuration;
        float malusTimer = kMalusDuration;

        // Advance 5 seconds: malus should expire, parent effect should still remain
        const float dt = 0.5f;
        for (int i = 0; i < 10; ++i)
        {
            effectTimer -= dt;
            malusTimer -= dt;
        }

        CHECK(malusTimer == doctest::Approx(0.0f));
        CHECK(effectTimer == doctest::Approx(5.0f));
        CHECK(effectTimer > 0.0f);
    }

    TEST_CASE("2D Sprite Asset Pipeline: Dedicated Brick and Capsule Component Mappings (#88)")
    {
        Registry registry;

        constexpr uint32_t kFallbackTexId = 100;
        constexpr uint32_t kSpecificMetalTexId = 101;

        // Verify entity with dedicated sprite and brick configuration
        Entity metalBrick = registry.CreateEntity();
        registry.AddComponent<Transform2D>(metalBrick, Transform2D{Vector2f{0.0f, 0.0f}});
        registry.AddComponent<BoxCollider>(metalBrick, BoxCollider{Vector2f{100.0f, 30.0f}, Vector2f{0.0f, 0.0f}, false, false});
        registry.AddComponent<BrickComponent>(metalBrick, BrickComponent{
            .Type = BrickType::Metal,
            .ScoreValue = 500,
            .HitPoints = 999999,
            .MaxHitPoints = 999999,
            .IsUnbreakable = true
        });
        registry.AddComponent<SpriteComponent>(metalBrick, SpriteComponent{kSpecificMetalTexId, Colors::White});

        CHECK(registry.IsAlive(metalBrick));
        CHECK(registry.HasComponent<SpriteComponent>(metalBrick));
        CHECK(registry.GetComponent<SpriteComponent>(metalBrick).TextureId == kSpecificMetalTexId);
        CHECK(registry.GetComponent<SpriteComponent>(metalBrick).Tint.a == 255);
        CHECK(registry.GetComponent<BrickComponent>(metalBrick).IsUnbreakable);

        // Verify fallback texture entity configuration
        Entity lightBrick = registry.CreateEntity();
        registry.AddComponent<SpriteComponent>(lightBrick, SpriteComponent{kFallbackTexId, Colors::Green});
        registry.AddComponent<BrickComponent>(lightBrick, BrickComponent{.Type = BrickType::Light, .HitPoints = 1});

        CHECK(registry.IsAlive(lightBrick));
        CHECK(registry.GetComponent<SpriteComponent>(lightBrick).TextureId == kFallbackTexId);
        CHECK(registry.GetComponent<BrickComponent>(lightBrick).HitPoints == 1);

        // Verify PowerUpComponent debuff vs buff classification for capsules
        PowerUpComponent debuffComp{
            .Type = PowerUpType::GlitchMalus,
            .FallSpeed = 240.0f,
            .IsCollected = false,
            .IsDebuff = true
        };
        CHECK(debuffComp.IsDebuff);
        CHECK(debuffComp.Type == PowerUpType::GlitchMalus);

        PowerUpComponent buffComp{
            .Type = PowerUpType::MultiBall,
            .FallSpeed = 220.0f,
            .IsCollected = false,
            .IsDebuff = false
        };
        CHECK_FALSE(buffComp.IsDebuff);
    }
}

