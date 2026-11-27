#pragma once

#include "Data/Color.h"
#include <string>
#include <vector>
#include <cstdint>

enum class UltimateAbilityType : int
{
    PlasmaOrb = 0,
    DiscoBall = 1,
    PowerUpRain = 2,
    Count = 3,

    // Aliases for compatibility
    NovaShockwave = 0,
    ThunderTempest = 1,
    TimeStasis = 2
};

enum class CosmeticSlot : int
{
    PaddleSkin = 0,
    BallTrail = 1,
    SoundPack = 2,
    PrimaryColor = 3,
    SecondaryColor = 4,
    BallSkin = 5,
    DeathEffect = 6,
    Count = 7
};

enum class CosmeticUnlockType : uint8_t
{
    Default = 0,
    Shop = 1,
    Achievement = 2
};

enum class PaddleSkinStyle : uint8_t
{
    ClassicSprite = 0,
    Cyber = 1,
    Neon = 2,
    RetroWood = 3,
    Golden = 4,
    Steampunk = 5,
    VoidObsidian = 6,
    PrismStriker = 7
};

enum class BallSkinStyle : uint8_t
{
    ClassicSprite = 0,
    ClassicCore = 1,
    CyberGyro = 2,
    NovaStar = 3,
    RetroPixel = 4,
    NeonSawblade = 5,
    ClockworkGear = 6,
    OmegaCore = 7
};

enum class TrailStyle : uint8_t
{
    ClassicWhite = 0,
    Ribbon = 1,
    Stardust = 2,
    Flame = 3,
    MatrixBinary = 4,
    Rainbow = 5,
    ArcPulse = 6
};

enum class DeathEffectStyle : uint8_t
{
    ClassicMinimal = 0,
    ClassicBurst = 1,
    ShockwaveRing = 2,
    PixelShatter = 3,
    PrismConfetti = 4,
    NovaImplosion = 5,
    MatrixGlitch = 6
};

enum class EndlessPerkType : int
{
    InitialRerolls = 0,
    ShieldRecovery = 1,
    ScoreMultiplier = 2,
    StartingLives = 3,
    Count = 4
};

struct CosmeticItem
{
    std::string        Id;
    std::string        Name;
    std::string        Description;
    CosmeticSlot       Slot = CosmeticSlot::PaddleSkin;
    int                Price = 0;
    bool               Unlocked = false;
    CosmeticUnlockType UnlockType = CosmeticUnlockType::Default;
    std::string        UnlockRequirement;
    Color              SwatchColor{255, 255, 255, 255};
    Color              SecondaryPreview{120, 220, 255, 255};
};

struct CosmeticMilestoneProgress
{
    int         Current = 0;
    int         Target = 1;
    std::string UnitLabel;
    bool        IsAchieved() const noexcept { return Current >= Target; }
    float       GetRatio() const noexcept
    {
        if (Target <= 0) return 1.0f;
        float r = static_cast<float>(Current) / static_cast<float>(Target);
        return (r < 0.0f) ? 0.0f : (r > 1.0f ? 1.0f : r);
    }
};

class ProfileManager
{
public:
    static ProfileManager& Get();

    // Persistence (SaveData/profile.json synced with PlayerPrefs)
    static void        SetCustomProfilePath(const std::string& path);
    static std::string GetProfileFilePath();
    void               LoadProfile();
    void               SaveProfile() const;

    // Player Nickname
    [[nodiscard]] std::string GetUsername() const;
    void SetUsername(const std::string& name);

    // Score to Credits conversion
    static uint32_t ConvertScoreToCredits(uint32_t score);

    // Career / Lifetime stats
    [[nodiscard]] uint32_t GetLifetimeScore() const;
    void AddLifetimeScore(uint32_t score);

    [[nodiscard]] uint32_t GetLifetimeCredits() const;
    void AddLifetimeCredits(uint32_t credits);

    // Ultimate Ability Selection & Upgrades (Levels 1 to 5)
    [[nodiscard]] UltimateAbilityType GetEquippedUltimate() const;
    void SetEquippedUltimate(UltimateAbilityType type);

    [[nodiscard]] int GetUltimateLevel(UltimateAbilityType type) const;
    void SetUltimateLevel(UltimateAbilityType type, int level);
    [[nodiscard]] int GetUltimateMaxLevel(UltimateAbilityType type) const;
    [[nodiscard]] int GetUltimateUpgradeCost(UltimateAbilityType type) const;
    [[nodiscard]] bool CanUpgradeUltimate(UltimateAbilityType type) const;
    bool UpgradeUltimate(UltimateAbilityType type);

    static std::string GetUltimateName(UltimateAbilityType type);
    static std::string GetUltimateCategory(UltimateAbilityType type);
    static std::string GetUltimateDescription(UltimateAbilityType type);
    static std::string GetUltimateTierInfo(UltimateAbilityType type, int level);

    // Cosmetics Architecture
    [[nodiscard]] const std::vector<CosmeticItem>& GetAvailableCosmetics();
    [[nodiscard]] std::vector<CosmeticItem>        GetCosmeticsBySlot(CosmeticSlot slot) const;
    [[nodiscard]] const CosmeticItem*              GetCosmeticById(const std::string& id) const;
    [[nodiscard]] bool                             IsCosmeticUnlocked(const std::string& id) const;
    bool                                           UnlockCosmetic(const std::string& id);
    void                                           GrantCosmetic(const std::string& id);
    [[nodiscard]] std::string                      GetEquippedCosmetic(CosmeticSlot slot) const;
    void                                           EquipCosmetic(CosmeticSlot slot, const std::string& id);
    [[nodiscard]] int                              GetUnlockedCosmeticCount(CosmeticSlot slot) const;
    [[nodiscard]] int                              GetTotalCosmeticCount(CosmeticSlot slot) const;
    [[nodiscard]] int                              GetTotalUnlockedCosmetics() const;
    [[nodiscard]] int                              GetTotalCosmetics() const;

    // Achievement Milestone Evaluation & Progress
    int                                            EvaluateAchievementUnlocks();
    [[nodiscard]] CosmeticMilestoneProgress        GetCosmeticMilestoneProgress(const std::string& id) const;

    // Equipped Visual Style & Color Resolvers
    [[nodiscard]] Color            GetEquippedPrimaryColor() const;
    [[nodiscard]] Color            GetEquippedSecondaryColor() const;
    [[nodiscard]] PaddleSkinStyle  GetEquippedPaddleSkinStyle() const;
    [[nodiscard]] BallSkinStyle    GetEquippedBallSkinStyle() const;
    [[nodiscard]] TrailStyle       GetEquippedTrailStyle() const;
    [[nodiscard]] DeathEffectStyle GetEquippedDeathEffectStyle() const;

    [[nodiscard]] Color            ResolveColorById(const std::string& id, Color fallback) const;
    static PaddleSkinStyle         ResolvePaddleSkinStyle(const std::string& id);
    static BallSkinStyle           ResolveBallSkinStyle(const std::string& id);
    static TrailStyle              ResolveTrailStyle(const std::string& id);
    static DeathEffectStyle        ResolveDeathEffectStyle(const std::string& id);
    static std::string             GetDefaultCosmeticId(CosmeticSlot slot);

    // Endless Mode Statistics
    [[nodiscard]] uint32_t    GetEndlessHighScore() const;
    [[nodiscard]] float       GetEndlessBestTime() const;
    [[nodiscard]] std::string GetEndlessBestTimeFormatted() const;
    [[nodiscard]] int         GetEndlessBestWaves() const;
    [[nodiscard]] int         GetEndlessTotalRuns() const;
    [[nodiscard]] int         GetEndlessTotalWaves() const;
    void                      RecordEndlessRun(float survivalTime, int wavesCleared, uint32_t score);

    // Dedicated Endless Mode Perks & Upgrades
    [[nodiscard]] int         GetEndlessPerkLevel(EndlessPerkType type) const;
    void                      SetEndlessPerkLevel(EndlessPerkType type, int level);
    [[nodiscard]] int         GetEndlessPerkMaxLevel(EndlessPerkType type) const;
    [[nodiscard]] int         GetEndlessPerkUpgradeCost(EndlessPerkType type) const;
    [[nodiscard]] bool        CanUpgradeEndlessPerk(EndlessPerkType type) const;
    bool                      UpgradeEndlessPerk(EndlessPerkType type);

    static std::string        GetEndlessPerkName(EndlessPerkType type);
    static std::string        GetEndlessPerkDescription(EndlessPerkType type);
    static std::string        GetEndlessPerkTierInfo(EndlessPerkType type, int level);

    void ResetToDefaults();

private:
    ProfileManager();
    void InitCosmetics();
    static std::string GetSlotPrefsKey(CosmeticSlot slot);

    std::vector<CosmeticItem> m_cosmetics;
    static std::string        s_customProfilePath;
};

