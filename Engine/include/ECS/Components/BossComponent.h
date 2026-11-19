#pragma once
#include "Math/Vector2.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

enum class BossPhase : uint8_t
{
    Intro = 0,
    Phase1 = 1,
    Phase2 = 2,
    Phase3 = 3,      // Enrage
    Defeated = 4
};

enum class BossAttackType : uint8_t
{
    None = 0,
    BulletHellVolley = 1,
    LaserBeamSweep = 2,
    BrickShieldDeploy = 3
};

struct BossWeakPoint
{
    Vector2f RelativeOffset{0.0f, 25.0f}; // Offset from boss center
    float    Radius = 30.0f;
    bool     IsExposed = true;
    float    DamageMultiplier = 2.5f;
    float    PulseTimer = 0.0f;
};

struct BossLaserBeam
{
    float StartX = 0.0f;
    float Width = 26.0f;
    float ChargeTimer = 0.0f;
    float ChargeDuration = 1.0f;
    float FireTimer = 0.0f;
    float FireDuration = 1.2f;
    bool  IsFiring = false;
    bool  IsCharging = false;
    float DamagePerSecond = 45.0f;
};

struct BossComponent
{
    std::string   Name = "NEXUS ARCHITECT // OMEGA-01";
    float         MaxHealth = 360.0f;
    float         CurrentHealth = 360.0f;
    BossPhase     Phase = BossPhase::Phase1;
    BossWeakPoint WeakPoint;

    // Movement
    Vector2f BasePosition{0.0f, -230.0f};
    Vector2f CurrentPosition{0.0f, -230.0f};
    float    SwayTimer = 0.0f;
    float    SwayAmplitude = 180.0f;
    float    SwayFrequency = 0.8f;

    // Attacks & Timers
    BossAttackType CurrentAttack = BossAttackType::None;
    float          AttackCooldownTimer = 1.5f;
    float          AttackInterval = 2.8f;
    float          VolleyTimer = 0.0f;

    // Laser attack tracking
    std::vector<BossLaserBeam> LaserBeams;

    // Brick Shields
    bool ShieldDeployed = false;
    int  ActiveShieldBricks = 0;

    // Hit feedback & Defeat
    float DamageFlashTimer = 0.0f;
    float FlinchOffset = 0.0f;
    float DefeatTimer = 0.0f;
    float DefeatDuration = 3.5f;
    bool  DefeatCinematicComplete = false;

    [[nodiscard]] float GetHealthRatio() const noexcept
    {
        return MaxHealth > 0.0f ? std::clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
    }

    [[nodiscard]] bool IsDefeated() const noexcept
    {
        return Phase == BossPhase::Defeated || CurrentHealth <= 0.0f;
    }

    // Weak point hits deal critical damage; armored hull hits deal reduced damage
    bool TakeDamage(float amount, bool isWeakPointHit)
    {
        if (IsDefeated()) return false;

        const float finalDamage = isWeakPointHit ? (amount * WeakPoint.DamageMultiplier) : (amount * 0.35f);
        CurrentHealth = std::max(0.0f, CurrentHealth - finalDamage);
        DamageFlashTimer = 0.18f;

        // Phase transitions based on HP thresholds
        const float ratio = GetHealthRatio();
        if (CurrentHealth <= 0.0f)
        {
            Phase = BossPhase::Defeated;
            DefeatTimer = 0.0f;
        }
        else if (ratio <= 0.33f && Phase != BossPhase::Phase3)
        {
            Phase = BossPhase::Phase3;
            AttackInterval = 1.6f;
            SwayFrequency = 1.4f;
            SwayAmplitude = 240.0f;
        }
        else if (ratio <= 0.66f && Phase != BossPhase::Phase2 && Phase != BossPhase::Phase3)
        {
            Phase = BossPhase::Phase2;
            AttackInterval = 2.2f;
            SwayFrequency = 1.1f;
        }
        return true;
    }
};
