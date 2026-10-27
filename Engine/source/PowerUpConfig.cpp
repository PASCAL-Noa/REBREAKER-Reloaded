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
        default:                        return 5;
    }
}

void PowerUpManager::SetLevel(PowerUpType type, int level)
{
    int maxLvl = GetMaxLevel(type);
    int clamped = std::clamp(level, 1, maxLvl);

    switch (type)
    {
        case PowerUpType::MultiBall:    m_multiBall.Level = clamped; break;
        case PowerUpType::ExpandPaddle: m_expand.Level = clamped; break;
        case PowerUpType::ShrinkPaddle: m_shrink.Level = clamped; break;
        case PowerUpType::Storm:        m_storm.Level = clamped; break;
        case PowerUpType::TempoBall:    m_tempo.Level = clamped; break;
        case PowerUpType::ExtraLife:    m_life.Level = clamped; break;
        case PowerUpType::BigBall:      m_big.Level = clamped; break;
        case PowerUpType::FireBall:     m_fireBall.Level = clamped; break;
        case PowerUpType::StickyPaddle: m_sticky.Level = clamped; break;
        case PowerUpType::BottomBarrier: m_barrier.Level = clamped; break;
        case PowerUpType::MagnetPaddle: m_magnet.Level = clamped; break;
        case PowerUpType::GhostBall:    m_ghost.Level = clamped; break;
        case PowerUpType::ItemRegen:    m_itemRegen.Level = clamped; break;
        default: break;
    }
    SaveToPrefs();
}

int PowerUpManager::GetUpgradeCost(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return m_multiBall.GetUpgradeCost();
        case PowerUpType::ExpandPaddle: return m_expand.GetUpgradeCost();
        case PowerUpType::ShrinkPaddle: return m_shrink.GetUpgradeCost();
        case PowerUpType::Storm:        return m_storm.GetUpgradeCost();
        case PowerUpType::TempoBall:    return m_tempo.GetUpgradeCost();
        case PowerUpType::ExtraLife:    return m_life.GetUpgradeCost();
        case PowerUpType::BigBall:      return m_big.GetUpgradeCost();
        case PowerUpType::FireBall:     return m_fireBall.GetUpgradeCost();
        case PowerUpType::StickyPaddle: return m_sticky.GetUpgradeCost();
        case PowerUpType::BottomBarrier: return m_barrier.GetUpgradeCost();
        case PowerUpType::MagnetPaddle: return m_magnet.GetUpgradeCost();
        case PowerUpType::GhostBall:    return m_ghost.GetUpgradeCost();
        case PowerUpType::ItemRegen:    return m_itemRegen.GetUpgradeCost();
        default:                        return 100;
    }
}

bool PowerUpManager::CanUpgrade(PowerUpType type) const
{
    int curLevel = GetLevel(type);
    int maxLevel = GetMaxLevel(type);
    if (curLevel >= maxLevel) return false;
    return m_coins >= GetUpgradeCost(type);
}

bool PowerUpManager::Upgrade(PowerUpType type)
{
    if (!CanUpgrade(type)) return false;
    int cost = GetUpgradeCost(type);
    m_coins -= cost;
    SetLevel(type, GetLevel(type) + 1);
    SaveToPrefs();
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
    PlayerPrefs::Load();

    m_coins = PlayerPrefs::GetInt("Economy_Coins", 0);

    // MultiBall
    m_multiBall.Level          = PlayerPrefs::GetInt("PowerUp_MultiBall_Level", m_multiBall.Level);
    m_multiBall.BaseExtraBalls = PlayerPrefs::GetInt("PowerUp_MultiBall_BaseExtra", m_multiBall.BaseExtraBalls);
    m_multiBall.MaxTotalBalls  = PlayerPrefs::GetInt("PowerUp_MultiBall_MaxTotal", m_multiBall.MaxTotalBalls);
    m_multiBall.SplitAngleDeg  = PlayerPrefs::GetFloat("PowerUp_MultiBall_SplitAngle", m_multiBall.SplitAngleDeg);

    // Expand Paddle
    m_expand.Level             = PlayerPrefs::GetInt("PowerUp_Expand_Level", m_expand.Level);
    m_expand.BaseDuration      = 10.0f;
    m_expand.DurationPerLevel  = 2.0f;
    m_expand.BaseScale         = 1.20f;
    m_expand.ScaleStepPerLevel = 0.20f;
    m_expand.BaseDropChance    = 0.10f;
    m_expand.DropChanceStepPerLevel = 0.10f;

    // Shrink Paddle
    m_shrink.Level             = PlayerPrefs::GetInt("PowerUp_Shrink_Level", m_shrink.Level);
    m_shrink.BaseDuration      = 10.0f;
    m_shrink.DurationPerLevel  = 2.0f;
    m_shrink.BaseScale         = 0.65f;
    m_shrink.BaseSpeedBonusPercent = 0.20f;
    m_shrink.SpeedBonusStepPerLevel = 0.20f;
    m_shrink.BaseDropChance    = 0.10f;
    m_shrink.DropChanceStepPerLevel = 0.10f;

    // Storm (Thunderstorm)
    m_storm.Level        = PlayerPrefs::GetInt("PowerUp_Storm_Level", PlayerPrefs::GetInt("PowerUp_Laser_Level", m_storm.Level));
    m_storm.StrikeRadius = PlayerPrefs::GetFloat("PowerUp_Storm_StrikeRadius", m_storm.StrikeRadius);

    // Tempo Ball
    m_tempo.Level              = PlayerPrefs::GetInt("PowerUp_Tempo_Level", m_tempo.Level);
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
    m_life.Level        = PlayerPrefs::GetInt("PowerUp_Life_Level", m_life.Level);
    m_life.LivesGranted = PlayerPrefs::GetInt("PowerUp_Life_Granted", m_life.LivesGranted);
    m_life.MaxLivesCap  = PlayerPrefs::GetInt("PowerUp_Life_Cap", m_life.MaxLivesCap);

    // Big Ball
    m_big.Level              = PlayerPrefs::GetInt("PowerUp_Big_Level", m_big.Level);
    m_big.BaseDuration       = PlayerPrefs::GetFloat("PowerUp_Big_BaseDuration", m_big.BaseDuration);
    m_big.DurationPerLevel   = PlayerPrefs::GetFloat("PowerUp_Big_DurationPerLvl", m_big.DurationPerLevel);
    m_big.BaseScale          = PlayerPrefs::GetFloat("PowerUp_Big_BaseScale", m_big.BaseScale);
    m_big.ScaleBonusPerLevel = PlayerPrefs::GetFloat("PowerUp_Big_ScaleBonus", m_big.ScaleBonusPerLevel);
    m_big.PiercesBricks      = PlayerPrefs::GetBool("PowerUp_Big_PiercesBricks", m_big.PiercesBricks);
    m_big.OneHitKill         = PlayerPrefs::GetBool("PowerUp_Big_OneHitKill", m_big.OneHitKill);

    // Fire Ball
    m_fireBall.Level              = PlayerPrefs::GetInt("PowerUp_Fire_Level", m_fireBall.Level);
    m_fireBall.BaseScale          = PlayerPrefs::GetFloat("PowerUp_Fire_BaseScale", m_fireBall.BaseScale);
    m_fireBall.ScaleBonusPerLevel = PlayerPrefs::GetFloat("PowerUp_Fire_ScaleBonus", m_fireBall.ScaleBonusPerLevel);
    m_fireBall.BaseAoERadius      = PlayerPrefs::GetFloat("PowerUp_Fire_BaseAoE", m_fireBall.BaseAoERadius);
    m_fireBall.AoERadiusPerLevel  = PlayerPrefs::GetFloat("PowerUp_Fire_AoEBonus", m_fireBall.AoERadiusPerLevel);
    m_fireBall.FuseDuration       = PlayerPrefs::GetFloat("PowerUp_Fire_FuseDuration", m_fireBall.FuseDuration);

    // Sticky Paddle
    m_sticky.Level             = PlayerPrefs::GetInt("PowerUp_Sticky_Level", m_sticky.Level);
    m_sticky.BaseDuration      = 12.0f;
    m_sticky.DurationPerLevel  = 2.0f;
    m_sticky.AutoLaunchDelay   = 3.0f;
    m_sticky.BaseBoostPercent  = 25.0f;
    m_sticky.BoostPercentPerLevel = 15.0f;

    // Bottom Barrier
    m_barrier.Level            = PlayerPrefs::GetInt("PowerUp_Barrier_Level", m_barrier.Level);
    m_barrier.BaseDuration     = PlayerPrefs::GetFloat("PowerUp_Barrier_BaseDuration", m_barrier.BaseDuration);
    m_barrier.DurationPerLevel = PlayerPrefs::GetFloat("PowerUp_Barrier_DurationPerLvl", m_barrier.DurationPerLevel);
    m_barrier.BaseCharges      = PlayerPrefs::GetInt("PowerUp_Barrier_BaseCharges", m_barrier.BaseCharges);
    m_barrier.ChargesPerLevel  = PlayerPrefs::GetInt("PowerUp_Barrier_ChargesPerLvl", m_barrier.ChargesPerLevel);

    // Magnet Paddle
    m_magnet.Level             = PlayerPrefs::GetInt("PowerUp_Magnet_Level", m_magnet.Level);
    m_magnet.BaseDuration      = 10.0f;
    m_magnet.DurationPerLevel  = 2.5f;
    m_magnet.MagnetRadius      = 850.0f;
    m_magnet.BasePullSpeed     = 380.0f;
    m_magnet.PullSpeedPerLevel = 70.0f;
    m_magnet.BaseBallPullForce = 240.0f;
    m_magnet.BallPullForcePerLevel = 80.0f;

    // Ghost Ball
    m_ghost.Level              = PlayerPrefs::GetInt("PowerUp_Ghost_Level", m_ghost.Level);
    m_ghost.BaseDuration       = 5.0f;
    m_ghost.DurationPerLevel   = 1.0f;
    m_ghost.Alpha              = PlayerPrefs::GetFloat("PowerUp_Ghost_Alpha", m_ghost.Alpha);

    // Item Brick Regen
    m_itemRegen.Level          = PlayerPrefs::GetInt("PowerUp_ItemRegen_Level", m_itemRegen.Level);
    m_itemRegen.BaseCooldown   = PlayerPrefs::GetFloat("PowerUp_ItemRegen_BaseCooldown", m_itemRegen.BaseCooldown);
    m_itemRegen.CooldownReductionPerLevel = PlayerPrefs::GetFloat("PowerUp_ItemRegen_CooldownReduction", m_itemRegen.CooldownReductionPerLevel);
}

void PowerUpManager::SaveToPrefs()
{
    PlayerPrefs::SetInt("Economy_Coins", m_coins);

    // MultiBall
    PlayerPrefs::SetInt("PowerUp_MultiBall_Level", m_multiBall.Level);
    PlayerPrefs::SetInt("PowerUp_MultiBall_BaseExtra", m_multiBall.BaseExtraBalls);
    PlayerPrefs::SetInt("PowerUp_MultiBall_MaxTotal", m_multiBall.MaxTotalBalls);
    PlayerPrefs::SetFloat("PowerUp_MultiBall_SplitAngle", m_multiBall.SplitAngleDeg);

    // Expand
    PlayerPrefs::SetInt("PowerUp_Expand_Level", m_expand.Level);
    PlayerPrefs::SetFloat("PowerUp_Expand_BaseDuration", m_expand.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Expand_DurationPerLvl", m_expand.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Expand_BaseScale", m_expand.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Expand_ScaleStep", m_expand.ScaleStepPerLevel);

    // Shrink
    PlayerPrefs::SetInt("PowerUp_Shrink_Level", m_shrink.Level);
    PlayerPrefs::SetFloat("PowerUp_Shrink_BaseDuration", m_shrink.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Shrink_DurationPerLvl", m_shrink.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Shrink_BaseScale", m_shrink.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Shrink_SpeedBonus", m_shrink.BaseSpeedBonusPercent);
    PlayerPrefs::SetFloat("PowerUp_Shrink_SpeedStep", m_shrink.SpeedBonusStepPerLevel);

    // Storm (Thunderstorm)
    PlayerPrefs::SetInt("PowerUp_Storm_Level", m_storm.Level);
    PlayerPrefs::SetFloat("PowerUp_Storm_StrikeRadius", m_storm.StrikeRadius);
    // Backward compatibility sync
    PlayerPrefs::SetInt("PowerUp_Laser_Level", m_storm.Level);
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseDuration", static_cast<float>(m_storm.GetEffectiveStrikes()));
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseCooldown", m_storm.GetEffectiveCooldown());
    PlayerPrefs::SetFloat("PowerUp_Laser_ProjSpeed", m_storm.ProjectileSpeed);

    // Tempo
    PlayerPrefs::SetInt("PowerUp_Tempo_Level", m_tempo.Level);
    PlayerPrefs::SetFloat("PowerUp_Tempo_BaseDuration", m_tempo.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Tempo_DurationPerLvl", m_tempo.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Tempo_FastSpeed", m_tempo.FastSpeed);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SpeedBonus", m_tempo.SpeedBonusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowSpeed", m_tempo.SlowSpeed);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowZoneTopY", m_tempo.SlowZoneTopY);
    PlayerPrefs::SetFloat("PowerUp_Tempo_SlowZoneBottomY", m_tempo.SlowZoneBottomY);

    // Extra Life
    PlayerPrefs::SetInt("PowerUp_Life_Level", m_life.Level);
    PlayerPrefs::SetInt("PowerUp_Life_Granted", m_life.LivesGranted);
    PlayerPrefs::SetInt("PowerUp_Life_Cap", m_life.MaxLivesCap);

    // Big Ball
    PlayerPrefs::SetInt("PowerUp_Big_Level", m_big.Level);
    PlayerPrefs::SetFloat("PowerUp_Big_BaseDuration", m_big.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Big_DurationPerLvl", m_big.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Big_BaseScale", m_big.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Big_ScaleBonus", m_big.ScaleBonusPerLevel);
    PlayerPrefs::SetBool("PowerUp_Big_PiercesBricks", m_big.PiercesBricks);
    PlayerPrefs::SetBool("PowerUp_Big_OneHitKill", m_big.OneHitKill);

    // Fire Ball
    PlayerPrefs::SetInt("PowerUp_Fire_Level", m_fireBall.Level);
    PlayerPrefs::SetFloat("PowerUp_Fire_BaseScale", m_fireBall.BaseScale);
    PlayerPrefs::SetFloat("PowerUp_Fire_ScaleBonus", m_fireBall.ScaleBonusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Fire_BaseAoE", m_fireBall.BaseAoERadius);
    PlayerPrefs::SetFloat("PowerUp_Fire_AoEBonus", m_fireBall.AoERadiusPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Fire_FuseDuration", m_fireBall.FuseDuration);

    // Sticky Paddle
    PlayerPrefs::SetInt("PowerUp_Sticky_Level", m_sticky.Level);
    PlayerPrefs::SetFloat("PowerUp_Sticky_BaseDuration", m_sticky.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Sticky_DurationPerLvl", m_sticky.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Sticky_AutoLaunch", m_sticky.AutoLaunchDelay);

    // Bottom Barrier
    PlayerPrefs::SetInt("PowerUp_Barrier_Level", m_barrier.Level);
    PlayerPrefs::SetFloat("PowerUp_Barrier_BaseDuration", m_barrier.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Barrier_DurationPerLvl", m_barrier.DurationPerLevel);
    PlayerPrefs::SetInt("PowerUp_Barrier_BaseCharges", m_barrier.BaseCharges);
    PlayerPrefs::SetInt("PowerUp_Barrier_ChargesPerLvl", m_barrier.ChargesPerLevel);

    // Magnet Paddle
    PlayerPrefs::SetInt("PowerUp_Magnet_Level", m_magnet.Level);
    PlayerPrefs::SetFloat("PowerUp_Magnet_BaseDuration", m_magnet.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Magnet_DurationPerLvl", m_magnet.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Magnet_Radius", m_magnet.MagnetRadius);
    PlayerPrefs::SetFloat("PowerUp_Magnet_BasePull", m_magnet.BasePullSpeed);
    PlayerPrefs::SetFloat("PowerUp_Magnet_PullBonus", m_magnet.PullSpeedPerLevel);

    // Ghost Ball
    PlayerPrefs::SetInt("PowerUp_Ghost_Level", m_ghost.Level);
    PlayerPrefs::SetFloat("PowerUp_Ghost_BaseDuration", m_ghost.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Ghost_DurationPerLvl", m_ghost.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Ghost_Alpha", m_ghost.Alpha);

    // Item Brick Regen
    PlayerPrefs::SetInt("PowerUp_ItemRegen_Level", m_itemRegen.Level);
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

    SaveToPrefs();
}

std::string PowerUpManager::GetName(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return "MULTI-BALL";
        case PowerUpType::ExpandPaddle: return "EXPAND PADDLE";
        case PowerUpType::ShrinkPaddle: return "SHRINK PADDLE";
        case PowerUpType::Storm:        return "STORM THUNDER";
        case PowerUpType::TempoBall:    return "TEMPO BALL";
        case PowerUpType::ExtraLife:    return "EXTRA LIFE";
        case PowerUpType::BigBall:      return "BIG CRUSH BALL";
        case PowerUpType::FireBall:     return "FIRE BALL AOE";
        case PowerUpType::StickyPaddle: return "STICKY CATCH";
        case PowerUpType::BottomBarrier:return "BOTTOM FORCEFIELD";
        case PowerUpType::MagnetPaddle: return "MAGNET PADDLE";
        case PowerUpType::GhostBall:    return "GHOST PHASE BALL";
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
            return "Expands paddle width (+20%/lvl up to +100%). Lucky activation: 10% to 50% chance to drop 1 to 3 bonus power-ups (+1 at lvl 3 & 5). Lasts 10s to 18s.";
        case PowerUpType::ShrinkPaddle:
            return "Paddle shrinks to 0.65x but paddle speed increases +20%/lvl (up to +100%). Lucky activation: 10% to 50% chance to drop 1 to 3 bonus power-ups (+1 at lvl 3 & 5). Lasts 10s to 18s.";
        case PowerUpType::Storm:
            return "Strikes thunderbolts damaging bricks & conducting through metal circuits.";
        case PowerUpType::TempoBall:
            return "Dynamic pacing: ball speed +60% to +140%, paddle speed +10% to +50%, smoothly slows down approaching paddle.";
        case PowerUpType::ExtraLife:
            return "Instantly grants one reserve heart to the paddle (cap 6).";
        case PowerUpType::BigBall:
            return "Massive kinetic sphere piercing and shattering regular bricks on contact.";
        case PowerUpType::FireBall:
            return "Incendiary blast dealing explosive area-of-effect damage to adjacent bricks.";
        case PowerUpType::StickyPaddle:
            return "Catches ball on contact (3s hold). Rhythm launch: timing release grants speed boost (+25% to +85%). Lasts 12s to 20s.";
        case PowerUpType::BottomBarrier:
            return "Deploys a reflective laser bottom forcefield saving balls from draining.";
        case PowerUpType::MagnetPaddle:
            return "Gravitational forcefield drawing bonus capsules AND curving ball trajectories towards paddle. Lasts 10s to 20s.";
        case PowerUpType::GhostBall:
            return "Phases cleanly through tough and metal bricks without damage; shatters only bricks within damage power (1 dmg at lvl 1-2, 2 dmg at lvl 3-4, 3 dmg at lvl 5). Solidifies safely once clear of bricks. Absolute immortality.";
        case PowerUpType::ItemRegen:
            return "Reduces the respawn cooldown of item capsule bricks on the field.";
        default:
            return "";
    }
}

std::string PowerUpManager::GetCurrentTierInfo(PowerUpType type) const
{
    int lvl = GetLevel(type);
    switch (type)
    {
        case PowerUpType::MultiBall:
            return "+" + std::to_string(m_multiBall.GetBallsToSpawn()) + " extra balls per split (cap 12)";
        case PowerUpType::ExpandPaddle:
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Width: +%d%% (x%.2f), Duration: %.0fs, Lucky Drop: %.1f%% (%d capsule%s)",
                     static_cast<int>((m_expand.GetEffectiveScale() - 1.0f) * 100.0f + 0.5f),
                     m_expand.GetEffectiveScale(),
                     m_expand.GetEffectiveDuration(),
                     m_expand.GetEffectiveDropChance() * 100.0f,
                     m_expand.GetEffectiveDropCount(),
                     m_expand.GetEffectiveDropCount() > 1 ? "s" : "");
            return std::string(buf);
        }
        case PowerUpType::ShrinkPaddle:
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Scale: 65%%, Speed: +%d%% (%.0f px/s), Duration: %.0fs, Lucky Drop: %.1f%% (%d capsule%s)",
                     static_cast<int>(m_shrink.GetEffectivePaddleSpeedBonusPercent()),
                     m_shrink.GetEffectivePaddleSpeed(),
                     m_shrink.GetEffectiveDuration(),
                     m_shrink.GetEffectiveDropChance() * 100.0f,
                     m_shrink.GetEffectiveDropCount(),
                     m_shrink.GetEffectiveDropCount() > 1 ? "s" : "");
            return std::string(buf);
        }
        case PowerUpType::Storm:
            if (lvl >= 5) return "Tempest Mode: 10s continuous barrage (60 strikes)";
            return std::to_string(m_storm.GetEffectiveStrikes()) + " strikes, " + std::to_string(m_storm.GetEffectiveDamage()) + " dmg/hit";
        case PowerUpType::TempoBall:
            return "Ball: +" + std::to_string(static_cast<int>(m_tempo.GetEffectiveBonusPercent())) + "% (" + std::to_string(static_cast<int>(m_tempo.GetEffectiveFastSpeed())) + " px/s), Paddle: +" + std::to_string(static_cast<int>(m_tempo.GetEffectivePaddleSpeedBonusPercent())) + "%, Duration: " + std::to_string(static_cast<int>(m_tempo.GetEffectiveDuration())) + "s";
        case PowerUpType::ExtraLife:
            return "+1 Heart (Capacity: " + std::to_string(m_life.MaxLivesCap) + ")";
        case PowerUpType::BigBall:
            return "Scale: " + std::to_string(static_cast<int>(m_big.GetEffectiveScale() * 100.0f)) + "%, Duration: " + std::to_string(static_cast<int>(m_big.GetEffectiveDuration())) + "s";
        case PowerUpType::FireBall:
            return "AoE Radius: " + std::to_string(static_cast<int>(m_fireBall.GetEffectiveAoERadius())) + " px, Scale: " + std::to_string(static_cast<int>(m_fireBall.GetEffectiveScale() * 100.0f)) + "%";
        case PowerUpType::StickyPaddle:
            return "Duration: " + std::to_string(static_cast<int>(m_sticky.GetEffectiveDuration())) + "s, Rhythm Boost: +" + std::to_string(static_cast<int>(m_sticky.GetEffectiveBoostPercent())) + "% speed (3s hold)";
        case PowerUpType::BottomBarrier:
            return "Charges: " + std::to_string(m_barrier.GetEffectiveCharges()) + " saves, Duration: " + std::to_string(static_cast<int>(m_barrier.GetEffectiveDuration())) + "s";
        case PowerUpType::MagnetPaddle:
            return "Duration: " + std::to_string(static_cast<int>(m_magnet.GetEffectiveDuration())) + "s, Ball Pull: " + std::to_string(static_cast<int>(m_magnet.GetEffectiveBallPullForce())) + " px/s², Capsule: " + std::to_string(static_cast<int>(m_magnet.GetEffectivePullSpeed())) + " px/s";
        case PowerUpType::GhostBall:
            return "Duration: " + std::to_string(static_cast<int>(m_ghost.GetEffectiveDuration())) + "s, Damage: " + std::to_string(m_ghost.GetEffectiveDamage()) + " dmg (Immortal & Selective Phase)";
        case PowerUpType::ItemRegen:
            return "Cooldown: " + std::to_string(static_cast<int>(m_itemRegen.GetEffectiveCooldown())) + "s";
        default:
            return "";
    }
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
            return "Next: Increases recovery reliability";
        case PowerUpType::BigBall:
            return "Next: +" + std::to_string(static_cast<int>(m_big.DurationPerLevel)) + "s duration, +" + std::to_string(static_cast<int>(m_big.ScaleBonusPerLevel * 100.0f)) + "% size";
        case PowerUpType::FireBall:
            return "Next: +" + std::to_string(static_cast<int>(m_fireBall.AoERadiusPerLevel)) + " px radius, +" + std::to_string(static_cast<int>(m_fireBall.ScaleBonusPerLevel * 100.0f)) + "% size";
        case PowerUpType::StickyPaddle:
            return "Next: +15% rhythm launch boost, +2s duration";
        case PowerUpType::BottomBarrier:
            return "Next: +1 charge save, +" + std::to_string(static_cast<int>(m_barrier.DurationPerLevel)) + "s duration";
        case PowerUpType::MagnetPaddle:
            return "Next: +80 px/s² ball pull force, +70 px/s capsule pull, +2.5s duration";
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
