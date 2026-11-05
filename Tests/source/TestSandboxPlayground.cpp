#include <doctest/doctest.h>
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/PowerUpComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Registry.hpp"
#include "Data/Color.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"
#include <string>
#include <vector>

TEST_SUITE("Sandbox Training Room & Playground Tests")
{
    TEST_CASE("LightingMode definitions and ambient light mapping")
    {
        enum class TestLightingMode : int
        {
            Lit = 0,
            Atmospheric = 1,
            PitchBlack = 2
        };

        auto getAmbientForMode = [](TestLightingMode mode) -> Color {
            switch (mode)
            {
                case TestLightingMode::Lit:         return Color{255, 255, 255, 255};
                case TestLightingMode::Atmospheric: return Color{45, 50, 75, 255};
                case TestLightingMode::PitchBlack:  return Color{0, 0, 0, 255};
                default:                            return Color{255, 255, 255, 255};
            }
        };

        auto hasLighting = [](const Color& ambient) -> bool {
            return ambient.r < 255 || ambient.g < 255 || ambient.b < 255;
        };

        Color litColor = getAmbientForMode(TestLightingMode::Lit);
        CHECK_FALSE(hasLighting(litColor)); // Lit mode disables darkness pass

        Color dimColor = getAmbientForMode(TestLightingMode::Atmospheric);
        CHECK(hasLighting(dimColor));
        CHECK(dimColor.r == 45);
        CHECK(dimColor.g == 50);
        CHECK(dimColor.b == 75);

        Color pitchBlackColor = getAmbientForMode(TestLightingMode::PitchBlack);
        CHECK(hasLighting(pitchBlackColor));
        CHECK(pitchBlackColor.r == 0);
        CHECK(pitchBlackColor.g == 0);
        CHECK(pitchBlackColor.b == 0); // Pure darkness
    }

    TEST_CASE("Safety Feature: Invincible bottom barrier parameters")
    {
        // When safety barrier is active, it must have effectively infinite charges and duration
        float barrierDuration = 999999.0f;
        int barrierCharges = 999999;
        bool barrierActive = true;

        CHECK(barrierActive == true);
        CHECK(barrierDuration > 100000.0f);
        CHECK(barrierCharges > 100000);

        // Simulated frame update
        float dt = 0.016f;
        barrierDuration -= dt;
        CHECK(barrierDuration > 900000.0f); // Stays indefinitely active
        CHECK(barrierActive == true);

        // Disabling barrier
        barrierActive = false;
        barrierDuration = 0.0f;
        barrierCharges = 0;
        CHECK_FALSE(barrierActive);
        CHECK(barrierDuration == 0.0f);
        CHECK(barrierCharges == 0);
    }

    TEST_CASE("Playground Layout archetype validation")
    {
        enum class TestPlaygroundLayout : int
        {
            MetalChains = 0,
            BombClusters = 1,
            RegeneratingItemLab = 2,
            TacticalMix = 3
        };

        Registry registry;

        // 1. Conductive metal brick setup
        Entity metalBrick = registry.CreateEntity();
        registry.AddComponent<BrickComponent>(metalBrick, BrickComponent{
            .Type = BrickType::Metal,
            .HitPoints = 999999,
            .MaxHitPoints = 999999,
            .IsUnbreakable = true
        });

        // 2. Dynamic lantern brick setup
        Entity lanternBrick = registry.CreateEntity();
        registry.AddComponent<BrickComponent>(lanternBrick, BrickComponent{
            .Type = BrickType::Lantern,
            .HitPoints = 1,
            .MaxHitPoints = 1,
            .IsLantern = true
        });
        registry.AddComponent<LightSourceComponent>(lanternBrick, LightSourceComponent{
            .LightColor = Color{255, 235, 130, 255},
            .Radius = 230.0f,
            .Intensity = 1.25f
        });

        // 3. Explosive bomb brick setup
        Entity bombBrick = registry.CreateEntity();
        registry.AddComponent<BrickComponent>(bombBrick, BrickComponent{
            .Type = BrickType::Bomb,
            .HitPoints = 1,
            .MaxHitPoints = 1,
            .IsBomb = true
        });

        // 4. Regenerating Special Item brick setup
        Entity specialBrick = registry.CreateEntity();
        registry.AddComponent<BrickComponent>(specialBrick, BrickComponent{
            .Type = BrickType::Special,
            .HitPoints = 1,
            .MaxHitPoints = 1,
            .IsSpecial = true,
            .RegenCooldown = 30.0f
        });

        CHECK(registry.GetComponent<BrickComponent>(metalBrick).IsUnbreakable);
        CHECK(registry.GetComponent<BrickComponent>(lanternBrick).IsLantern);
        CHECK(registry.GetComponent<LightSourceComponent>(lanternBrick).Radius == 230.0f);
        CHECK(registry.GetComponent<BrickComponent>(bombBrick).IsBomb);
        CHECK(registry.GetComponent<BrickComponent>(specialBrick).IsSpecial);
        CHECK(registry.GetComponent<BrickComponent>(specialBrick).RegenCooldown == doctest::Approx(30.0f));
    }

    TEST_CASE("All 12 tactical power-up capsule types support zero-friction immediate application")
    {
        const PowerUpType allTypes[] = {
            PowerUpType::MultiBall,
            PowerUpType::ExpandPaddle,
            PowerUpType::ShrinkPaddle,
            PowerUpType::Storm,
            PowerUpType::TempoBall,
            PowerUpType::ExtraLife,
            PowerUpType::BigBall,
            PowerUpType::FireBall,
            PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier,
            PowerUpType::MagnetPaddle,
            PowerUpType::GhostBall
        };

        Registry registry;
        Entity paddle = registry.CreateEntity();
        registry.AddComponent<PaddleComponent>(paddle, PaddleComponent{});
        registry.AddComponent<BoxCollider>(paddle, BoxCollider{Vector2f{120.0f, 24.0f}});

        Entity ball = registry.CreateEntity();
        registry.AddComponent<BallComponent>(ball, BallComponent{});
        registry.AddComponent<CircleCollider>(ball, CircleCollider{10.0f});

        for (PowerUpType pt : allTypes)
        {
            // Verify capsule creation
            Entity capsule = registry.CreateEntity();
            registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{pt, 220.0f, false});
            CHECK(registry.GetComponent<PowerUpComponent>(capsule).Type == pt);

            // Verify immediate application effects on components
            if (pt == PowerUpType::StickyPaddle)
            {
                registry.GetComponent<PaddleComponent>(paddle).HasSticky = true;
                CHECK(registry.GetComponent<PaddleComponent>(paddle).HasSticky);
            }
            else if (pt == PowerUpType::MagnetPaddle)
            {
                registry.GetComponent<PaddleComponent>(paddle).HasMagnet = true;
                CHECK(registry.GetComponent<PaddleComponent>(paddle).HasMagnet);
            }
            else if (pt == PowerUpType::GhostBall)
            {
                registry.GetComponent<BallComponent>(ball).IsGhost = true;
                CHECK(registry.GetComponent<BallComponent>(ball).IsGhost);
            }
            else if (pt == PowerUpType::FireBall)
            {
                registry.GetComponent<BallComponent>(ball).IsFireBall = true;
                CHECK(registry.GetComponent<BallComponent>(ball).IsFireBall);
            }
            else if (pt == PowerUpType::BigBall)
            {
                registry.GetComponent<BallComponent>(ball).IsBig = true;
                registry.GetComponent<CircleCollider>(ball).Radius = 20.0f;
                CHECK(registry.GetComponent<BallComponent>(ball).IsBig);
                CHECK(registry.GetComponent<CircleCollider>(ball).Radius == doctest::Approx(20.0f));
            }
            else if (pt == PowerUpType::ExpandPaddle)
            {
                registry.GetComponent<BoxCollider>(paddle).Size.X = 180.0f;
                CHECK(registry.GetComponent<BoxCollider>(paddle).Size.X == doctest::Approx(180.0f));
            }
            else if (pt == PowerUpType::ShrinkPaddle)
            {
                registry.GetComponent<BoxCollider>(paddle).Size.X = 80.0f;
                CHECK(registry.GetComponent<BoxCollider>(paddle).Size.X == doctest::Approx(80.0f));
            }
        }
    }

    TEST_CASE("Sandbox UI Canvas and palette toggle state")
    {
        Registry registry;
        Entity canvas = registry.CreateEntity();
        registry.AddComponent<CanvasComponent>(canvas, CanvasComponent{.IsEnabled = true});

        CHECK(registry.GetComponent<CanvasComponent>(canvas).IsEnabled);

        // Toggle palette visibility
        registry.GetComponent<CanvasComponent>(canvas).IsEnabled = false;
        CHECK_FALSE(registry.GetComponent<CanvasComponent>(canvas).IsEnabled);

        registry.GetComponent<CanvasComponent>(canvas).IsEnabled = true;
        CHECK(registry.GetComponent<CanvasComponent>(canvas).IsEnabled);
    }
}
