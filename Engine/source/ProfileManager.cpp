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

std::string ProfileManager::GetUltimateName(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::NovaShockwave:
            return "NOVA SHOCKWAVE";
        case UltimateAbilityType::ThunderTempest:
            return "THUNDER TEMPEST";
        case UltimateAbilityType::TimeStasis:
            return "TIME STASIS";
        default:
            return "NOVA SHOCKWAVE";
    }
}

std::string ProfileManager::GetUltimateDescription(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::NovaShockwave:
            return "Radial thermal energy pulse clearing hazards\nand fracturing bricks in 360 degrees.";
        case UltimateAbilityType::ThunderTempest:
            return "Overcharges the sky, unleashing an immediate\nmulti-target lightning barrage\nthrough conductive circuits.";
        case UltimateAbilityType::TimeStasis:
            return "Manipulates local gravity, slowing velocity\nby 75% for 6s for precision trajectory control.";
        default:
            return "";
    }
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
    PlayerPrefs::SetString("Equipped_PaddleSkin", "paddle_neon_cyan");
    PlayerPrefs::SetString("Equipped_BallTrail", "trail_plasma_blue");

    for (auto& item : m_cosmetics)
    {
        item.Unlocked = (item.Price == 0);
        PlayerPrefs::SetBool("Cosmetic_Unlocked_" + item.Id, item.Unlocked);
    }
    PlayerPrefs::Save();
}
