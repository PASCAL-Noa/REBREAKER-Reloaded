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

// Configuration for Paddle Size modifications (Expand / Shrink)
struct PaddleSizeConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;      // Duration in seconds
    float DurationPerLevel = 1.5f;   // +1.5s per level
    float BaseScale = 1.5f;          // Scale factor (1.5x for Expand, 0.65x for Shrink)
    float ScaleStepPerLevel = 0.08f; // Stronger effect with level
    float BaseWidth = 120.0f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveScale() const {
        return BaseScale + (Level - 1) * ScaleStepPerLevel;
    }
    int GetUpgradeCost() const {
        return 100 * Level;
    }
};

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
    float DurationPerLevel = 1.5f;
    float FastSpeed = 850.0f;        // Speed heading towards bricks
    float SpeedBonusPerLevel = 30.0f;// Fast speed increases with level
    float SlowSpeed = 280.0f;        // Speed approaching paddle
    float SlowZoneTopY = 60.0f;      // Y where slowdown zone begins
    float SlowZoneBottomY = 280.0f;  // Y where slow speed is reached

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveFastSpeed() const {
        return FastSpeed + (Level - 1) * SpeedBonusPerLevel;
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
    float AutoLaunchDelay = 3.5f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
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
    float BaseDuration = 12.0f;
    float DurationPerLevel = 2.0f;
    float MagnetRadius = 650.0f;
    float BasePullSpeed = 380.0f;
    float PullSpeedPerLevel = 60.0f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectivePullSpeed() const {
        return BasePullSpeed + (Level - 1) * PullSpeedPerLevel;
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
    float BaseDuration = 8.0f;
    float DurationPerLevel = 1.5f;
    float Alpha = 120.0f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
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
    PaddleSizeConfig&   Expand()      { return m_expand; }
    PaddleSizeConfig&   Shrink()      { return m_shrink; }
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
    const PaddleSizeConfig&   Expand()    const { return m_expand; }
    const PaddleSizeConfig&   Shrink()    const { return m_shrink; }
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
    PaddleSizeConfig    m_expand;
    PaddleSizeConfig    m_shrink;
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
