#pragma once
#include "ECS/Components/PowerUpComponent.h"
#include <string>
#include <vector>
#include <algorithm>

// Configuration for MultiBall
struct MultiBallConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    int   BaseExtraBalls = 2;       // Extra balls spawned per split
    int   MaxTotalBalls = 12;       // Cap on active balls
    float SplitAngleDeg = 30.0f;    // Ejection angle offset

    int GetBallsToSpawn() const {
        return BaseExtraBalls + (Level - 1);
    }
    int GetUpgradeCost() const {
        return 100 * Level;
    }
};

// Configuration for Expand Paddle
struct ExpandPaddleConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 2.0f;
    float BaseScale = 1.20f;          // +20% size at level 1
    float ScaleStepPerLevel = 0.20f;  // +20% size per level (up to +100% / 2.0x at level 5)
    float BaseDropChance = 0.10f;     // 10% drop chance at level 1
    float DropChanceStepPerLevel = 0.10f; // +10% per level (up to 50% at level 5)

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveScale() const {
        return BaseScale + (Level - 1) * ScaleStepPerLevel;
    }
    float GetEffectiveDropChance() const {
        return BaseDropChance + (Level - 1) * DropChanceStepPerLevel;
    }
    int GetEffectiveDropCount() const {
        int count = 1;
        if (Level >= 3) count++;
        if (Level >= 5) count++;
        return count;
    }
    int GetUpgradeCost() const {
        return 100 * Level;
    }
};

// Configuration for Shrink Paddle
struct ShrinkPaddleConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 2.0f;
    float BaseScale = 0.65f;          // Scale remains constant for shrink
    float BaseSpeedBonusPercent = 0.20f;     // +20% paddle speed at level 1
    float SpeedBonusStepPerLevel = 0.20f;    // +20% paddle speed per level
    float BaseDropChance = 0.10f;     // 10% drop chance at level 1
    float DropChanceStepPerLevel = 0.10f; // +10% per level (up to 50% at level 5)

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveScale() const {
        return BaseScale;
    }
    float GetEffectivePaddleSpeedBonusPercent() const {
        return (BaseSpeedBonusPercent + (Level - 1) * SpeedBonusStepPerLevel) * 100.0f;
    }
    float GetEffectivePaddleSpeed() const {
        constexpr float kBasePaddleSpeed = 700.0f;
        return kBasePaddleSpeed * (1.0f + BaseSpeedBonusPercent + (Level - 1) * SpeedBonusStepPerLevel);
    }
    float GetEffectiveDropChance() const {
        return BaseDropChance + (Level - 1) * DropChanceStepPerLevel;
    }
    int GetEffectiveDropCount() const {
        int count = 1;
        if (Level >= 3) count++;
        if (Level >= 5) count++;
        return count;
    }
    int GetUpgradeCost() const {
        return 100 * Level;
    }
};

using PaddleSizeConfig = ExpandPaddleConfig;

// Configuration for Storm / Thunderstorm (Strike charges & tempest based)
struct StormConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float StrikeRadius = 55.0f;
    float ProjectileSpeed = 800.0f; // compatibility

    // Strikes count:
    // Level 1: 1 strike (deals 2 hits)
    // Level 2: 2 strikes (+1 strike)
    // Level 3: 3 strikes (+1 strike)
    // Level 4: 3 strikes (damage upgraded to 3 hits)
    // Level 5: 60 strikes (Tempest mode: 10s continuous barrage)
    int GetEffectiveStrikes() const {
        if (Level <= 1) return 1;
        if (Level == 2) return 2;
        if (Level == 3) return 3;
        if (Level == 4) return 3;
        return 60; // Level 5 Tempest mode
    }

    // Damage inflicted to bricks (equivalent hits)
    int GetEffectiveDamage() const {
        if (Level >= 4) return 3;
        return 2;
    }

    bool IsTempest() const {
        return Level >= 5;
    }

    float GetTempestDuration() const {
        return 10.0f;
    }

    float GetEffectiveCooldown() const {
        if (Level >= 5) return 0.16f; // Rapid barrage during 10s tempest
        return 0.55f;
    }

    int GetUpgradeCost() const {
        return 150 * Level;
    }

    // Compatibility method
    float GetEffectiveDuration() const {
        if (Level >= 5) return 10.0f;
        return static_cast<float>(GetEffectiveStrikes());
    }
};

using LaserConfig = StormConfig;

// Configuration for Tempo Ball
struct TempoBallConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 1.0f;
    float BaseSpeedMultiplier = 0.60f;     // +60% at level 1
    float SpeedMultiplierPerLevel = 0.20f; // +20% per level
    float FastSpeed = 960.0f;              // Base fast speed (600 * 1.60)
    float SpeedBonusPerLevel = 120.0f;     // Fast speed bonus per level (600 * 0.20)
    float PaddleSpeedBonusPerLevel = 0.10f;// +10% paddle speed per level
    float SlowSpeed = 280.0f;              // Speed approaching paddle
    float SlowZoneTopY = 60.0f;            // Y where slowdown zone begins
    float SlowZoneBottomY = 280.0f;        // Y where slow speed is reached

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveBonusPercent() const {
        return (BaseSpeedMultiplier + (Level - 1) * SpeedMultiplierPerLevel) * 100.0f;
    }
    float GetEffectiveFastSpeed() const {
        constexpr float kBaseSpeed = 600.0f;
        return kBaseSpeed * (1.0f + BaseSpeedMultiplier + (Level - 1) * SpeedMultiplierPerLevel);
    }
    float GetEffectivePaddleSpeedBonusPercent() const {
        return (Level * PaddleSpeedBonusPerLevel) * 100.0f;
    }
    float GetEffectivePaddleSpeed() const {
        constexpr float kBasePaddleSpeed = 700.0f;
        return kBasePaddleSpeed * (1.0f + Level * PaddleSpeedBonusPerLevel);
    }
    int GetUpgradeCost() const {
        return 120 * Level;
    }
};

// Configuration for Extra Life
struct ExtraLifeConfig
{
    int Level = 1;
    int MaxLevel = 5;
    int LivesGranted = 1;
    int MaxLivesCap = 6;

    int GetUpgradeCost() const {
        return 200 * Level;
    }
};

// Configuration for Big Ball
struct BigBallConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 1.5f;
    float BaseScale = 1.4f;
    float ScaleBonusPerLevel = 0.2f;
    bool  PiercesBricks = true;
    bool  OneHitKill = true;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveScale() const {
        return BaseScale + (Level - 1) * ScaleBonusPerLevel;
    }
    int GetUpgradeCost() const {
        return 150 * Level;
    }
};

// Configuration for Fire Ball
struct FireBallConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseScale = 1.3f;
    float ScaleBonusPerLevel = 0.15f;
    float BaseAoERadius = 110.0f;
    float AoERadiusPerLevel = 25.0f;
    float FuseDuration = 1.0f;

    float GetEffectiveScale() const {
        return BaseScale + (Level - 1) * ScaleBonusPerLevel;
    }
    float GetEffectiveAoERadius() const {
        return BaseAoERadius + (Level - 1) * AoERadiusPerLevel;
    }
    int GetUpgradeCost() const {
        return 160 * Level;
    }
};

// Configuration for Sticky / Catch Paddle
struct StickyPaddleConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 12.0f;
    float DurationPerLevel = 2.0f;
    float AutoLaunchDelay = 3.0f;        // 3 seconds window to launch
    float BaseBoostPercent = 25.0f;      // +25% launch speed boost at Level 1
    float BoostPercentPerLevel = 15.0f;  // +15% per level (up to +85% at Level 5)

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveBoostPercent() const {
        return BaseBoostPercent + (Level - 1) * BoostPercentPerLevel;
    }
    float GetEffectiveBoostMultiplier() const {
        return 1.0f + (GetEffectiveBoostPercent() / 100.0f);
    }
    int GetUpgradeCost() const {
        return 140 * Level;
    }
};

// Configuration for Bottom Barrier / Safety Forcefield
struct BottomBarrierConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 15.0f;
    float DurationPerLevel = 2.5f;
    int   BaseCharges = 1;
    int   ChargesPerLevel = 1;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    int GetEffectiveCharges() const {
        return BaseCharges + (Level - 1) * ChargesPerLevel;
    }
    int GetUpgradeCost() const {
        return 150 * Level;
    }
};

// Configuration for Magnet Paddle
struct MagnetConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 2.5f;       // 10s, 12.5s, 15s, 17.5s, 20s
    float MagnetRadius = 850.0f;
    float BasePullSpeed = 380.0f;
    float PullSpeedPerLevel = 70.0f;
    float BaseBallPullForce = 240.0f;    // Attracts the ball towards paddle
    float BallPullForcePerLevel = 80.0f; // Stronger magnet with level (240 to 560 px/s²)

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectivePullSpeed() const {
        return BasePullSpeed + (Level - 1) * PullSpeedPerLevel;
    }
    float GetEffectiveBallPullForce() const {
        return BaseBallPullForce + (Level - 1) * BallPullForcePerLevel;
    }
    int GetUpgradeCost() const {
        return 130 * Level;
    }
};

// Configuration for Ghost / Phase Ball
struct GhostBallConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 5.0f;
    float DurationPerLevel = 1.0f;
    float Alpha = 120.0f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    int GetEffectiveDamage() const {
        int dmg = 1;
        if (Level >= 3) dmg++;
        if (Level >= 5) dmg++;
        return dmg;
    }
    int GetUpgradeCost() const {
        return 160 * Level;
    }
};

// Configuration for Item Brick Regeneration (Groundwork for dynamic respawn)
struct ItemBrickRegenConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseCooldown = 30.0f;
    float CooldownReductionPerLevel = 3.5f;

    float GetEffectiveCooldown() const {
        return std::max(10.0f, BaseCooldown - (Level - 1) * CooldownReductionPerLevel);
    }
    int GetUpgradeCost() const {
        return 120 * Level;
    }
};

class PowerUpManager
{
public:
    static PowerUpManager& Get();

    // Config accessors
    MultiBallConfig&    MultiBall()   { return m_multiBall; }
    ExpandPaddleConfig& Expand()      { return m_expand; }
    ShrinkPaddleConfig& Shrink()      { return m_shrink; }
    StormConfig&        Storm()       { return m_storm; }
    StormConfig&        Laser()       { return m_storm; }
    TempoBallConfig&    Tempo()       { return m_tempo; }
    ExtraLifeConfig&    Life()        { return m_life; }
    BigBallConfig&      Big()         { return m_big; }
    FireBallConfig&     FireBall()    { return m_fireBall; }
    StickyPaddleConfig& Sticky()      { return m_sticky; }
    BottomBarrierConfig& Barrier()    { return m_barrier; }
    MagnetConfig&       Magnet()      { return m_magnet; }
    GhostBallConfig&    Ghost()       { return m_ghost; }
    ItemBrickRegenConfig& ItemRegen() { return m_itemRegen; }

    const MultiBallConfig&    MultiBall() const { return m_multiBall; }
    const ExpandPaddleConfig& Expand()    const { return m_expand; }
    const ShrinkPaddleConfig& Shrink()    const { return m_shrink; }
    const StormConfig&        Storm()     const { return m_storm; }
    const StormConfig&        Laser()     const { return m_storm; }
    const TempoBallConfig&    Tempo()     const { return m_tempo; }
    const ExtraLifeConfig&    Life()      const { return m_life; }
    const BigBallConfig&      Big()       const { return m_big; }
    const FireBallConfig&     FireBall()  const { return m_fireBall; }
    const StickyPaddleConfig& Sticky()    const { return m_sticky; }
    const BottomBarrierConfig& Barrier()  const { return m_barrier; }
    const MagnetConfig&       Magnet()    const { return m_magnet; }
    const GhostBallConfig&    Ghost()     const { return m_ghost; }
    const ItemBrickRegenConfig& ItemRegen() const { return m_itemRegen; }

    // Leveling & Upgrades API
    int  GetLevel(PowerUpType type) const;
    void SetLevel(PowerUpType type, int level);
    bool CanUpgrade(PowerUpType type) const;
    bool Upgrade(PowerUpType type);
    int  GetUpgradeCost(PowerUpType type) const;
    int  GetMaxLevel(PowerUpType type) const;

    // Descriptive Metadata API for Shop & Workshop UI
    std::string GetName(PowerUpType type) const;
    std::string GetDescription(PowerUpType type) const;
    std::string GetCurrentTierInfo(PowerUpType type) const;
    std::string GetNextTierInfo(PowerUpType type) const;

    // Currency API (for future economy / shop system)
    int  GetCoins() const { return m_coins; }
    void AddCoins(int amount);
    bool SpendCoins(int amount);

    // Persistence with PlayerPrefs
    void LoadFromPrefs();
    void SaveToPrefs();
    void ResetToDefaults();

private:
    PowerUpManager();

    MultiBallConfig     m_multiBall;
    ExpandPaddleConfig  m_expand;
    ShrinkPaddleConfig  m_shrink;
    StormConfig         m_storm;
    TempoBallConfig     m_tempo;
    ExtraLifeConfig     m_life;
    BigBallConfig       m_big;
    FireBallConfig      m_fireBall;
    StickyPaddleConfig  m_sticky;
    BottomBarrierConfig m_barrier;
    MagnetConfig        m_magnet;
    GhostBallConfig     m_ghost;
    ItemBrickRegenConfig m_itemRegen;

    int m_coins = 0;
};
