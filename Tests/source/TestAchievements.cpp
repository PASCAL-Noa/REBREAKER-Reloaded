#include <doctest/doctest.h>
#include "Core/AchievementManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/ProfileManager.h"
#include "Events/ArenaEvents.h"
#include "Events/EventDispatcher.h"
#include "Events/GameplayEvents.h"
#include "PowerUps/PowerUpConfig.h"
#include <filesystem>

namespace
{
    struct AchievementTestScope
    {
        std::filesystem::path TempProfile;

        AchievementTestScope()
        {
            TempProfile = std::filesystem::temp_directory_path() / "rebreaker_test_achievements_profile.json";
            std::error_code ec;
            std::filesystem::remove(TempProfile, ec);
            std::filesystem::remove(TempProfile.string() + ".tmp", ec);

            PlayerPrefs::DeleteAll();
            ProfileManager::SetCustomProfilePath(TempProfile.string());
            PowerUpManager::Get().ResetToDefaults();
            ProfileManager::Get().ResetToDefaults();
            AchievementManager::Get().SetTrackingEnabled(true);
            AchievementManager::Get().ClearPendingToasts();
        }

        ~AchievementTestScope()
        {
            AchievementManager::Get().UnbindEvents();
            AchievementManager::Get().SetTrackingEnabled(true);
            AchievementManager::Get().ResetToDefaults();
            ProfileManager::Get().ResetToDefaults();
            PowerUpManager::Get().ResetToDefaults();
            PlayerPrefs::DeleteAll();
            ProfileManager::SetCustomProfilePath("");
            std::error_code ec;
            std::filesystem::remove(TempProfile, ec);
            std::filesystem::remove(TempProfile.string() + ".tmp", ec);
        }
    };
}

TEST_SUITE("Achievement System")
{
    TEST_CASE("Catalog completeness and initial zeroed state")
    {
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();

        CHECK(am.GetTotalCount() >= 20);
        CHECK(am.GetUnlockedCount() == 0);
        CHECK(am.GetCompletionPercentage() == doctest::Approx(0.0f));
        CHECK_FALSE(am.HasPendingToast());

        CHECK(am.GetDefinitionById("single_launch_50") != nullptr);
        CHECK(am.GetDefinitionById("endless_10_min") != nullptr);
        CHECK(am.GetDefinitionById("boss_flawless") != nullptr);
        CHECK(am.GetDefinitionById("all_powerups") != nullptr);

        const auto* konami = am.GetDefinitionById("secret_konami");
        REQUIRE(konami != nullptr);
        CHECK(konami->IsSecret);
    }

    TEST_CASE("Hit 50 bricks in a single ball launch via EventDispatcher")
    {
        EventDispatcher dispatcher;
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();
        am.BindEvents(dispatcher);

        am.NotifyBallLaunched();
        for (int i = 0; i < 30; ++i)
        {
            dispatcher.Publish(BrickHitEvent(static_cast<Entity>(i + 1), true, false, static_cast<uint32_t>(i + 1)));
        }
        CHECK_FALSE(am.IsUnlocked("single_launch_50"));
        CHECK(am.GetState("single_launch_50").Progress == 30);
        CHECK(am.IsUnlocked("combo_25"));
        am.ClearPendingToasts();

        // Paddle hit resets current launch streak, keeping historical best progress
        dispatcher.Publish(PaddleHitEvent(100, 101));
        CHECK(am.GetCurrentLaunchBricks() == 0);
        CHECK(am.GetState("single_launch_50").Progress == 30);

        const int coinsBefore = PowerUpManager::Get().GetCoins();
        for (int i = 0; i < 50; ++i)
        {
            dispatcher.Publish(BrickHitEvent(static_cast<Entity>(i + 200), false, false, 1));
        }

        CHECK(am.IsUnlocked("single_launch_50"));
        CHECK(am.GetState("single_launch_50").Progress == 50);
        CHECK_FALSE(am.GetState("single_launch_50").UnlockedTimestamp.empty());
        CHECK(PowerUpManager::Get().GetCoins() == coinsBefore + 350);

        AchievementToast toast{};
        REQUIRE(am.PopNextToast(toast));
        CHECK(toast.Id == "single_launch_50");
        CHECK(toast.RewardCredits == 350);
        CHECK_FALSE(am.HasPendingToast());

        // Further hits must not double-grant credits or duplicate toasts
        const int coinsAfterUnlock = PowerUpManager::Get().GetCoins();
        dispatcher.Publish(BrickHitEvent(999, false, false, 1));
        CHECK(PowerUpManager::Get().GetCoins() == coinsAfterUnlock);
        CHECK_FALSE(am.HasPendingToast());
    }

    TEST_CASE("Survive 10 minutes in Endless unlocks immediately with live waves and cosmetic reward")
    {
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();
        auto& pm = ProfileManager::Get();

        am.NotifyEndlessProgress(599.0f, 4);
        CHECK_FALSE(am.IsUnlocked("endless_10_min"));
        CHECK(am.GetState("endless_10_min").Progress == 599);
        CHECK_FALSE(am.IsUnlocked("endless_wave_5"));
        CHECK_FALSE(pm.IsCosmeticUnlocked("color_pri_obsidian"));

        am.NotifyEndlessProgress(600.0f, 5);
        CHECK(am.IsUnlocked("endless_10_min"));
        CHECK(am.IsUnlocked("endless_wave_5"));
        CHECK(pm.IsCosmeticUnlocked("color_pri_obsidian"));

        AchievementToast first{};
        AchievementToast second{};
        REQUIRE(am.PopNextToast(first));
        REQUIRE(am.PopNextToast(second));
        CHECK(first.Id == "endless_10_min");
        CHECK(second.Id == "endless_wave_5");
        CHECK(second.RewardCosmeticId == "color_pri_obsidian");
    }

    TEST_CASE("Clear a boss without taking paddle damage")
    {
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();

        // Attempt 1: took paddle damage -> unlocks omega slayer, NOT flawless
        am.NotifyBossEncounterStarted();
        am.NotifyPaddleDamaged();
        am.NotifyBossDefeated();

        CHECK(am.IsUnlocked("boss_defeat_omega"));
        CHECK(ProfileManager::Get().IsCosmeticUnlocked("paddle_obsidian"));
        CHECK_FALSE(am.IsUnlocked("boss_flawless"));
        am.ClearPendingToasts();

        // Attempt 2: flawless boss kill -> unlocks boss_flawless
        const int coinsBefore = PowerUpManager::Get().GetCoins();
        am.NotifyBossEncounterStarted();
        am.NotifyBossDefeated();

        CHECK(am.IsUnlocked("boss_flawless"));
        CHECK(PowerUpManager::Get().GetCoins() == coinsBefore + 600);
        AchievementToast toast{};
        REQUIRE(am.PopNextToast(toast));
        CHECK(toast.Id == "boss_flawless");
    }

    TEST_CASE("Trigger all 13 core power-ups via EventDispatcher")
    {
        EventDispatcher dispatcher;
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();
        am.BindEvents(dispatcher);

        // Cheat/null-entity PowerUpEvent must NOT count
        dispatcher.Publish(PowerUpEvent(NULL_ENTITY, PowerUpType::MultiBall));
        CHECK(am.GetState("all_powerups").Progress == 0);

        const PowerUpType allCore[13] = {
            PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::ShrinkPaddle,
            PowerUpType::Storm, PowerUpType::TempoBall, PowerUpType::ExtraLife,
            PowerUpType::BigBall, PowerUpType::FireBall, PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall,
            PowerUpType::ItemRegen
        };

        for (int i = 0; i < 12; ++i)
        {
            dispatcher.Publish(PowerUpEvent(static_cast<Entity>(50 + i), allCore[i]));
            // Duplicate trigger should not advance count
            dispatcher.Publish(PowerUpEvent(static_cast<Entity>(90 + i), allCore[i]));
            CHECK(am.GetState("all_powerups").Progress == i + 1);
            CHECK_FALSE(am.IsUnlocked("all_powerups"));
        }

        dispatcher.Publish(PowerUpEvent(777, allCore[12]));
        CHECK(am.GetState("all_powerups").Progress == 13);
        CHECK(am.IsUnlocked("all_powerups"));
    }

    TEST_CASE("Secret achievements and tracking disabled guard")
    {
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();

        // While tracking is disabled (e.g. Preview/Sandbox), gameplay events are ignored
        am.SetTrackingEnabled(false);
        am.NotifyEndlessProgress(700.0f, 12);
        am.NotifyBossEncounterStarted();
        am.NotifyBossDefeated();
        CHECK_FALSE(am.IsUnlocked("endless_10_min"));
        CHECK_FALSE(am.IsUnlocked("boss_flawless"));

        // Re-enable tracking and test secret triggers
        am.SetTrackingEnabled(true);
        am.NotifyStageCleared(2, 3);
        CHECK_FALSE(am.IsUnlocked("secret_last_stand"));

        am.NotifyStageCleared(2, 1);
        CHECK(am.IsUnlocked("secret_last_stand"));

        am.NotifyKonamiCodeActivated();
        CHECK(am.IsUnlocked("secret_konami"));
    }

    TEST_CASE("Achievement progress, timestamps, and power-up mask persist through ProfileManager")
    {
        AchievementTestScope scope;
        auto& am = AchievementManager::Get();
        auto& pm = ProfileManager::Get();

        am.NotifyPowerUpTriggered(PowerUpType::FireBall);
        am.NotifyPowerUpTriggered(PowerUpType::GhostBall);
        am.NotifyPowerUpTriggered(PowerUpType::MultiBall);
        am.NotifyKonamiCodeActivated();
        for (int i = 0; i < 12; ++i)
        {
            am.NotifyBrickHit(true, 5);
        }

        const std::string savedTimestamp = am.GetState("secret_konami").UnlockedTimestamp;
        const uint32_t savedMask = am.GetCollectedPowerUpMask();
        REQUIRE_FALSE(savedTimestamp.empty());
        pm.SaveProfile();

        // Reset in-memory state via DeserializeFromJson("{}") and reload from profile.json
        am.DeserializeFromJson("{}");
        CHECK_FALSE(am.IsUnlocked("secret_konami"));
        CHECK(am.GetState("all_powerups").Progress == 0);
        CHECK(am.GetTotalBricksDestroyed() == 0);

        pm.LoadProfile();

        CHECK(am.IsUnlocked("secret_konami"));
        CHECK(am.GetState("secret_konami").UnlockedTimestamp == savedTimestamp);
        CHECK(am.GetState("all_powerups").Progress == 3);
        CHECK(am.GetCollectedPowerUpMask() == savedMask);
        CHECK(am.GetTotalBricksDestroyed() == 12);
        CHECK(am.GetState("single_launch_50").Progress == 12);
    }
}
