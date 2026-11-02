#pragma once

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
    Count = 3
};

struct CosmeticItem
{
    std::string Id;
    std::string Name;
    std::string Description;
    CosmeticSlot Slot = CosmeticSlot::PaddleSkin;
    int Price = 0;
    bool Unlocked = false;
};

class ProfileManager
{
public:
    static ProfileManager& Get();

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
    [[nodiscard]] bool IsCosmeticUnlocked(const std::string& id) const;
    bool UnlockCosmetic(const std::string& id);
    [[nodiscard]] std::string GetEquippedCosmetic(CosmeticSlot slot) const;
    void EquipCosmetic(CosmeticSlot slot, const std::string& id);

    void ResetToDefaults();

private:
    ProfileManager();
    void InitCosmetics();

    std::vector<CosmeticItem> m_cosmetics;
};
