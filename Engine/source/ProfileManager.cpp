#include "Core/ProfileManager.h"
#include "Core/PlayerPrefs.h"
#include "PowerUps/PowerUpConfig.h"
#include <algorithm>

ProfileManager& ProfileManager::Get()
{
    static ProfileManager instance;
    return instance;
}

ProfileManager::ProfileManager()
{
    InitCosmetics();
}

void ProfileManager::InitCosmetics()
{
    m_cosmetics = {
        {"paddle_neon_cyan", "Neon Cyber", "Sleek retro-futuristic cyan paddle hull", CosmeticSlot::PaddleSkin, 0, true},
        {"paddle_gold_plating", "Gold Armor", "Gleaming polished gold champion chassis", CosmeticSlot::PaddleSkin, 500, false},
        {"paddle_obsidian", "Void Obsidian", "Matte dark stealth frame infused with void core", CosmeticSlot::PaddleSkin, 1000, false},
        {"trail_plasma_blue", "Plasma Ribbon", "Electric blue energetic particle ribbon", CosmeticSlot::BallTrail, 0, true},
        {"trail_inferno_sparks", "Inferno Sparks", "Blazing fiery trail particles leaving heat trails", CosmeticSlot::BallTrail, 600, false},
        {"trail_rainbow_warp", "Rainbow Warp", "Chromatic spectral light ribbon with prism flair", CosmeticSlot::BallTrail, 1200, false}
    };

    // Load unlocked states from PlayerPrefs
    for (auto& item : m_cosmetics)
    {
        if (item.Price == 0)
        {
            item.Unlocked = true;
        }
        else
        {
            item.Unlocked = PlayerPrefs::GetBool("Cosmetic_Unlocked_" + item.Id, false);
        }
    }
}

std::string ProfileManager::GetUsername() const
{
    std::string name = PlayerPrefs::GetString("PlayerProfile_Username", "PLAYER");
    if (name.empty()) name = "PLAYER";
    return name;
}

void ProfileManager::SetUsername(const std::string& name)
{
    std::string trimmed = name;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    if (trimmed.find_last_not_of(" \t\n\r") != std::string::npos)
    {
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    }
    if (trimmed.empty())
    {
        trimmed = "PLAYER";
    }
    if (trimmed.length() > 16)
    {
        trimmed = trimmed.substr(0, 16);
    }

    PlayerPrefs::SetString("PlayerProfile_Username", trimmed);
    PlayerPrefs::Save();
}

uint32_t ProfileManager::ConvertScoreToCredits(uint32_t score)
{
    if (score == 0) return 0;
    uint32_t credits = score / 50;
    return (credits == 0) ? 1 : credits;
}

uint32_t ProfileManager::GetLifetimeScore() const
{
    return static_cast<uint32_t>(std::max(0, PlayerPrefs::GetInt("Economy_LifetimeScore", 0)));
}

void ProfileManager::AddLifetimeScore(uint32_t score)
{
    uint32_t current = GetLifetimeScore();
    PlayerPrefs::SetInt("Economy_LifetimeScore", static_cast<int>(current + score));
    PlayerPrefs::Save();
}

uint32_t ProfileManager::GetLifetimeCredits() const
{
    return static_cast<uint32_t>(std::max(0, PlayerPrefs::GetInt("Economy_LifetimeCredits", 0)));
}

void ProfileManager::AddLifetimeCredits(uint32_t credits)
{
    uint32_t current = GetLifetimeCredits();
    PlayerPrefs::SetInt("Economy_LifetimeCredits", static_cast<int>(current + credits));
    PlayerPrefs::Save();
}

UltimateAbilityType ProfileManager::GetEquippedUltimate() const
{
    int val = PlayerPrefs::GetInt("Profile_EquippedUltimate", 0);
    if (val < 0 || val >= static_cast<int>(UltimateAbilityType::Count))
    {
        val = 0;
    }
    return static_cast<UltimateAbilityType>(val);
}

void ProfileManager::SetEquippedUltimate(UltimateAbilityType type)
{
    int val = static_cast<int>(type);
    if (val < 0 || val >= static_cast<int>(UltimateAbilityType::Count))
    {
        val = 0;
    }
    PlayerPrefs::SetInt("Profile_EquippedUltimate", val);
    PlayerPrefs::Save();
}

int ProfileManager::GetUltimateLevel(UltimateAbilityType type) const
{
    std::string key = "Profile_UltimateLevel_" + std::to_string(static_cast<int>(type));
    int lvl = PlayerPrefs::GetInt(key, 1);
    return std::clamp(lvl, 1, 5);
}

void ProfileManager::SetUltimateLevel(UltimateAbilityType type, int level)
{
    std::string key = "Profile_UltimateLevel_" + std::to_string(static_cast<int>(type));
    PlayerPrefs::SetInt(key, std::clamp(level, 1, 5));
    PlayerPrefs::Save();
}

int ProfileManager::GetUltimateMaxLevel(UltimateAbilityType /*type*/) const
{
    return 5;
}

int ProfileManager::GetUltimateUpgradeCost(UltimateAbilityType type) const
{
    int lvl = GetUltimateLevel(type);
    if (lvl >= 5) return 0;
    constexpr int kCosts[4] = { 250, 400, 650, 950 };
    return kCosts[std::clamp(lvl - 1, 0, 3)];
}

bool ProfileManager::CanUpgradeUltimate(UltimateAbilityType type) const
{
    int lvl = GetUltimateLevel(type);
    if (lvl >= 5) return false;
    return PowerUpManager::Get().GetCoins() >= GetUltimateUpgradeCost(type);
}

bool ProfileManager::UpgradeUltimate(UltimateAbilityType type)
{
    if (!CanUpgradeUltimate(type)) return false;
    int cost = GetUltimateUpgradeCost(type);
    if (PowerUpManager::Get().SpendCoins(cost))
    {
        SetUltimateLevel(type, GetUltimateLevel(type) + 1);
        return true;
    }
    return false;
}

std::string ProfileManager::GetUltimateName(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "PLASMA ORB";
        case UltimateAbilityType::DiscoBall:
            return "DISCO BALL";
        case UltimateAbilityType::PowerUpRain:
            return "POWER-UP RAIN";
        default:
            return "PLASMA ORB";
    }
}

std::string ProfileManager::GetUltimateCategory(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "[DESTRUCTION / CHAIN REACTION]";
        case UltimateAbilityType::DiscoBall:
            return "[ACOUSTIC SHOCKWAVE / LIGHT SHOW]";
        case UltimateAbilityType::PowerUpRain:
            return "[SUPPORT / ORBITAL DROP]";
        default:
            return "[ULTIMATE ABILITY]";
    }
}

std::string ProfileManager::GetUltimateDescription(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "Launches undulating blue-violet plasma orbs that vaporize bricks and split into cascading energy spheres.";
        case UltimateAbilityType::DiscoBall:
            return "Ignites disco fever: devastating shockwaves, rainbow spotlights, dancing big balls and a giant mirror ball climax.";
        case UltimateAbilityType::PowerUpRain:
            return "Summons a massive orbital cascade of upgraded enhancement capsules.";
        default:
            return "";
    }
}

std::string ProfileManager::GetUltimateTierInfo(UltimateAbilityType type, int level)
{
    int safeLvl = std::clamp(level, 1, 5);
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            switch (safeLvl)
            {
                case 1: return "1 plasma orb, splits into 2 small explosive orbs";
                case 2: return "2 plasma orbs, each splits into 2 small orbs";
                case 3: return "2 enlarged plasma orbs, splits into 4 then 2 orbs";
                case 4: return "3 enlarged plasma orbs, splits into 4 then 2 orbs";
                case 5: return "3 mega plasma orbs, splits into 8 then 4 then 2";
                default: break;
            }
            break;
        case UltimateAbilityType::DiscoBall:
            switch (safeLvl)
            {
                case 1: return "Shockwave 50% height, 1 dmg, 2 big balls & funk beat";
                case 2: return "Shockwave 60% height, 1 dmg, 3 orbiting big balls";
                case 3: return "Shockwave 70% height, 2 dmg, 4 orbiting big balls";
                case 4: return "Shockwave 80% height, 2 dmg, 2 extra shockwaves, 5 big balls";
                case 5: return "Shockwave 90% height, 6 big balls & Mirror Ball unleash (5s)";
                default: break;
            }
            break;
        case UltimateAbilityType::PowerUpRain:
            switch (safeLvl)
            {
                case 1: return "Cascade of 5 to 12 power-ups (levels 1 to 3)";
                case 2: return "Cascade of 10 to 24 power-ups (levels 1 to 3)";
                case 3: return "Cascade of 15 to 36 power-ups (levels 1 to 4)";
                case 4: return "Cascade of 20 to 48 power-ups (levels 2 to 5)";
                case 5: return "Cascade of 25 to 60 power-ups (levels 2 to 5)";
                default: break;
            }
            break;
        default:
            break;
    }
    return "Standard Tier";
}

const std::vector<CosmeticItem>& ProfileManager::GetAvailableCosmetics()
{
    return m_cosmetics;
}

bool ProfileManager::IsCosmeticUnlocked(const std::string& id) const
{
    for (const auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            return item.Unlocked;
        }
    }
    return false;
}

bool ProfileManager::UnlockCosmetic(const std::string& id)
{
    for (auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            if (item.Unlocked) return true;
            if (PowerUpManager::Get().SpendCoins(item.Price))
            {
                item.Unlocked = true;
                PlayerPrefs::SetBool("Cosmetic_Unlocked_" + id, true);
                PlayerPrefs::Save();
                return true;
            }
            return false;
        }
    }
    return false;
}

std::string ProfileManager::GetEquippedCosmetic(CosmeticSlot slot) const
{
    std::string key = (slot == CosmeticSlot::PaddleSkin) ? "Equipped_PaddleSkin" :
                      (slot == CosmeticSlot::BallTrail)   ? "Equipped_BallTrail" :
                                                            "Equipped_SoundPack";
    std::string fallback = (slot == CosmeticSlot::PaddleSkin) ? "paddle_neon_cyan" :
                           (slot == CosmeticSlot::BallTrail)   ? "trail_plasma_blue" :
                                                                 "default_audio";
    return PlayerPrefs::GetString(key, fallback);
}

void ProfileManager::EquipCosmetic(CosmeticSlot slot, const std::string& id)
{
    if (!IsCosmeticUnlocked(id)) return;
    std::string key = (slot == CosmeticSlot::PaddleSkin) ? "Equipped_PaddleSkin" :
                      (slot == CosmeticSlot::BallTrail)   ? "Equipped_BallTrail" :
                                                            "Equipped_SoundPack";
    PlayerPrefs::SetString(key, id);
    PlayerPrefs::Save();
}

void ProfileManager::ResetToDefaults()
{
    PlayerPrefs::SetString("PlayerProfile_Username", "PLAYER");
    PlayerPrefs::SetInt("Economy_LifetimeScore", 0);
    PlayerPrefs::SetInt("Economy_LifetimeCredits", 0);
    PlayerPrefs::SetInt("Profile_EquippedUltimate", 0);
    for (int i = 0; i < static_cast<int>(UltimateAbilityType::Count); ++i)
    {
        PlayerPrefs::SetInt("Profile_UltimateLevel_" + std::to_string(i), 1);
    }
    PlayerPrefs::SetString("Equipped_PaddleSkin", "paddle_neon_cyan");
    PlayerPrefs::SetString("Equipped_BallTrail", "trail_plasma_blue");

    for (auto& item : m_cosmetics)
    {
        item.Unlocked = (item.Price == 0);
        PlayerPrefs::SetBool("Cosmetic_Unlocked_" + item.Id, item.Unlocked);
    }
    PlayerPrefs::Save();
}
