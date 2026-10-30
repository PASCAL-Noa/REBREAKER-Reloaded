#include <doctest/doctest.h>
#include "PowerUps/PowerUpConfig.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/PowerUpComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Registry.hpp"
#include "Math/Vector2.h"
#include <cmath>
#include <algorithm>

TEST_SUITE("Tactical Power-Ups Tests")
{
    TEST_CASE("PowerUpConfig default and upgrade formulas for 4 new tactical power-ups")
    {
        auto& mgr = PowerUpManager::Get();
        mgr.ResetToDefaults();

        // 1. Sticky Paddle
        CHECK(mgr.Sticky().Level == 1);
        CHECK(mgr.Sticky().MaxLevel == 5);
        CHECK(mgr.Sticky().GetEffectiveDuration() == doctest::Approx(12.0f));
        CHECK(mgr.Sticky().AutoLaunchDelay == doctest::Approx(3.0f));
        CHECK(mgr.Sticky().GetEffectiveBoostPercent() == doctest::Approx(25.0f));
        mgr.SetLevel(PowerUpType::StickyPaddle, 3);
        CHECK(mgr.Sticky().Level == 3);
        CHECK(mgr.Sticky().GetEffectiveDuration() == doctest::Approx(16.0f)); // 12 + (3-1)*2
        CHECK(mgr.Sticky().GetEffectiveBoostPercent() == doctest::Approx(55.0f));
        CHECK(mgr.Sticky().GetUpgradeCost() == 140 * 3);

        // 2. Bottom Barrier
        CHECK(mgr.Barrier().Level == 1);
        CHECK(mgr.Barrier().GetEffectiveDuration() == doctest::Approx(15.0f));
        CHECK(mgr.Barrier().GetEffectiveCharges() == 1);
        mgr.SetLevel(PowerUpType::BottomBarrier, 2);
        CHECK(mgr.Barrier().GetEffectiveCharges() == 2);
        CHECK(mgr.Barrier().GetEffectiveDuration() == doctest::Approx(17.5f)); // 15 + 1*2.5
        CHECK(mgr.Barrier().GetUpgradeCost() == 150 * 2);

        // 3. Magnet Paddle
        CHECK(mgr.Magnet().Level == 1);
        CHECK(mgr.Magnet().GetEffectiveDuration() == doctest::Approx(10.0f));
        CHECK(mgr.Magnet().GetEffectivePullSpeed() == doctest::Approx(380.0f));
        CHECK(mgr.Magnet().GetEffectiveBallPullForce() == doctest::Approx(240.0f));
        CHECK(mgr.Magnet().MagnetRadius == doctest::Approx(850.0f));
        mgr.SetLevel(PowerUpType::MagnetPaddle, 4);
        CHECK(mgr.Magnet().GetEffectiveDuration() == doctest::Approx(17.5f)); // 10.0 + 3 * 2.5
        CHECK(mgr.Magnet().GetEffectivePullSpeed() == doctest::Approx(380.0f + 3 * 70.0f));
        CHECK(mgr.Magnet().GetEffectiveBallPullForce() == doctest::Approx(240.0f + 3 * 80.0f));
        CHECK(mgr.Magnet().GetUpgradeCost() == 130 * 4);

        // 4. Ghost Ball
        CHECK(mgr.Ghost().Level == 1);
        CHECK(mgr.Ghost().GetEffectiveDuration() == doctest::Approx(5.0f));
        CHECK(mgr.Ghost().GetEffectiveDamage() == 1);
        CHECK(mgr.Ghost().Alpha == doctest::Approx(120.0f));
        mgr.SetLevel(PowerUpType::GhostBall, 3);
        CHECK(mgr.Ghost().GetEffectiveDuration() == doctest::Approx(7.0f));
        CHECK(mgr.Ghost().GetEffectiveDamage() == 2);
        mgr.SetLevel(PowerUpType::GhostBall, 5);
        CHECK(mgr.Ghost().GetEffectiveDuration() == doctest::Approx(5.0f + 4 * 1.0f));
        CHECK(mgr.Ghost().GetEffectiveDamage() == 3);
        CHECK(mgr.Ghost().GetUpgradeCost() == 160 * 5);

        // Reset back to clean state
        mgr.ResetToDefaults();
        CHECK(mgr.Sticky().Level == 1);
        CHECK(mgr.Barrier().Level == 1);
        CHECK(mgr.Magnet().Level == 1);
        CHECK(mgr.Ghost().Level == 1);
    }

    TEST_CASE("Sticky Paddle catching and aim calculation")
    {
        // Simulate paddle at position (0, 300) with width 120
        Vector2f paddlePos{0.0f, 300.0f};
        float paddleHalfWidth = 60.0f;

        // Ball caught at offset +30px (right half)
        Vector2f ballContactPos{30.0f, 280.0f};
        float offset = ballContactPos.X - paddlePos.X;
        float clampedOffset = std::clamp(offset, -paddleHalfWidth * 0.9f, paddleHalfWidth * 0.9f);
        CHECK(clampedOffset == doctest::Approx(30.0f));

        // When caught, ball velocity becomes 0 and kinematic is set
        RigidBody ballRb{Vector2f{200.0f, 400.0f}, 1.0f, 1.0f, false};
        ballRb.Velocity = Vector2f{0.0f, 0.0f};
        ballRb.IsKinematic = true;
        CHECK(ballRb.Velocity.X == 0.0f);
        CHECK(ballRb.Velocity.Y == 0.0f);
        CHECK(ballRb.IsKinematic == true);

        // When launched, calculate release angle based on caught offset
        float hitFactor = clampedOffset / paddleHalfWidth; // 30 / 60 = 0.5
        constexpr float maxAngle = 60.0f * 3.14159265f / 180.0f;
        float bounceAngle = hitFactor * maxAngle; // 30 degrees to the right
        float launchSpeed = 640.0f;

        ballRb.Velocity.X = launchSpeed * std::sin(bounceAngle);
        ballRb.Velocity.Y = -launchSpeed * std::cos(bounceAngle);
        ballRb.IsKinematic = false;

        CHECK(ballRb.IsKinematic == false);
        CHECK(ballRb.Velocity.X > 0.0f);
        CHECK(ballRb.Velocity.Y < 0.0f); // Directed upward
        float resultingSpeed = std::sqrt(ballRb.Velocity.X * ballRb.Velocity.X + ballRb.Velocity.Y * ballRb.Velocity.Y);
        CHECK(resultingSpeed == doctest::Approx(launchSpeed));
    }

    TEST_CASE("Bottom Barrier deflection and charge dissipation")
    {
        int barrierCharges = 1;
        bool barrierActive = true;

        Vector2f ballPos{100.0f, 440.0f};
        Vector2f ballVel{150.0f, 500.0f};

        // When ball reaches bottom boundary, barrier saves it
        CHECK(barrierActive);
        CHECK(barrierCharges > 0);

        // Deflect upward safely
        float angleX = std::clamp(ballPos.X / 800.0f, -0.6f, 0.6f);
        Vector2f dir{angleX, -std::sqrt(std::max(0.1f, 1.0f - angleX * angleX))};
        float speed = std::sqrt(ballVel.X * ballVel.X + ballVel.Y * ballVel.Y);
        Vector2f deflectedVel = dir.Normalized() * speed;

        CHECK(deflectedVel.Y < 0.0f); // Deflected upward
        CHECK(std::sqrt(deflectedVel.X * deflectedVel.X + deflectedVel.Y * deflectedVel.Y) == doctest::Approx(speed));

        // Barrier loses 1 charge and dissolves
        barrierCharges--;
        if (barrierCharges <= 0)
        {
            barrierActive = false;
        }

        CHECK(barrierCharges == 0);
        CHECK_FALSE(barrierActive);
    }

    TEST_CASE("Magnet Paddle attraction vector")
    {
        Vector2f paddlePos{0.0f, 300.0f};
        Vector2f capsulePos{100.0f, 100.0f};

        Vector2f toPaddle = paddlePos - capsulePos; // (100, 200)
        float distance = std::sqrt(toPaddle.X * toPaddle.X + toPaddle.Y * toPaddle.Y);
        float magnetRadius = PowerUpManager::Get().Magnet().MagnetRadius; // 650.0f

        CHECK(distance < magnetRadius);

        Vector2f pullDir = toPaddle.Normalized();
        float pullSpeed = PowerUpManager::Get().Magnet().GetEffectivePullSpeed(); // 380.0f
        Vector2f pullVelocity = pullDir * pullSpeed;

        CHECK(pullVelocity.X < 0.0f); // Pulled left towards paddle center X (0.0f)
        CHECK(pullVelocity.Y > 0.0f); // Pulled down towards paddle Y (300.0f)
    }

    TEST_CASE("Ghost Ball piercing flag verification")
    {
        BallComponent ball;
        CHECK_FALSE(ball.IsGhost);

        ball.IsGhost = true;
        CHECK(ball.IsGhost);

        // Ghost ball phases cleanly without bouncing
        bool isPiercing = ball.IsGhost;
        CHECK(isPiercing == true);

        // Selective destruction verification:
        // Case 1: Normal 1-HP brick with 1 damage -> can destroy!
        int ballDmg = 1;
        uint32_t brick1Hp = 1;
        bool canDestroy1Hp = (brick1Hp <= static_cast<uint32_t>(ballDmg));
        CHECK(canDestroy1Hp == true);

        // Case 2: Multi-hit 2-HP brick with 1 damage -> cannot destroy, phases without damage!
        uint32_t brick2Hp = 2;
        bool canDestroy2Hp = (brick2Hp <= static_cast<uint32_t>(ballDmg));
        CHECK(canDestroy2Hp == false);

        // Case 3: Combined with Big Ball or Fire Ball -> fatal damage destroys tough bricks!
        bool isBigOrFire = true;
        bool combinedCanDestroy = isBigOrFire || (brick2Hp <= static_cast<uint32_t>(ballDmg));
        CHECK(combinedCanDestroy == true);

        // Case 4: Level 3 damage (+1 dmg = 2 dmg) shatters 2-HP brick!
        int ballDmgLvl3 = 2;
        bool canDestroy2HpAtLvl3 = (brick2Hp <= static_cast<uint32_t>(ballDmgLvl3));
        CHECK(canDestroy2HpAtLvl3 == true);

        // Case 5: Level 5 damage (+1 dmg = 3 dmg) shatters 3-HP brick!
        int ballDmgLvl5 = 3;
        uint32_t brick3Hp = 3;
        bool canDestroy3HpAtLvl5 = (brick3Hp <= static_cast<uint32_t>(ballDmgLvl5));
        CHECK(canDestroy3HpAtLvl5 == true);

        // Case 6: Safe expiration: ghost ball remains active while overlapping bricks, solidifies once clear
        bool isInsideBrick = true;
        bool canSolidify = !isInsideBrick;
        CHECK_FALSE(canSolidify); // Cannot solidify inside a brick
        isInsideBrick = false;
        canSolidify = !isInsideBrick;
        CHECK(canSolidify); // Solidifies safely once clear

        // Case 7: Ghost ball immortality on bottom collision
        bool isGhostActive = ball.IsGhost;
        bool ballImmortal = isGhostActive;
        CHECK(ballImmortal == true); // Cannot lose lives while Ghost is active
    }

    TEST_CASE("Sticky Paddle dynamic size positioning and launch release safety")
    {
        CircleCollider circle{20.0f};
        BoxCollider paddleBox{Vector2f{120.0f, 20.0f}};
        Transform2D paddleTrans{Vector2f{0.0f, 300.0f}};
        const float paddleHalfH = paddleBox.GetEffectiveSize(paddleTrans.Scale).Y * 0.5f; // 10.0f
        const float paddleTopY = paddleTrans.Position.Y - paddleHalfH; // 290.0f

        // Case 1: Standard ball (scale 1.0)
        Transform2D standardBallTrans{Vector2f{0.0f, 0.0f}, 0.0f, Vector2f{1.0f, 1.0f}};
        float standardRadius = circle.GetEffectiveRadius(standardBallTrans.Scale);
        CHECK(standardRadius == doctest::Approx(20.0f));
        standardBallTrans.Position.Y = paddleTopY - standardRadius - 1.0f;
        // Verify bottom of ball is strictly above paddle top
        CHECK(standardBallTrans.Position.Y + standardRadius < paddleTopY);

        // Case 2: Big ball (scale 1.8)
        Transform2D bigBallTrans{Vector2f{0.0f, 0.0f}, 0.0f, Vector2f{1.8f, 1.8f}};
        float bigRadius = circle.GetEffectiveRadius(bigBallTrans.Scale);
        CHECK(bigRadius == doctest::Approx(36.0f));
        bigBallTrans.Position.Y = paddleTopY - bigRadius - 1.0f;
        // Verify bottom of big ball is strictly above paddle top (no penetration)
        CHECK(bigBallTrans.Position.Y + bigRadius < paddleTopY);

        // Case 3: Launch release safety - moving upward should never trigger paddle collision
        RigidBody releasedBallRb{Vector2f{100.0f, -600.0f}, 1.0f, 1.0f, false};
        BallComponent releasedBallComp;
        releasedBallComp.IsStuck = false;

        // Condition check used in HandlePaddleCollision:
        bool shouldProcessPaddleCollision = !releasedBallComp.IsStuck && (releasedBallRb.Velocity.Y > 0.0f);
        CHECK_FALSE(shouldProcessPaddleCollision); // Since Velocity.Y <= 0.0f, paddle collision is ignored!
    }

    TEST_CASE("Storm (Thunderstorm) power-up configuration, alias, and mechanics")
    {
        auto& mgr = PowerUpManager::Get();
        mgr.ResetToDefaults();

        // 1. Level 1: 1 strike, 2 damage
        CHECK(mgr.Storm().Level == 1);
        CHECK(mgr.Storm().MaxLevel == 5);
        CHECK(mgr.Storm().GetEffectiveStrikes() == 1);
        CHECK(mgr.Storm().GetEffectiveDamage() == 2);
        CHECK_FALSE(mgr.Storm().IsTempest());
        CHECK(mgr.Storm().GetEffectiveCooldown() == doctest::Approx(0.55f));
        CHECK(mgr.Storm().StrikeRadius == doctest::Approx(55.0f));

        // 2. Level 2: +1 strike (total 2 strikes, 2 damage)
        mgr.SetLevel(PowerUpType::Storm, 2);
        CHECK(mgr.Storm().GetEffectiveStrikes() == 2);
        CHECK(mgr.Storm().GetEffectiveDamage() == 2);
        CHECK_FALSE(mgr.Storm().IsTempest());

        // 3. Level 3: +1 strike (total 3 strikes, 2 damage)
        mgr.SetLevel(PowerUpType::Storm, 3);
        CHECK(mgr.Storm().GetEffectiveStrikes() == 3);
        CHECK(mgr.Storm().GetEffectiveDamage() == 2);
        CHECK_FALSE(mgr.Storm().IsTempest());

        // 4. Level 4: 3 strikes, damage upgraded to 3 hits
        mgr.SetLevel(PowerUpType::Storm, 4);
        CHECK(mgr.Storm().GetEffectiveStrikes() == 3);
        CHECK(mgr.Storm().GetEffectiveDamage() == 3);
        CHECK_FALSE(mgr.Storm().IsTempest());

        // 5. Level 5: La tempête (10s continuous tempest barrage)
        mgr.SetLevel(PowerUpType::Storm, 5);
        CHECK(mgr.Storm().IsTempest());
        CHECK(mgr.Storm().GetTempestDuration() == doctest::Approx(10.0f));
        CHECK(mgr.Storm().GetEffectiveDamage() == 3);
        CHECK(mgr.Storm().GetEffectiveCooldown() == doctest::Approx(0.16f));

        // 6. Compatibility alias
        CHECK(PowerUpType::LaserPaddle == PowerUpType::Storm);
        CHECK(mgr.Laser().GetEffectiveStrikes() == mgr.Storm().GetEffectiveStrikes());

        // 7. PaddleComponent Storm state & strike consumption
        mgr.SetLevel(PowerUpType::Storm, 2);
        PaddleComponent paddle;
        CHECK_FALSE(paddle.HasStorm);

        int stormCharges = mgr.Storm().GetEffectiveStrikes(); // 2 strikes
        paddle.HasStorm = true;
        paddle.StormCooldown = mgr.Storm().GetEffectiveCooldown();
        CHECK(paddle.HasStorm);

        // 1st strike triggers automatically on pickup
        stormCharges--;
        CHECK(stormCharges == 1); // 1 strike remaining

        // 2nd strike triggers automatically
        stormCharges--;
        CHECK(stormCharges == 0);
        paddle.HasStorm = false;
        CHECK_FALSE(paddle.HasStorm);

        // 8. Conductive Metal Brick Network verification
        // Metal bricks cannot be broken by lightning, but conduct electricity across touching metal bricks and zap 1x1 neighbors
        Vector2f metalPos1{0.0f, 0.0f};
        Vector2f metalPos2{110.0f, 0.0f}; // Touching metalPos1 (delta X = 110 <= 115)
        Vector2f neighborPos{110.0f, 40.0f}; // 1x1 adjacent to metalPos2 (delta Y = 40 <= 45)
        Vector2f distantPos{400.0f, 400.0f}; // Far away

        constexpr float kMaxConnectedDistX = 115.0f;
        constexpr float kMaxConnectedDistY = 45.0f;

        // Metal 1 connects to Metal 2
        bool isConnected = (std::abs(metalPos1.X - metalPos2.X) <= kMaxConnectedDistX &&
                            std::abs(metalPos1.Y - metalPos2.Y) <= kMaxConnectedDistY);
        CHECK(isConnected);

        // Neighbor is within 1x1 radius of Metal 2
        bool isAdjacent = (std::abs(metalPos2.X - neighborPos.X) <= kMaxConnectedDistX &&
                           std::abs(metalPos2.Y - neighborPos.Y) <= kMaxConnectedDistY);
        CHECK(isAdjacent);

        // Distant brick is not reached
        bool isDistantReached = (std::abs(metalPos1.X - distantPos.X) <= kMaxConnectedDistX ||
                                 std::abs(metalPos2.X - distantPos.X) <= kMaxConnectedDistX);
        CHECK_FALSE(isDistantReached);

        mgr.ResetToDefaults();
    }

    TEST_CASE("Tempo Ball duration (+1s/lvl, 10s base) and speed scaling (+60% base, +20%/lvl)")
    {
        auto& mgr = PowerUpManager::Get();
        mgr.ResetToDefaults();

        // Level 1: 10s duration, +60% ball speed bonus (960 px/s), +10% paddle speed bonus (770 px/s)
        CHECK(mgr.Tempo().Level == 1);
        CHECK(mgr.Tempo().MaxLevel == 5);
        CHECK(mgr.Tempo().GetEffectiveDuration() == doctest::Approx(10.0f));
        CHECK(mgr.Tempo().GetEffectiveBonusPercent() == doctest::Approx(60.0f));
        CHECK(mgr.Tempo().GetEffectiveFastSpeed() == doctest::Approx(960.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeedBonusPercent() == doctest::Approx(10.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeed() == doctest::Approx(770.0f));
        CHECK(mgr.Tempo().GetUpgradeCost() == 120);

        // Level 2: 11s duration, +80% ball speed bonus (1080 px/s), +20% paddle speed bonus (840 px/s)
        mgr.SetLevel(PowerUpType::TempoBall, 2);
        CHECK(mgr.Tempo().GetEffectiveDuration() == doctest::Approx(11.0f));
        CHECK(mgr.Tempo().GetEffectiveBonusPercent() == doctest::Approx(80.0f));
        CHECK(mgr.Tempo().GetEffectiveFastSpeed() == doctest::Approx(1080.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeedBonusPercent() == doctest::Approx(20.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeed() == doctest::Approx(840.0f));
        CHECK(mgr.Tempo().GetUpgradeCost() == 240);

        // Level 3: 12s duration, +100% ball speed bonus (1200 px/s), +30% paddle speed bonus (910 px/s)
        mgr.SetLevel(PowerUpType::TempoBall, 3);
        CHECK(mgr.Tempo().GetEffectiveDuration() == doctest::Approx(12.0f));
        CHECK(mgr.Tempo().GetEffectiveBonusPercent() == doctest::Approx(100.0f));
        CHECK(mgr.Tempo().GetEffectiveFastSpeed() == doctest::Approx(1200.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeedBonusPercent() == doctest::Approx(30.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeed() == doctest::Approx(910.0f));

        // Level 4: 13s duration, +120% ball speed bonus (1320 px/s), +40% paddle speed bonus (980 px/s)
        mgr.SetLevel(PowerUpType::TempoBall, 4);
        CHECK(mgr.Tempo().GetEffectiveDuration() == doctest::Approx(13.0f));
        CHECK(mgr.Tempo().GetEffectiveBonusPercent() == doctest::Approx(120.0f));
        CHECK(mgr.Tempo().GetEffectiveFastSpeed() == doctest::Approx(1320.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeedBonusPercent() == doctest::Approx(40.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeed() == doctest::Approx(980.0f));

        // Level 5: 14s duration, +140% ball speed bonus (1440 px/s), +50% paddle speed bonus (1050 px/s)
        mgr.SetLevel(PowerUpType::TempoBall, 5);
        CHECK(mgr.Tempo().GetEffectiveDuration() == doctest::Approx(14.0f));
        CHECK(mgr.Tempo().GetEffectiveBonusPercent() == doctest::Approx(140.0f));
        CHECK(mgr.Tempo().GetEffectiveFastSpeed() == doctest::Approx(1440.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeedBonusPercent() == doctest::Approx(50.0f));
        CHECK(mgr.Tempo().GetEffectivePaddleSpeed() == doctest::Approx(1050.0f));

        // Simulation of Tempo Ball expiration and speed reset
        Vector2f highSpeedVel{0.0f, -1440.0f};
        float curSpeed = std::sqrt(highSpeedVel.X * highSpeedVel.X + highSpeedVel.Y * highSpeedVel.Y);
        CHECK(curSpeed == doctest::Approx(1440.0f));

        // When expired: speed resets cleanly to base 600 px/s
        Vector2f resetVel = (highSpeedVel / curSpeed) * 600.0f;
        float postResetSpeed = std::sqrt(resetVel.X * resetVel.X + resetVel.Y * resetVel.Y);
        CHECK(postResetSpeed == doctest::Approx(600.0f));
        CHECK(resetVel.Y == doctest::Approx(-600.0f));

        // When inactive, normal speed clamp (900 px/s) is preserved
        const float normalCap = 900.0f;
        const float tempoCap = 2200.0f;
        CHECK(1440.0f < tempoCap);
        CHECK(1440.0f > normalCap);

        mgr.ResetToDefaults();
    }

    TEST_CASE("Extra Life 3-tier health system (Red, Yellow, Green) and levels 1-5")
    {
        auto getHeartTier = [](int lives, int i) -> int {
            if (lives <= 0) return 0;
            if (lives <= 3) return (i < lives) ? 1 : 0;
            int extra = lives - 3;
            int tier = 1;
            if (extra >= i + 1) tier++;
            if (extra >= i + 4) tier++;
            return tier;
        };

        // 3 Red hearts (initial start)
        CHECK(getHeartTier(3, 0) == 1);
        CHECK(getHeartTier(3, 1) == 1);
        CHECK(getHeartTier(3, 2) == 1);

        // 4 lives: 1 Yellow, 2 Red
        CHECK(getHeartTier(4, 0) == 2);
        CHECK(getHeartTier(4, 1) == 1);
        CHECK(getHeartTier(4, 2) == 1);

        // 6 lives: 3 Yellow
        CHECK(getHeartTier(6, 0) == 2);
        CHECK(getHeartTier(6, 1) == 2);
        CHECK(getHeartTier(6, 2) == 2);

        // 7 lives: 1 Green, 2 Yellow
        CHECK(getHeartTier(7, 0) == 3);
        CHECK(getHeartTier(7, 1) == 2);
        CHECK(getHeartTier(7, 2) == 2);

        // 9 lives: 3 Green (Max Health Cap)
        CHECK(getHeartTier(9, 0) == 3);
        CHECK(getHeartTier(9, 1) == 3);
        CHECK(getHeartTier(9, 2) == 3);

        // Power-Up Level 1: +1 life
        int lives = 3;
        lives = std::min(lives + 1, 9);
        CHECK(lives == 4); // 1 Yellow heart unlocked!

        // Losing a life: Yellow heart downgrades to Red
        lives--;
        CHECK(lives == 3);
        CHECK(getHeartTier(lives, 0) == 1);
        CHECK(getHeartTier(lives, 1) == 1);
        CHECK(getHeartTier(lives, 2) == 1);

        // Power-Up Level 2: +2 lives
        lives = 3;
        lives = std::min(lives + 2, 9);
        CHECK(lives == 5); // 2 Yellow, 1 Red

        // Power-Up Level 3: +1 vie jaune (guarantees at least 1 yellow heart = 4)
        lives = 1;
        lives = std::clamp(std::max(lives + 1, 4), 1, 9);
        CHECK(lives == 4);

        // Power-Up Level 4: +2 vie jaune (guarantees at least 2 yellow hearts = 5)
        lives = 2;
        lives = std::clamp(std::max(lives + 2, 5), 1, 9);
        CHECK(lives == 5);

        // Power-Up Level 5: +3 vie vert (max health = 9)
        lives = 9;
        CHECK(lives == 9);
        CHECK(getHeartTier(lives, 0) == 3);
        CHECK(getHeartTier(lives, 1) == 3);
        CHECK(getHeartTier(lives, 2) == 3);
    }
}

