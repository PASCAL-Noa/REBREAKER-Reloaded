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

// Configuration for Laser Paddle
struct LaserConfig
{
    int   Level = 1;
    int   MaxLevel = 5;
    float BaseDuration = 10.0f;
    float DurationPerLevel = 1.5f;
    float BaseCooldown = 0.6f;
    float CooldownReductionPerLevel = 0.06f; // Fire rate increases with level
    float ProjectileSpeed = 800.0f;

    float GetEffectiveDuration() const {
        return BaseDuration + (Level - 1) * DurationPerLevel;
    }
    float GetEffectiveCooldown() const {
        return std::max(0.18f, BaseCooldown - (Level - 1) * CooldownReductionPerLevel);
    }
    int GetUpgradeCost() const {
        return 150 * Level;
    }
};

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

class PowerUpManager
{
public:
    static PowerUpManager& Get();

    // Config accessors
    MultiBallConfig&   MultiBall()   { return m_multiBall; }
    PaddleSizeConfig&  Expand()      { return m_expand; }
    PaddleSizeConfig&  Shrink()      { return m_shrink; }
    LaserConfig&       Laser()       { return m_laser; }
    TempoBallConfig&   Tempo()       { return m_tempo; }
    ExtraLifeConfig&   Life()        { return m_life; }
    BigBallConfig&     Big()         { return m_big; }
    FireBallConfig&    FireBall()    { return m_fireBall; }

    const MultiBallConfig&  MultiBall() const { return m_multiBall; }
    const PaddleSizeConfig& Expand()    const { return m_expand; }
    const PaddleSizeConfig& Shrink()    const { return m_shrink; }
    const LaserConfig&      Laser()     const { return m_laser; }
    const TempoBallConfig&  Tempo()     const { return m_tempo; }
    const ExtraLifeConfig&  Life()      const { return m_life; }
    const BigBallConfig&    Big()       const { return m_big; }
    const FireBallConfig&   FireBall()  const { return m_fireBall; }

    // Leveling & Upgrades API
    int  GetLevel(PowerUpType type) const;
    void SetLevel(PowerUpType type, int level);
    bool CanUpgrade(PowerUpType type) const;
    bool Upgrade(PowerUpType type);
    int  GetUpgradeCost(PowerUpType type) const;
    int  GetMaxLevel(PowerUpType type) const;

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

    MultiBallConfig   m_multiBall;
    PaddleSizeConfig  m_expand;
    PaddleSizeConfig  m_shrink;
    LaserConfig       m_laser;
    TempoBallConfig   m_tempo;
    ExtraLifeConfig   m_life;
    BigBallConfig     m_big;
    FireBallConfig    m_fireBall;

    int m_coins = 0;
};
