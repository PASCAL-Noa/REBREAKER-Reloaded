#include "PowerUps/PowerUpConfig.h"
#include "Core/PlayerPrefs.h"

PowerUpManager& PowerUpManager::Get()
{
    static PowerUpManager instance;
    return instance;
}

PowerUpManager::PowerUpManager()
{
    // Configure default differences for shrink vs expand
    m_shrink.BaseScale = 0.65f;
    m_shrink.ScaleStepPerLevel = -0.05f;
    m_shrink.BaseDuration = 8.0f;
    m_shrink.DurationPerLevel = 1.0f;

    LoadFromPrefs();
}

int PowerUpManager::GetLevel(PowerUpType type) const
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return m_multiBall.Level;
        case PowerUpType::ExpandPaddle: return m_expand.Level;
        case PowerUpType::ShrinkPaddle: return m_shrink.Level;
        case PowerUpType::LaserPaddle:  return m_laser.Level;
        case PowerUpType::TempoBall:    return m_tempo.Level;
        case PowerUpType::ExtraLife:    return m_life.Level;
        case PowerUpType::BigBall:      return m_big.Level;
        case PowerUpType::FireBall:     return m_fireBall.Level;
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
        case PowerUpType::LaserPaddle:  return m_laser.MaxLevel;
        case PowerUpType::TempoBall:    return m_tempo.MaxLevel;
        case PowerUpType::ExtraLife:    return m_life.MaxLevel;
        case PowerUpType::BigBall:      return m_big.MaxLevel;
        case PowerUpType::FireBall:     return m_fireBall.MaxLevel;
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
        case PowerUpType::LaserPaddle:  m_laser.Level = clamped; break;
        case PowerUpType::TempoBall:    m_tempo.Level = clamped; break;
        case PowerUpType::ExtraLife:    m_life.Level = clamped; break;
        case PowerUpType::BigBall:      m_big.Level = clamped; break;
        case PowerUpType::FireBall:     m_fireBall.Level = clamped; break;
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
        case PowerUpType::LaserPaddle:  return m_laser.GetUpgradeCost();
        case PowerUpType::TempoBall:    return m_tempo.GetUpgradeCost();
        case PowerUpType::ExtraLife:    return m_life.GetUpgradeCost();
        case PowerUpType::BigBall:      return m_big.GetUpgradeCost();
        case PowerUpType::FireBall:     return m_fireBall.GetUpgradeCost();
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
    m_expand.BaseDuration      = PlayerPrefs::GetFloat("PowerUp_Expand_BaseDuration", m_expand.BaseDuration);
    m_expand.DurationPerLevel  = PlayerPrefs::GetFloat("PowerUp_Expand_DurationPerLvl", m_expand.DurationPerLevel);
    m_expand.BaseScale         = PlayerPrefs::GetFloat("PowerUp_Expand_BaseScale", m_expand.BaseScale);
    m_expand.ScaleStepPerLevel = PlayerPrefs::GetFloat("PowerUp_Expand_ScaleStep", m_expand.ScaleStepPerLevel);

    // Shrink Paddle
    m_shrink.Level             = PlayerPrefs::GetInt("PowerUp_Shrink_Level", m_shrink.Level);
    m_shrink.BaseDuration      = PlayerPrefs::GetFloat("PowerUp_Shrink_BaseDuration", m_shrink.BaseDuration);
    m_shrink.DurationPerLevel  = PlayerPrefs::GetFloat("PowerUp_Shrink_DurationPerLvl", m_shrink.DurationPerLevel);
    m_shrink.BaseScale         = PlayerPrefs::GetFloat("PowerUp_Shrink_BaseScale", m_shrink.BaseScale);
    m_shrink.ScaleStepPerLevel = PlayerPrefs::GetFloat("PowerUp_Shrink_ScaleStep", m_shrink.ScaleStepPerLevel);

    // Laser Paddle
    m_laser.Level                     = PlayerPrefs::GetInt("PowerUp_Laser_Level", m_laser.Level);
    m_laser.BaseDuration              = PlayerPrefs::GetFloat("PowerUp_Laser_BaseDuration", m_laser.BaseDuration);
    m_laser.DurationPerLevel          = PlayerPrefs::GetFloat("PowerUp_Laser_DurationPerLvl", m_laser.DurationPerLevel);
    m_laser.BaseCooldown              = PlayerPrefs::GetFloat("PowerUp_Laser_BaseCooldown", m_laser.BaseCooldown);
    m_laser.CooldownReductionPerLevel = PlayerPrefs::GetFloat("PowerUp_Laser_CooldownRed", m_laser.CooldownReductionPerLevel);
    m_laser.ProjectileSpeed           = PlayerPrefs::GetFloat("PowerUp_Laser_ProjSpeed", m_laser.ProjectileSpeed);

    // Tempo Ball
    m_tempo.Level              = PlayerPrefs::GetInt("PowerUp_Tempo_Level", m_tempo.Level);
    m_tempo.BaseDuration       = PlayerPrefs::GetFloat("PowerUp_Tempo_BaseDuration", m_tempo.BaseDuration);
    m_tempo.DurationPerLevel   = PlayerPrefs::GetFloat("PowerUp_Tempo_DurationPerLvl", m_tempo.DurationPerLevel);
    m_tempo.FastSpeed          = PlayerPrefs::GetFloat("PowerUp_Tempo_FastSpeed", m_tempo.FastSpeed);
    m_tempo.SpeedBonusPerLevel = PlayerPrefs::GetFloat("PowerUp_Tempo_SpeedBonus", m_tempo.SpeedBonusPerLevel);
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
    PlayerPrefs::SetFloat("PowerUp_Shrink_ScaleStep", m_shrink.ScaleStepPerLevel);

    // Laser
    PlayerPrefs::SetInt("PowerUp_Laser_Level", m_laser.Level);
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseDuration", m_laser.BaseDuration);
    PlayerPrefs::SetFloat("PowerUp_Laser_DurationPerLvl", m_laser.DurationPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Laser_BaseCooldown", m_laser.BaseCooldown);
    PlayerPrefs::SetFloat("PowerUp_Laser_CooldownRed", m_laser.CooldownReductionPerLevel);
    PlayerPrefs::SetFloat("PowerUp_Laser_ProjSpeed", m_laser.ProjectileSpeed);

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

    PlayerPrefs::Save();
}

void PowerUpManager::ResetToDefaults()
{
    m_multiBall = MultiBallConfig{};
    m_expand    = PaddleSizeConfig{};
    m_shrink    = PaddleSizeConfig{};
    m_shrink.BaseScale = 0.65f;
    m_shrink.ScaleStepPerLevel = -0.05f;
    m_shrink.BaseDuration = 8.0f;
    m_shrink.DurationPerLevel = 1.0f;
    m_laser     = LaserConfig{};
    m_tempo     = TempoBallConfig{};
    m_life      = ExtraLifeConfig{};
    m_big       = BigBallConfig{};
    m_fireBall  = FireBallConfig{};
    m_coins     = 0;

    SaveToPrefs();
}
