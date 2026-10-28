#include <doctest/doctest.h>
#include "Core/ProfileManager.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/PlayerPrefs.h"
#include "ECS/Components/UI/TextComponent.h"
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

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Power-up description readability and TextComponent wrapping")
    {
        auto& pum = PowerUpManager::Get();
        for (auto type : {PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::ShrinkPaddle,
                          PowerUpType::Storm, PowerUpType::TempoBall, PowerUpType::ExtraLife,
                          PowerUpType::BigBall, PowerUpType::FireBall, PowerUpType::StickyPaddle,
                          PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall,
                          PowerUpType::ItemRegen})
        {
            const std::string desc = pum.GetDescription(type);
            CHECK(!desc.empty());
            CHECK(desc.length() <= 160);
        }

        TextComponent comp;
        comp.Text = "A long text designed to test wrapping boundaries in UI components.";
        comp.MaxWidth = 400.0f;
        CHECK(comp.MaxWidth == 400.0f);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Ultimate Ability selection and persistence")
    {
        auto& pm = ProfileManager::Get();
        pm.ResetToDefaults();

        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::NovaShockwave);
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::NovaShockwave) == "PLASMA ORB");
        CHECK(!ProfileManager::GetUltimateDescription(UltimateAbilityType::NovaShockwave).empty());

        pm.SetEquippedUltimate(UltimateAbilityType::ThunderTempest);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::ThunderTempest);
        CHECK(ProfileManager::GetUltimateName(UltimateAbilityType::ThunderTempest) == "DISCO BALL");

        pm.SetEquippedUltimate(UltimateAbilityType::TimeStasis);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::TimeStasis);

        // Check persistence
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetInt("Profile_EquippedUltimate") == static_cast<int>(UltimateAbilityType::TimeStasis));
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Ultimate Ability level progression, upgrade costs and tier descriptions")
    {
        auto& pm = ProfileManager::Get();
        auto& pum = PowerUpManager::Get();
        pm.ResetToDefaults();
        pum.ResetToDefaults();

        // Default levels are 1
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 1);
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::ThunderTempest) == 1);
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::TimeStasis) == 1);
        CHECK(pm.GetUltimateMaxLevel(UltimateAbilityType::NovaShockwave) == 5);

        // Categories
        CHECK(!ProfileManager::GetUltimateCategory(UltimateAbilityType::NovaShockwave).empty());
        CHECK(!ProfileManager::GetUltimateCategory(UltimateAbilityType::ThunderTempest).empty());
        CHECK(!ProfileManager::GetUltimateCategory(UltimateAbilityType::TimeStasis).empty());

        // Tier info for levels 1 to 5
        for (int lvl = 1; lvl <= 5; ++lvl)
        {
            CHECK(!ProfileManager::GetUltimateTierInfo(UltimateAbilityType::NovaShockwave, lvl).empty());
            CHECK(!ProfileManager::GetUltimateTierInfo(UltimateAbilityType::ThunderTempest, lvl).empty());
            CHECK(!ProfileManager::GetUltimateTierInfo(UltimateAbilityType::TimeStasis, lvl).empty());
        }

        // Upgrade costs: 250, 400, 650, 950
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::NovaShockwave) == 250);
        CHECK(!pm.CanUpgradeUltimate(UltimateAbilityType::NovaShockwave)); // 0 coins
        CHECK(!pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave));
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 1);

        // Add 250 coins
        pum.AddCoins(250);
        CHECK(pm.CanUpgradeUltimate(UltimateAbilityType::NovaShockwave));
        CHECK(pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave));
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 2);
        CHECK(pum.GetCoins() == 0);

        // Next level cost is 400
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::NovaShockwave) == 400);

        // Upgrade to level 5
        pum.AddCoins(400 + 650 + 950);
        CHECK(pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave)); // -> 3
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 3);
        CHECK(pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave)); // -> 4
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 4);
        CHECK(pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave)); // -> 5
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 5);
        CHECK(pum.GetCoins() == 0);

        // Max level reached
        CHECK(pm.GetUltimateUpgradeCost(UltimateAbilityType::NovaShockwave) == 0);
        CHECK(!pm.CanUpgradeUltimate(UltimateAbilityType::NovaShockwave));
        CHECK(!pm.UpgradeUltimate(UltimateAbilityType::NovaShockwave));
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 5);

        // Persistence test
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetInt("Profile_UltimateLevel_0") == 5);

        // Reset to defaults
        pm.ResetToDefaults();
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::NovaShockwave) == 1);
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

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Expand Paddle scaling, duration and lucky drop progression")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // Level 1: Base duration 10s, scale 1.20x (+20%), drop chance 10%, drop count 1
        pum.SetLevel(PowerUpType::ExpandPaddle, 1);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveScale()).epsilon(0.001f) == 1.20f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDuration()).epsilon(0.001f) == 10.0f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDropChance()).epsilon(0.001f) == 0.10f);
        CHECK(pum.Expand().GetEffectiveDropCount() == 1);

        // Level 2: scale 1.40x (+40%), duration 12s, drop chance 20%, drop count 1
        pum.SetLevel(PowerUpType::ExpandPaddle, 2);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveScale()).epsilon(0.001f) == 1.40f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDuration()).epsilon(0.001f) == 12.0f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDropChance()).epsilon(0.001f) == 0.20f);
        CHECK(pum.Expand().GetEffectiveDropCount() == 1);

        // Level 3: scale 1.60x (+60%), duration 14s, drop chance 30%, drop count 2 (+1 at lvl 3)
        pum.SetLevel(PowerUpType::ExpandPaddle, 3);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveScale()).epsilon(0.001f) == 1.60f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDuration()).epsilon(0.001f) == 14.0f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDropChance()).epsilon(0.001f) == 0.30f);
        CHECK(pum.Expand().GetEffectiveDropCount() == 2);

        // Level 4: scale 1.80x (+80%), duration 16s, drop chance 40%, drop count 2
        pum.SetLevel(PowerUpType::ExpandPaddle, 4);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveScale()).epsilon(0.001f) == 1.80f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDuration()).epsilon(0.001f) == 16.0f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDropChance()).epsilon(0.001f) == 0.40f);
        CHECK(pum.Expand().GetEffectiveDropCount() == 2);

        // Level 5: scale 2.00x (+100%), duration 18s, drop chance 50%, drop count 3 (+1 at lvl 5)
        pum.SetLevel(PowerUpType::ExpandPaddle, 5);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveScale()).epsilon(0.001f) == 2.00f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDuration()).epsilon(0.001f) == 18.0f);
        CHECK(doctest::Approx(pum.Expand().GetEffectiveDropChance()).epsilon(0.001f) == 0.50f);
        CHECK(pum.Expand().GetEffectiveDropCount() == 3);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Shrink Paddle speed bonus, scale retention and lucky drop progression")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // Level 1: Scale 0.65x, duration 10s, speed +20% (840 px/s), drop chance 10%, drop count 1
        pum.SetLevel(PowerUpType::ShrinkPaddle, 1);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveScale()).epsilon(0.001f) == 0.65f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDuration()).epsilon(0.001f) == 10.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeedBonusPercent()).epsilon(0.001f) == 20.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeed()).epsilon(0.001f) == 840.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDropChance()).epsilon(0.001f) == 0.10f);
        CHECK(pum.Shrink().GetEffectiveDropCount() == 1);

        // Level 3: Scale 0.65x, duration 14s, speed +60% (1120 px/s), drop chance 30%, drop count 2
        pum.SetLevel(PowerUpType::ShrinkPaddle, 3);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveScale()).epsilon(0.001f) == 0.65f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDuration()).epsilon(0.001f) == 14.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeedBonusPercent()).epsilon(0.001f) == 60.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeed()).epsilon(0.001f) == 1120.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDropChance()).epsilon(0.001f) == 0.30f);
        CHECK(pum.Shrink().GetEffectiveDropCount() == 2);

        // Level 5: Scale 0.65x, duration 18s, speed +100% (1400 px/s), drop chance 50%, drop count 3
        pum.SetLevel(PowerUpType::ShrinkPaddle, 5);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveScale()).epsilon(0.001f) == 0.65f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDuration()).epsilon(0.001f) == 18.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeedBonusPercent()).epsilon(0.001f) == 100.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectivePaddleSpeed()).epsilon(0.001f) == 1400.0f);
        CHECK(doctest::Approx(pum.Shrink().GetEffectiveDropChance()).epsilon(0.001f) == 0.50f);
        CHECK(pum.Shrink().GetEffectiveDropCount() == 3);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Sticky Paddle rhythm launch boost and hold timer bounds")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // 3.0s auto launch delay
        CHECK(doctest::Approx(pum.Sticky().AutoLaunchDelay).epsilon(0.001f) == 3.0f);

        // Level 1: boost +25% (multiplier 1.25x)
        pum.SetLevel(PowerUpType::StickyPaddle, 1);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostPercent()).epsilon(0.001f) == 25.0f);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostMultiplier()).epsilon(0.001f) == 1.25f);

        // Level 3: boost +55% (multiplier 1.55x)
        pum.SetLevel(PowerUpType::StickyPaddle, 3);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostPercent()).epsilon(0.001f) == 55.0f);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostMultiplier()).epsilon(0.001f) == 1.55f);

        // Level 5: boost +85% (multiplier 1.85x)
        pum.SetLevel(PowerUpType::StickyPaddle, 5);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostPercent()).epsilon(0.001f) == 85.0f);
        CHECK(doctest::Approx(pum.Sticky().GetEffectiveBoostMultiplier()).epsilon(0.001f) == 1.85f);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Magnet Paddle duration scaling, capsule pull speed and ball attraction force")
    {
        auto& pum = PowerUpManager::Get();
        pum.ResetToDefaults();

        // Level 1: duration 10.0s, capsule pull speed 380 px/s, ball pull force 240 px/s²
        pum.SetLevel(PowerUpType::MagnetPaddle, 1);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveDuration()).epsilon(0.001f) == 10.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectivePullSpeed()).epsilon(0.001f) == 380.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveBallPullForce()).epsilon(0.001f) == 240.0f);

        // Level 3: duration 15.0s, capsule pull speed 520 px/s, ball pull force 400 px/s²
        pum.SetLevel(PowerUpType::MagnetPaddle, 3);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveDuration()).epsilon(0.001f) == 15.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectivePullSpeed()).epsilon(0.001f) == 520.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveBallPullForce()).epsilon(0.001f) == 400.0f);

        // Level 5: duration 20.0s, capsule pull speed 660 px/s, ball pull force 560 px/s²
        pum.SetLevel(PowerUpType::MagnetPaddle, 5);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveDuration()).epsilon(0.001f) == 20.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectivePullSpeed()).epsilon(0.001f) == 660.0f);
        CHECK(doctest::Approx(pum.Magnet().GetEffectiveBallPullForce()).epsilon(0.001f) == 560.0f);
    }

    TEST_CASE_FIXTURE(WorkshopEconomyFixture, "Full save data reset wipes coins, upgrades, ultimates, cosmetics and level progression")
    {
        auto& pum = PowerUpManager::Get();
        auto& pm = ProfileManager::Get();

        // Dirty the save data
        pum.AddCoins(2500);
        pum.SetLevel(PowerUpType::MultiBall, 4);
        pum.SetLevel(PowerUpType::ItemRegen, 3);
        pm.SetUsername("CaptainRebreaker");
        pm.AddLifetimeScore(50000);
        pm.AddLifetimeCredits(1200);
        pm.SetEquippedUltimate(UltimateAbilityType::DiscoBall);
        pm.SetUltimateLevel(UltimateAbilityType::PlasmaOrb, 5);
        pm.UnlockCosmetic("paddle_gold_plating");
        PlayerPrefs::SetInt("UnlockedLevel", 5);
        PlayerPrefs::SetInt("HighestClearedLevel", 4);
        PlayerPrefs::SetInt("HighScore_Level_1", 15000);

        CHECK(pum.GetCoins() > 0);
        CHECK(pum.GetLevel(PowerUpType::MultiBall) == 4);
        CHECK(pm.GetUsername() == "CaptainRebreaker");
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::DiscoBall);
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::PlasmaOrb) == 5);
        CHECK(pm.IsCosmeticUnlocked("paddle_gold_plating"));
        CHECK(PlayerPrefs::GetInt("UnlockedLevel") == 5);
        CHECK(PlayerPrefs::GetInt("HighestClearedLevel") == 4);
        CHECK(PlayerPrefs::GetInt("HighScore_Level_1") == 15000);

        // Perform Reset To Defaults on all progression systems
        pum.ResetToDefaults();
        pm.ResetToDefaults();
        PlayerPrefs::SetInt("UnlockedLevel", 1);
        PlayerPrefs::SetInt("HighestClearedLevel", 0);
        PlayerPrefs::SetInt("HighScore_Level_1", 0);
        PlayerPrefs::Save();

        // Check in-memory and persisted values
        CHECK(pum.GetCoins() == 0);
        CHECK(pum.GetLevel(PowerUpType::MultiBall) == 1);
        CHECK(pum.GetLevel(PowerUpType::ItemRegen) == 1);
        CHECK(pm.GetUsername() == "PLAYER");
        CHECK(pm.GetLifetimeScore() == 0);
        CHECK(pm.GetLifetimeCredits() == 0);
        CHECK(pm.GetEquippedUltimate() == UltimateAbilityType::PlasmaOrb);
        CHECK(pm.GetUltimateLevel(UltimateAbilityType::PlasmaOrb) == 1);
        CHECK(!pm.IsCosmeticUnlocked("paddle_gold_plating"));
        CHECK(PlayerPrefs::GetInt("UnlockedLevel") == 1);
        CHECK(PlayerPrefs::GetInt("HighestClearedLevel") == 0);
        CHECK(PlayerPrefs::GetInt("HighScore_Level_1") == 0);
    }
}
