#include "PowerUps/PowerUpConfig.h"
#include "Core/PlayerPrefs.h"

PowerUpManager& PowerUpManager::Get()
{
    static PowerUpManager instance;
    return instance;
}

PowerUpManager::PowerUpManager()
{
    LoadFromPrefs();
}

int PowerUpManager::GetLevel(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return m_multiBall.Level;
        case PowerUpType::ExpandPaddle: return m_expand.Level;
        case PowerUpType::ShrinkPaddle: return m_shrink.Level;
        case PowerUpType::Storm:        return m_storm.Level;
        case PowerUpType::TempoBall:    return m_tempo.Level;
        case PowerUpType::ExtraLife:    return m_life.Level;
        case PowerUpType::BigBall:      return m_big.Level;
        case PowerUpType::FireBall:     return m_fireBall.Level;
        case PowerUpType::StickyPaddle: return m_sticky.Level;
        case PowerUpType::BottomBarrier: return m_barrier.Level;
        case PowerUpType::MagnetPaddle: return m_magnet.Level;
        case PowerUpType::GhostBall:    return m_ghost.Level;
        case PowerUpType::ItemRegen:    return m_itemRegen.Level;
        default:                        return 1;
    }
}

int PowerUpManager::GetPersistedLevel(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:     return m_persistedMultiBallLevel;
        case PowerUpType::ExpandPaddle:  return m_persistedExpandLevel;
        case PowerUpType::ShrinkPaddle:  return m_persistedShrinkLevel;
        case PowerUpType::Storm:         return m_persistedStormLevel;
        case PowerUpType::TempoBall:     return m_persistedTempoLevel;
        case PowerUpType::ExtraLife:     return m_persistedLifeLevel;
        case PowerUpType::BigBall:       return m_persistedBigLevel;
        case PowerUpType::FireBall:      return m_persistedFireBallLevel;
        case PowerUpType::StickyPaddle:  return m_persistedStickyLevel;
        case PowerUpType::BottomBarrier: return m_persistedBarrierLevel;
        case PowerUpType::MagnetPaddle:  return m_persistedMagnetLevel;
        case PowerUpType::GhostBall:     return m_persistedGhostLevel;
        case PowerUpType::ItemRegen:     return m_persistedItemRegenLevel;
        default:                         return 1;
    }
}

void PowerUpManager::SetPersistedLevel(PowerUpType type, int level)
{
    int maxLvl = GetMaxLevel(type);
    int clamped = std::clamp(level, 1, maxLvl);
    switch (type)
    {
        case PowerUpType::MultiBall:     m_persistedMultiBallLevel = clamped; break;
        case PowerUpType::ExpandPaddle:  m_persistedExpandLevel = clamped; break;
        case PowerUpType::ShrinkPaddle:  m_persistedShrinkLevel = clamped; break;
        case PowerUpType::Storm:         m_persistedStormLevel = clamped; break;
        case PowerUpType::TempoBall:     m_persistedTempoLevel = clamped; break;
        case PowerUpType::ExtraLife:     m_persistedLifeLevel = clamped; break;
        case PowerUpType::BigBall:       m_persistedBigLevel = clamped; break;
        case PowerUpType::FireBall:      m_persistedFireBallLevel = clamped; break;
        case PowerUpType::StickyPaddle:  m_persistedStickyLevel = clamped; break;
        case PowerUpType::BottomBarrier: m_persistedBarrierLevel = clamped; break;
        case PowerUpType::MagnetPaddle:  m_persistedMagnetLevel = clamped; break;
        case PowerUpType::GhostBall:     m_persistedGhostLevel = clamped; break;
        case PowerUpType::ItemRegen:     m_persistedItemRegenLevel = clamped; break;
        default: break;
    }
}

void PowerUpManager::ClearTransientOverrides()
{
    m_multiBall.Level   = m_persistedMultiBallLevel;
    m_expand.Level      = m_persistedExpandLevel;
    m_shrink.Level      = m_persistedShrinkLevel;
    m_storm.Level       = m_persistedStormLevel;
    m_tempo.Level       = m_persistedTempoLevel;
    m_life.Level        = m_persistedLifeLevel;
    m_big.Level         = m_persistedBigLevel;
    m_fireBall.Level    = m_persistedFireBallLevel;
    m_sticky.Level      = m_persistedStickyLevel;
    m_barrier.Level     = m_persistedBarrierLevel;
    m_magnet.Level      = m_persistedMagnetLevel;
    m_ghost.Level       = m_persistedGhostLevel;
    m_itemRegen.Level   = m_persistedItemRegenLevel;
}

int PowerUpManager::GetMaxLevel(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return m_multiBall.MaxLevel;
        case PowerUpType::ExpandPaddle: return m_expand.MaxLevel;
        case PowerUpType::ShrinkPaddle: return m_shrink.MaxLevel;
        case PowerUpType::Storm:        return m_storm.MaxLevel;
        case PowerUpType::TempoBall:    return m_tempo.MaxLevel;
        case PowerUpType::ExtraLife:    return m_life.MaxLevel;
        case PowerUpType::BigBall:      return m_big.MaxLevel;
        case PowerUpType::FireBall:     return m_fireBall.MaxLevel;
        case PowerUpType::StickyPaddle: return m_sticky.MaxLevel;
        case PowerUpType::BottomBarrier: return m_barrier.MaxLevel;
        case PowerUpType::MagnetPaddle: return m_magnet.MaxLevel;
        case PowerUpType::GhostBall:    return m_ghost.MaxLevel;
        case PowerUpType::ItemRegen:    return m_itemRegen.MaxLevel;
        case PowerUpType::GlitchMalus:  return 1;
        default:                        return 5;
    }
}

void PowerUpManager::SetLevel(PowerUpType type, int level)
{
    int maxLvl = GetMaxLevel(type);
    int clamped = std::clamp(level, 1, maxLvl);
    SetPersistedLevel(type, clamped);
    SetLevelTransient(type, clamped);
    SaveToPrefs();
}

void PowerUpManager::SetLevelTransient(PowerUpType type, int level)
{
    int maxLvl = GetMaxLevel(type);
    int clamped = std::clamp(level, 1, maxLvl);

    switch (type)
    {
        case PowerUpType::MultiBall:     m_multiBall.Level = clamped; break;
        case PowerUpType::ExpandPaddle:  m_expand.Level = clamped; break;
        case PowerUpType::ShrinkPaddle:  m_shrink.Level = clamped; break;
        case PowerUpType::Storm:         m_storm.Level = clamped; break;
        case PowerUpType::TempoBall:     m_tempo.Level = clamped; break;
        case PowerUpType::ExtraLife:     m_life.Level = clamped; break;
        case PowerUpType::BigBall:       m_big.Level = clamped; break;
        case PowerUpType::FireBall:      m_fireBall.Level = clamped; break;
        case PowerUpType::StickyPaddle:  m_sticky.Level = clamped; break;
        case PowerUpType::BottomBarrier: m_barrier.Level = clamped; break;
        case PowerUpType::MagnetPaddle:  m_magnet.Level = clamped; break;
        case PowerUpType::GhostBall:     m_ghost.Level = clamped; break;
        case PowerUpType::ItemRegen:     m_itemRegen.Level = clamped; break;
        default: break;
    }
}

int PowerUpManager::GetUpgradeCost(PowerUpType type) const
{
    int level = GetPersistedLevel(type);
    switch (type)
    {
        case PowerUpType::MultiBall:    return 100 * level;
        case PowerUpType::ExpandPaddle: return 100 * level;
        case PowerUpType::ShrinkPaddle: return 100 * level;
        case PowerUpType::Storm:        return 150 * level;
        case PowerUpType::TempoBall:    return 120 * level;
        case PowerUpType::ExtraLife:    return 200 * level;
        case PowerUpType::BigBall:      return 150 * level;
        case PowerUpType::FireBall:     return 160 * level;
        case PowerUpType::StickyPaddle: return 140 * level;
        case PowerUpType::BottomBarrier: return 150 * level;
        case PowerUpType::MagnetPaddle: return 130 * level;
        case PowerUpType::GhostBall:    return 160 * level;
        case PowerUpType::ItemRegen:    return 120 * level;
        default:                        return 100 * level;
    }
}

bool PowerUpManager::CanUpgrade(PowerUpType type) const
{
    int curLevel = GetPersistedLevel(type);
    int maxLevel = GetMaxLevel(type);
    if (curLevel >= maxLevel) return false;
    return m_coins >= GetUpgradeCost(type);
}

bool PowerUpManager::Upgrade(PowerUpType type)
{
    if (!CanUpgrade(type)) return false;
    int cost = GetUpgradeCost(type);
    m_coins -= cost;
    SetLevel(type, GetPersistedLevel(type) + 1);
    return true;
}

void PowerUpManager::AddCoins(int amount)
{
    if (amount <= 0) return;
    m_coins += amount;
    PlayerPrefs::SetInt("Economy_Coins", m_coins);
    PlayerPrefs::Save();
}

bool PowerUpManager::SpendCoins(int amount)
{
    if (amount <= 0 || m_coins < amount) return false;
    m_coins -= amount;
    PlayerPrefs::SetInt("Economy_Coins", m_coins);
    PlayerPrefs::Save();
    return true;
}

void PowerUpManager::LoadFromPrefs()
{
    // Preferences are preloaded by main() into memory. Read values directly.
    m_coins = PlayerPrefs::GetInt("Economy_Coins", 0);

    // MultiBall
    m_persistedMultiBallLevel  = std::clamp(PlayerPrefs::GetInt("PowerUp_MultiBall_Level", 1), 1, 5);
    m_multiBall.Level          = m_persistedMultiBallLevel;
    m_multiBall.BaseExtraBalls = PlayerPrefs::GetInt("PowerUp_MultiBall_BaseExtra", m_multiBall.BaseExtraBalls);
    m_multiBall.MaxTotalBalls  = PlayerPrefs::GetInt("PowerUp_MultiBall_MaxTotal", m_multiBall.MaxTotalBalls);
    m_multiBall.SplitAngleDeg  = PlayerPrefs::GetFloat("PowerUp_MultiBall_SplitAngle", m_multiBall.SplitAngleDeg);

    // Expand Paddle
    m_persistedExpandLevel     = std::clamp(PlayerPrefs::GetInt("PowerUp_Expand_Level", 1), 1, 5);
    m_expand.Level             = m_persistedExpandLevel;
    m_expand.BaseDuration      = 10.0f;
    m_expand.DurationPerLevel  = 2.0f;
    m_expand.BaseScale         = 1.20f;
    m_expand.ScaleStepPerLevel = 0.20f;
    m_expand.BaseDropChance    = 0.10f;
    m_expand.DropChanceStepPerLevel = 0.10f;

    // Shrink Paddle
    m_persistedShrinkLevel     = std::clamp(PlayerPrefs::GetInt("PowerUp_Shrink_Level", 1), 1, 5);
    m_shrink.Level             = m_persistedShrinkLevel;
    m_shrink.BaseDuration      = 10.0f;
    m_shrink.DurationPerLevel  = 2.0f;
    m_shrink.BaseScale         = 0.65f;
    m_shrink.BaseSpeedBonusPercent = 0.20f;
    m_shrink.SpeedBonusStepPerLevel = 0.20f;
    m_shrink.BaseDropChance    = 0.10f;
    m_shrink.DropChanceStepPerLevel = 0.10f;

    // Storm (Thunderstorm)
    m_persistedStormLevel = std::clamp(PlayerPrefs::GetInt("PowerUp_Storm_Level", PlayerPrefs::GetInt("PowerUp_Laser_Level", 1)), 1, 5);
    m_storm.Level        = m_persistedStormLevel;
    m_storm.StrikeRadius = PlayerPrefs::GetFloat("PowerUp_Storm_StrikeRadius", m_storm.StrikeRadius);

    // Tempo Ball
    m_persistedTempoLevel      = std::clamp(PlayerPrefs::GetInt("PowerUp_Tempo_Level", 1), 1, 5);
    m_tempo.Level              = m_persistedTempoLevel;
    m_tempo.BaseDuration       = 10.0f;
    m_tempo.DurationPerLevel   = 1.0f;
    m_tempo.BaseSpeedMultiplier = 0.60f;
    m_tempo.SpeedMultiplierPerLevel = 0.20f;
    m_tempo.FastSpeed          = m_tempo.GetEffectiveFastSpeed();
    m_tempo.SpeedBonusPerLevel = 120.0f;
    m_tempo.SlowSpeed          = PlayerPrefs::GetFloat("PowerUp_Tempo_SlowSpeed", m_tempo.SlowSpeed);
    m_tempo.SlowZoneTopY       = PlayerPrefs::GetFloat("PowerUp_Tempo_SlowZoneTopY", m_tempo.SlowZoneTopY);
    m_tempo.SlowZoneBottomY    = PlayerPrefs::GetFloat("PowerUp_Tempo_SlowZoneBottomY", m_tempo.SlowZoneBottomY);

    // Extra Life
    m_persistedLifeLevel= std::clamp(PlayerPrefs::GetInt("PowerUp_Life_Level", 1), 1, 5);
    m_life.Level        = m_persistedLifeLevel;
    m_life.LivesGranted = PlayerPrefs::GetInt("PowerUp_Life_Granted", m_life.LivesGranted);
    m_life.MaxLivesCap  = PlayerPrefs::GetInt("PowerUp_Life_Cap", m_life.MaxLivesCap);

    // Big Ball
    m_persistedBigLevel      = std::clamp(PlayerPrefs::GetInt("PowerUp_Big_Level", 1), 1, 5);
    m_big.Level              = m_persistedBigLevel;
    m_big.BaseDuration       = PlayerPrefs::GetFloat("PowerUp_Big_BaseDuration", m_big.BaseDuration);
    m_big.DurationPerLevel   = PlayerPrefs::GetFloat("PowerUp_Big_DurationPerLvl", m_big.DurationPerLevel);
    m_big.BaseScale          = PlayerPrefs::GetFloat("PowerUp_Big_BaseScale", m_big.BaseScale);
    m_big.ScaleBonusPerLevel = PlayerPrefs::GetFloat("PowerUp_Big_ScaleBonus", m_big.ScaleBonusPerLevel);
    m_big.PiercesBricks      = PlayerPrefs::GetBool("PowerUp_Big_PiercesBricks", m_big.PiercesBricks);
    m_big.OneHitKill         = PlayerPrefs::GetBool("PowerUp_Big_OneHitKill", m_big.OneHitKill);

    // Fire Ball
    m_persistedFireBallLevel      = std::clamp(PlayerPrefs::GetInt("PowerUp_Fire_Level", 1), 1, 5);
    m_fireBall.Level              = m_persistedFireBallLevel;
    m_fireBall.BaseScale          = PlayerPrefs::GetFloat("PowerUp_Fire_BaseScale", m_fireBall.BaseScale);
    m_fireBall.ScaleBonusPerLevel = PlayerPrefs::GetFloat("PowerUp_Fire_ScaleBonus", m_fireBall.ScaleBonusPerLevel);
    m_fireBall.BaseAoERadius      = PlayerPrefs::GetFloat("PowerUp_Fire_BaseAoE", m_fireBall.BaseAoERadius);
    m_fireBall.AoERadiusPerLevel  = PlayerPrefs::GetFloat("PowerUp_Fire_AoEBonus", m_fireBall.AoERadiusPerLevel);
    m_fireBall.FuseDuration       = PlayerPrefs::GetFloat("PowerUp_Fire_FuseDuration", m_fireBall.FuseDuration);

    // Sticky Paddle
    m_persistedStickyLevel     = std::clamp(PlayerPrefs::GetInt("PowerUp_Sticky_Level", 1), 1, 5);
    m_sticky.Level             = m_persistedStickyLevel;
    m_sticky.BaseDuration      = 12.0f;
    m_sticky.DurationPerLevel  = 2.0f;
    m_sticky.AutoLaunchDelay   = 3.0f;
    m_sticky.BaseBoostPercent  = 25.0f;
    m_sticky.BoostPercentPerLevel = 15.0f;

    // Bottom Barrier
    m_persistedBarrierLevel    = std::clamp(PlayerPrefs::GetInt("PowerUp_Barrier_Level", 1), 1, 5);
    m_barrier.Level            = m_persistedBarrierLevel;
    m_barrier.BaseDuration     = PlayerPrefs::GetFloat("PowerUp_Barrier_BaseDuration", m_barrier.BaseDuration);
    m_barrier.DurationPerLevel = PlayerPrefs::GetFloat("PowerUp_Barrier_DurationPerLvl", m_barrier.DurationPerLevel);
    m_barrier.BaseCharges      = PlayerPrefs::GetInt("PowerUp_Barrier_BaseCharges", m_barrier.BaseCharges);
    m_barrier.ChargesPerLevel  = PlayerPrefs::GetInt("PowerUp_Barrier_ChargesPerLvl", m_barrier.ChargesPerLevel);

    // Magnet Paddle
    m_persistedMagnetLevel     = std::clamp(PlayerPrefs::GetInt("PowerUp_Magnet_Level", 1), 1, 5);
    m_magnet.Level             = m_persistedMagnetLevel;
    m_magnet.BaseDuration      = 10.0f;
    m_magnet.DurationPerLevel  = 2.5f;
    m_magnet.MagnetRadius      = 850.0f;
    m_magnet.BasePullSpeed     = 380.0f;
    m_magnet.PullSpeedPerLevel = 70.0f;
    m_magnet.BaseBallPullForce = 240.0f;
    m_magnet.BallPullForcePerLevel = 80.0f;

    // Ghost Ball
    m_persistedGhostLevel      = std::clamp(PlayerPrefs::GetInt("PowerUp_Ghost_Level", 1), 1, 5);
    m_ghost.Level              = m_persistedGhostLevel;
    m_ghost.BaseDuration       = 5.0f;
    m_ghost.DurationPerLevel   = 1.0f;
    m_ghost.Alpha              = PlayerPrefs::GetFloat("PowerUp_Ghost_Alpha", m_ghost.Alpha);

    // Item Brick Regen
    m_persistedItemRegenLevel  = std::clamp(PlayerPrefs::GetInt("PowerUp_ItemRegen_Level", 1), 1, 5);
    m_itemRegen.Level          = m_persistedItemRegenLevel;
    m_itemRegen.BaseCooldown   = PlayerPrefs::GetFloat("PowerUp_ItemRegen_BaseCooldown", m_itemRegen.BaseCooldown);
    m_itemRegen.CooldownReductionPerLevel = PlayerPrefs::GetFloat("PowerUp_ItemRegen_CooldownReduction", m_itemRegen.CooldownReductionPerLevel);
}

void PowerUpManager::SaveToPrefs()
{
    PlayerPrefs::SetInt("Economy_Coins", m_coins);

    // MultiBall
    PlayerPrefs::SetInt("PowerUp_MultiBall_Level", GetPersistedLevel(PowerUpType::MultiBall));
    PlayerPrefs::SetInt("PowerUp_MultiBall_BaseExtra", m_multiBall.BaseExtraBalls);
    PlayerPrefs::SetInt("PowerUp_MultiBall_MaxTotal", m_multiBall.MaxTotalBalls);
    PlayerPrefs::SetFloat("PowerUp_MultiBall_SplitAngle", m_multiBall.SplitAngleDeg);

    // Expand
    PlayerPrefs::SetInt("PowerUp_Expand_Level", GetPersistedLevel(PowerUpType::ExpandPaddle));
    PlayerPrefs::SetFloat("PowerUp_Expand_BaseDuration", m_expand.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Expand_DurationPerLvl", m_expand.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Expand_BaseScale", m_expand.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Expand_ScaleStep", m_expand.ScaleStepPerLevel);

    // Shrink
    PlayerPrefs::SetInt("PowerUp_Shrink_Level", GetPersistedLevel(PowerUpType::ShrinkPaddle));
    PlayerPrefs::SetFloat("PowerUp_Shrink_BaseDuration", m_shrink.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Shrink_DurationPerLvl", m_shrink.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Shrink_BaseScale", m_shrink.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Shrink_SpeedBonus", m_shrink.BaseSpeedBonusPercent);
    PlayerPrefs::SetFloat("PowerUp_Shrink_SpeedStep", m_shrink.SpeedBonusStepPerLevel);

    // Storm (Thunderstorm)
    int stormPersisted = GetPersistedLevel(PowerUpType::Storm);
    PlayerPrefs::SetInt("PowerUp_Storm_Level", stormPersisted);
    PlayerPrefs::SetFloat("PowerUp_Storm_StrikeRadius", m_storm.StrikeRadius);
    // Backward compatibility sync
    PlayerPrefs::SetInt("PowerUp_Laser_Level", stormPersisted);
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseDuration", static_cast<float>(m_storm.GetEffectiveStrikes()));
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseCooldown", m_storm.GetEffectiveCooldown());
    PlayerPrefs::SetFloat("PowerUp_Laser_ProjSpeed", m_storm.ProjectileSpeed);

    // Tempo
    PlayerPrefs::SetInt("PowerUp_Tempo_Level", GetPersistedLevel(PowerUpType::TempoBall));
    PlayerPrefs::SetFloat("PowerUp_Tempo_BaseDuration", m_tempo.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Tempo_DurationPerLvl", m_tempo.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Tempo_FastSpeed", m_tempo.FastSpeed);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SpeedBonus", m_tempo.SpeedBonusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowSpeed", m_tempo.SlowSpeed);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowZoneTopY", m_tempo.SlowZoneTopY);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowZoneBottomY", m_tempo.SlowZoneBottomY);

    // Extra Life
    PlayerPrefs::SetInt("PowerUp_Life_Level", GetPersistedLevel(PowerUpType::ExtraLife));
    PlayerPrefs::SetInt("PowerUp_Life_Granted", m_life.LivesGranted);
    PlayerPrefs::SetInt("PowerUp_Life_Cap", m_life.MaxLivesCap);

    // Big Ball
    PlayerPrefs::SetInt("PowerUp_Big_Level", GetPersistedLevel(PowerUpType::BigBall));
    PlayerPrefs::SetFloat("PowerUp_Big_BaseDuration", m_big.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Big_DurationPerLvl", m_big.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Big_BaseScale", m_big.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Big_ScaleBonus", m_big.ScaleBonusPerLevel);
    PlayerPrefs::SetBool("PowerUp_Big_PiercesBricks", m_big.PiercesBricks);
    PlayerPrefs::SetBool("PowerUp_Big_OneHitKill", m_big.OneHitKill);

    // Fire Ball
    PlayerPrefs::SetInt("PowerUp_Fire_Level", GetPersistedLevel(PowerUpType::FireBall));
    PlayerPrefs::SetFloat("PowerUp_Fire_BaseScale", m_fireBall.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Fire_ScaleBonus", m_fireBall.ScaleBonusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Fire_BaseAoE", m_fireBall.BaseAoERadius);
    PlayerPrefs::SetFloat("PowerUp_Fire_AoEBonus", m_fireBall.AoERadiusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Fire_FuseDuration", m_fireBall.FuseDuration);

    // Sticky Paddle
    PlayerPrefs::SetInt("PowerUp_Sticky_Level", GetPersistedLevel(PowerUpType::StickyPaddle));
    PlayerPrefs::SetFloat("PowerUp_Sticky_BaseDuration", m_sticky.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Sticky_DurationPerLvl", m_sticky.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Sticky_AutoLaunch", m_sticky.AutoLaunchDelay);

    // Bottom Barrier
    PlayerPrefs::SetInt("PowerUp_Barrier_Level", GetPersistedLevel(PowerUpType::BottomBarrier));
    PlayerPrefs::SetFloat("PowerUp_Barrier_BaseDuration", m_barrier.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Barrier_DurationPerLvl", m_barrier.DurationPerLevel);
    PlayerPrefs::SetInt("PowerUp_Barrier_BaseCharges", m_barrier.BaseCharges);
    PlayerPrefs::SetInt("PowerUp_Barrier_ChargesPerLvl", m_barrier.ChargesPerLevel);

    // Magnet Paddle
    PlayerPrefs::SetInt("PowerUp_Magnet_Level", GetPersistedLevel(PowerUpType::MagnetPaddle));
    PlayerPrefs::SetFloat("PowerUp_Magnet_BaseDuration", m_magnet.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Magnet_DurationPerLvl", m_magnet.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Magnet_Radius", m_magnet.MagnetRadius);
    PlayerPrefs::SetFloat("PowerUp_Magnet_BasePull", m_magnet.BasePullSpeed);
    PlayerPrefs::SetFloat("PowerUp_Magnet_PullBonus", m_magnet.PullSpeedPerLevel);

    // Ghost Ball
    PlayerPrefs::SetInt("PowerUp_Ghost_Level", GetPersistedLevel(PowerUpType::GhostBall));
    PlayerPrefs::SetFloat("PowerUp_Ghost_BaseDuration", m_ghost.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Ghost_DurationPerLvl", m_ghost.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Ghost_Alpha", m_ghost.Alpha);

    // Item Brick Regen
    PlayerPrefs::SetInt("PowerUp_ItemRegen_Level", GetPersistedLevel(PowerUpType::ItemRegen));
    PlayerPrefs::SetFloat("PowerUp_ItemRegen_BaseCooldown", m_itemRegen.BaseCooldown);
    PlayerPrefs::SetFloat("PowerUp_ItemRegen_CooldownReduction", m_itemRegen.CooldownReductionPerLevel);

    PlayerPrefs::Save();
}

void PowerUpManager::ResetToDefaults()
{
    m_multiBall = MultiBallConfig{};
    m_expand    = ExpandPaddleConfig{};
    m_shrink    = ShrinkPaddleConfig{};
    m_storm     = StormConfig{};
    m_tempo     = TempoBallConfig{};
    m_life      = ExtraLifeConfig{};
    m_big       = BigBallConfig{};
    m_fireBall  = FireBallConfig{};
    m_sticky    = StickyPaddleConfig{};
    m_barrier   = BottomBarrierConfig{};
    m_magnet    = MagnetConfig{};
    m_ghost     = GhostBallConfig{};
    m_itemRegen = ItemBrickRegenConfig{};
    m_coins     = 0;

    m_persistedMultiBallLevel = 1;
    m_persistedExpandLevel    = 1;
    m_persistedShrinkLevel    = 1;
    m_persistedStormLevel     = 1;
    m_persistedTempoLevel     = 1;
    m_persistedLifeLevel      = 1;
    m_persistedBigLevel       = 1;
    m_persistedFireBallLevel  = 1;
    m_persistedStickyLevel    = 1;
    m_persistedBarrierLevel   = 1;
    m_persistedMagnetLevel    = 1;
    m_persistedGhostLevel     = 1;
    m_persistedItemRegenLevel = 1;

    SaveToPrefs();
}

std::string PowerUpManager::GetName(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return "MULTI-BALL";
        case PowerUpType::ExpandPaddle: return "EXPAND PADDLE";
        case PowerUpType::ShrinkPaddle: return "SHRINK PADDLE";
        case PowerUpType::Storm:        return "STORM";
        case PowerUpType::TempoBall:    return "TEMPO BALL";
        case PowerUpType::ExtraLife:    return "EXTRA LIFE";
        case PowerUpType::BigBall:      return "BIG BALL";
        case PowerUpType::FireBall:     return "FIRE BALL";
        case PowerUpType::StickyPaddle: return "STICKY";
        case PowerUpType::BottomBarrier:return "BOTTOM FORCEFIELD";
        case PowerUpType::MagnetPaddle: return "MAGNET PADDLE";
        case PowerUpType::GhostBall:    return "GHOST BALL";
        case PowerUpType::ItemRegen:    return "ITEM BRICK REGEN";
        default:                        return "UNKNOWN";
    }
}

std::string PowerUpManager::GetDescription(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:
            return "Splits active balls into multiple secondary balls.";
        case PowerUpType::ExpandPaddle:
            return "Expands paddle width and catch zone. Lucky trigger: chance to spawn bonus capsules.";
        case PowerUpType::ShrinkPaddle:
            return "Shrinks paddle width while granting bonus speed. Lucky trigger: chance to spawn bonus capsules.";
        case PowerUpType::Storm:
            return "Strikes thunderbolts damaging bricks & conducting through metal circuits.";
        case PowerUpType::TempoBall:
            return "Dynamic pacing: accelerates ball and paddle, with smooth slowdown near the paddle.";
        case PowerUpType::ExtraLife:
            return "Restaure et renforce les coeurs : Vert (Niv. 3 max), Jaune (Niv. 2) et Rouge (Base).";
        case PowerUpType::BigBall:
            return "Massive kinetic sphere piercing and shattering regular bricks on contact.";
        case PowerUpType::FireBall:
            return "Incendiary blast dealing explosive area-of-effect damage to adjacent bricks.";
        case PowerUpType::StickyPaddle:
            return "Catches ball on paddle contact. Rhythm launch: timed release grants a speed boost.";
        case PowerUpType::BottomBarrier:
            return "Deploys a reflective laser bottom forcefield saving balls from draining.";
        case PowerUpType::MagnetPaddle:
            return "Gravitational field pulling bonus capsules and curving ball trajectories towards paddle.";
        case PowerUpType::GhostBall:
            return "Phases through tough and metal bricks safely while damaging regular bricks.";
        case PowerUpType::ItemRegen:
            return "Reduces the respawn cooldown of item capsule bricks on the field.";
        default:
            return "";
    }
}

std::string PowerUpManager::GetTierInfoForLevel(PowerUpType type, int level) const
{
    level = std::clamp(level, 1, GetMaxLevel(type));
    switch (type)
    {
        case PowerUpType::MultiBall:
        {
            int extraBalls = m_multiBall.BaseExtraBalls + (level - 1);
            return "+" + std::to_string(extraBalls) + " extra balls per split (cap 12)";
        }
        case PowerUpType::ExpandPaddle:
        {
            float scale = m_expand.BaseScale + (level - 1) * m_expand.ScaleStepPerLevel;
            float duration = m_expand.BaseDuration + (level - 1) * m_expand.DurationPerLevel;
            float dropChance = (m_expand.BaseDropChance + (level - 1) * m_expand.DropChanceStepPerLevel) * 100.0f;
            int dropCount = 1 + (level >= 3 ? 1 : 0) + (level >= 5 ? 1 : 0);
            char buf[128];
            snprintf(buf, sizeof(buf), "Width: +%d%% (x%.2f), Duration: %.0fs, Lucky Drop: %.0f%% (%d capsule%s)",
                     static_cast<int>((scale - 1.0f) * 100.0f + 0.5f),
                     scale,
                     duration,
                     dropChance,
                     dropCount,
                     dropCount > 1 ? "s" : "");
            return std::string(buf);
        }
        case PowerUpType::ShrinkPaddle:
        {
            float speedBonus = (m_shrink.BaseSpeedBonusPercent + (level - 1) * m_shrink.SpeedBonusStepPerLevel) * 100.0f;
            float speed = 700.0f * (1.0f + speedBonus / 100.0f);
            float duration = m_shrink.BaseDuration + (level - 1) * m_shrink.DurationPerLevel;
            float dropChance = (m_shrink.BaseDropChance + (level - 1) * m_shrink.DropChanceStepPerLevel) * 100.0f;
            int dropCount = 1 + (level >= 3 ? 1 : 0) + (level >= 5 ? 1 : 0);
            char buf[128];
            snprintf(buf, sizeof(buf), "Scale: 65%%, Speed: +%d%% (%.0f px/s), Duration: %.0fs, Lucky Drop: %.0f%% (%d capsule%s)",
                     static_cast<int>(speedBonus),
                     speed,
                     duration,
                     dropChance,
                     dropCount,
                     dropCount > 1 ? "s" : "");
            return std::string(buf);
        }
        case PowerUpType::Storm:
        {
            if (level >= 5) return "Tempest Mode: 10s continuous barrage (60 strikes)";
            int strikes = (level <= 1 ? 1 : (level == 2 ? 2 : 3));
            int dmg = (level >= 4 ? 3 : 2);
            return std::to_string(strikes) + " strikes, " + std::to_string(dmg) + " dmg/hit";
        }
        case PowerUpType::TempoBall:
        {
            float ballBonus = (m_tempo.BaseSpeedMultiplier + (level - 1) * m_tempo.SpeedMultiplierPerLevel) * 100.0f;
            float fastSpeed = 600.0f * (1.0f + m_tempo.BaseSpeedMultiplier + (level - 1) * m_tempo.SpeedMultiplierPerLevel);
            float paddleBonus = (static_cast<float>(level) * m_tempo.PaddleSpeedBonusPerLevel) * 100.0f;
            float duration = m_tempo.BaseDuration + (level - 1) * m_tempo.DurationPerLevel;
            return "Ball: +" + std::to_string(static_cast<int>(ballBonus)) + "% (" + std::to_string(static_cast<int>(fastSpeed)) + " px/s), Paddle: +" + std::to_string(static_cast<int>(paddleBonus)) + "%, Duration: " + std::to_string(static_cast<int>(duration)) + "s";
        }
        case PowerUpType::ExtraLife:
            switch (level)
            {
                case 1: return "+1 Vie | Cap : 4 vies (Debloque 1er coeur Jaune)";
                case 2: return "+2 Vies | Cap : 5 vies (Jusqu'a 2 coeurs Jaunes)";
                case 3: return "+3 Vies | Cap : 6 vies (3 coeurs Jaunes)";
                case 4: return "+4 Vies | Cap : 7 vies (Debloque 1er coeur Vert)";
                case 5: return "+6 Vies | Cap : 9 vies (3 coeurs Verts max)";
                default: return "+1 Vie | Cap : 4 vies";
            }
        case PowerUpType::BigBall:
        {
            float scale = (m_big.BaseScale + (level - 1) * m_big.ScaleBonusPerLevel) * 100.0f;
            float duration = m_big.BaseDuration + (level - 1) * m_big.DurationPerLevel;
            return "Scale: " + std::to_string(static_cast<int>(scale)) + "%, Duration: " + std::to_string(static_cast<int>(duration)) + "s (Crushes metal & 1-hit kills)";
        }
        case PowerUpType::FireBall:
        {
            float radius = m_fireBall.BaseAoERadius + (level - 1) * m_fireBall.AoERadiusPerLevel;
            float scale = (m_fireBall.BaseScale + (level - 1) * m_fireBall.ScaleBonusPerLevel) * 100.0f;
            return "AoE Radius: " + std::to_string(static_cast<int>(radius)) + " px, Scale: " + std::to_string(static_cast<int>(scale)) + "%";
        }
        case PowerUpType::StickyPaddle:
        {
            float duration = m_sticky.BaseDuration + (level - 1) * m_sticky.DurationPerLevel;
            float boost = m_sticky.BaseBoostPercent + (level - 1) * m_sticky.BoostPercentPerLevel;
            return "Duration: " + std::to_string(static_cast<int>(duration)) + "s, Rhythm Boost: +" + std::to_string(static_cast<int>(boost)) + "% speed (3s hold)";
        }
        case PowerUpType::BottomBarrier:
        {
            int charges = m_barrier.BaseCharges + (level - 1) * m_barrier.ChargesPerLevel;
            float duration = m_barrier.BaseDuration + (level - 1) * m_barrier.DurationPerLevel;
            return "Charges: " + std::to_string(charges) + " saves, Duration: " + std::to_string(static_cast<int>(duration)) + "s";
        }
        case PowerUpType::MagnetPaddle:
        {
            float duration = m_magnet.BaseDuration + (level - 1) * m_magnet.DurationPerLevel;
            float ballPull = m_magnet.BaseBallPullForce + (level - 1) * m_magnet.BallPullForcePerLevel;
            float capsulePull = m_magnet.BasePullSpeed + (level - 1) * m_magnet.PullSpeedPerLevel;
            return "Duration: " + std::to_string(static_cast<int>(duration)) + "s, Ball Pull: " + std::to_string(static_cast<int>(ballPull)) + " px/s^2, Capsule: " + std::to_string(static_cast<int>(capsulePull)) + " px/s";
        }
        case PowerUpType::GhostBall:
        {
            float duration = m_ghost.BaseDuration + (level - 1) * m_ghost.DurationPerLevel;
            int dmg = (level >= 5 ? 3 : (level >= 3 ? 2 : 1));
            return "Duration: " + std::to_string(static_cast<int>(duration)) + "s, Damage: " + std::to_string(dmg) + " dmg (Immortal & Selective Phase)";
        }
        case PowerUpType::ItemRegen:
        {
            float cd = m_itemRegen.BaseCooldown - (level - 1) * m_itemRegen.CooldownReductionPerLevel;
            return "Cooldown: " + std::to_string(static_cast<int>(cd)) + "s";
        }
        default:
            return "";
    }
}

std::string PowerUpManager::GetCurrentTierInfo(PowerUpType type) const
{
    return GetTierInfoForLevel(type, GetLevel(type));
}

std::string PowerUpManager::GetNextTierInfo(PowerUpType type) const
{
    int lvl = GetLevel(type);
    int maxLvl = GetMaxLevel(type);
    if (lvl >= maxLvl)
    {
        return "MAX LEVEL REACHED";
    }

    switch (type)
    {
        case PowerUpType::MultiBall:
            return "Next: +" + std::to_string(m_multiBall.GetBallsToSpawn() + 1) + " balls per split";
        case PowerUpType::ExpandPaddle:
        {
            int nextLvl = lvl + 1;
            std::string extra = (nextLvl == 3 || nextLvl == 5) ? ", +1 lucky drop count" : "";
            return "Next: +20% width, +2s duration, +10% drop chance" + extra;
        }
        case PowerUpType::ShrinkPaddle:
        {
            int nextLvl = lvl + 1;
            std::string extra = (nextLvl == 3 || nextLvl == 5) ? ", +1 lucky drop count" : "";
            return "Next: +20% paddle speed, +2s duration, +10% drop chance" + extra;
        }
        case PowerUpType::Storm:
            if (lvl == 1) return "Next: +1 strike (2 total)";
            if (lvl == 2) return "Next: +1 strike (3 total)";
            if (lvl == 3) return "Next: upgraded damage (3 hits/strike)";
            return "Next: Tempest Mode (10s continuous lightning barrage)";
        case PowerUpType::TempoBall:
            return "Next: +" + std::to_string(static_cast<int>(m_tempo.SpeedMultiplierPerLevel * 100.0f)) + "% ball speed, +" + std::to_string(static_cast<int>(m_tempo.PaddleSpeedBonusPerLevel * 100.0f)) + "% paddle speed, +" + std::to_string(static_cast<int>(m_tempo.DurationPerLevel)) + "s duration";
        case PowerUpType::ExtraLife:
            if (lvl == 1) return "Next: +2 vies par capsule, Cap 5 vies";
            if (lvl == 2) return "Next: +3 vies par capsule, Cap 6 vies (3 coeurs Jaunes)";
            if (lvl == 3) return "Next: +4 vies par capsule, Cap 7 vies (1er coeur Vert)";
            return "Next: +6 vies par capsule, Cap 9 vies (3 coeurs Verts)";
        case PowerUpType::BigBall:
            return "Next: +" + std::to_string(static_cast<int>(m_big.DurationPerLevel)) + "s duration, +" + std::to_string(static_cast<int>(m_big.ScaleBonusPerLevel * 100.0f)) + "% size";
        case PowerUpType::FireBall:
            return "Next: +" + std::to_string(static_cast<int>(m_fireBall.AoERadiusPerLevel)) + " px radius, +" + std::to_string(static_cast<int>(m_fireBall.ScaleBonusPerLevel * 100.0f)) + "% size";
        case PowerUpType::StickyPaddle:
            return "Next: +15% rhythm launch boost, +2s duration";
        case PowerUpType::BottomBarrier:
            return "Next: +1 charge save, +" + std::to_string(static_cast<int>(m_barrier.DurationPerLevel)) + "s duration";
        case PowerUpType::MagnetPaddle:
            return "Next: +80 px/s^2 ball pull force, +70 px/s capsule pull, +2.5s duration";
        case PowerUpType::GhostBall:
            if (lvl == 2) return "Next: +1 damage (2 dmg total), +1s duration";
            if (lvl == 4) return "Next: +1 damage (3 dmg total), +1s duration";
            return "Next: +" + std::to_string(static_cast<int>(m_ghost.DurationPerLevel)) + "s duration";
        case PowerUpType::ItemRegen:
            return "Next: -" + std::to_string(static_cast<int>(m_itemRegen.CooldownReductionPerLevel)) + "s cooldown reduction";
        default:
            return "";
    }
}
