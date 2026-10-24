#include <doctest/doctest.h>
#include "Core/ProfileManager.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/PlayerPrefs.h"
#include <filesystem>

struct WorkshopEconomyFixture
{
    std::filesystem::path prefsPath{"playerprefs.json"};
    std::filesystem::path backupPath{"playerprefs.json.econbackup"};
    bool hadOriginal = false;

    WorkshopEconomyFixture()
    {
        std::error_code ec;
        if (std::filesystem::exists(prefsPath, ec))
        {
            std::filesystem::copy_file(prefsPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
            hadOriginal = true;
        }
        PlayerPrefs::DeleteAll();
        ProfileManager::Get().ResetToDefaults();
        PowerUpManager::Get().ResetToDefaults();
    }

    ~WorkshopEconomyFixture()
    {
        std::error_code ec;
        std::filesystem::remove(prefsPath, ec);
        std::filesystem::remove("playerprefs.json.tmp", ec);

        if (hadOriginal)
        {
            std::filesystem::copy_file(backupPath, prefsPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(backupPath, ec);
            PlayerPrefs::Load();
            PowerUpManager::Get().LoadFromPrefs();
        }
    }
};

TEST_SUITE("Workshop, Economy & Player Profile Tests")
{
    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Player Profile Username management and persistence")
    {
        CHECK(ProfileManager::Get().GetUsername() == "PLAYER");

        ProfileManager::Get().SetUsername("Maverick");
        CHECK(ProfileManager::Get().GetUsername() == "Maverick");

        // Verify persistence in PlayerPrefs
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetString("PlayerProfile_Username") == "Maverick");

        // Whitespace trimming & empty fallback
        ProfileManager::Get().SetUsername("   ");
        CHECK(ProfileManager::Get().GetUsername() == "PLAYER");

        ProfileManager::Get().SetUsername("   AcePilot   ");
        CHECK(ProfileManager::Get().GetUsername() == "AcePilot");

        // Max length capping (16 characters)
        ProfileManager::Get().SetUsername("SuperLongPilotNameExceeding16Chars");
        CHECK(ProfileManager::Get().GetUsername().length() == 16);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Score to Currency banking conversion and career stats")
    {
        // 0 score gives 0 credits
        CHECK(ProfileManager::ConvertScoreToCredits(0) == 0);

        // Small scores > 0 give minimum 1 credit
        CHECK(ProfileManager::ConvertScoreToCredits(10) == 1);
        CHECK(ProfileManager::ConvertScoreToCredits(49) == 1);

        // Standard 50 pts per credit conversion
        CHECK(ProfileManager::ConvertScoreToCredits(50) == 1);
        CHECK(ProfileManager::ConvertScoreToCredits(1500) == 30);
        CHECK(ProfileManager::ConvertScoreToCredits(25000) == 500);

        // Lifetime stats tracking
        CHECK(ProfileManager::Get().GetLifetimeScore() == 0);
        CHECK(ProfileManager::Get().GetLifetimeCredits() == 0);

        ProfileManager::Get().AddLifetimeScore(12500);
        ProfileManager::Get().AddLifetimeCredits(250);

        CHECK(ProfileManager::Get().GetLifetimeScore() == 12500);
        CHECK(ProfileManager::Get().GetLifetimeCredits() == 250);

        ProfileManager::Get().AddLifetimeScore(5000);
        CHECK(ProfileManager::Get().GetLifetimeScore() == 17500);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Power-up upgrading, coin transactions and tier bounds")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();
        CHECK(pum.GetCoins() == 0);

        // Cannot upgrade without coins
        CHECK(!pum.CanUpgrade(PowerUpType::StickyPaddle));
        CHECK(!pum.Upgrade(PowerUpType::StickyPaddle));
        CHECK(pum.GetLevel(PowerUpType::StickyPaddle) == 1);

        // Add coins
        pum.AddCoins(500);
        CHECK(pum.GetCoins() == 500);
        CHECK(pum.CanUpgrade(PowerUpType::StickyPaddle));

        int costLvl1 = pum.GetUpgradeCost(PowerUpType::StickyPaddle);
        CHECK(costLvl1 == 140);

        // Perform upgrade
        bool upgraded = pum.Upgrade(PowerUpType::StickyPaddle);
        CHECK(upgraded == true);
        CHECK(pum.GetLevel(PowerUpType::StickyPaddle) == 2);
        CHECK(pum.GetCoins() == 500 - 140);

        // SpendCoins check
        CHECK(!pum.SpendCoins(10000));
        CHECK(pum.SpendCoins(100));
        CHECK(pum.GetCoins() == 500 - 140 - 100);

        // Upgrade until max level
        pum.AddCoins(2000);
        pum.SetLevel(PowerUpType::StickyPaddle, 5);
        CHECK(pum.GetLevel(PowerUpType::StickyPaddle) == 5);
        CHECK(!pum.CanUpgrade(PowerUpType::StickyPaddle));
        CHECK(!pum.Upgrade(PowerUpType::StickyPaddle));
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Item Brick Regeneration cooldown scaling")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // Level 1: default 30s
        CHECK(pum.ItemRegen().Level == 1);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(30.0f));

        // Level 2: 30 - 3.5 = 26.5s
        pum.SetLevel(PowerUpType::ItemRegen, 2);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(26.5f));

        // Level 3: 30 - 7.0 = 23.0s
        pum.SetLevel(PowerUpType::ItemRegen, 3);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(23.0f));

        // Level 5: 30 - 14.0 = 16.0s
        pum.SetLevel(PowerUpType::ItemRegen, 5);
        CHECK(pum.ItemRegen().GetEffectiveCooldown() == doctest::Approx(16.0f));
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Descriptive Metadata API for power-ups and tier milestones")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        for (auto type : {PowerUpType::MultiBall, PowerUpType::Storm, PowerUpType::StickyPaddle,
                          PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall,
                          PowerUpType::ItemRegen})
        {
            CHECK(!pum.GetName(type).empty());
            CHECK(!pum.GetDescription(type).empty());
            CHECK(!pum.GetCurrentTierInfo(type).empty());
            CHECK(!pum.GetNextTierInfo(type).empty());
        }

        // When max level reached
        pum.SetLevel(PowerUpType::GhostBall, 5);
        CHECK(pum.GetNextTierInfo(PowerUpType::GhostBall) == "MAX LEVEL REACHED");
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Ultimate Ability selection and persistence")
    {
        auto& pm = ProfileManager::Get();
        pm.ResetToDefaults();

        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::NovaShockwave);
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::NovaShockwave) == "NOVA SHOCKWAVE");
        CHECK(!ProfileManager::GetUltimateDescription(UltimateAbilityType::NovaShockwave).empty());

        pm.SetEquippedUltimate(UltimateAbilityType::ThunderTempest);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::ThunderTempest);
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::ThunderTempest) == "THUNDER TEMPEST");

        pm.SetEquippedUltimate(UltimateAbilityType::TimeStasis);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::TimeStasis);

        // Check persistence
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetInt("Profile_EquippedUltimate") == static_cast<int>(UltimateAbilityType::TimeStasis));
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Cosmetic Item architecture and unlocking system")
    {
        auto& pm = ProfileManager::Get();
        auto& pum = PowerUpManager::Get();
        pm.ResetToDefaults();
        pum.ResetToDefaults();

        const auto& cosmetics = pm.GetAvailableCosmetics();
        CHECK(cosmetics.size() >= 4);

        // Default unlocked items
        CHECK(pm.IsCosmeticUnlocked("paddle_neon_cyan") == true);
        CHECK(pm.IsCosmeticUnlocked("trail_plasma_blue") == true);

        // Locked items
        CHECK(pm.IsCosmeticUnlocked("paddle_gold_plating") == false);

        // Attempt unlock without coins
        CHECK(pm.UnlockCosmetic("paddle_gold_plating") == false);
        CHECK(pm.IsCosmeticUnlocked("paddle_gold_plating") == false);

        // Add coins and unlock
        pum.AddCoins(1000);
        CHECK(pm.UnlockCosmetic("paddle_gold_plating") == true);
        CHECK(pm.IsCosmeticUnlocked("paddle_gold_plating") == true);
        CHECK(pum.GetCoins() == 500); // 1000 - 500

        // Equip cosmetic
        pm.EquipCosmetic(CosmeticSlot::PaddleSkin, "paddle_gold_plating");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::PaddleSkin) == "paddle_gold_plating");
    }
}
