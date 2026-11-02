#include <doctest/doctest.h>
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Registry.hpp"
#include "ECS/Systems/PhysicsSystem.h"
#include "Events/EventDispatcher.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"
#include <algorithm>
#include <cmath>

TEST_SUITE("Stage Progression and Ultimate Abilities Tests")
{
    TEST_CASE("BrickComponent regenerating fields default state")
    {
        BrickComponent brick;
        CHECK(brick.IsRegenerating == false);
        CHECK(brick.RegenTimer == doctest::Approx(0.0f));
        CHECK(brick.RegenCooldown == doctest::Approx(30.0f));
    }

    TEST_CASE("PhysicsSystem ignores collisions with regenerating bricks")
    {
        Registry registry;
        EventDispatcher dispatcher;
        PhysicsSystem physicsSystem(registry, dispatcher);

        // Create a ball moving right toward the brick
        Entity ball = registry.CreateEntity();
        registry.AddComponent<Transform2D>(ball, Transform2D{Vector2f{0.0f, 0.0f}});
        registry.AddComponent<CircleCollider>(ball, CircleCollider{10.0f});
        registry.AddComponent<RigidBody>(ball, RigidBody{Vector2f{100.0f, 0.0f}, false, false});

        // Create a brick overlapping the ball's position
        Entity brick = registry.CreateEntity();
        registry.AddComponent<Transform2D>(brick, Transform2D{Vector2f{5.0f, 0.0f}});
        registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{20.0f, 20.0f}});
        auto& brickComp = registry.AddComponent<BrickComponent>(brick, BrickComponent{});
        brickComp.Type = BrickType::Special;
        brickComp.IsSpecial = true;
        brickComp.IsRegenerating = true; // Actively recharging!

        // Update physics: should ignore collision because brick is regenerating
        physicsSystem.OnUpdate(0.016f);

        auto& boxCol = registry.GetComponent<BoxCollider>(brick);
        auto& circleCol = registry.GetComponent<CircleCollider>(ball);
        CHECK(boxCol.IsColliding == false);
        CHECK(circleCol.IsColliding == false);

        // When regeneration completes, collisions are restored
        brickComp.IsRegenerating = false;
        physicsSystem.OnUpdate(0.016f);
        CHECK(boxCol.IsColliding == true);
        CHECK(circleCol.IsColliding == true);
    }

    TEST_CASE("Regenerating Item Brick cooldown links to Workshop upgrades")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // Level 1: 30s base cooldown
        CHECK(pum.ItemRegen().Level == 1);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(30.0f));

        // Level 2: 26.5s
        pum.SetLevel(PowerUpType::ItemRegen, 2);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(26.5f));

        // Level 3: 23.0s
        pum.SetLevel(PowerUpType::ItemRegen, 3);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(23.0f));

        // Level 5: 16.0s
        pum.SetLevel(PowerUpType::ItemRegen, 5);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(16.0f));

        // Simulate Regeneration lifecycle
        BrickComponent brick;
        brick.Type = BrickType::Special;
        brick.IsSpecial = true;
        brick.MaxHitPoints = 1;
        brick.HitPoints = 0; // Destroyed
        brick.IsRegenerating = true;
        brick.RegenCooldown = pum.ItemRegen().GetEffectiveCooldown();
        brick.RegenTimer = brick.RegenCooldown;

        CHECK(brick.RegenCooldown == doctest::Approx(16.0f));
        CHECK(brick.RegenTimer == doctest::Approx(16.0f));

        // Advance timer by 10 seconds
        float dt = 10.0f;
        brick.RegenTimer -= dt;
        CHECK(brick.RegenTimer == doctest::Approx(6.0f));
        CHECK(brick.IsRegenerating == true);

        // Advance remaining 6 seconds -> restores
        dt = 6.0f;
        brick.RegenTimer -= dt;
        if (brick.RegenTimer <= 0.0f)
        {
            brick.RegenTimer = 0.0f;
            brick.IsRegenerating = false;
            brick.HitPoints = brick.MaxHitPoints;
        }

        CHECK(brick.IsRegenerating == false);
        CHECK(brick.HitPoints == 1);
        CHECK(brick.RegenTimer == doctest::Approx(0.0f));
    }

    TEST_CASE("ProfileManager Ultimate Ability Selection and Tiers")
    {
        auto& pm = ProfileManager::Get();
        pm.ResetToDefaults();

        // Default equipped ultimate is PlasmaOrb / NovaShockwave
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::PlasmaOrb);
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::PlasmaOrb) == "PLASMA ORB");
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::DiscoBall) == "DISCO BALL");
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::PowerUpRain) == "POWER-UP RAIN");

        // Backward compatible aliases
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::NovaShockwave) == "PLASMA ORB");
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::ThunderTempest) == "DISCO BALL");
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::TimeStasis) == "POWER-UP RAIN");

        // Change equipped ultimate
        pm.SetEquippedUltimate(UltimateAbilityType::DiscoBall);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::DiscoBall);

        pm.SetEquippedUltimate(UltimateAbilityType::PowerUpRain);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::PowerUpRain);

        // Levels and Upgrades
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::PlasmaOrb) == 1);
        CHECK(pm.GetUltimateMaxLevel(UltimateAbilityType::PlasmaOrb) == 5);
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::PlasmaOrb) == 250);

        pm.SetUltimateLevel(UltimateAbilityType::PlasmaOrb, 3);
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::PlasmaOrb) == 3);
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::PlasmaOrb) == 650);

        pm.SetUltimateLevel(UltimateAbilityType::PlasmaOrb, 5);
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::PlasmaOrb) == 0);

        // Verify tier strings contain expected gameplay parameters
        std::string plasmaLvl1 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::PlasmaOrb, 1);
        CHECK(plasmaLvl1.find("2 small explosive orbs") != std::string::npos);

        std::string plasmaLvl5 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::PlasmaOrb, 5);
        CHECK(plasmaLvl5.find("splits into 8 then 4 then 2") != std::string::npos);

        std::string discoLvl1 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::DiscoBall, 1);
        CHECK(discoLvl1.find("30% height") != std::string::npos);

        std::string discoLvl5 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::DiscoBall, 5);
        CHECK(discoLvl5.find("Mirror Ball") != std::string::npos);

        std::string rainLvl1 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::PowerUpRain, 1);
        CHECK(rainLvl1.find("5 to 12 power-ups") != std::string::npos);

        std::string rainLvl5 = ProfileManager::GetUltimateTierInfo(UltimateAbilityType::PowerUpRain, 5);
        CHECK(rainLvl5.find("25 to 60 power-ups") != std::string::npos);
    }

    TEST_CASE("Ultimate Meter charging, combo scaling, and activation simulation")
    {
        float ultimateCharge = 0.0f;
        const float maxCharge = 100.0f;
        bool ultimateReady = false;

        auto addCharge = [&](float amount)
        {
            if (ultimateReady) return;
            ultimateCharge = std::min(ultimateCharge + amount, maxCharge);
            if (ultimateCharge >= maxCharge)
            {
                ultimateCharge = maxCharge;
                ultimateReady = true;
            }
        };

        auto calcGain = [](bool destroyed, uint32_t combo)
        {
            float comboBonus = 1.0f;
            if (combo > 1)
            {
                comboBonus += static_cast<float>(std::min(combo, 10u) - 1) * 0.25f;
            }
            float baseGain = destroyed ? 2.0f : 0.6f;
            return baseGain * comboBonus;
        };

        // First hit before any combo is established (multiplier = 0): no underflow!
        float firstHitCharge = calcGain(false, 0);
        CHECK(firstHitCharge == doctest::Approx(0.6f));
        addCharge(firstHitCharge);
        CHECK(ultimateCharge == doctest::Approx(0.6f));
        CHECK(ultimateReady == false);

        // First brick destroyed without combo chain (multiplier = 1) -> 2.0f
        float firstDestroyCharge = calcGain(true, 1);
        CHECK(firstDestroyCharge == doctest::Approx(2.0f));
        addCharge(firstDestroyCharge);
        CHECK(ultimateCharge == doctest::Approx(2.6f));
        CHECK(ultimateReady == false);

        // Brick destroyed with 4x combo -> 2.0f * (1.0 + 3 * 0.25) = 2.0 * 1.75 = 3.5f
        float combo4Charge = calcGain(true, 4);
        CHECK(combo4Charge == doctest::Approx(3.5f));
        addCharge(combo4Charge);
        CHECK(ultimateCharge == doctest::Approx(6.1f));
        CHECK(ultimateReady == false);

        // Fills to 100%
        addCharge(98.0f);
        CHECK(ultimateCharge == doctest::Approx(100.0f));
        CHECK(ultimateReady == true);

        // Overcharging does not exceed 100%
        addCharge(50.0f);
        CHECK(ultimateCharge == doctest::Approx(100.0f));

        // Discharge / Activate Ultimate
        CHECK(ultimateReady == true);
        ultimateCharge = 0.0f;
        ultimateReady = false;
        CHECK(ultimateCharge == doctest::Approx(0.0f));
        CHECK(ultimateReady == false);

        // While Ultimate is actively running, no charge can be accumulated
        bool isUltimateActive = true;
        auto addChargeWithActiveCheck = [&](float amount)
        {
            if (ultimateReady || isUltimateActive) return;
            ultimateCharge = std::min(ultimateCharge + amount, maxCharge);
        };

        addChargeWithActiveCheck(calcGain(true, 5));
        addChargeWithActiveCheck(calcGain(true, 8));
        CHECK(ultimateCharge == doctest::Approx(0.0f));
        CHECK(ultimateReady == false);

        // Once the ultimate effect concludes, meter resumes filling normally
        isUltimateActive = false;
        addChargeWithActiveCheck(calcGain(true, 1));
        CHECK(ultimateCharge == doctest::Approx(2.0f));
        CHECK(ultimateReady == false);
    }

    TEST_CASE("Ultimate clear on level change and plasma trail lifecycle")
    {
        struct MockPlasmaTrailPoint
        {
            Vector2f Position;
            float    Lifetime = 0.22f;
            float    MaxLifetime = 0.22f;
        };

        struct MockPlasmaOrb
        {
            Vector2f Position{0.0f, 100.0f};
            Vector2f Velocity{0.0f, -500.0f};
            std::vector<MockPlasmaTrailPoint> Trail;
        };

        MockPlasmaOrb orb;
        orb.Trail.push_back({orb.Position, 0.22f, 0.22f});
        CHECK(orb.Trail.size() == 1);

        // Advance time: trail ages
        float dt = 0.10f;
        for (auto& pt : orb.Trail) pt.Lifetime -= dt;
        CHECK(orb.Trail[0].Lifetime == doctest::Approx(0.12f));

        // Age beyond lifetime -> pruned
        dt = 0.15f;
        for (auto& pt : orb.Trail) pt.Lifetime -= dt;
        std::erase_if(orb.Trail, [](const MockPlasmaTrailPoint& pt) { return pt.Lifetime <= 0.0f; });
        CHECK(orb.Trail.empty());

        // Level clear simulation: vectors are wiped
        std::vector<MockPlasmaOrb> activeOrbs;
        activeOrbs.push_back(orb);
        CHECK(activeOrbs.size() == 1);

        auto clearActiveUltimates = [&]() {
            activeOrbs.clear();
        };

        clearActiveUltimates();
        CHECK(activeOrbs.empty());
    }

    TEST_CASE("Plasma orb wall bounce and playfield containment")
    {
        Vector2f pos{-795.0f, 0.0f};
        Vector2f vel{-500.0f, 0.0f};
        float radius = 18.0f;

        const float minX = -800.0f + radius;
        const float maxX = 800.0f - radius;
        const float minY = -450.0f + radius;
        const float maxY = 450.0f - radius;

        // Left wall bounce
        if (pos.X < minX)
        {
            pos.X = minX;
            if (vel.X < 0.0f) vel.X = std::abs(vel.X);
        }
        CHECK(pos.X == doctest::Approx(-782.0f));
        CHECK(vel.X == doctest::Approx(500.0f));

        // Right wall bounce
        pos.X = 810.0f;
        vel.X = 400.0f;
        if (pos.X > maxX)
        {
            pos.X = maxX;
            if (vel.X > 0.0f) vel.X = -std::abs(vel.X);
        }
        CHECK(pos.X == doctest::Approx(782.0f));
        CHECK(vel.X == doctest::Approx(-400.0f));

        // Top wall bounce
        pos.Y = -460.0f;
        vel.Y = -600.0f;
        if (pos.Y < minY)
        {
            pos.Y = minY;
            if (vel.Y < 0.0f) vel.Y = std::abs(vel.Y);
        }
        CHECK(pos.Y == doctest::Approx(-432.0f));
        CHECK(vel.Y == doctest::Approx(600.0f));

        // Bottom bounce
        pos.Y = 460.0f;
        vel.Y = 600.0f;
        if (pos.Y > maxY)
        {
            pos.Y = maxY;
            if (vel.Y > 0.0f) vel.Y = -std::abs(vel.Y);
        }
        CHECK(pos.Y == doctest::Approx(432.0f));
        CHECK(vel.Y == doctest::Approx(-600.0f));
    }
}
